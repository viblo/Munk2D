import { test } from 'node:test';
import assert from 'node:assert/strict';
import './core.js';
const C = globalThis.MunkBenchCore;
function pair(ratio, comparison = 'primary', scene = 'A', profile = 'reference') {
  return { scene, profile, comparison, steps: 100, base: { batch: 2, runSeconds: 1, initSeconds: 0.1 }, candidate: { batch: 2, runSeconds: ratio, initSeconds: 0.1 }, ratio, totalRatio: (ratio + 0.1) / 1.1, warnings: [] };
}
test('median, geometric mean and deterministic bootstrap', () => {
  assert.equal(C.median([]), null); assert.equal(C.median([3,1,2]), 2); assert.equal(C.median([4,1,2,3]), 2.5);
  assert.equal(C.geometricMean([0.5,2]), 1); assert.deepEqual(C.interval([0.8,0.8,0.8,0.8], 13), [0.8,0.8]);
  assert.deepEqual(C.interval([0.8,0.9,1.1,1.2], 4), C.interval([0.8,0.9,1.1,1.2], 4));
});
test('SIMD feature probe is valid in a supported Node runtime', () => { assert.equal(C.supportsSimd(), true); });
test('quick/full preserve scene steps and profile sizes, balanced pairs', () => {
  const scenes = C.QUICK.map(name => ({ name, steps: 100, smokeSize: 1, referenceSize: 2 }));
  const q = C.protocol('quick', scenes), f = C.protocol('full', scenes);
  assert.equal(q.scenarios.length, 8); assert.equal(q.primaryRounds % 2, 0); assert.equal(f.controlRounds % 2, 0);
  assert.equal(C.plan(q, true).pairCount, 128); assert.equal(C.plan(q, false).pairCount, 32);
  assert.equal(q.scenarios[0].steps, 100); assert.equal(q.scenarios[0].referenceSize, 2);
  assert.throws(() => C.protocol('other', scenes)); assert.throws(() => C.protocol('quick', []));
  assert.equal(C.validateConfig(q), q); assert.throws(() => C.validateConfig({ ...q, primaryRounds: 3 }));
  assert.throws(() => C.validateConfig({ ...q, maxBatch: 1000000 }));
});
test('CSV parsing rejects malformed or duplicate results and preserves zero for calibration', () => {
  const row = C.parseTiming(['version,benchmark,size,sample,batch,init_time,run_time', 'unknown,A,5,1,2,0.1,0.2'], 'A');
  assert.equal(row.runSeconds, 0.2); assert.equal(row.batch, 2);
  assert.throws(() => C.parseTiming(['unknown,A,5,1,2,0.1,NaN'], 'A'));
  assert.throws(() => C.parseTiming(['unknown,A,5,1,2,0.1,0.2','unknown,A,5,1,2,0.1,0.2'], 'A'));
  assert.equal(C.parseTiming(['unknown,A,5,1,2,0.1,0'], 'A').runSeconds, 0);
});
test('checkpoints require finite-state counters and a matching scene', () => {
  const good = { benchmarks: [{ benchmark: 'A', checkpoints: [{ step: 1, invalid_values: 0 }] }] };
  assert.equal(C.validateSnapshot(good, 'A'), good);
  assert.throws(() => C.validateSnapshot(good, 'B'));
  assert.throws(() => C.validateSnapshot({ benchmarks: [{ benchmark: 'A', checkpoints: [{ invalid_values: 1 }] }] }, 'A'));
});
test('speedups, regressions and inconclusive classifications retain controls', () => {
  for (const [ratio, verdict] of [[0.8,'faster'], [1.2,'slower'], [1.005,'inconclusive']]) {
    const pairs = [...Array(6)].map(() => pair(ratio)).concat([pair(1,'aa'), pair(1,'aa'), pair(1.03,'module')]);
    const row = C.rowSummary(pairs, 'A', 'reference'); assert.equal(row.verdict, verdict); assert.equal(row.moduleRatio, 1.03);
    assert.equal(row.originalMsPerStep, 5); assert.equal(row.simdMsPerStep, 5*ratio);
  }
  const noisy = [...Array(6)].map(() => pair(0.95)).concat([pair(1.1,'aa'),pair(0.9,'aa')]);
  assert.equal(C.rowSummary(noisy,'A','reference').verdict, 'inconclusive');
});
test('suite is equal-scene geometric mean, not arithmetic average of percentages', () => {
  const pairs = [];
  for (const [scene,ratio] of [['A',0.5],['B',2]]) for (let i=0;i<4;i++) pairs.push(pair(ratio,'primary',scene));
  const summary = C.summarize(pairs,13,100);
  assert.equal(summary.rows.length,2); assert.equal(summary.suites[0].ratio,1); assert.equal(summary.suites[0].verdict,'inconclusive');
});
test('scalar-only and empty campaigns do not fabricate SIMD scores', () => {
  const summary = C.summarize([pair(1,'scalar-aa')],13,100);
  assert.equal(summary.rows[0].verdict,'scalar-only'); assert.equal(summary.rows[0].ratio,null); assert.deepEqual(summary.suites,[]);
  assert.deepEqual(C.summarize([]), { rows:[], suites:[] });
});
test('timer resolution sampling is bounded and handles coarse/unobservable clocks', () => {
  let tick=0; assert.equal(C.timerResolution(() => tick++),1); assert.equal(C.timerResolution(() => 1),null);
});
