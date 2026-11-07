# Interactive Munk2D browser benchmark

A dependency-free static page for an ordinary browser, reusing MunkBench and the
existing solver correctness tests. No compiler, Node, browser automation or
Pyodide is needed **on the user's machine**. This measures direct Munk2D/Wasm,
not Python integration, rendering FPS or a standalone SIMD kernel.

## User workflow

1. Open the published URL in each browser on the same device.
2. Optionally enter the device/CPU/RAM model, correct hidden OS/browser details,
   and confirm the power source.
3. Keep the tab visible and other work/power settings consistent. Choose quick
   or full; allow several minutes, especially on a phone.
4. Download the result JSON **after each run** and share it for analysis.

Quick covers eight existing scenes (including small/contact-heavy/fallback
workloads), smoke and reference sizes, four primary pairs and two pairs of each
control: **128 pairs**. Full covers all 26 scenes and both sizes, six primary
pairs and four pairs of each control: **728 pairs**. Original step counts and
scenario sizes are not reduced. Scalar-only browsers run noise controls without
inventing a SIMD score or fetching the SIMD module.

The normal and scalar-only artifacts remain separate. The primary comparison
is BASELINE versus WASM_SIMD in the **same SIMD-enabled module**. A/A repeats
BASELINE. Module controls compare BASELINE between scalar-only/SIMD modules.
A difference in that last control is not itself a SIMD solver speedup.

## Protocol / acceptance criteria

- Standard `f64x2`, double precision, existing four-contact packets; unchanged
  engine selection/quality/fallback. No relaxed SIMD, worker-thread physics,
  shared memory or special COOP/COEP hosting requirements.
- One **dedicated JavaScript worker** keeps the UI responsive and permits
  immediate cancellation. The physics itself is still single-threaded. The
  worker waits for UI acknowledgments between measured pairs/cases; rendering,
  JSON serialization and local-storage writes are not part of the timed steps.
- Existing API/direct-packet/allocation/21-probe quality checks run first.
  Every measured scene also checks finite checkpoints and deterministic repeat;
  BASELINE checkpoints match across module variants. Different solver schedules
  are deliberately **not** required to match one another.
- Warmup/calibration is untimed. One common batch per case is chosen using all
  available variants, targeting at least ~80 ms of simulation time (scaled for
  timer resolution), bounded to 2048 runs. Short samples/caps are retained and
  flagged, not silently dropped. Extremely coarse/unobservable clocks return a
  diagnostic result rather than a misleading performance comparison.
- Adjacent alternating AB/BA order is balanced for primary and both controls;
  block order also rotates. All paired samples, setup times, entire-call wall
  times, calibration trials, checkpoint snapshots and hashes are downloadable.
- Per-scene medians and equal-scene geometric means are reported separately for
  smoke/reference. Bootstrap intervals must clear A/A noise and a minimum 1%
  margin to label faster/slower. Otherwise the result is **inconclusive**. Few
  samples, thermal/frequency changes and correlated browser noise mean these
  intervals are exploratory, not a universal confidence guarantee.
- Hiding/freezing/leaving the tab or Stop terminates the worker and preserves
  explicitly incomplete data. Partial results do not receive complete-suite
  headline scores. The latest result is saved locally when cases complete and
  restored after reload. Quota/storage failures are visible and downloading
  still works. A new run replaces the local last-run slot; download first.
- All Wasm/JS module bytes are SHA256 checked before execution. Wasm bytes used
  by the factory are the verified bytes. Immutable release paths avoid mixed
  caches. Emscripten `callMain()` argument stacks are restored on every call.
- No automatic uploads, analytics or third-party runtime requests. Only the
  explicit static package is published; no source checkout, results, secrets,
  build caches or unofficial Pymunk wheels are copied into the site.

## Environment data

The JSON records raw UA and User-Agent Client Hints (including high-entropy
versions/architecture/bitness/device model where the browser exposes them),
browser/OS labels **with their information source**, logical processor/RAM
hints, GPU renderer/vendor, screen/viewport/DPR, locale, timezone offset, timer
resolution, secure/isolated context, SIMD/worker support, automation hint,
battery state, optional network/JS heap hints, timestamps, lifecycle events,
source revisions, compiler/configuration and module/UI hashes.

The actual CPU model, true physical cores, installed RAM, OS power mode,
temperature, frequency/throttling and other applications are not reliably
observable. Unknown values stay unknown; processor count/RAM buckets are
explicitly hints. GPU renderer is **not** relabeled as CPU. Manual annotations
are separate and marked user-entered. Missing/restricted APIs are handled. No
geolocation, IP lookup, account data, invasive device enumeration or CPU-model
fingerprinting benchmark is requested. Most iOS browser apps share WebKit;
different app names need not imply independent engine implementations.

## Build

From a checkout with an activated Emscripten SDK (4.0.9 is the reference
measurement toolchain), CMake and Node:

```sh
JOBS=4 benchmarks/browser/build.sh /absolute/path/to/static-package /absolute/path/to/build-cache
```

This runs the existing CTest suites for enabled and baseline builds, builds the
callable browser modules, and packages only static runtime files. The packager
extracts scene names/step counts/profile sizes from `munkbench.c` and asserts
26 scenes. Metadata separates the last engine-affecting Git revision from the
harness revision/dirty flag and fingerprints the bytes. No version upgrade or
release/default-policy change is implied.

For existing `tests/build-wasm.sh` output, packaging alone is sufficient:

```sh
node benchmarks/browser/package.mjs STATIC_PACKAGE SIMD_BUILD/browser SCALAR_BUILD/browser
```

Do not compile with fast-math. Do not remove the module-load feature detection
or treat C availability as a loader guard. See
[the Wasm guide](../../docs/wasm-contact-solver.md) for the engine contract.

## Development tests

```sh
node --test benchmarks/browser/core.test.mjs
node benchmarks/browser/test-deploy.mjs STATIC_PACKAGE
npm install --no-save --package-lock=false playwright@1.63.0
npx playwright install chromium firefox
node benchmarks/browser/test-browser.mjs STATIC_PACKAGE chromium diagnostic OUTPUT
node benchmarks/browser/test-browser.mjs STATIC_PACKAGE firefox quick OUTPUT
node benchmarks/browser/test-browser.mjs STATIC_PACKAGE chromium full OUTPUT
```

The diagnostic worker run is explicitly labeled diagnostic and uses one scene,
short batches and fewer repeats for functional coverage; **it is not a user
benchmark score**. Browser tests also exercise the real UI, mobile overflow,
responsive worker execution, manual CPU annotations, JSON download, cancellation,
reload restoration, hidden-tab interruption, unsupported-SIMD loading and
corrupted-module rejection. Quick/full modes above additionally run the actual
unmodified UI schedules. WebKit can be selected when its host dependencies are
available; that is not the same as testing Safari on every Apple device.

## Publish to the workspace static host

Workspace hosting maps `/var/www/<name>` to `https://<name>.viblo.se/`.
Names must match `^[a-z0-9]([a-z0-9-]{0,38}[a-z0-9])$`; blocked names are
`pc`, `picoclaw`, `m`, `www`, `nanobot`, `nb`, `cgi-bin`. A new hostname may take a
few seconds on first request while TLS is generated.

```sh
node benchmarks/browser/deploy.mjs STATIC_PACKAGE munkbench
```

The deployer verifies hashes and ordinary-file types, copies an allowlist only,
refuses unrelated existing sites and duplicate immutable releases, keeps old
releases, and atomically switches the root index **last**. It does not modify web
server configuration or other sites. It prints the normal URL and an immutable
release URL. Verify public HTML/JS/Wasm response bytes and actual HTTPS browser
execution after publishing. The optional third argument overrides `/var/www`
for a local deployment test only.

JSON schema: `munk2d-browser-benchmark/1`. No server-side result ingestion is
included; users download/share their own files.
