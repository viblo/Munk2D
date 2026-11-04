#!/usr/bin/env node
// Workspace hosting: /var/www/<name> -> https://<name>.viblo.se/.
// Keep immutable releases and switch index.html last. Never replace other apps.
import { readFile, writeFile, mkdir, copyFile, rename, stat, lstat, readdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve, join, dirname } from 'node:path';
const [directory, site = 'munkbench', webRoot = '/var/www'] = process.argv.slice(2);
if (!directory) throw new Error('usage: node benchmarks/browser/deploy.mjs STATIC_PACKAGE [SITE_NAME] [WEB_ROOT]');
if (!/^[a-z0-9]([a-z0-9-]{0,38}[a-z0-9])$/.test(site) || ['pc', 'picoclaw', 'm', 'www', 'nanobot', 'nb', 'cgi-bin'].includes(site)) throw new Error('Invalid or blocked hosting name');
const source = resolve(directory), manifest = JSON.parse(await readFile(join(source, 'manifest.json'), 'utf8'));
const id = manifest.build?.id;
if (!id || !/^[A-Za-z0-9-]+$/.test(id)) throw new Error('Unsafe/missing release ID');
if (manifest.schema !== 'munk2d-browser-benchmark/1') throw new Error('Unexpected package schema');
const assets = new Map();
for (const name of ['index.html', 'style.css', 'core.js', 'app.js', 'environment.js', 'runner.worker.js', 'favicon.svg']) assets.set(name, manifest.build.uiAssets[name]?.sha256);
for (const group of ['simd', 'scalar']) for (const name of ['munkbench', 'contact_solver', 'contact_solver_alloc', 'contact_solver_quality']) {
  const entry = manifest.modules[group][name];
  if (entry.js !== `modules/${group}/${name}.js` || entry.wasm !== `modules/${group}/${name}.wasm`) throw new Error('Unexpected module path');
  assets.set(entry.js, entry.jsSha256); assets.set(entry.wasm, entry.wasmSha256);
}
for (const [name, expected] of assets) {
  if (!(await lstat(join(source, name))).isFile()) throw new Error('Asset is not an ordinary file: ' + name);
  const actual = createHash('sha256').update(await readFile(join(source, name))).digest('hex');
  if (actual !== expected) throw new Error('Package hash mismatch: ' + name);
}
const root = join(resolve(webRoot), site), releases = join(root, 'releases'), release = join(releases, id);
const marker = join(root, '.munk2d-benchmark-site');
try {
  const entries = await readdir(root);
  if (entries.length && !entries.includes('.munk2d-benchmark-site')) throw new Error('Refusing to overwrite an unrelated site');
  if (entries.includes('.munk2d-benchmark-site') && (await readFile(marker, 'utf8')).trim() !== manifest.schema) throw new Error('Wrong site marker');
} catch (e) { if (e.code !== 'ENOENT') throw e; }
await mkdir(releases, { recursive: true });
await writeFile(marker, manifest.schema + '\n');
try { await stat(release); throw new Error('Immutable release already exists: ' + release); }
catch (e) { if (e.code !== 'ENOENT') throw e; }
const staging = join(releases, '.staging-' + id + '-' + process.pid);
await mkdir(staging);
for (const name of [...assets.keys(), 'manifest.json']) {
  await mkdir(dirname(join(staging, name)), { recursive: true });
  await copyFile(join(source, name), join(staging, name));
}
await rename(staging, release);
let html = await readFile(join(release, 'index.html'), 'utf8');
for (const asset of ['style.css', 'favicon.svg', 'core.js', 'app.js']) html = html.replaceAll('"' + asset + '"', '"releases/' + id + '/' + asset + '"');
const next = join(root, '.index-next-' + process.pid);
await writeFile(next, html); await rename(next, join(root, 'index.html'));
console.log(JSON.stringify({ site, root, release, id, url: `https://${site}.viblo.se/`, immutableUrl: `https://${site}.viblo.se/releases/${id}/` }, null, 2));
