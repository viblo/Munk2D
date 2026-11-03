Munk2D native Windows x64 AVX2 comparison -- PR #15
=================================================

1. Extract the ENTIRE ZIP to a normal writable folder (not inside the ZIP).
2. Plug the laptop into AC power, close CPU-heavy apps, then double-click Run.cmd.
3. Choose F for the full test (recommended, about 10-30 minutes), or Q for a
   quick selected-scene screen (about 2-5 minutes). Actual time varies by CPU.
4. Send back the ZIP created inside the results folder. SUMMARY.txt is readable.

No WSL, Bun, Python, compiler, administrator rights or installation is needed.
Requires native Windows 10/11 x64, Intel/AMD AVX2 with OS support, and the bundled
Windows PowerShell 5.1. Unsupported AVX2 is detected and reported, not forced.
These are unsigned test executables; Windows/security software may flag or
quarantine unsigned downloads. Do not disable security mitigations or antivirus.

FILES
  munkbench-baseline.exe   current 2.1.0 engine, commit 3d20709
  munkbench-candidate.exe  PR #15, commit 5784f8f
  Run.cmd / Run.ps1        paired comparison and exact state checks
  manifest.json           full source refs, compiler/build details, SHA256 hashes

Both executables are built natively with the same MSVC x64 Release/LTO settings,
static Chipmunk and static MSVC runtime (/MT). No companion DLL is required.
Only the existing AVX2 translation unit receives /arch:AVX2. The original solver
remains the default; the script explicitly selects each comparison mode.

BENCHMARK TIMER NOTE
The same benchmark-only patch is applied to BOTH sources: Windows uses
QueryPerformanceCounter instead of MSVC clock(), whose millisecond precision is
poor for small differences. Engine/solver source is otherwise exactly the pinned
revision. Results are high-resolution WALL time inside the complete physics
step, including graph preparation, packing, solving, writeback and fallback.
World creation, cleanup and process startup are outside the timed region.
Windows/MSVC results should not be treated as identical to Linux/GCC results.

WHAT THE SCRIPT DOES
- Verifies executable SHA256 hashes; sets one allowed logical CPU affinity for
  itself/children (prefers CPU 2) and restores its own affinity at the end.
- Records CPU/model/OS/power-plan details, not usernames or machine serials.
- Checks all 26 smoke/reference scenes, original and AVX2, at multiple checkpoints
  against the matching baseline mode, with repeated determinism checks.
- Runs untimed warmups then alternating AB/BA paired batches, A/A noise controls
  and original-to-original controls. Full: 26 scenes, 7 rounds, both profiles.
- Saves all raw data and regressions. Negative candidate-avx2 percent = faster
  than EXISTING AVX2, not faster than the original default solver.
- Creates a results ZIP; it does NOT upload anything or access the network.

Do not change power plans, background load, affinity or security settings between
variants. Leave the machine idle during the test. Quick is only a screen; use
Full and inspect A/A noise before judging a ~1% gain. A different CPU/compiler
may show smaller gains or regressions.

If validation fails or the CPU lacks AVX2, send the diagnostic results ZIP anyway.
No timing result is trusted after a correctness failure. Ctrl+C may interrupt a
run; completed raw pairs are saved after each pair. Close other runs first.

Run.cmd uses a process-local PowerShell execution-policy override to run this
local script; it does not permanently change your execution policy. The scripts
are plain text for inspection.

Optional command-line usage (ordinary non-admin PowerShell):
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run.ps1 -Mode Full
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run.ps1 -Mode Quick
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run.ps1 -Mode Full -Cpu 2

Source PR: https://github.com/vibloteket/munk2d/pull/15
