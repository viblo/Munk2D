Munk2D PR #15 -- native Linux x86-64 AVX2 comparison
==================================================

FOR LINUX MINT 20 OR NEWER, 64-BIT INTEL/AMD

1. Extract the entire .tar.gz into a normal writable folder under your home.
2. Open a terminal in the extracted munk2d-avx2-linux-x86_64 folder.
3. Run: ./run.sh
4. Choose F for full, or Q for a quick reference-only screen.
5. Send back the .tar.gz created inside the results folder.

Example:
  tar -xzf munk2d-avx2-linux-x86_64.tar.gz
  cd munk2d-avx2-linux-x86_64
  ./run.sh

No compiler, Bun, pip packages, administrator rights or installation is needed.
Bash and Python 3.8+ are required; these are normally already installed on Mint
20+. Nothing is auto-installed, uploaded or downloaded by the kit. This is NOT a
Windows package or an ARM binary. AVX2 CPU/OS support is checked by the engine;
unsupported machines fail with diagnostics, rather than forcing AVX2 execution.

The two executables use the same native GCC build/settings, static Chipmunk and
only standard Mint system libraries (glibc/libm; exact dependencies in manifest).
They are built against Ubuntu 20.04's older glibc ABI, with required GLIBC symbols
no newer than 2.31, to avoid requiring the build server's newer libc on Mint.
They are NOT globally AVX2/native-tuned; only the existing AVX2 source gets that
flag. No security mitigation, governor, turbo or power setting is changed.

BASELINE / CANDIDATE
  munkbench-baseline   2.1.0, 3d2070976911b7e4b659a5365b0313f99dc98788
  munkbench-candidate PR #15, 5784f8f48728e4fdd71638f0dc11aa26fa07f8ce
Both must stay beside run.py, run.sh and manifest.json. SHA256 checks are mandatory
before and after the test; they use Python's standard-library hashlib.

HOW THIS DIFFERS FROM THE EARLIER WINDOWS KIT
- Linux's unmodified MunkBench clock() measures PROCESS CPU time inside the whole
  physics step (graph/preparation, packing, solver, writeback and fallback).
  It excludes process start/world initialization/cleanup and descheduled waiting.
  Frequency changes and shared-resource contention can still affect it.
- Windows used high-resolution QPC WALL time. Do NOT compare absolute Linux and
  Windows times, or attribute compiler/OS differences solely to the CPU.
- Each scene/case now uses FOUR ADJACENT calls in ABBA or BAAB order. Full has
  8 paired ratios/scene, equally split AB/BA; the old Windows Full had 7.
  A/A and original controls are nearby, not separated by a complete 26-scene run.
- There is a 60-second untimed warmup after the full correctness checks.
- UTC timestamps, whole-invocation wall/child-CPU times and optional sysfs
  frequency/temperature/power snapshots help identify load or clock changes.
  These snapshots are instantaneous/driver-reported, NOT average actual CPU
  frequency during a sample. Whole-invocation diagnostic times INCLUDE setup;
  they must not be confused with the primary physics-only CPU times.

FULL / QUICK
  Full: 26 scenes, smoke + reference, 8 balanced pairs, 3 comparisons = 1248 rows.
        Roughly 20-50 minutes on an older laptop; actual duration varies.
  Quick: 8 collision-focused reference scenes, 4 balanced pairs, 3 comparisons
         = 96 rows. A screen only, NOT a complete suite result.
Both first check all 26 smoke/reference scenes in original and AVX2 modes, twice,
for exact baseline/candidate results, determinism and finite values. Failures
stop the run; no timing result is trusted after a correctness failure.

COMPARISONS
  candidate-avx2  PR #15 AVX2 versus EXISTING AVX2, not the original default.
  AA-control      baseline AVX2 executable compared with itself (noise).
  original-control  candidate original versus baseline original (not SIMD gain).
Negative percentages mean faster. Summaries use the geometric mean of each
scene's median paired ratio. All raw data and regressions are kept. A/A warnings
are diagnostic; no outlier is deleted or score automatically corrected.

PRACTICAL ADVICE
Plug into AC power and close CPU/GPU-heavy apps if practical. Balanced is fine;
Performance is also valid, but keep the same mode throughout. Do not disable
security software/mitigations. Affinity (prefers allowed logical CPU 2) prevents
migration, not interference or frequency changes. Warmup does not guarantee
thermal stability; inspect the noise controls and telemetry before small claims.

No hardware monitoring tool or root access is required. Missing/inaccessible
sysfs readings are stored as null, not invented and not a fatal error. Metadata
collects CPU/OS information, not hostnames, usernames, environment dumps or
serial numbers; tar owner names are normalized. Error traces may contain file
paths, so inspect diagnostic archives before sharing them. No network is used.

Completed raw pairs are appended after each pair, including on interruption.
Ctrl+C stops the benchmark child process group and writes an Interrupted archive.
Each run gets a fresh output folder; existing results are never overwritten.

Optional:
  ./run.sh --mode quick
  ./run.sh --mode full --cpu 2
  ./run.sh --mode full --warmup-seconds 120
  ./run.sh --mode quick --output /new/results/directory
If extraction lost execute permissions, restore them with:
  chmod +x run.sh munkbench-baseline munkbench-candidate
If your filesystem is mounted noexec, extract under your home rather than a
shared/Windows/temp filesystem. No sudo should be needed.

Source PR: https://github.com/vibloteket/munk2d/pull/15
Build provenance, dependencies and executable hashes: manifest.json
License: LICENSE.txt
