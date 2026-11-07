# Optional AVX2 contact solver

The baseline contact solver remains the default. The AVX2 solver is an explicit
per-space opt-in for `cpSpaceStep()`, not a replacement for all workloads.
It uses double precision and four-lane SIMD for independent contacts. Joint-free
steps keep the bounded graph-coloring fast path. Mixed steps preserve contact
dependencies and interleave SIMD contacts with existing scalar joint sweeps.
It does not vectorize the joint formulas or add worker threads.
For Emscripten, see the separate [WebAssembly SIMD backend](wasm-contact-solver.md);
its SIMD module-loading requirement differs from native AVX2 CPU dispatch.

## Selecting a solver

```c
cpSpace *space = cpSpaceNew();
if(cpContactSolverIsAvailable(CP_CONTACT_SOLVER_AVX2)){
    if(!cpSpaceSetContactSolver(space, CP_CONTACT_SOLVER_AVX2)){
        /* Selection can fail, for example if scratch-context allocation fails. */
    }
}

/* Simulation setup and cpSpaceStep() as usual. */

/* This releases the optional solver's retained scratch allocations. */
cpSpaceSetContactSolver(space, CP_CONTACT_SOLVER_BASELINE);
cpSpaceFree(space);
```

`cpSpaceSetContactSolver()` returns false for an unavailable/invalid selector,
a locked space, or allocation failure, and leaves the previous selection
unchanged. Change it outside callbacks executed while the space is locked;
a post-step callback can change it. Selecting the already selected mode is
supported. Changing mode does not wake sleeping bodies, similarly to changing
the iteration count.

`cpSpaceGetContactSolver()` returns the requested mode. It does not promise that
every individual step executes SIMD. `CP_CONTACT_SOLVER_TYPE_MAX` is reserved,
not a selectable mode.

This option applies to **`cpSpaceStep()` only**. The separate
`cpHastySpaceStep()` keeps its existing solver and does not use this setting.
Using a HastySpace allocation with the normal `cpSpaceStep()` follows the normal
stepper's setting; this option is not an extension of HastySpace threading.

## Availability and fallback

The first backend supports the ordinary **double-precision x86-64 build**.
Build-time compiler capability and runtime CPU/OS capability are separate:

- CMake checks whether the actual compiler target supports the x86-64 AVX2 code.
  Unsupported targets, including ARM and 32-bit x86, keep the baseline solver.
- Runtime selection checks CPUID XSAVE/OSXSAVE/AVX, XGETBV XMM/YMM enablement,
  and CPUID leaf7 AVX2. XGETBV is not executed before checking OSXSAVE.
- Availability is checked when selecting the backend. As with conventional
  runtime-dispatched libraries, the process is expected to run on CPUs exposing
  a consistent supported instruction set.
- Float builds do not enable this backend. A successful compile probe alone
  does not imply that the runtime API will report availability.
- No CPU-model whitelist or performance claim is implied by AVX2 availability.

Even when requested and available, the whole step uses the baseline solver if:

- an active constraint has an unknown/custom implementation class;
- fewer than four arbiters are active, or the iteration count is not positive;
- a joint-free contact graph cannot fit the fixed12-color budget;
- an arbiter aliases its two bodies, has an unsupported contact count, or both
  bodies have zero inverse mass and inverse inertia;
- checked scratch sizing/allocation cannot be satisfied.

Fallback decisions are made before modifying body velocities or accumulated
contact impulses. Cached/warm-start impulses retain the baseline phase/order.
The backend never skips a collision to make a graph fit.

## Mixed contacts and joints

Native AVX2 supports all ten built-in constraint classes: pin, slide, pivot,
groove, gear, ratchet, rotary limit, simple motor, damped spring and damped
rotary spring. Joints in the **same connected group** as the contacts are
supported; unrelated joints no longer disable all SIMD contact solving.

Each mixed solver iteration performs one contact-packet sweep followed by the
baseline scalar joint sweep, in its baseline joint order. Only physical linear
and angular velocities of bodies shared by contacts and joints are synchronized
between scratch and body storage. Joint-only bodies stay in ordinary storage;
built-in joints do not read/write the contact solver's split-impulse bias
velocities. Shared frozen endpoints are read back but never overwritten by the
contact solver. All velocities and contact accumulators are committed before
post-solve callbacks.

Mixed steps use dependency-aware ready queues instead of graph-color ordering.
On every writable body, contact solves retain their baseline arbiter order;
only independent contacts can enter the same packet. Successors become ready
only after the whole preceding packet has been selected. Mixed graphs are not
subject to the joint-free12-color limit. The existing joint-free path and its
all-iterations kernel remain separate.

Pre-solve, spring force/torque, warm-start, post-solve and post-step phases remain
in their existing locations. The supported spring callbacks run before packet
preparation. A custom C constraint class can change unrelated bodies or contact
coefficients inside its impulse function, so its presence retains complete
baseline-solver fallback rather than attempting partial synchronization.

The optional [Wasm SIMD backend](wasm-contact-solver.md) also uses this shared
mixed schedule. Its module-loading requirement and opt-in build policy are
unchanged; there is no Python AUTO or wheel change here.

## Numerical behavior and quality

Graph coloring changes the order of some dependent contact solves compared with
the baseline solver. Therefore trajectories, contact evolution and sleeping
behavior can differ. The algorithm remains deterministic for a fixed input,
backend, build and execution environment; identical results across solver modes,
compiler versions or hardware are not promised.

Positions, geometry, coefficients, velocities, impulse accumulators and solver
arithmetic remain double. There is no reduction of iterations or collision
quality settings and no mixed-precision path in this implementation.

Tests compare SIMD with a scalar implementation of the same ordered formulas,
check baseline fallback, and exercise physical diagnostics for penetration,
resting behavior, friction, restitution, center-of-mass-frame energy, momentum,
large coordinates/common velocities and mass ratios. These diagnostics are not
a universal error bound for all applications. Check your own workloads before
opting in, particularly sensitive stacks, extreme scales and configurations
with changing constraints. A passing test suite does not establish superiority
over the baseline solver for every quality metric.

## Build isolation

`MUNK2D_ENABLE_AVX2_CONTACT_SOLVER` defaults to ON, meaning **build the optional
backend where supported**, not select it at runtime. Disable it with:

```sh
cmake -S . -B build -DBUILD_DEMOS=OFF \
  -DMUNK2D_ENABLE_AVX2_CONTACT_SOLVER=OFF
```

Only `cpContactSolverAvx2.c` receives the AVX2 target flags. CPU detection, API,
scheduling and allocation compile for the baseline target. The AVX2 translation
unit is excluded from cross-ISA LTO (`-fno-lto` or `/GL-`); floating-point
contraction is disabled for its formulas and the scalar reference.

For a portable binary, do not apply `-march=native`, global `-mavx2` or an
AVX-only `/arch` option to the rest of the library/application. This dispatch
cannot make such globally specialized code safe on older CPUs.

## Ownership and ABI

No fields are added to `cpSpace`, `cpBody`, `cpArbiter` or other published
structures. The allocation ledger is backed by the private
`cpSpaceBufferStorage`, whose first member is an ordinary `cpArray` and whose
additional pointer owns the optional solver context. The ordinary ledger
contents/order are unchanged; solver state is not a fake arbiter-buffer entry
and does not use application `userData` or a process-global registry.

A context is allocated only on successful opt-in. It owns separate reusable
arenas for graph metadata and solver working data, plus a lazily allocated mixed
arena for dependencies, packet order and shared-body indices. Joint-free spaces
do not allocate that mixed arena. Large solver buffers are
reserved only after graph eligibility is established. Active hash length and
packed body count depend on current work, not on retained peak capacity. Scratch
velocities use six adjacent doubles per body. No graph coloring is cached across
steps.

Preparation initializes only color/kind groups that are actually used, while
retaining the same ascending color/kind order and order within each group.
Active packet lanes are fully assigned; inactive lanes that the kernel can read
are initialized explicitly. Unused contact slots are not read. This reduces
redundant preparation work without changing solver selection or arithmetic.
Tests poison reusable working buffers to catch stale or uninitialized reads.

Arenas retain their high-water capacities until disabling the backend or
freeing the space. A grow-allocation failure preserves the previous arenas and
falls back for the current step. Destruction only frees storage: it never
commits stale body/contact references from an earlier step. Allocation/free
uses the library's existing allocator macros.

The struct definitions are private API as before. Consumers must not replace
`space->allocatedBuffers` manually or assume its owning allocation is exactly
`sizeof(cpArray)`. Normal opaque/public API usage is unaffected.

## Benchmarking

MunkBench accepts an explicit selector without changing its CSV schema or size
profiles:

```sh
./build/benchmarks/munkbench --contact-solver baseline --profile reference --batch auto
./build/benchmarks/munkbench --contact-solver avx2 --profile reference --batch auto
```

An unavailable AVX2 request is an error, not a mislabeled baseline-only run.
Per-step eligibility fallback still applies. Use alternating paired runs and
baseline-versus-baseline controls, include setup costs, and check results and
physical quality separately. Small percentage differences can be code-layout or
measurement effects. Collision-heavy workloads may benefit while others do not.

The additional standalone `munkbench_mixed` executable exercises light-joint piles,
dense joint/contact networks and motor-driven mechanisms, without changing the
published26-scene browser protocol:

```sh
./build/benchmarks/munkbench_mixed --scene light --solver baseline --count 256
./build/benchmarks/munkbench_mixed --scene light --solver avx2 --count 256
# Also: --scene dense or --scene mechanisms; --steps/--batch/--samples/--warmup.
```

It reports complete-step CPU time, initialization separately, workload counts,
finite-state checks and final body/joint hashes. The identical public-API-only
driver can be linked against an older library for baseline comparisons. These
synthetic scenes supplement rather than replace application workloads.

### Measured performance guidance (2.1.0)

The backend is strictly opt-in because it does not win everywhere. Measured
with MunkBench paired runs (median geometric means over 26-scenario suites,
negative = faster than the baseline solver):

- Wins on newer CPUs: roughly 3–4 % suite-wide on AMD Zen 4 (EPYC 9V74) and
  Intel Meteor Lake / Alder Lake (Ultra 9 185H, i5-12400T), with 20–30 % in
  contact-heavy scenes (friction pyramids, falling square piles) and additional
  gains in collision-callback and sleep/wake scenes.
- Scenes with few active contacts per step (a couple of resting or bouncing
  bodies, frictionless single-point contacts) are 5–20 % **slower** on every
  measured CPU, including Zen 4. If your typical frame has only a handful of
  contacts, keep the baseline solver.
- Older AVX2 Intel CPUs (Skylake-derived, e.g. i5-8265U) were a net loss before
  the gather-free loads (#58) and should be re-measured per workload.

Always measure with your own workload before enabling; treat the backend as a
targeted optimization, not a universal acceleration.

### Older AVX2 CPUs and indexed loads

The vector kernel avoids hardware gather instructions. It loads four plus two
doubles per body and transposes these body-major values into four SIMD lanes.
Stores transpose back, touching only active, writable bodies: dummy lanes cannot
clobber real lanes or shared frozen bodies. Loads do not read beyond six doubles
at the final body, and scratch memory usage is unchanged. Lane order, double
values, arithmetic, graph scheduling and fallback decisions are unchanged.
Avoiding hardware gathers is useful on older CPUs, notably Intel systems
with Gather Data Sampling (GDS) microcode mitigation. Intel documents the added
gather-result latency and recommends reducing gather use in affected workloads
in its [GDS guidance](https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/technical-documentation/gather-data-sampling.html).
No security mitigation needs to be disabled.

AVX2 capability alone still does not establish that this solver is faster than
the baseline: graph preparation, data copying, partly filled packets and
per-step fallback all cost time. Measure both modes on the actual workload and
CPU. See `benchmarks/OPTIMIZATION-LOG.md` for local measurements and limitations.
