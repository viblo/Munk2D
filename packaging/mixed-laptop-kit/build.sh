#!/usr/bin/env bash
set -euo pipefail
root=${GITHUB_WORKSPACE:-$(git rev-parse --show-toplevel)}
tools="$root/packaging/mixed-laptop-kit"
package="$root/out/munk2d-mixed-linux-x86_64"
mkdir -p "$package"
cd "$tools"
go test ./cmd/runner
CGO_ENABLED=0 GOAMD64=v1 go build -trimpath -ldflags='-s -w' -o "$package/runner" ./cmd/runner
go run ./cmd/prepare --stage sources --root "$root"
for variant in baseline candidate; do
 source="$root/$variant"; build="$root/build-$variant"
 cmake -S "$source" -B "$build" -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=OFF -DBUILD_SHARED=OFF -DBUILD_STATIC=ON -DBUILD_TESTING=ON -DBUILD_BENCHMARKS=ON -DMUNK2D_ENABLE_LTO=ON '-DCMAKE_C_FLAGS=-march=x86-64 -mtune=generic -pthread' '-DCMAKE_CXX_FLAGS=-march=x86-64 -mtune=generic -pthread' -DCMAKE_EXE_LINKER_FLAGS=-pthread
 cmake --build "$build" --parallel 2
 ctest --test-dir "$build" --output-on-failure | tee "$root/out/tests-$variant.log"
 for driver in munkbench munkbench_mixed; do
  cp "$build/benchmarks/$driver" "$package/$driver-$variant";strip --strip-unneeded "$package/$driver-$variant"
  readelf --version-info "$package/$driver-$variant" >> "$root/out/abi.txt"
  readelf -d "$package/$driver-$variant" >> "$root/out/dependencies.txt"
  max=$(readelf --version-info "$package/$driver-$variant" | grep -oE 'GLIBC_[0-9.]+' | sort -Vu | tail -1)
  [[ $(printf '%s\n' GLIBC_2.31 "$max" | sort -V | tail -1) == GLIBC_2.31 ]]
 done
done
# Only glibc/libm/pthread/libgcc are acceptable dynamic requirements.
if grep NEEDED "$root/out/dependencies.txt" | grep -Ev '\[(libc.so.6|libm.so.6|libpthread.so.0|libgcc_s.so.1)\]'; then echo 'Unexpected dynamic dependency';exit 1;fi
go run ./cmd/prepare --stage manifest --root "$root" --package "$package" --compiler "$(cc --version | head -1)"
cp "$tools/run.sh" "$package/run.sh";chmod 755 "$package/runner" "$package/run.sh" "$package"/munkbench*
"$package/runner" --mode ci --output "$root/out/self-test" | tee "$root/out/self-test.log"
printf '%s\n' 'PASS: old/new full CTests, Go unit tests, all26 repeated exact-state checks, mixed checkpoint matrix, 64-row balanced ABBA/control/telemetry/archive self-test, ABI <=GLIBC2.31.' > "$package/VERIFIED.txt"
tar --owner=0 --group=0 --numeric-owner -czf "$root/out/munk2d-mixed-linux-x86_64.tar.gz" -C "$root/out" munk2d-mixed-linux-x86_64
sha256sum "$root/out/munk2d-mixed-linux-x86_64.tar.gz" > "$root/out/package.sha256"
