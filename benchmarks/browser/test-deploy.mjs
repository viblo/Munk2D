#!/usr/bin/env node
import { mkdtemp, cp, readFile, writeFile, mkdir, stat, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import assert from 'node:assert/strict';
const source = resolve(process.argv[2] || ''), script = fileURLToPath(new URL('deploy.mjs', import.meta.url));
if (!process.argv[2]) throw new Error('usage: node test-deploy.mjs STATIC_PACKAGE');
const temp = await mkdtemp(join(tmpdir(), 'munkbench-deploy-test-')), fixture = join(temp,'fixture'), web = join(temp,'web');
try {
  await cp(source, fixture, { recursive:true });
  await writeFile(join(fixture,'PRIVATE-DO-NOT-PUBLISH.txt'),'test-only secret');
  function run(site) { return spawnSync(process.execPath,[script,fixture,site,web],{encoding:'utf8'}); }
  const first=run('munkbench'); assert.equal(first.status,0,first.stderr);
  const deployment=JSON.parse(first.stdout), html=await readFile(join(web,'munkbench','index.html'),'utf8');
  assert.match(html,new RegExp('releases/'+deployment.id+'/app\\.js'));
  assert.equal(await readFile(join(deployment.release,'index.html'),'utf8'),await readFile(join(fixture,'index.html'),'utf8'));
  await assert.rejects(stat(join(deployment.release,'PRIVATE-DO-NOT-PUBLISH.txt')), {code:'ENOENT'});
  const duplicate=run('munkbench'); assert.notEqual(duplicate.status,0); assert.match(duplicate.stderr,/Immutable release already exists/);
  assert.equal(await readFile(join(web,'munkbench','index.html'),'utf8'),html);
  for(const site of ['www','../evil','a','UPPER','m']) assert.notEqual(run(site).status,0);
  await mkdir(join(web,'unrelated')); await writeFile(join(web,'unrelated','index.html'),'existing user app');
  const unrelated=run('unrelated'); assert.notEqual(unrelated.status,0); assert.match(unrelated.stderr,/unrelated site/);
  assert.equal(await readFile(join(web,'unrelated','index.html'),'utf8'),'existing user app');
  await writeFile(join(fixture,'app.js'),'corrupt package');
  const corrupt=run('freshsite'); assert.notEqual(corrupt.status,0); assert.match(corrupt.stderr,/hash mismatch/);
  console.log('Deployment PASS: allowlist, hashes, index-last release paths, immutable duplicates, naming/blocklist, unrelated-site preservation');
} finally { await rm(temp,{recursive:true,force:true}); }
