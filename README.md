# Munk2D

<img src="assets/branding/logos/Munk2D%20logo.svg" alt="Munk2D logo" width="336" />

## FORK INFO

The main purpose of this fork is to be a companion for the Python 2D physics
library [Pymunk](https://www.pymunk.org) which is built on Chipmunk2D. Given the
slow pace of development of Chipmunk2D, and some unique requirements and
opportunities of Pymunk this is something that have grown over a long time. What
really made me consider making this fork more formal was the discussion
[here](https://github.com/slembcke/Chipmunk2D/issues/237) with Slembcke, the
creator of Chipmunk2D.

I do not foresee that I have the time, motivation or skills to really revive
Chipmunk2D. However, I hope to incorporate minor new features, and a bunch of
fixes. The changes are mainly driven by what make sense from the Pymunk use
case. I do think many of these changes can be useful also to users outside
Pymunk, and you are of course free to use the fork for other projects /
languages as well.

/Victor 2025-04-08

## ABOUT

This is a fork of the 2D physics library
[Chipmunk2D](https://github.com/slembcke/Chipmunk2D). Munk2D is a simple,
lightweight, fast and portable 2D rigid body physics library written in C. It's
licensed under the permissive MIT license. I hope you enjoy using Munk2D!

### Expectations from Munk2D

- Munk2D will add fixes for various issues, especially issues affecting Pymunk.
- Munk2D will add or improve features useful for Pymunk.
- Munk2D does not aim for ABI stability between versions.
- Munk2D will try to avoid API breaking changes, but ultimately be less strict
  than Chipmunk2D.
- While Munk2D will not try to actively break features or targets, breakage is
  more likely than with Chipmunk2D.
- Testing of Munk2D will mainly happen through Pymunk, parts not useful from
  Pymunk (e.g. ObjectiveC support), will be untested and therefor likely to
  break over time.
- While Munk2D is focused on Pymunk, I will happily accept PRs to fix or improve
  other parts of the code, or improvements to tests, documentation and so on.

## FEATURES

- Designed specifically for 2D video games, but usable in many situations.
- Circle, convex polygon, and beveled line segment collision primitives.
- Multiple collision primitives can be attached to a single rigid body.
- Fast broad phase collision detection by using a bounding box tree with great
  temporal coherence or a spatial hash.
- Extremely fast impulse solving by utilizing Erin Catto's contact persistence
  algorithm.
- Supports sleeping objects that have come to rest to reduce the CPU load.
- An [optional AVX2 contact solver](docs/avx2-contact-solver.md) for supported
  double-precision x86-64 builds. Explicit per-space opt-in; the original solver
  remains the default.
- An [optional WebAssembly SIMD contact solver](docs/wasm-contact-solver.md)
  for explicit SIMD-enabled Emscripten builds, using two double lanes without
  browser threads. Scalar-compatible modules remain the default build.
- Support for collision event callbacks based on user definable object types.
- Flexible collision filtering system with layers, exclusion groups and
  callbacks. \*\* Can be used to create all sorts of effects like one way
  platforms or buoyancy areas. (Examples included)
- Supports nearest point, segment (raycasting), shape and bounding box queries
  to the collision detection system.
- Collision impulses amounts can be retrieved for gameplay effects, sound
  effects, etc.
- Large variety of joints - easily make vehicles, ragdolls, and more.
- Joint callbacks. \*\* Can be used to easily implement breakable or animated
  joints. (Examples included)
- Maintains a contact graph of all colliding objects.
- Lightweight C99 implementation with no external dependencies outside the Std.
  C library.
- Simple, read the documentation: https://viblo.github.io/Munk2D/ and see!
- Unrestrictive MIT license

## BUILDING

Mac OS X: There is an included Xcode project file for building the static
library and demo application. Alternatively you could use the CMake files or the
macstatic.command script inside the xcode/ directory to build a static lib and
package up the headers for you.

iPhone: A native Objective-C API is included. The Xcode project can build a
static library with all the proper compiler settings. Alternatively, you can
just run iphonestatic.command in the xcode/ directory. It will build you a fat
library compiled as release for the device and debug for the simulator. After
running it, you can simply drop the Chipmunk-iOS directory into your iPhone
project!

UNIXes: A forum user was kind enough to make a set of CMake files for Chipmunk.
This will require you to have CMake installed. To build run 'cmake .' then
'make'. This should build a dynamic library, a static library, and the demo
application. A number of people have had build errors on Ubuntu due to not
having GLUT or libxmu installed.

To build and run the portable C test suite without the demos, use:

```sh
cmake -B build -S . -DBUILD_DEMOS=OFF -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The current CI pipeline uses this path and runs the tests on Linux, Windows and
macOS while still building the demos to catch compile issues there too.

CMake enables supported interprocedural/link-time optimization for Release,
RelWithDebInfo, and MinSizeRel builds. Disable it for controlled comparisons or
incompatible toolchains with `-DMUNK2D_ENABLE_LTO=OFF`. Debug builds do not use
LTO.

CMake builds the optional AVX2 contact backend when the compiler target supports
it. Runtime selection still requires CPU/OS support and explicit opt-in. Disable
building it with `-DMUNK2D_ENABLE_AVX2_CONTACT_SOLVER=OFF`. The AVX2 translation
unit is kept separate from cross-ISA LTO. See the
[solver guide](docs/avx2-contact-solver.md) for availability, fallback and numerical
behavior. Legacy project files include the new generic sources but retain the
original-only configuration; CMake is the supported AVX2 build path.

Emscripten builds can explicitly opt in with
`-DMUNK2D_ENABLE_WASM_SIMD_CONTACT_SOLVER=ON` (default OFF). Such modules require
SIMD **at load time**, even when the original solver is selected. Preserve a
separate scalar artifact for older runtimes. See the
[Wasm solver guide](docs/wasm-contact-solver.md) for compilation/linking, browser
checks, Pyodide integration and fallback policy. Both optional packet backends
support mixed contacts with all built-in joints using dependency-preserving
contact order; unknown/custom C constraints retain full original fallback.
Joint kernels themselves remain scalar.

Windows: Visual Studio projects are included in the msvc/ directory. While I try
to make sure the MSVC 10 project is up-to-date, I don't have MSVC 9 to keep that
project updated regularly. It may not work. I'd appreciate a hand fixing it if
that's the case.

## GETTING STARTED

The C API documentation is in the docs/ directory.

A good starting point is to take a look at the included Demo application. The
demos all just set up a Munk2D simulation space and the demo app draws the
graphics directly out of that. This makes it easy to see how the Munk2D API
works without worrying about the graphics code. You are free to use the demo
drawing routines in your own projects, though it is certainly not the
recommended way of drawing Munk2D objects as it pokes around at the
undocumented/private APIs of Munk2D.

## SUPPORT

The best way to get support for Munk2D is on its GitHub, open an Issue at 
https://github.com/viblo/Munk2D/issues

Since Munk2D shares a lot with Chipmunk2D, the same documentation and support 
channels that Chipmunk2D uses can be useful for questions not specific to Munk2D:

There is a forum for Chipmunk2D at http://chipmunk2d.net/forum/ Unfortunately
its very inactive nowadays, but much of the discussions are still relevant.

If you are working on a commercial project and need professional support,
you can try contacting Howling Moon Software (the company behind Chipmunk2D)
at http://howlingmoonsoftware.com/.
