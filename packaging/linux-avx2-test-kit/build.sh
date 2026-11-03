#!/usr/bin/env bash
# Build-host only. Ubuntu 20.04 gives a glibc 2.31-or-older runtime requirement.
set -euo pipefail
root=${GITHUB_WORKSPACE:-$(git rev-parse --show-toplevel)}
tools="$root/packaging/linux-avx2-test-kit"
out="$root/out"
package="$out/munk2d-avx2-linux-x86_64"
export PYTHONDONTWRITEBYTECODE=1
mkdir -p "$package"
refs=(3d2070976911b7e4b659a5365b0313f99dc98788 5784f8f48728e4fdd71638f0dc11aa26fa07f8ce)
labels=(baseline candidate)
for i in 0 1; do
  label=${labels[$i]}; source="$root/$label"; build="$root/build-$label"
  [[ $(git -C "$source" rev-parse HEAD) == "${refs[$i]}" ]]
  [[ -z $(git -C "$source" status --porcelain) ]]
  cmake -S "$source" -B "$build" -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_DEMOS=OFF -DBUILD_SHARED=OFF -DBUILD_STATIC=ON -DBUILD_TESTING=ON \
    -DBUILD_BENCHMARKS=ON -DMUNK2D_ENABLE_LTO=ON \
    '-DCMAKE_C_FLAGS=-march=x86-64 -mtune=generic' '-DCMAKE_CXX_FLAGS=-march=x86-64 -mtune=generic'
  cmake --build "$build" --parallel 2
  (cd "$build" && ctest --output-on-failure) | tee "$out/tests-$label.log"
  cp "$build/benchmarks/munkbench" "$package/munkbench-$label"
  strip --strip-unneeded "$package/munkbench-$label"
  chmod 755 "$package/munkbench-$label"
  { echo "$label"; file "$package/munkbench-$label"; ldd "$package/munkbench-$label"; readelf --version-info "$package/munkbench-$label"; } >> "$out/abi-verification.txt"
done
for file in run.py run.sh README.txt; do cp "$tools/$file" "$package/$file"; done
cp "$root/baseline/LICENSE.txt" "$package/LICENSE.txt"
chmod 755 "$package/run.sh"
PACKAGE="$package" ROOT="$root" python3 - <<'PY'
import hashlib, json, os, re, subprocess
from pathlib import Path
p, root = Path(os.environ['PACKAGE']), Path(os.environ['ROOT'])
entries=[]
for label in ('baseline','candidate'):
    exe=p/('munkbench-'+label)
    versions=subprocess.check_output(['readelf','--version-info',str(exe)],universal_newlines=True)
    needed=sorted(set(re.findall(r'GLIBC_([0-9.]+)',versions)),key=lambda x:tuple(map(int,x.split('.'))))
    assert needed and all(tuple(map(int,x.split('.'))) <= (2,31) for x in needed),needed
    dynamic=subprocess.check_output(['readelf','-d',str(exe)],universal_newlines=True)
    libraries=re.findall(r'\(NEEDED\).*?\[(.*?)\]',dynamic)
    assert set(libraries) <= {'libm.so.6','libc.so.6','libpthread.so.0','libgcc_s.so.1'},libraries
    assert 'Advanced Micro Devices X86-64' in subprocess.check_output(['readelf','-h',str(exe)],universal_newlines=True)
    entries.append({'name':exe.name,'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'glibcVersions':needed,'neededLibraries':libraries})
assert (root/'baseline/benchmarks/munkbench.c').read_bytes()==(root/'candidate/benchmarks/munkbench.c').read_bytes()
manifest={'kit':'Munk2D PR #15 native Linux x86-64 comparison','baseline':'3d2070976911b7e4b659a5365b0313f99dc98788','candidate':'5784f8f48728e4fdd71638f0dc11aa26fa07f8ce',
 'compiler':subprocess.check_output(['cc','--version'],universal_newlines=True).splitlines()[0],'buildOS':Path('/etc/os-release').read_text(),
 'configuration':'Release/O3/default LTO ON, static chipmunk, system glibc/libm; baseline -march=x86-64 -mtune=generic, AVX2 only in its existing TU',
 'timer':'Unmodified MunkBench clock(): Linux process CPU time inside complete physics stepping; setup/cleanup excluded',
 'benchmarkSHA256':hashlib.sha256((root/'baseline/benchmarks/munkbench.c').read_bytes()).hexdigest(),
 'buildRun':'https://github.com/'+os.environ.get('GITHUB_REPOSITORY','vibloteket/munk2d')+'/actions/runs/'+os.environ.get('GITHUB_RUN_ID','local'),
 'packagingCommit':os.environ.get('GITHUB_SHA','local'),'executables':entries}
(p/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
PY
python3 "$tools/test_runner.py" 2>&1 | tee "$out/runner-unit-tests.log"
python3 "$package/run.py" --mode ci --warmup-seconds 1 --output "$out/self-test" | tee "$out/runner-self-test.log"
OUTPUT="$out/self-test" python3 - <<'PY'
import json,os
from pathlib import Path
p=Path(os.environ['OUTPUT']);m=json.loads((p/'metadata.json').read_text());r=json.loads((p/'timing-pairs.json').read_text());v=json.loads((p/'validation.json').read_text())
assert m['Status']=='PASS' and len(r)==24 and len(v)==4
assert all(x['Exact'] and x['BaselineDeterministic'] and x['CandidateDeterministic'] for x in v)
assert Path(str(p)+'.tar.gz').exists()
assert all('Before' in x and 'After' in x and 'Order' in x for x in r)
print('PASS native Linux full-state/adjacent ABBA/noise-control/telemetry/result-archive self-test')
PY
printf '%s\n' 'PASS: both native baseline/candidate complete CTest suites; checksum/parser/protocol/child-cleanup unit tests; all26 exact repeated smoke/reference original+AVX2 summaries; adjacent ABBA/A/A/original-control/telemetry self-test. Linux ABI <= glibc2.31, no nonstandard shared libraries.' > "$package/VERIFIED.txt"
tar --owner=0 --group=0 --numeric-owner -czf "$out/munk2d-avx2-linux-x86_64.tar.gz" -C "$out" munk2d-avx2-linux-x86_64
sha256sum "$out/munk2d-avx2-linux-x86_64.tar.gz" > "$out/package.sha256"
echo 'PASS Linux kit build and packaging'
