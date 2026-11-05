#!/usr/bin/env node
// Development-only Playwright coverage. The published page has no npm dependency.
import { chromium, firefox, webkit } from 'playwright';
import { createServer } from 'node:http';
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { resolve, sep } from 'node:path';
import assert from 'node:assert/strict';
const [directory, kind = 'chromium', mode = 'diagnostic', outputArg = 'browser-test-output'] = process.argv.slice(2);
if (!directory || !['chromium','firefox','webkit'].includes(kind) || !['diagnostic','quick','full'].includes(mode)) throw new Error('usage: node test-browser.mjs STATIC_DIR [chromium|firefox|webkit] [diagnostic|quick|full] [OUTPUT]');
const root = resolve(directory), output = resolve(outputArg); await mkdir(output, { recursive: true });
const server = createServer(async(req,res) => {
  const url = new URL(req.url, 'http://localhost'), path = resolve(root, '.' + (url.pathname === '/' ? '/index.html' : url.pathname));
  if (!path.startsWith(root + sep)) { res.writeHead(403).end(); return; }
  try {
    res.setHeader('Content-Type', path.endsWith('.wasm') ? 'application/wasm' : path.endsWith('.js') ? 'text/javascript' : path.endsWith('.css') ? 'text/css' : path.endsWith('.json') ? 'application/json' : path.endsWith('.svg') ? 'image/svg+xml' : 'text/html');
    res.end(await readFile(path));
  } catch { res.writeHead(404).end(); }
});
await new Promise(r=>server.listen(0,'127.0.0.1',r)); let browser;
const report = { kind, mode, startedAt: new Date().toISOString(), checks: [] };
try {
  browser = await ({chromium,firefox,webkit}[kind]).launch({ headless: true }); report.browserVersion = browser.version();
  const context = await browser.newContext({ viewport:{width:390,height:844}, acceptDownloads:true });
  const page = await context.newPage(), errors=[], requests=[];
  page.on('pageerror', e=>errors.push(e.message)); context.on('request', r=>requests.push({url:r.url(),method:r.method()}));
  const origin=`http://127.0.0.1:${server.address().port}`;
  async function ready() { await page.goto(origin); await page.waitForFunction(()=>!document.getElementById('quick').disabled); }
  await ready();
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
  const environment=JSON.parse(await page.locator('#environment').textContent());
  assert.equal(environment.hardware.cpuModel,null); assert.equal(environment.runtime.physicsThreadsUsed,1);
  assert.match(environment.hardware.cpuModelReason,/not reliably/); report.environment=environment;
  report.checks.push('mobile viewport, metadata, honest CPU fallback');
  // Short, explicitly diagnostic worker protocol exercises actual Wasm and controls.
  const diagnostic = await page.evaluate(async()=>{
    const manifest=await(await fetch('manifest.json')).json(), protocol=MunkBenchCore.protocol('quick',manifest.scenarios);
    protocol.mode='diagnostic'; protocol.profiles=['smoke']; protocol.scenarios=manifest.scenarios.filter(x=>x.name==='N2');
    protocol.primaryRounds=2; protocol.controlRounds=2; protocol.targetMs=10; protocol.bootstrapDraws=100;
    const events=[], worker=new Worker('runner.worker.js'); let heartbeats=0; const timer=setInterval(()=>heartbeats++,10);
    try { return await new Promise((resolve,reject)=>{
      worker.onmessage=event=>{const d=event.data;events.push(d);if(d.ackId)worker.postMessage({type:'ack',ackId:d.ackId});
        if(d.type==='done')resolve({events,heartbeats});if(d.type==='failed')reject(new Error(d.error.message));};
      worker.onerror=e=>reject(new Error(e.message)); worker.postMessage({type:'start',manifest,baseUrl:new URL('./',location.href).href,protocol});
    }); } finally {clearInterval(timer);worker.terminate();}
  });
  const pairs=diagnostic.events.filter(x=>x.type==='pair').map(x=>x.pair), done=diagnostic.events.find(x=>x.type==='done');
  assert.equal(done.status,'complete'); assert.equal(pairs.length,6); assert(diagnostic.heartbeats>0);
  for(const comparison of ['primary','aa','module'])assert.deepEqual(pairs.filter(x=>x.comparison===comparison).map(x=>x.order).sort(),['AB','BA']);
  assert(pairs.every(x=>x.base.batch===x.candidate.batch&&Number.isFinite(x.ratio)));
  assert(diagnostic.events.some(x=>x.type==='checkpoint'&&x.checkpoint.invalidValues===0));
  assert(done.workerInfo.modules.every(x=>x.hashesVerified));
  report.diagnostic=diagnostic; report.checks.push('actual worker/Wasm, module hashes, correctness, balanced AB/BA and controls, responsive page');
  await page.locator('#cpu').fill('TEST: manually entered CPU');
  await page.locator('#device').fill('TEST: browser automation');
  await page.locator('#os').fill('TEST: manually entered OS');
  await page.locator('#browser').fill('TEST: manually entered browser');
  await page.locator('#quick').click();
  await page.waitForFunction(()=>/Loading|Correctness/.test(document.getElementById('status').textContent));
  await page.locator('#stop').click();
  await page.waitForFunction(()=>/cancelled:/.test(document.getElementById('status').textContent));
  const downloadPromise=page.waitForEvent('download'); await page.locator('#download').click(); const download=await downloadPromise;
  const partial=JSON.parse(await readFile(await download.path(),'utf8'));
  assert.equal(partial.status,'cancelled'); assert.equal(partial.annotations.cpuModel,'TEST: manually entered CPU');
  assert.equal(partial.annotations.osVersion,'TEST: manually entered OS'); assert.equal(partial.annotations.browserVersion,'TEST: manually entered browser');
  assert.equal(partial.environment.hardware.cpuModel,null); assert.equal(partial.schema,'munk2d-browser-benchmark/1');
  assert(partial.exportedAt); assert(partial.endedAt); report.checks.push('stop preserves honest partial JSON and user annotations');
  await page.reload(); await page.waitForFunction(()=>/restored/.test(document.getElementById('status').textContent));
  assert.equal(await page.locator('#cpu').inputValue(),'TEST: manually entered CPU');
  assert.equal(await page.locator('#os').inputValue(),'TEST: manually entered OS');
  await page.locator('#quick').click(); await page.waitForFunction(()=>/Loading|Correctness/.test(document.getElementById('status').textContent));
  await page.evaluate(()=>{Object.defineProperty(document,'visibilityState',{configurable:true,get:()=> 'hidden'});document.dispatchEvent(new Event('visibilitychange'));});
  await page.waitForFunction(()=>/interrupted:/.test(document.getElementById('status').textContent));
  report.checks.push('local restore and hidden-tab interruption');
  await page.reload(); await page.waitForFunction(()=>!document.getElementById('quick').disabled);
  if(mode!=='diagnostic') {
    await page.locator('#'+mode).click();
    await page.waitForFunction(()=>/Complete\.|failed:|precision-limited:/.test(document.getElementById('status').textContent),null,{timeout:900000});
    assert.match(await page.locator('#status').textContent(),/^Complete\./);
    const complete=JSON.parse(await page.evaluate(()=>localStorage.getItem('munk2d-browser-benchmark:last:v1')));
    assert.equal(complete.status,'complete'); assert.equal(complete.pairs.length,mode==='quick'?128:728);
    assert.equal(complete.checkpoints.length,mode==='quick'?16:52); assert.equal(complete.summary.suites.length,2);
    assert(complete.calibration.every(x=>x.batch>=1));
    assert.equal(complete.environment.runtime.crossOriginIsolated,false); // No special hosting headers needed.
    await writeFile(resolve(output,kind+'-'+mode+'-result.json'),JSON.stringify(complete,null,2));
    report.checks.push('actual '+mode+' UI run, exact scene/pair coverage, two suites, no cross-origin isolation');
  }
  await page.screenshot({path:resolve(output,kind+'-mobile.png'),fullPage:true});
  await page.setViewportSize({width:1280,height:900}); await page.screenshot({path:resolve(output,kind+'-desktop.png'),fullPage:true});
  // Unsupported SIMD: both page and worker feature detection are deliberately
  // stubbed in this separate test context; no SIMD artifact may be fetched.
  const fallback=await browser.newContext({viewport:{width:390,height:844}}), fallbackRequests=[];
  await fallback.route('**/core.js',async route=>{const response=await route.fetch();await route.fulfill({response,body:await response.text()+'\nMunkBenchCore.supportsSimd=()=>false;\n'});});
  fallback.on('request',r=>fallbackRequests.push(r.url()));
  const fallbackPage=await fallback.newPage(); await fallbackPage.goto(origin); await fallbackPage.waitForFunction(()=>!document.getElementById('quick').disabled);
  const scalar=await fallbackPage.evaluate(async()=>{
    const manifest=await(await fetch('manifest.json')).json(), protocol=MunkBenchCore.protocol('quick',manifest.scenarios);
    Object.assign(protocol,{mode:'diagnostic',profiles:['smoke'],scenarios:manifest.scenarios.filter(x=>x.name==='N2'),primaryRounds:2,controlRounds:2,targetMs:10,bootstrapDraws:100});
    const worker=new Worker('runner.worker.js'),events=[];
    try{return await new Promise((resolve,reject)=>{worker.onmessage=e=>{const d=e.data;events.push(d);if(d.ackId)worker.postMessage({type:'ack',ackId:d.ackId});if(d.type==='done')resolve(events);if(d.type==='failed')reject(new Error(d.error.message));};worker.onerror=e=>reject(new Error(e.message));worker.postMessage({type:'start',manifest,baseUrl:new URL('./',location.href).href,protocol});});}finally{worker.terminate();}
  });
  assert.equal(scalar.filter(x=>x.type==='pair').length,2);
  assert.equal(scalar.find(x=>x.type==='done').summary.suites.length,0);
  assert.equal(fallbackRequests.some(x=>x.includes('/modules/simd/')),false); report.checks.push('unsupported-SIMD scalar path without fetching SIMD artifacts');
  await fallback.close();
  // Corrupt a module response: must fail before execution/timing, not silently
  // proceed with a stale/mixed build.
  const corrupt=await browser.newContext();
  await corrupt.route('**/modules/simd/contact_solver.wasm',async route=>{const response=await route.fetch();const body=Buffer.from(await response.body());body[0]^=1;await route.fulfill({response,body});});
  const corruptPage=await corrupt.newPage(); await corruptPage.goto(origin);await corruptPage.waitForFunction(()=>!document.getElementById('quick').disabled);
  await corruptPage.locator('#quick').click();await corruptPage.waitForFunction(()=>/failed:/.test(document.getElementById('status').textContent));
  assert.match(await corruptPage.locator('#status').textContent(),/SHA256 mismatch/);report.checks.push('stale/corrupted Wasm fails closed');await corrupt.close();
  assert.equal(requests.some(x=>x.method!=='GET'),false); assert.deepEqual(errors,[]); report.checks.push('no automatic uploads or page exceptions');
  report.finishedAt=new Date().toISOString();report.status='PASS';
  await writeFile(resolve(output,kind+'-'+mode+'-checks.json'),JSON.stringify(report,null,2)); console.log(kind,mode,'PASS',report.checks);
}finally{await browser?.close();await new Promise(r=>server.close(r));}
