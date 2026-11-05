#!/bin/sh
# Requires an activated Emscripten SDK, CMake, Node. No npm runtime dependency.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
output=${1:?usage: benchmarks/browser/build.sh OUTPUT [BUILD_CACHE]}
cache=${2:-"$root/build-browser-cache"}
JOBS=${JOBS:-2} "$root/tests/build-wasm.sh" "$cache/simd"
SIMD=OFF JOBS=${JOBS:-2} "$root/tests/build-wasm.sh" "$cache/scalar"
node "$root/benchmarks/browser/package.mjs" "$output" "$cache/simd/browser" "$cache/scalar/browser"
