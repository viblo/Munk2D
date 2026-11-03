#!/usr/bin/env python3
"""Build-host tests; not required on the laptop."""
import importlib.util
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
import time
import unittest

spec = importlib.util.spec_from_file_location("kit_runner", str(Path(__file__).with_name("run.py")))
kit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(kit)
HEADER = ",".join(kit.HEADER)


class RunnerTests(unittest.TestCase):
    def test_sha(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "hash [fixture] with spaces"
            path.write_bytes(b"abc")
            self.assertEqual(kit.sha256(path), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
            path.write_bytes(b"")
            self.assertEqual(kit.sha256(path), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")

    def test_csv(self):
        def text(time="2.0", sample="1", name="Test", batch="4"):
            return HEADER + "\n2.1.0,%s,10,%s,%s,0.1,%s\n" % (name, sample, batch, time)
        a = kit.parse_timings(text(), ["Test"])
        self.assertEqual(a[0]["Time"], 2)
        for value in ("NaN", "nan", "Infinity", "-Infinity", "0", "-1"):
            with self.assertRaises(ValueError): kit.parse_timings(text(time=value), ["Test"])
        for t in (text(sample="0"), text(name="Wrong"), text(batch="0"), text() + text().splitlines()[1] + "\n"):
            with self.assertRaises(ValueError): kit.parse_timings(t, ["Test"])
        with self.assertRaises(ValueError): kit.check_pair(a, kit.parse_timings(text(batch="5"), ["Test"]))

    def test_summary_validation(self):
        obj = {"benchmarks": [{"benchmark": "Test%d" % i, "checkpoints": [{"invalid_values": 0}]} for i in range(26)]}
        kit.check_summary(json.dumps(obj))
        obj["benchmarks"][0]["checkpoints"][0]["invalid_values"] = 1
        with self.assertRaises(ValueError): kit.check_summary(json.dumps(obj))

    def test_stats_and_protocol(self):
        rows = []
        for label in kit.LABELS:
            for r in range(2):
                rows.append({"Profile": "reference", "Label": label, "Benchmark": "Test", "Round": r, "Size": 1, "Batch": 1, "Order": "AB" if r == 0 else "BA", "Ratio": 0.9 if label == "candidate-avx2" else 1.0})
        result = kit.summarize(rows, ["reference"], ["Test"], 2)
        self.assertAlmostEqual(result["Totals"][0]["GeomeanPercent"], -10)
        for field, value in (("Round", 0), ("Order", "AB"), ("Batch", 2)):
            damaged = [dict(x) for x in rows]
            damaged[1][field] = value
            with self.assertRaises(ValueError): kit.summarize(damaged, ["reference"], ["Test"], 2)

    def test_manifest_and_interruption(self):
        with tempfile.TemporaryDirectory() as directory:
            package, output = Path(directory) / "package", Path(directory) / "output"
            package.mkdir(); output.mkdir()
            entries = []
            for name in ("munkbench-baseline", "munkbench-candidate"):
                path = package / name
                path.write_bytes(b"#!/bin/sh\nsleep 30\n")
                path.chmod(0o755)
                entries.append({"name": name, "sha256": kit.sha256(path)})
            (package / "manifest.json").write_text(json.dumps({"baseline": kit.BASELINE, "candidate": kit.CANDIDATE, "executables": entries}))
            runner = kit.Runner(package, output, "ci", min(os.sched_getaffinity(0)), 0)
            try:
                runner.verify_hashes()
                (package / "munkbench-candidate").write_bytes(b"tampered")
                with self.assertRaises(ValueError): runner.verify_hashes()
                runner.process = subprocess.Popen(["/bin/sh", "-c", "sleep 30"], start_new_session=True)
                process = runner.process
                runner.stop()
                self.assertIsNotNone(process.poll())
            finally:
                runner.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
