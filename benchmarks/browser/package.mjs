#!/usr/bin/env node
// Package only static runtime assets; no sources, build caches or credentials.
import { readFile, writeFile, mkdir, copyFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, resolve, join } from 'node:path';
const here = dirname(fileURLToPath(import.meta.url)), root = resolve(here, '../..');
const [outputArg, simdArg, scalarArg] = process.argv.slice(2);
if (!outputArg || !simdArg || !scalarArg) throw new Error('usage: node benchmarks/browser/package.mjs OUTPUT SIMD_BROWSER_DIR SCALAR_BROWSER_DIR');
const output = resolve(outputArg), simdDir = resolve(simdArg), scalarDir = resolve(scalarArg);
const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
const git = args => execFileSync('git', ['-C', root, ...args], { encoding: 'utf8' }).trim();
await mkdir(output, { recursive: true });
const source = await readFile(join(root, 'benchmarks/munkbench.c'), 'utf8');
const cmake = await readFile(join(root, 'CMakeLists.txt'), 'utf8');
const engineVersion = ['MAJOR','MINOR','PATCH'].map(part => {
  const value = cmake.match(new RegExp('set\\(MUNK2D_VERSION_' + part + ' (\\d+)\\)'));
  if (!value) throw new Error('Cannot identify source engine version');
  return value[1];
}).join('.');
const entries = [...source.matchAll(/\{"(\w+)", (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+),/g)];
if (entries.length !== 26) throw new Error('Expected the existing 26 MunkBench scenarios');
const scenarios = entries.map(x => ({ name: x[1], steps: +x[2], smokeSize: +x[4], referenceSize: +x[5], smokeAutoBatch: +x[8], referenceAutoBatch: +x[9] }));
const uiFiles = ['index.html', 'style.css', 'core.js', 'app.js', 'environment.js', 'runner.worker.js', 'favicon.svg'];
const ui = {};
for (const name of uiFiles) { const bytes = await readFile(join(here, name)); await copyFile(join(here, name), join(output, name)); ui[name] = { sha256: sha256(bytes), bytes: bytes.length }; }
const modules = {};
for (const [group, directory] of [['simd', simdDir], ['scalar', scalarDir]]) {
  modules[group] = {}; await mkdir(join(output, 'modules', group), { recursive: true });
  for (const name of ['munkbench', 'contact_solver', 'contact_solver_alloc', 'contact_solver_quality']) {
    const js = `modules/${group}/${name}.js`, wasm = `modules/${group}/${name}.wasm`;
    const jsBytes = await readFile(join(directory, name + '.js')), wasmBytes = await readFile(join(directory, name + '.wasm'));
    await copyFile(join(directory, name + '.js'), join(output, js)); await copyFile(join(directory, name + '.wasm'), join(output, wasm));
    modules[group][name] = { js, wasm, jsSha256: sha256(jsBytes), wasmSha256: sha256(wasmBytes), jsBytes: jsBytes.length, wasmBytes: wasmBytes.length };
  }
}
const generatedAt = new Date().toISOString(), bundleHash = sha256(JSON.stringify({ ui, modules }));
const engineRevision = git(['log', '-1', '--format=%H', '--', 'src', 'include', 'CMakeLists.txt', 'benchmarks/munkbench.c']);
const harnessRevision = git(['rev-parse', 'HEAD']);
let compiler = process.env.EMSCRIPTEN_VERSION;
if (!compiler) compiler = execFileSync('emcc', ['--version'], { encoding: 'utf8' }).split('\n')[0].match(/\b\d+\.\d+\.\d+\b/)?.[0];
if (!compiler) throw new Error('Cannot identify the Emscripten compiler');
const id = generatedAt.replace(/[-:.]/g, '').replace(/\d{3}Z$/, 'Z') + '-' + bundleHash.slice(0, 12);
const manifest = { schema: 'munk2d-browser-benchmark/1',
  build: { id, generatedAt, bundleSha256: bundleHash, engineRevision, harnessRevision,
    harnessDirty: !!git(['status', '--porcelain', '--', 'benchmarks/browser']),
    engineDirty: !!git(['status', '--porcelain', '--', 'src', 'include', 'CMakeLists.txt', 'benchmarks/munkbench.c']), engineVersion,
    emscriptenVersion: compiler, precision: 'double', optimization: 'Release/O3; LTO enabled except SIMD translation unit',
    packetContacts: 4, vectorDoubleLanes: 2, relaxedSimd: false, physicsThreads: 1,
    benchmarkSourceSha256: sha256(Buffer.from(source)), uiAssets: ui }, scenarios, modules };
await writeFile(join(output, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
console.log(JSON.stringify({ output, id, engineRevision, harnessRevision, bundleHash }));
