#!/usr/bin/env python3
"""Offline Linux AVX2 comparison; Python 3.8+ standard library only."""
import argparse
import csv
import datetime
import hashlib
import io
import json
import math
import os
from pathlib import Path
import platform
import resource
import signal
import statistics
import subprocess
import sys
import tarfile
import time
import traceback

BASELINE = "3d2070976911b7e4b659a5365b0313f99dc98788"
CANDIDATE = "5784f8f48728e4fdd71638f0dc11aa26fa07f8ce"
LABELS = ("candidate-avx2", "AA-control", "original-control")
QUICK_SCENES = ("FallingSquares", "FallingCircles", "N2", "MostlyStaticSingleBody", "MostlyStaticMultiBody", "FrictionalPyramid", "CollisionCallbacks", "SleepWake")
HEADER = ["version", "benchmark", "size", "sample", "batch", "init_time", "run_time"]


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat().replace("+00:00", "Z")


def save_json(path, value):
    temp = path.with_name(path.name + ".tmp")
    temp.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    temp.replace(path)


def sha256(path):
    digest = hashlib.sha256()
    with open(str(path), "rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(data)
    return digest.hexdigest()


def read_text(path):
    try:
        return Path(path).read_text(encoding="utf-8").strip()
    except (OSError, UnicodeError):
        return None


def number(path, divisor=1):
    value = read_text(path)
    try:
        value = float(value) / divisor
        return value if math.isfinite(value) else None
    except (ValueError, TypeError):
        return None


def optional_glob(path, pattern):
    try:
        return sorted(Path(path).glob(pattern))
    except OSError:
        return []


def cpu_description(cpu):
    for section in (read_text("/proc/cpuinfo") or "").split("\n\n"):
        lines = section.splitlines()
        if lines and lines[0].split(":")[-1].strip() == str(cpu):
            return section
    return None


def telemetry(cpu):
    root = Path("/sys/devices/system/cpu/cpu%d/cpufreq" % cpu)
    thermal = []
    for zone in optional_glob("/sys/class/thermal", "thermal_zone*"):
        kind = read_text(zone / "type")
        if kind in ("x86_pkg_temp", "acpitz", "cpu-thermal", "soc_thermal"):
            thermal.append({"Type": kind, "Celsius": number(zone / "temp", 1000)})
    power = []
    for device in optional_glob("/sys/class/power_supply", "*"):
        kind = read_text(device / "type")
        if kind in ("Mains", "USB", "USB_C", "Battery"):
            power.append({"Type": kind, "Online": number(device / "online"), "Status": read_text(device / "status"), "CapacityPercent": number(device / "capacity")})
    proc_stat = read_text("/proc/stat") or ""
    stat = next((line for line in proc_stat.splitlines() if line.startswith("cpu%d " % cpu)), None)
    return {"Utc": utc(), "MonotonicSeconds": time.monotonic(), "LoadAverage": list(os.getloadavg()), "CpuStat": stat,
            "ScalingCurrentKHz": number(root / "scaling_cur_freq"), "HardwareCurrentKHz": number(root / "cpuinfo_cur_freq"),
            "ScalingMinKHz": number(root / "scaling_min_freq"), "ScalingMaxKHz": number(root / "scaling_max_freq"),
            "Governor": read_text(root / "scaling_governor"), "EnergyPreference": read_text(root / "energy_performance_preference"),
            "Temperatures": thermal, "Power": power}


def parse_timings(text, expected):
    reader = csv.DictReader(io.StringIO(text))
    if reader.fieldnames != HEADER:
        raise ValueError("Unexpected MunkBench CSV header")
    rows, seen = [], set()
    for row in reader:
        name = row["benchmark"]
        if name not in expected or name in seen:
            raise ValueError("Unexpected or duplicate benchmark: " + name)
        seen.add(name)
        t, size, batch = float(row["run_time"]), int(row["size"]), int(row["batch"])
        if not math.isfinite(t) or t <= 0 or size < 1 or batch < 1 or row["sample"] != "1":
            raise ValueError("Invalid timing or sample metadata: " + str(row))
        rows.append({"Benchmark": name, "Size": size, "Batch": batch, "Time": t})
    if seen != set(expected) or len(rows) != len(expected):
        raise ValueError("Missing benchmark rows")
    return rows


def check_pair(a, b):
    if len(a) != len(b) or any((x["Benchmark"], x["Size"], x["Batch"]) != (y["Benchmark"], y["Size"], y["Batch"]) for x, y in zip(a, b)):
        raise ValueError("Baseline/candidate protocol mismatch")


def check_summary(text):
    obj = json.loads(text)
    scenes = obj["benchmarks"]
    if len(scenes) != 26 or len(set(x["benchmark"] for x in scenes)) != 26:
        raise ValueError("Expected 26 unique validation scenes")
    def finite(x):
        if isinstance(x, float) and not math.isfinite(x):
            raise ValueError("Non-finite physics summary")
        if isinstance(x, dict):
            for value in x.values(): finite(value)
        elif isinstance(x, list):
            for value in x: finite(value)
    finite(obj)
    for scene in scenes:
        if not scene["checkpoints"] or any(x["invalid_values"] != 0 for x in scene["checkpoints"]):
            raise ValueError("Invalid physics values: " + scene["benchmark"])
    return obj


def geomean(values):
    return math.exp(sum(math.log(x) for x in values) / len(values))


def summarize(raw, profiles, scenes, rounds):
    table, totals = [], []
    for profile in profiles:
        for label in LABELS:
            medians, round_gm = [], []
            for scene in scenes:
                rows = [x for x in raw if x["Profile"] == profile and x["Label"] == label and x["Benchmark"] == scene]
                if len(rows) != rounds or set(x["Round"] for x in rows) != set(range(rounds)) or sum(x["Order"] == "AB" for x in rows) != rounds // 2:
                    raise ValueError("Missing/duplicate/unbalanced timing pairs")
                if len(set((x["Size"], x["Batch"]) for x in rows)) != 1:
                    raise ValueError("Scene size/batch changed across rounds")
                ratios = [x["Ratio"] for x in rows]
                median = statistics.median(ratios)
                medians.append(median)
                table.append({"Profile": profile, "Label": label, "Benchmark": scene, "Percent": 100 * (median - 1), "FasterPairs": sum(x < 1 for x in ratios), "Pairs": rounds,
                              "MinPercent": 100 * (min(ratios) - 1), "MaxPercent": 100 * (max(ratios) - 1)})
            for round_index in range(rounds):
                values = [x["Ratio"] for x in raw if x["Profile"] == profile and x["Label"] == label and x["Round"] == round_index]
                round_gm.append(100 * (geomean(values) - 1))
            totals.append({"Profile": profile, "Label": label, "Scenes": len(scenes), "PairsPerScene": rounds, "GeomeanPercent": 100 * (geomean(medians) - 1), "RoundGeomeanPercent": round_gm})
    return {"Totals": totals, "Scenes": table}


class Runner:
    def __init__(self, package, output, mode, cpu, warmup):
        self.package, self.output, self.mode, self.cpu, self.warmup = package, output, mode, cpu, warmup
        self.process = None
        self.sequence = 0
        self.raw = []
        self.calls = open(str(output / "invocations.jsonl"), "w", encoding="utf-8")
        self.pairs = open(str(output / "timing-pairs.jsonl"), "w", encoding="utf-8")
        self.env = dict(os.environ, LC_ALL="C", LANG="C")
        self.base, self.candidate = package / "munkbench-baseline", package / "munkbench-candidate"
        self.manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))

    def verify_hashes(self):
        entries = self.manifest["executables"]
        if len(entries) != 2 or set(x["name"] for x in entries) != {self.base.name, self.candidate.name}:
            raise ValueError("Wrong executable manifest")
        if self.manifest["baseline"] != BASELINE or self.manifest["candidate"] != CANDIDATE:
            raise ValueError("Unexpected source revisions")
        for item in entries:
            path = self.package / item["name"]
            if sha256(path) != item["sha256"]:
                raise ValueError("Executable checksum mismatch: " + path.name)
            if not os.access(str(path), os.X_OK):
                raise ValueError("Not executable; extract the tar.gz under your home directory: " + path.name)

    def stop(self):
        if self.process is not None:
            if self.process.poll() is None:
                try: os.killpg(self.process.pid, signal.SIGTERM)
                except ProcessLookupError: pass
                try: self.process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    os.killpg(self.process.pid, signal.SIGKILL)
                    self.process.wait()
            self.process = None

    def invoke(self, exe, args, stage):
        self.sequence += 1
        record = {"Sequence": self.sequence, "Stage": stage, "Executable": exe.name, "Arguments": args, "StartedUtc": utc()}
        usage = resource.getrusage(resource.RUSAGE_CHILDREN)
        start = time.monotonic()
        try:
            self.process = subprocess.Popen([str(exe)] + args, cwd=str(self.package), env=self.env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, encoding="utf-8", errors="strict", start_new_session=True)
            stdout, stderr = self.process.communicate(timeout=900)
            code = self.process.returncode
        except BaseException:
            self.stop()
            raise
        self.process = None
        end_usage = resource.getrusage(resource.RUSAGE_CHILDREN)
        record.update({"EndedUtc": utc(), "WallSecondsIncludingSetup": time.monotonic() - start,
                       "ChildCpuSecondsIncludingSetup": end_usage.ru_utime + end_usage.ru_stime - usage.ru_utime - usage.ru_stime, "ExitCode": code})
        self.calls.write(json.dumps(record, allow_nan=False) + "\n"); self.calls.flush()
        if code != 0:
            (self.output / "last-error-stdout.txt").write_text(stdout, encoding="utf-8")
            (self.output / "last-error-stderr.txt").write_text(stderr, encoding="utf-8")
            raise RuntimeError("%s failed (exit %d). AVX2 may be unavailable.\n%s" % (exe.name, code, stderr))
        return stdout, record

    def validate(self):
        records, names = [], None
        for profile in ("smoke", "reference"):
            for solver in ("original", "avx2"):
                print("Exact validation: %s / %s" % (profile, solver), flush=True)
                args = ["--profile", profile, "--contact-solver", solver, "--summary-json", "--checkpoints", "0,1,10,50,180,181,final"]
                a, _ = self.invoke(self.base, args, "validation")
                b, _ = self.invoke(self.candidate, args, "validation")
                c, _ = self.invoke(self.base, args, "validation-repeat")
                e, _ = self.invoke(self.candidate, args, "validation-repeat")
                obj = check_summary(a); check_summary(b)
                if names is None: names = [x["benchmark"] for x in obj["benchmarks"]]
                (self.output / ("baseline-%s-%s.json" % (profile, solver))).write_text(a, encoding="utf-8")
                (self.output / ("candidate-%s-%s.json" % (profile, solver))).write_text(b, encoding="utf-8")
                record = {"Profile": profile, "Solver": solver, "Exact": a == b, "BaselineDeterministic": a == c, "CandidateDeterministic": b == e}
                records.append(record); save_json(self.output / "validation.json", records)
                if not all(record[x] for x in ("Exact", "BaselineDeterministic", "CandidateDeterministic")):
                    raise RuntimeError("Numerical validation mismatch; timings must not be trusted")
        return names

    def measure(self, names):
        if self.mode == "full": scenes, rounds, profiles = names, 8, ["smoke", "reference"]
        elif self.mode == "quick": scenes, rounds, profiles = list(QUICK_SCENES), 4, ["reference"]
        else: scenes, rounds, profiles = ["FallingSquares", "N2"], 2, ["smoke", "reference"]
        print("Untimed warmup: at least %d seconds after validation" % self.warmup, flush=True)
        warm_start, turn = time.monotonic(), 0
        while time.monotonic() - warm_start < self.warmup:
            exe = self.base if turn % 2 == 0 else self.candidate
            self.invoke(exe, ["--profile", "reference", "--contact-solver", "avx2", "--batch", "auto", "--samples", "1", "-b", "FallingSquares", "FrictionalPyramid"], "warmup")
            turn += 1
        cases = [(LABELS[0], self.candidate, "avx2"), (LABELS[1], self.base, "avx2"), (LABELS[2], self.candidate, "original")]
        for profile in profiles:
            for block in range(rounds // 2):
                scene_order = list(scenes) if block % 2 == 0 else list(reversed(scenes))
                for index, scene in enumerate(scene_order):
                    shift = (index + block) % len(cases)
                    case_order = cases[shift:] + cases[:shift]
                    for label, candidate, solver in case_order:
                        print("%s / %s / rounds %d-%d of %d / %s" % (profile, scene, 2 * block + 1, 2 * block + 2, rounds, label), flush=True)
                        args = ["--profile", profile, "--contact-solver", solver, "--batch", "auto", "--samples", "1", "-b", scene]
                        # Four adjacent calls are ABBA or BAAB; each yields two paired ratios.
                        for half in range(2):
                            ab = (block + half) % 2 == 0
                            order = [("A", self.base), ("B", candidate)] if ab else [("B", candidate), ("A", self.base)]
                            before, outputs = telemetry(self.cpu), {}
                            for tag, exe in order:
                                text, invocation = self.invoke(exe, args, "timing")
                                outputs[tag] = (parse_timings(text, [scene]), invocation)
                            after = telemetry(self.cpu)
                            check_pair(outputs["A"][0], outputs["B"][0])
                            a, b = outputs["A"][0][0], outputs["B"][0][0]
                            row = {"Profile": profile, "Label": label, "Round": 2 * block + half, "Benchmark": scene, "Size": a["Size"], "Batch": a["Batch"],
                                   "BaselineSeconds": a["Time"], "CandidateSeconds": b["Time"], "Ratio": b["Time"] / a["Time"], "Order": "AB" if ab else "BA",
                                   "BaselineInvocation": outputs["A"][1]["Sequence"], "CandidateInvocation": outputs["B"][1]["Sequence"], "Before": before, "After": after}
                            self.raw.append(row)
                            self.pairs.write(json.dumps(row, allow_nan=False) + "\n"); self.pairs.flush()
        result = summarize(self.raw, profiles, scenes, rounds)
        save_json(self.output / "timing-pairs.json", self.raw)
        save_json(self.output / "summary.json", result)
        with open(str(self.output / "summary.csv"), "w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(result["Scenes"][0]))
            writer.writeheader(); writer.writerows(result["Scenes"])
        lines = ["Munk2D Linux AVX2 comparison", "Mode: %s; CPU: %d; correctness: PASS" % (self.mode, self.cpu),
                 "Negative = faster. candidate-avx2 compares PR #15 to EXISTING AVX2, not the original default.",
                 "Primary time: MunkBench clock(), Linux PROCESS CPU seconds in complete physics stepping; setup/cleanup excluded.",
                 "Invocation wall/CPU diagnostics include setup and MUST NOT be called physics-only times.",
                 "Adjacent ABBA/BAAB calls, equal AB/BA counts, separate A/A and original controls; no outliers deleted."]
        if self.mode == "quick": lines.append("QUICK: 8 selected reference scenes only; not a full-suite conclusion.")
        if self.mode == "ci": lines.append("CI SELF-TEST ONLY: these timings are not performance evidence.")
        for total in result["Totals"]:
            lines.append("%-10s %-20s %+.2f%% (%d scenes, %d pairs/scene)" % (total["Profile"], total["Label"], total["GeomeanPercent"], total["Scenes"], total["PairsPerScene"]))
            if total["Label"] == "AA-control" and (abs(total["GeomeanPercent"]) > 2 or max(abs(x) for x in total["RoundGeomeanPercent"]) > 5):
                lines.append("WARNING: A/A variation is large relative to the expected small gain; performance may be inconclusive.")
        lines.extend(["", "Frequency/temperature readings are optional instantaneous snapshots, NOT measured averages during a sample.", "Affinity does not reserve a core or fix frequency. Power/governor/security settings were not changed.", "All regressions and raw data are retained. Review controls before drawing performance conclusions."])
        text = "\n".join(lines) + "\n"
        (self.output / "SUMMARY.txt").write_text(text, encoding="utf-8")
        print("\n" + text, flush=True)
        return len(self.raw)

    def close(self):
        self.stop(); self.calls.close(); self.pairs.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("full", "quick", "ci"), default="full")
    parser.add_argument("--cpu", type=int, default=None)
    parser.add_argument("--warmup-seconds", type=int, default=60)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    if sys.version_info < (3, 8) or sys.platform != "linux" or platform.machine().lower() not in ("x86_64", "amd64"):
        parser.error("Requires 64-bit x86 Linux and Python 3.8+ (standard Linux Mint 20+)")
    if args.warmup_seconds < 0: parser.error("Warmup must be nonnegative")
    package = Path(__file__).resolve().parent
    allowed = os.sched_getaffinity(0)
    cpu = args.cpu if args.cpu is not None else (2 if 2 in allowed else min(allowed))
    if cpu not in allowed: parser.error("Requested CPU is not in the allowed affinity mask")
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = (args.output or package / "results" / ("linux-avx2-%s-%s-%d" % (args.mode, stamp, os.getpid()))).resolve()
    output.mkdir(parents=True, exist_ok=False)
    started, runner = time.monotonic(), None
    def interrupted(*unused):
        raise KeyboardInterrupt()
    old_term = signal.signal(signal.SIGTERM, interrupted)
    metadata = {"Status": "Running", "StartedUtc": utc(), "Mode": args.mode, "Protocol": "Linux adjacent ABBA v1: full8/quick4 balanced pairs", "AffinityCpu": cpu,
                "Python": platform.python_version(), "Kernel": platform.release(), "Architecture": platform.machine(), "OSRelease": read_text("/etc/os-release"),
                "Libc": os.confstr("CS_GNU_LIBC_VERSION"), "CpuInfo": cpu_description(cpu),
                "GatherDataSamplingStatus": read_text("/sys/devices/system/cpu/vulnerabilities/gather_data_sampling"), "Source": None, "RunnerSHA256": sha256(Path(__file__)), "Executables": []}
    code = 0
    try:
        print("Munk2D native Linux x86-64 AVX2 comparison\nPlug into AC power and leave the machine idle; no settings or security mitigations will be changed.", flush=True)
        os.sched_setaffinity(0, {cpu})
        runner = Runner(package, output, args.mode, cpu, args.warmup_seconds)
        runner.verify_hashes()
        metadata["Source"] = runner.manifest
        metadata["Executables"] = [{"Name": x["name"], "SHA256": x["sha256"]} for x in runner.manifest["executables"]]
        metadata["InitialTelemetry"] = telemetry(cpu)
        save_json(output / "metadata.json", metadata)
        print("Mode: %s; logical CPU: %d; results: %s" % (args.mode, cpu, output), flush=True)
        names = runner.validate()
        metadata["Pairs"] = runner.measure(names)
        runner.verify_hashes()
        metadata["Status"] = "PASS"
    except KeyboardInterrupt:
        metadata["Status"] = "Interrupted"; code = 130
        print("Interrupted; completed raw pairs are preserved.", file=sys.stderr)
    except Exception:
        metadata["Status"] = "FAIL"; code = 1
        error = traceback.format_exc()
        (output / "ERROR.txt").write_text(error, encoding="utf-8")
        print(error + "Please send the diagnostic results archive even if this failed.", file=sys.stderr)
    finally:
        if runner is not None: runner.close()
        os.sched_setaffinity(0, allowed)
        signal.signal(signal.SIGTERM, old_term)
        metadata.update({"EndedUtc": utc(), "ElapsedSeconds": time.monotonic() - started, "FinalTelemetry": telemetry(cpu)})
        save_json(output / "metadata.json", metadata)
        archive = Path(str(output) + ".tar.gz")
        def anonymous_owner(info):
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            return info
        with tarfile.open(str(archive), "w:gz") as tar:
            for path in sorted(output.iterdir()):
                if path.is_file() and not path.is_symlink(): tar.add(str(path), arcname=path.name, filter=anonymous_owner)
        print("\nPlease send this file back: %s" % archive, flush=True)
    return code


if __name__ == "__main__":
    sys.exit(main())
