/* One dedicated JS worker, one Wasm physics thread. No shared memory or pthreads. */
'use strict';
importScripts('core.js');
const C = self.MunkBenchCore;
const acknowledgments = new Map();
let sequence = 0, started = false;
function emit(type, data, wait = true) {
  if (!wait) { self.postMessage({ type, ...data }); return Promise.resolve(); }
  const ackId = ++sequence;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => { acknowledgments.delete(ackId); reject(new Error('Page stopped acknowledging progress')); }, 30000);
    acknowledgments.set(ackId, () => { clearTimeout(timer); resolve(); });
    self.postMessage({ type, ackId, ...data });
  });
}
function sameOrigin(path, base) {
  const url = new URL(path, base);
  if (url.origin !== self.location.origin) throw new Error('Only same-origin benchmark assets are allowed');
  return url.href;
}
async function digest(bytes) {
  const value = await crypto.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(value), x => x.toString(16).padStart(2, '0')).join('');
}
async function execute(input) {
  const protocol = C.validateConfig(input.protocol), manifest = input.manifest, baseUrl = input.baseUrl;
  const simd = C.supportsSimd(), schedule = C.plan(protocol, simd), modules = new Map(), pairs = [];
  const resolutionMs = C.timerResolution(), workerInfo = { simd, timerResolutionMs: resolutionMs,
    hardwareConcurrencyHint: navigator.hardwareConcurrency || null, userAgent: navigator.userAgent,
    crossOriginIsolated: self.crossOriginIsolated, physicsThreads: 1, sharedWasmMemory: false, modules: [] };
  const targetMs = Math.min(500, Math.max(protocol.targetMs, 200 * (resolutionMs || 0)));
  await emit('ready', { workerInfo, plannedPairs: schedule.pairCount, caseCount: schedule.cases.length, effectiveTargetMs: targetMs });
  async function load(group, name) {
    const key = group + '/' + name;
    if (modules.has(key)) return modules.get(key);
    const entry = manifest.modules[group]?.[name];
    if (!entry) throw new Error('Missing module ' + key);
    if (group === 'simd' && !simd) throw new Error('Refusing to load SIMD on an unsupported runtime');
    await emit('progress', { message: 'Loading ' + key });
    const begin = performance.now(), jsUrl = sameOrigin(entry.js, baseUrl), wasmUrl = sameOrigin(entry.wasm, baseUrl);
    const [jsResponse, wasmResponse] = await Promise.all([fetch(jsUrl), fetch(wasmUrl)]);
    if (!jsResponse.ok || !wasmResponse.ok) throw new Error('Failed to download ' + key);
    const [jsBytes, wasmBytes] = await Promise.all([jsResponse.arrayBuffer(), wasmResponse.arrayBuffer()]);
    if (await digest(jsBytes) !== entry.jsSha256 || await digest(wasmBytes) !== entry.wasmSha256) throw new Error('Asset SHA256 mismatch: ' + key);
    // The immutable same-origin JS is imported only after its hash is checked.
    importScripts(jsUrl);
    const factory = self.createMunkModule, out = [], err = [];
    const m = await factory({ wasmBinary: new Uint8Array(wasmBytes), locateFile: file => new URL(file, jsUrl).href,
      print: line => out.push(String(line)), printErr: line => err.push(String(line)) });
    const resource = { key, loadMs: performance.now() - begin, jsSha256: entry.jsSha256, wasmSha256: entry.wasmSha256,
      wasmBytes: wasmBytes.byteLength, hashesVerified: true, initialStderr: err.slice(), heapBytes: m.HEAPU8?.byteLength || null };
    workerInfo.modules.push(resource);
    const value = { m, out, err };
    modules.set(key, value);
    await emit('resource', { resource });
    return value;
  }
  function call(module, args) {
    const { m, out, err } = module;
    out.length = err.length = 0;
    const stack = m.stackSave(), begin = performance.now();
    let status;
    try { status = m.callMain(args.slice()); }
    finally { m.stackRestore(stack); }
    const wallMs = performance.now() - begin;
    if (status !== 0 || err.length) throw new Error(JSON.stringify({ status, stderr: err, stdout: out.slice(0, 5) }));
    return { lines: out.slice(), wallMs };
  }
  const group = simd ? 'simd' : 'scalar';
  for (const name of ['contact_solver', 'contact_solver_alloc', 'contact_solver_quality']) {
    await emit('progress', { message: 'Correctness: ' + name + ' (' + group + ')' });
    const output = call(await load(group, name), []);
    if (!output.lines.some(x => x.startsWith('PASS:'))) throw new Error('Missing test PASS: ' + name);
    await emit('validation', { validation: { kind: name, module: group, passed: true, output: output.lines, wallMs: output.wallMs } });
    modules.delete(group + '/' + name); // Do not retain test heaps during timing.
  }
  if (simd) {
    const output = call(await load('scalar', 'contact_solver'), []);
    if (!output.lines.some(x => x.startsWith('PASS:'))) throw new Error('Scalar fallback API check failed');
    await emit('validation', { validation: { kind: 'scalar-fallback-api', module: 'scalar', passed: true, output: output.lines, wallMs: output.wallMs } });
    modules.delete('scalar/contact_solver');
  }
  if (resolutionMs === null || resolutionMs >= 10) {
    await emit('done', { status: 'precision-limited', message: 'Timer precision is too coarse or unobservable for useful comparisons; correctness checks passed.', workerInfo, summary: { rows: [], suites: [] } }, false);
    return;
  }
  const scalarModule = await load('scalar', 'munkbench'), simdModule = simd ? await load('simd', 'munkbench') : null;
  const original = simd ? { module: 'simd', solver: 'original' } : { module: 'scalar', solver: 'original' };
  const accelerated = { module: 'simd', solver: 'wasm-simd' }, scalar = { module: 'scalar', solver: 'original' };
  function bench(variant, test, batch, summary = false) {
    const args = ['--contact-solver', variant.solver, '--profile', test.profile, '-b', test.name];
    if (summary) args.push('--summary-json', '--checkpoints', protocol.checkpointSteps);
    else args.push('--samples', '1', '--warmup', '0', '--batch', String(batch));
    const output = call(variant.module === 'simd' ? simdModule : scalarModule, args);
    if (summary) return C.validateSnapshot(JSON.parse(output.lines.join('\n')), test.name);
    const value = C.parseTiming(output.lines, test.name);
    if (value.batch !== batch || value.size !== test[test.profile + 'Size']) throw new Error('Unexpected batch/scene size');
    return { ...value, module: variant.module, solver: variant.solver, wallMs: output.wallMs,
      msPerStep: 1000 * value.runSeconds / value.batch / test.steps };
  }
  for (let index = 0; index < schedule.cases.length; ++index) {
    const test = schedule.cases[index];
    await emit('progress', { message: `Checking ${test.name} / ${test.profile} (${index + 1}/${schedule.cases.length})` });
    const originalSnapshot = bench(original, test, 1, true), scalarSnapshot = bench(scalar, test, 1, true);
    if (JSON.stringify(originalSnapshot) !== JSON.stringify(scalarSnapshot)) throw new Error('ORIGINAL module checkpoint mismatch: ' + test.name);
    const simdSnapshot = simd ? bench(accelerated, test, 1, true) : null;
    const repeated = bench(simd ? accelerated : original, test, 1, true);
    if (JSON.stringify(repeated) !== JSON.stringify(simd ? simdSnapshot : originalSnapshot)) throw new Error('Deterministic repeat mismatch: ' + test.name);
    await emit('checkpoint', { checkpoint: { scene: test.name, profile: test.profile, passed: true,
      originalModuleExact: true, repeatExact: true, invalidValues: 0,
      originalSha256: await digest(new TextEncoder().encode(JSON.stringify(originalSnapshot))),
      simdSha256: simd ? await digest(new TextEncoder().encode(JSON.stringify(simdSnapshot))) : null,
      original: originalSnapshot, simd: simdSnapshot,
      note: 'Different ORIGINAL/SIMD schedules are not required to have identical trajectories.' } });
    const variants = simd ? [original, accelerated, scalar] : [original], trials = [];
    let batch = 1, minimumMs = 0;
    // Calibration and JIT warmup are explicitly outside all reported pairs.
    for (let attempt = 0; attempt < 5; ++attempt) {
      minimumMs = Infinity;
      for (const variant of variants) {
        const value = bench(variant, test, batch);
        trials.push({ attempt, batch, ...value });
        minimumMs = Math.min(minimumMs, value.runSeconds * 1000);
      }
      if (minimumMs >= targetMs * 0.9 || batch >= protocol.maxBatch) break;
      batch = Math.min(protocol.maxBatch, Math.max(batch + 1, Math.ceil(batch * targetMs / Math.max(minimumMs, 0.05))));
    }
    if (!(minimumMs > 0)) throw new Error('Clock did not advance during calibration');
    const caseWarnings = minimumMs < targetMs * 0.9 ? ['Calibration did not reach the target duration; inspect short samples'] : [];
    await emit('calibration', { calibration: { scene: test.name, profile: test.profile, targetMs, batch, minimumMs, trials, warnings: caseWarnings } });
    const comparisons = simd ? [['primary', original, accelerated, protocol.primaryRounds], ['aa', original, original, protocol.controlRounds], ['module', scalar, original, protocol.controlRounds]]
      : [['scalar-aa', scalar, scalar, protocol.controlRounds]];
    for (let round = 0; round < Math.max(protocol.primaryRounds, protocol.controlRounds); ++round) {
      // Rotate comparison blocks as well as alternating the adjacent A/B order.
      const blocks = round % 2 ? comparisons.slice().reverse() : comparisons;
      for (const [comparison, a, b, count] of blocks) {
        if (round >= count) continue;
        const reverse = (round + index) % 2 === 1, order = reverse ? [b, a] : [a, b], values = [];
        for (const variant of order) values.push(bench(variant, test, batch));
        const [base, candidate] = reverse ? values.reverse() : values;
        if (!(base.runSeconds > 0 && candidate.runSeconds > 0)) throw new Error('Zero-duration measured sample');
        const warnings = caseWarnings.slice();
        if (Math.min(base.runSeconds, candidate.runSeconds) * 1000 < targetMs * 0.7) warnings.push('Measured sample shorter than calibrated target');
        const pair = { scene: test.name, profile: test.profile, steps: test.steps, round, comparison, order: reverse ? 'BA' : 'AB',
          firstVariant: reverse ? b : a, batch, base, candidate, ratio: candidate.runSeconds / base.runSeconds,
          totalRatio: (candidate.initSeconds + candidate.runSeconds) / (base.initSeconds + base.runSeconds), warnings };
        pairs.push(pair);
        await emit('pair', { pair, completedPairs: pairs.length, plannedPairs: schedule.pairCount });
      }
    }
    await emit('case-complete', { row: C.rowSummary(pairs, test.name, test.profile, 13 + index, protocol.bootstrapDraws), completedCases: index + 1, caseCount: schedule.cases.length });
  }
  if (pairs.length !== schedule.pairCount) throw new Error('Incomplete paired measurement schedule');
  await emit('done', { status: 'complete', workerInfo, summary: C.summarize(pairs, 13, protocol.bootstrapDraws) }, false);
}
self.onmessage = event => {
  const data = event.data;
  if (data.type === 'ack') { const resolve = acknowledgments.get(data.ackId); if (resolve) { acknowledgments.delete(data.ackId); resolve(); } return; }
  if (data.type !== 'start' || started) return;
  started = true;
  execute(data).catch(error => emit('failed', { error: { name: error.name, message: error.message, stack: error.stack || null } }, false));
};
