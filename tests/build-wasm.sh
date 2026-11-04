#!/bin/sh
# Build existing tests and MunkBench as callable browser modules. Requires an
# activated Emscripten SDK. SIMD=OFF builds the baseline, load-compatible module.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:?usage: tests/build-wasm.sh BUILD_DIRECTORY}
simd=${SIMD:-ON}
emcmake cmake -S "$root" -B "$build" -DBUILD_DEMOS=OFF -DBUILD_SHARED=OFF \
  -DBUILD_STATIC=ON -DBUILD_TESTING=ON -DBUILD_BENCHMARKS=ON \
  -DMUNK2D_ENABLE_WASM_SIMD_CONTACT_SOLVER="$simd" ${CMAKE_EXTRA_ARGS:-}
cmake --build "$build" -j "${JOBS:-2}"
ctest --test-dir "$build" --output-on-failure
mkdir -p "$build/browser"
if [ "$simd" = ON ]; then simdflags='-msimd128 -DCP_WASM_SIMD_CONTACT_SOLVER=1'; else simdflags=; fi
for name in contact_solver contact_solver_alloc contact_solver_quality; do
  emcc -O3 -ffp-contract=off $simdflags ${EXTRA_CFLAGS:-} -I"$root/include" \
    "$root/tests/test_$name.c" "$build/src/libchipmunk.a" \
    -sMODULARIZE=1 -sEXPORT_NAME=createMunkModule -sINVOKE_RUN=0 \
    -sEXPORTED_RUNTIME_METHODS=callMain,stackSave,stackRestore -sALLOW_MEMORY_GROWTH=1 \
    -o "$build/browser/$name.js"
done
emcc -O3 $simdflags ${EXTRA_CFLAGS:-} -I"$root/include" \
  "$root/benchmarks/munkbench.c" "$build/src/libchipmunk.a" \
  -sMODULARIZE=1 -sEXPORT_NAME=createMunkModule -sINVOKE_RUN=0 \
  -sEXPORTED_RUNTIME_METHODS=callMain,stackSave,stackRestore -sALLOW_MEMORY_GROWTH=1 \
  -o "$build/browser/munkbench.js"
