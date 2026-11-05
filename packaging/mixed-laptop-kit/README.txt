Munk2D native AVX2 mixed-joint comparison — PR #17

OFFLINE, UNSIGNED TEST BINARIES. x86-64 CPU with AVX2 required (both your
Intel i5-8265U and Ultra 9 185H support it). No compiler, Python, Bun,
installation, network access, admin privileges, or system changes needed.
Linux: Mint 20+ / glibc 2.31+, Bash and taskset (normally preinstalled).
Windows: Windows 10/11 x64; built natively with MSVC, static /MT CRT.

1. Extract the whole archive to a writable local folder under Downloads/home.
   Windows: right-click ZIP > Extract All; do NOT run inside the ZIP.
2. Connect AC power, close CPU-heavy programs and keep the same power mode.
   This does NOT change your governor, power plan, security settings or clock.
3. Windows: double-click Run.cmd. Linux: open terminal here, ./run.sh
4. Choose F (Full, recommended) or Q (Quick). Leave it running until complete.
   Full can take tens of minutes on a laptop; actual time depends on the CPU.
5. Send the generated results/<platform>-mixed-...zip back in chat.
   Everything remains local; nothing is automatically uploaded.

Quick: 4 existing scenes at both sizes + 6 mixed cases, 4 pairs/comparison
       = 224 pair rows. Selected diagnostic screen, NOT a full-suite result.
Full:  all 26 existing scenes at both sizes + 6 mixed cases, 8 pairs/comparison
       = 1856 pair rows. Both include Tumbler and ConstraintMix regressions.
The three extra mixed fixtures have light joints, dense joints, and motor/pivot
mechanisms plus balls, at 64 and 256 bodies. This kit does NOT include EBM,
Pymunk, Pyodide, Wasm, browser rendering or an application FPS benchmark.

What is compared (A -> B):
 prior-avx2:       previous AVX2 -> PR17 AVX2 (main incremental result)
 primary:         PR17 original solver -> PR17 AVX2 (total SIMD effect)
 aa:              PR17 original -> identical PR17 original (noise control)
 original-control: previous original -> PR17 original (build/noise control)
Old engine: 0c4b0ff81d1d47040c7043d487c9414058c30faa (merged Wasm PR16).
New engine: 19dfdeacbf14b0b24a0d789e0330dab15c82a1d3 (PR17).
Includes the previously merged Wasm baseline, NOT the old PR15 laptop kit.

Protocol:
- SHA256 binaries/runner verified before and after, and all26 smoke/reference
  original/AVX2 states compared old/new and independently repeated.
- Six mixed checkpoints compare both builds/solvers; candidate repeated.
  Finite state required; a mismatch stops timings (send the failed ZIP).
- One-minute sustained warmup. This is NOT proof of thermal stability.
- Equal batch per case, calibrated across oldAVX2/newAVX2/new-original to
  >=80ms stepping, initialization separate. Batch/world recreation is timed
  for setup separately; whole-step includes packing/preparation/sync/fallback.
- Locally interleaved ABBA/BAAB blocks, equal AB/BA counts, rotating/reversing
  case order. No outlier removal or mathematical control subtraction.
- CPU2 if allowed, otherwise first allowed logical CPU. Affinity does not
  reserve a core or fix frequency. On hybrid CPUs this samples one logical
  core, not every P/E-core class. Optional: runner[.exe] --mode full --cpu N
- Windows: high-resolution QPC wall time; Linux: process CPU time (clock).
  Do NOT compare absolute times between OSes or call differences purely CPU.
- Before/after each child sample: timestamps and available power/load data.
  Linux includes optional driver instantaneous frequency, temperatures and
  governor. Missing readings are empty, not errors. Windows uses stock native
  power/CPU counters; CPU/OS/model/power plan are queried once via PowerShell.
- InvocationWall/CPUSeconds INCLUDE setup+cleanup, unlike physics RunSeconds.

SUMMARY.txt: per-case median paired ratios, full range, faster counts, order
counts; equal-case geometric means. Negative = less time. A/A warnings are
diagnostics, NOT automatic causality or universal performance conclusions.
JSONL preserves raw invocations/pairs; raw/ contains all stdout/stderr.
Ctrl+C stops the current engine, retains complete blocks/pairs, and writes
an Interrupted ZIP. Do not mix partial and complete results or resume manually.
Each run creates a fresh directory, refusing to overwrite existing output.

Privacy: hardware model, OS/build, compiler, versions, power-plan name, optional
thermal/frequency/load/battery readings and raw timings are collected. No
hostname, username, serial number, IP or environment listing is deliberately
collected. Error text may include local paths: inspect before sharing.
Unsigned Windows executables can trigger SmartScreen. Do not disable security;
only run these files if you trust their source. No signing/release claim.
