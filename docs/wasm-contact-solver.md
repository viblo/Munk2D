# Optional WebAssembly SIMD contact solver

This is an explicit, double-precision, single-threaded backend for
`cpSpaceStep()`. It uses standard WebAssembly SIMD (`f64x2`), not AVX2/NEON
intrinsics, relaxed SIMD, mixed precision or browser threads. The original
solver remains the default. SIMD availability is **not** a speed guarantee.

## Build and select

Activate an Emscripten SDK, then:

```sh
emcmake cmake -S . -B build-wasm -DBUILD_DEMOS=OFF -DBUILD_SHARED=OFF \
  -DBUILD_STATIC=ON -DMUNK2D_ENABLE_WASM_SIMD_CONTACT_SOLVER=ON
cmake --build build-wasm
```

Link consumers with `emcc`; the CMake static target propagates `-msimd128` to the
final link. Manual archive consumers must also pass `-msimd128` at the final
link. Only `cpContactSolverWasm.c` needs SIMD at compilation; its formulas and
the same-schedule scalar reference disable floating-point contraction, and the
SIMD translation unit is excluded from LTO. Do not use fast-math.

```c
cpSpace *space = cpSpaceNew();
if(cpContactSolverIsAvailable(CP_CONTACT_SOLVER_WASM_SIMD)){
    if(!cpSpaceSetContactSolver(space, CP_CONTACT_SOLVER_WASM_SIMD)){
        /* Allocation failure, for example. The selection is unchanged. */
    }
}
cpSpaceStep(space, 1.0/60.0);
cpSpaceSetContactSolver(space, CP_CONTACT_SOLVER_ORIGINAL); /* releases scratch */
cpSpaceFree(space);
```

The option defaults to **OFF**, even for Emscripten. Explicit ON with a
non-Emscripten compiler, float build or missing SIMD support is a configure
error. AVX2 stays unavailable in Wasm; WASM_SIMD stays unavailable in ordinary
native or SIMD-disabled builds. There is no new AUTO policy in the C API.

For non-CMake integrations, compile all generic solver sources with
`-DCP_WASM_SIMD_CONTACT_SOLVER=1`, compile `cpContactSolverWasm.c` with
`-msimd128 -ffp-contract=off -fno-lto`, compile `cpContactSolverScalar.c` with
`-ffp-contract=off`, and link with `-msimd128`. Do not define the backend macro
without actually compiling/linking its implementation.

## Loading policy: two artifacts, not an in-module feature fallback

A module containing SIMD instructions requires a SIMD-capable runtime **at
validation/loading time**, even if ORIGINAL is selected and the SIMD function
is never called. `cpContactSolverIsAvailable()` cannot protect an unsupported
runtime from loading such a module: it runs too late. In a successfully loaded
SIMD-enabled module, WASM_SIMD availability is true without a CPUID-style probe.

Keep a separate scalar artifact (`MUNK2D_ENABLE_WASM_SIMD_CONTACT_SOLVER=OFF`)
when supporting runtimes without SIMD. Feature-detect in JavaScript **before**
choosing and instantiating the artifact, for example with a standard Wasm SIMD
feature detector. Do not download/import a SIMD-only extension first and then
ask its C availability function whether SIMD is supported.

For Pyodide, the SIMD requirement can belong to the Pymunk extension's side
module rather than Pyodide's main module. The same rule applies before importing
that extension. SIMD does not require SharedArrayBuffer, cross-origin isolation
or worker threads here; threading deployment is outside this change.

## Preparation, packing and fallback

The backend shares the existing graph preparation, 12 colors, 12 contact kinds,
body-major six-double scratch velocities, warm starting, allocation ownership
and commit path. Logical packets hold four independent contacts, solved as two
128-bit, two-double halves. A final single contact uses the same-order scalar
kernel. Two-contact logical packets were also evaluated; retaining four reduces
packet metadata/dispatch overhead and reuses the existing preparation layout.
No general gather/scatter is assumed: each pair loads exactly six doubles per
body and transposes with `i64x2.shuffle`. Stores touch only active writable
bodies, never dummy lanes or a shared frozen side. Scalar min/max comparison
semantics, including signed-zero ties, are retained explicitly.

Selection, locked-space errors, sleeping bodies, retained arenas and per-step
fallback follow the [AVX2 guide](avx2-contact-solver.md). In particular, active
constraints, fewer than four arbiters, nonpositive iterations, graph overflow,
unsupported contacts or allocation failure use the original solver for the
whole step, before committing any packet results. `cpHastySpaceStep()` is
unchanged. No published structure layout changes.

Coloring can change trajectories versus ORIGINAL. Doubles, iterations and
quality settings are unchanged; bit-identical trajectories between different
solver schedules/builds/platforms are not promised. Exact same-schedule scalar
checks are a test of these kernels, not a general physical error bound.

## Pyodide / Pymunk integration boundary

The existing Pymunk `SolverMode.DEFAULT` / `AUTO` policy need not gain a new user
mode: a downstream AUTO implementation can try WASM_SIMD availability after its
AVX2 check. However, current Pymunk's AUTO only checks AVX2; updating this Munk2D
library alone does **not** change that Python policy or publish a SIMD wheel.
The extension builder must opt in to the SIMD module requirement, compile the
new source and expose the enum value in its CFFI declarations. A separate scalar
wheel/artifact remains necessary for non-SIMD runtimes.

Integration validation uses an experimental Pymunk 7.3.1 source fixture, not a
released package: Pyodide 0.29.5 (Python 3.13, Emscripten 4.0.9), normal CFFI
wheel building/loading, explicit low-level selection, 600 steps, collision
callbacks and exact independent-contact comparison with ORIGINAL. Existing
Pymunk DEFAULT and AUTO behavior is checked unchanged. No downstream Python API
change is included here.

## Reproduce the targeted checks

```sh
# With an activated Emscripten SDK, CMake and Node:
tests/build-wasm.sh build-wasm
SIMD=OFF tests/build-wasm.sh build-wasm-scalar

# Optional browser dependency, installed locally (not needed by the library):
npm install --no-save --package-lock=false playwright@1.63.0
npx playwright install chromium firefox
node tests/run-wasm-browser.mjs build-wasm/browser chromium
node tests/run-wasm-browser.mjs build-wasm/browser firefox
SIMD=OFF node tests/run-wasm-browser.mjs build-wasm-scalar/browser chromium
```

The build script runs the existing 15 CTest tests in Emscripten/Node, then
packages the existing solver/API, allocation and 21-probe physical-quality tests
plus MunkBench as callable browser modules. Browser checks run those tests and
verify that the CLI accepts SIMD only in an enabled build. WebAssembly has no
observable floating-point exception flags; native poisoned-buffer tests retain
fenv checks, and Wasm retains the poison, finite-result and exact-result checks.
The CI matrix covers Emscripten 4.0.9 (Pyodide 0.29.x) and 5.0.3 (314.0.x
compiler generation), Chromium and Firefox. That does not establish Safari,
mobile or ARM performance, or validate a Pyodide 314.0.x wheel.

With SDK Binaryen, the baseline module must validate with SIMD disabled, and
the SIMD module must fail that validation:

```sh
"$EMSDK/upstream/bin/wasm-opt" --all-features --disable-simd --disable-relaxed-simd \
  build-wasm-scalar/browser/munkbench.wasm -o /dev/null
# Expected failure, specifically 'SIMD operations require SIMD':
"$EMSDK/upstream/bin/wasm-opt" --all-features --disable-simd --disable-relaxed-simd \
  build-wasm/browser/munkbench.wasm -o /dev/null
```

MunkBench accepts `--contact-solver wasm-simd` and rejects unavailable requests
instead of silently timing ORIGINAL. Measure actual complete simulation steps,
including preparation, packing, fallback and writeback, not just kernel time.
Report initialization separately or include it too, alternate paired order and
include original/original and scalar-module controls. Browser JIT/code layout
and timer resolution can dominate small differences. Repeated `callMain()`
users must save/restore the Emscripten stack around each invocation; it allocates
its argument vector on that stack.

## Browser whole-step evaluation (2026-11-04)

One i5-12400T / Linux host, CPU 2 affinity, headless Chromium 153.0.8010.12 and
Firefox 155.0; Emscripten 4.0.9, Release/O3, generic compilation, LTO enabled
except the explicit SIMD translation unit. No fast-math, workers or shared
memory. These are browser C/MunkBench timings, not Python-loop speedups.

All 26 existing scenarios, smoke and reference profiles, six adjacent
alternating AB/BA pairs per scenario for SIMD versus ORIGINAL in the **same
SIMD-enabled module**, plus two A/A and two scalar-module/ORIGINAL control
pairs. Reported values are geometric means of per-scene median paired ratios;
negative means less time. Preparation/packing/fallback/writeback is inside
every measured step. Initialization is also reported separately/included below.

| Browser / profile | Whole step | Including initialization | A/A control | Scalar-module → SIMD-module ORIGINAL control |
|---|---:|---:|---:|---:|
| chromium / smoke | -0.77% | -0.71% | -0.70% | -1.30% |
| chromium / reference | -3.09% | -3.02% | -0.17% | -0.23% |
| firefox / smoke | -0.81% | -0.75% | -0.27% | +0.35% |
| firefox / reference | -2.38% | -2.22% | +0.32% | +0.03% |

Reference contact-heavy examples: falling squares about 12–13% faster,
frictional pyramid about 10–13%, collision callbacks about 8–9%. This is not a
universal win: smoke N2 is about 15% slower in Chromium and 8% in Firefox.
Smoke suite changes are close to control/noise levels and do **not** establish
a reliable overall gain. Reference constraint/fallback scenes are mostly near
zero, with small positive and negative outliers retained in the raw results.

The bounded campaign scales the existing auto batches down by five (rounded
up, minimum one); no scenario/step/size reductions. Some short samples are only
a few milliseconds with 100µs timer granularity, so small percentages and
individual fallback-scene deltas are not precision measurements. Each browser
has 520 final timing pairs. Four-packet/two-packet exploratory runs separately
have 728 pairs per browser: four packets were about 0.9–1.1% faster on smoke
and 0.2–0.8% on reference. The final default-four benchmark Wasm bytes match
the explicitly configured four-packet experiment exactly (SHA256
`1ed8dfe62703bcc6c33cec2681daedc30fe8445133cc37da128ba136ba33326d`).

Both browsers also pass exact deterministic checkpoint comparisons at
0/1/10/50/180/181/final for all 26 scenes and both profiles: two- versus
four-packet SIMD layouts, repeated SIMD runs, and ORIGINAL in scalar versus
SIMD modules. The existing physical-quality tests check SIMD against its
same-schedule scalar kernel separately. Only this CPU/browser environment was
measured; Safari, mobile, ARM and end-user Pyodide performance remain unmeasured.
