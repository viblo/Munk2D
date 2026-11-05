import { collectEnvironment, batteryInfo } from './environment.js';
const C = globalThis.MunkBenchCore, $ = id => document.getElementById(id), STORE = 'munk2d-browser-benchmark:last:v1';
const baseUrl = new URL('./', import.meta.url).href;
let manifest, environment, worker = null, result = null, beginTime = 0, storageWarning = '', preparing = false, launch = 0;
const fields = ['device', 'cpu', 'os', 'browser', 'ram', 'power', 'notes'];
const fmt = (value, digits = 2) => value === null || value === undefined || !Number.isFinite(value) ? '—' : value.toFixed(digits);
const percent = value => value === null || value === undefined ? '—' : (value > 0 ? '+' : '') + fmt(value) + '%';
const delta = ratio => ratio === null || ratio === undefined ? '—' : percent(100 * (ratio - 1));
function annotations() { return { deviceModel: $('device').value.trim(), cpuModel: $('cpu').value.trim(), osVersion: $('os').value.trim(), browserVersion: $('browser').value.trim(), installedRam: $('ram').value.trim(), powerSource: $('power').value, notes: $('notes').value.trim(), source: 'user-entered, not automatically verified' }; }
function setAnnotations(value) {
  $('device').value = value?.deviceModel || ''; $('cpu').value = value?.cpuModel || ''; $('ram').value = value?.installedRam || '';
  $('os').value = value?.osVersion || ''; $('browser').value = value?.browserVersion || '';
  $('power').value = value?.powerSource || 'unknown'; $('notes').value = value?.notes || '';
}
function setActive(active) {
  $('quick').disabled = active || !manifest || !environment || !environment.runtime.webAssembly || !environment.runtime.worker;
  $('full').disabled = $('quick').disabled; $('stop').disabled = !active;
  fields.forEach(id => { $(id).disabled = active; });
  $('clear').disabled = active || !result;
}
function persist() {
  if (!result) return;
  try { localStorage.setItem(STORE, JSON.stringify(result)); storageWarning = ''; }
  catch (e) { storageWarning = 'This browser could not save the run locally. Download the JSON before leaving. (' + e.name + ')'; }
}
function warnings() {
  const values = [];
  if (worker && !environment.runtime.simd) values.push('SIMD is not available here. This run measures the scalar baseline and noise controls only.');
  if (result && !['complete', 'running'].includes(result.status)) values.push('This is an incomplete or non-comparison result: ' + result.status + '. ' + (result.error?.message || result.message || ''));
  if (result && result.build.id !== manifest?.build.id) values.push('Saved results are from a different build. Download them before starting a new run.');
  if (storageWarning) values.push(storageWarning);
  if (result?.warnings?.length) values.push(...result.warnings);
  $('warnings').textContent = Array.from(new Set(values)).join('\n');
}
function render() {
  $('download').disabled = !result;
  $('download').textContent = result && result.status !== 'complete' ? 'Download partial / diagnostic JSON' : 'Download results JSON';
  $('clear').disabled = !!worker || !result;
  const cards = $('suite-cards'); cards.replaceChildren();
  for (const suite of result?.status === 'complete' ? result.summary.suites : []) {
    const box = document.createElement('div'); box.className = 'suite';
    const title = document.createElement('h3'); title.textContent = suite.profile + ' · ' + suite.sceneCount + ' scenes';
    const score = document.createElement('div'); score.className = 'big ' + suite.verdict; score.textContent = percent(suite.percent) + ' time';
    const note = document.createElement('p'); note.className = suite.verdict; note.textContent = suite.verdict + ' · interval ' + delta(suite.interval[0]) + ' to ' + delta(suite.interval[1]);
    const controls = document.createElement('p'); controls.className = 'muted'; controls.textContent = 'A/A ' + delta(suite.aaRatio) + ' · module control ' + delta(suite.moduleRatio) + ' · including setup ' + delta(suite.includingInitRatio);
    box.append(title, score, note, controls); cards.append(box);
  }
  const tables = $('tables'); tables.replaceChildren();
  for (const profile of ['smoke', 'reference']) {
    const rows = (result?.summary?.rows || []).filter(x => x.profile === profile);
    if (!rows.length) continue;
    const heading = document.createElement('h3'); heading.textContent = profile + ' sizes';
    const scroll = document.createElement('div'); scroll.className = 'table-scroll'; scroll.tabIndex = 0;
    const table = document.createElement('table'), head = document.createElement('thead'), tr = document.createElement('tr');
    for (const name of ['Scene', 'Original ms/step', 'SIMD ms/step', 'Δ time', 'Bootstrap interval', 'A/A Δ', 'Module Δ', 'Assessment']) { const th = document.createElement('th'); th.scope = 'col'; th.textContent = name; tr.append(th); }
    head.append(tr); table.append(head); const body = document.createElement('tbody');
    for (const row of rows) {
      const line = document.createElement('tr');
      const values = [row.scene.replace(/([a-z])([A-Z])/g, '$1 $2'), fmt(row.originalMsPerStep, 4), fmt(row.simdMsPerStep, 4), percent(row.percent), row.interval ? delta(row.interval[0]) + ' … ' + delta(row.interval[1]) : '—', delta(row.aaRatio), delta(row.moduleRatio), row.verdict + (row.warnings.length ? ' ⚠' : '')];
      values.forEach((value, i) => { const cell = document.createElement('td'); cell.textContent = value; if (i === 3 || i === 7) cell.className = row.verdict; if (row.warnings.length) cell.title = row.warnings.join('\n'); line.append(cell); });
      body.append(line);
    }
    table.append(body); scroll.append(table); tables.append(heading, scroll);
  }
  if (result) {
    const elapsed = result.durationMs !== undefined ? (result.durationMs / 60000).toFixed(1) + ' min' : 'in progress';
    $('result-description').textContent = `${result.status} · ${result.protocol.mode} · ${result.pairs.length}/${result.plannedPairs ?? '?'} pairs · ${elapsed}. Download this JSON and send it back for analysis.`;
    $('diagnostics').textContent = JSON.stringify({ schema: result.schema, runId: result.runId, status: result.status, build: result.build,
      protocol: result.protocol, annotations: result.annotations, workerInfo: result.workerInfo, validation: result.validation,
      calibration: result.calibration.map(x => ({ scene: x.scene, profile: x.profile, batch: x.batch, minimumMs: x.minimumMs, targetMs: x.targetMs, warnings: x.warnings })),
      lifecycle: result.lifecycle, error: result.error || null, rawPairsInDownload: result.pairs.length }, null, 2);
  }
  warnings();
}
function lifecycle(type) {
  if (!result || !worker) return;
  result.lifecycle.push({ type, at: new Date().toISOString(), elapsedMs: performance.now() - beginTime, visibility: document.visibilityState, focused: document.hasFocus() });
}
async function finish(status, details = {}) {
  if (!result || !worker) return;
  worker.terminate(); worker = null;
  result.status = status; Object.assign(result, details);
  result.endedAt = new Date().toISOString(); result.durationMs = performance.now() - beginTime;
  // A partial run is never silently presented as a complete balanced campaign.
  if (status !== 'complete') {
    result.summary = C.summarize(result.pairs, 13, 500);
    result.summary.rows.forEach(row => { if (row.count < result.protocol.primaryRounds) row.verdict = 'incomplete'; });
  }
  $('status').textContent = status === 'complete' ? 'Complete. Download the JSON to compare browsers.' : status + ': ' + (result.error?.message || result.message || 'Partial results retained.');
  result.annotations = annotations();
  persist(); setActive(false); render();
  const finishedRun = result;
  finishedRun.endBattery = await batteryInfo();
  if (result === finishedRun && !worker) persist();
}
async function start(mode) {
  if (worker || preparing || document.visibilityState !== 'visible') return;
  preparing = true;
  const token = ++launch;
  $('status').textContent = 'Collecting current environment…'; setActive(true);
  try {
    environment = await collectEnvironment();
    if (token !== launch) return;
    preparing = false;
    $('environment').textContent = JSON.stringify(environment, null, 2);
    if (document.visibilityState !== 'visible') { setActive(false); $('status').textContent = 'Keep the tab visible before starting.'; return; }
    const protocol = C.protocol(mode, manifest.scenarios);
    result = { schema: C.SCHEMA, runId: crypto.randomUUID ? crypto.randomUUID() : Date.now().toString(36) + '-' + Math.random().toString(36).slice(2),
      status: 'running', startedAt: new Date().toISOString(), build: manifest.build, artifactManifest: manifest,
      page: { url: location.origin + location.pathname, title: document.title }, environment, annotations: annotations(), protocol,
      lifecycle: [], warnings: [], validation: [], checkpoints: [], calibration: [], pairs: [], summary: { rows: [], suites: [] },
      privacy: 'No automatic upload. Manually download/share this file.' };
    beginTime = performance.now();
    const activeWorker = new Worker(new URL('runner.worker.js', import.meta.url)); worker = activeWorker;
    lifecycle('start'); persist(); render();
    $('progress').value = 0; $('progress').max = 1;
    activeWorker.onmessage = event => {
      if (worker !== activeWorker) return;
      const d = event.data;
      try {
        if (d.type === 'ready') {
          result.workerInfo = d.workerInfo; result.plannedPairs = d.plannedPairs; result.effectiveTargetMs = d.effectiveTargetMs;
          $('progress').max = d.plannedPairs || 1;
        } else if (d.type === 'progress') $('status').textContent = d.message;
        else if (d.type === 'resource') result.workerInfo.modules = [...result.workerInfo.modules.filter(x => x.key !== d.resource.key), d.resource];
        else if (d.type === 'validation') result.validation.push(d.validation);
        else if (d.type === 'checkpoint') result.checkpoints.push(d.checkpoint);
        else if (d.type === 'calibration') { result.calibration.push(d.calibration); $('status').textContent = `Measuring ${d.calibration.scene} / ${d.calibration.profile} · batch ${d.calibration.batch}`; }
        else if (d.type === 'pair') {
          result.pairs.push(d.pair); $('progress').value = d.completedPairs;
          $('counts').textContent = `${d.completedPairs}/${d.plannedPairs} paired comparisons · ${((performance.now() - beginTime) / 60000).toFixed(1)} min elapsed`;
        } else if (d.type === 'case-complete') { result.summary.rows.push(d.row); persist(); render(); }
        else if (d.type === 'done') { finish(d.status, { summary: d.summary, workerInfo: d.workerInfo, message: d.message || null }); return; }
        else if (d.type === 'failed') { finish('failed', { error: d.error }); return; }
        if (d.ackId) setTimeout(() => { if (worker === activeWorker) activeWorker.postMessage({ type: 'ack', ackId: d.ackId }); }, 0);
      } catch (error) { finish('failed', { error: { message: error.message, stack: error.stack } }); }
    };
    activeWorker.onerror = event => { event.preventDefault(); finish('failed', { error: { message: event.message || 'Worker crashed or ran out of memory', filename: event.filename || null, line: event.lineno || null } }); };
    activeWorker.postMessage({ type: 'start', manifest, baseUrl, protocol });
    setActive(true);
  } catch (error) {
    preparing = false;
    if (worker) finish('failed', { error: { message: error.message, stack: error.stack } });
    else { $('status').textContent = error.message; setActive(false); }
  }
}
$('quick').addEventListener('click', () => start('quick'));
$('full').addEventListener('click', () => start('full'));
$('stop').addEventListener('click', () => {
  if (preparing) { ++launch; preparing = false; setActive(false); $('status').textContent = 'Preparation cancelled. No benchmark started.'; return; }
  lifecycle('user-stop'); finish('cancelled', { error: { message: 'Stopped by user; unfinished measurements are not included.' } });
});
$('download').addEventListener('click', () => {
  if (!result) return;
  result.annotations = annotations();
  const payload = { ...result, exportedAt: new Date().toISOString(), ...(worker ? { status: 'in-progress-snapshot' } : {}) };
  const blob = new Blob([JSON.stringify(payload, null, 2) + '\n'], { type: 'application/json' }), url = URL.createObjectURL(blob), link = document.createElement('a');
  const label = (result.environment.browser.name || 'browser').toLowerCase().replace(/[^a-z0-9]+/g, '-');
  link.href = url; link.download = `munk2d-${label}-${result.protocol.mode}-${result.startedAt.replace(/[:.]/g, '-')}${result.status === 'complete' ? '' : '-partial'}.json`;
  document.body.append(link); link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(url), 30000); persist();
});
$('clear').addEventListener('click', () => {
  if (worker || !confirm('Clear the last saved run from this browser? Download it first if you want to keep it.')) return;
  try { localStorage.removeItem(STORE); } catch (_) { /* storage may be blocked */ }
  result = null; storageWarning = ''; $('diagnostics').textContent = 'No run yet.'; $('result-description').textContent = 'Saved result cleared. Start a new run when ready.'; render();
});
for (const id of fields) $(id).addEventListener('change', () => { if (result && !worker) { result.annotations = annotations(); persist(); } });
document.addEventListener('visibilitychange', () => { lifecycle('visibilitychange'); if (worker && document.visibilityState !== 'visible') finish('interrupted', { error: { message: 'Tab became hidden or the screen was locked. Partial results retained; rerun for a complete balanced comparison.' } }); });
window.addEventListener('pagehide', () => { lifecycle('pagehide'); if (worker) finish('interrupted', { error: { message: 'Page was closed or navigated away.' } }); });
document.addEventListener('freeze', () => { lifecycle('freeze'); if (worker) finish('interrupted', { error: { message: 'Browser froze the page.' } }); });
window.addEventListener('blur', () => lifecycle('blur'));
window.addEventListener('focus', () => lifecycle('focus'));
async function initialize() {
  try {
    const response = await fetch(new URL('manifest.json', baseUrl)); if (!response.ok) throw new Error('Could not load build manifest'); manifest = await response.json();
    if (manifest.schema !== C.SCHEMA || manifest.scenarios.length !== 26) throw new Error('Unsupported/incomplete build manifest');
    environment = await collectEnvironment();
    $('environment').textContent = JSON.stringify(environment, null, 2);
    const b = environment.browser, os = environment.os, h = environment.hardware;
    $('device-summary').textContent = `${b.name} ${b.version || ''} · ${os.name}${os.version ? ' (reported version ' + os.version + ')' : ''} · ${h.logicalProcessorsHint ?? '?'} logical processors reported. CPU model is not exposed; enter it above if known.`;
    $('simd-badge').textContent = environment.runtime.simd ? 'Wasm SIMD supported' : 'Scalar-only runtime';
    $('build-info').textContent = `Build ${manifest.build.id} · engine ${manifest.build.engineRevision.slice(0, 12)} · Emscripten ${manifest.build.emscriptenVersion} · ${manifest.build.generatedAt}`;
    try {
      const saved = JSON.parse(localStorage.getItem(STORE) || 'null');
      if (saved?.schema === C.SCHEMA) {
        result = saved; setAnnotations(saved.annotations);
        if (saved.status === 'running') { result.status = 'interrupted'; result.error = { message: 'The previous page session ended during a run.' }; persist(); }
        $('status').textContent = 'Last saved result restored. Download it before starting another run.';
      } else $('status').textContent = 'Ready. Choose quick or full.';
    } catch (_) { $('status').textContent = 'Ready. Local result storage may be unavailable.'; }
    setActive(false); render();
    if (!environment.runtime.webAssembly || !environment.runtime.worker) $('status').textContent = 'This browser does not expose WebAssembly and dedicated workers; timing is unavailable.';
  } catch (error) { $('status').textContent = 'Initialization failed: ' + error.message; $('warnings').textContent = 'Reload once the connection is available. No benchmark has started.'; }
}
initialize();
