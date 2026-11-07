/* Shared, dependency-free protocol/statistics. Works in a classic worker and tests. */
(function (root) {
  'use strict';
  const SCHEMA = 'munk2d-browser-benchmark/1';
  const QUICK = ['FallingSquares', 'FallingCircles', 'FrictionalPyramid', 'CollisionCallbacks', 'N2', 'MostlyStaticSingleBody', 'SurfaceVelocity', 'GrooveConstraints'];
  const median = values => {
    if (!values.length) return null;
    const a = values.slice().sort((x, y) => x - y), i = Math.floor(a.length / 2);
    return a.length % 2 ? a[i] : (a[i - 1] + a[i]) / 2;
  };
  const geometricMean = values => values.length ? Math.exp(values.reduce((s, x) => s + Math.log(x), 0) / values.length) : null;
  const percentile = (sorted, p) => {
    const i = (sorted.length - 1) * p, lo = Math.floor(i), hi = Math.ceil(i);
    return sorted[lo] + (sorted[hi] - sorted[lo]) * (i - lo);
  };
  function random(seed) {
    let state = seed >>> 0 || 1;
    return () => { state ^= state << 13; state ^= state >>> 17; state ^= state << 5; return (state >>> 0) / 4294967296; };
  }
  function interval(ratios, seed, draws = 1500) {
    if (ratios.length < 2) return null;
    const rng = random(seed), samples = [];
    for (let i = 0; i < draws; ++i) {
      const sample = [];
      for (let j = 0; j < ratios.length; ++j) sample.push(ratios[Math.floor(rng() * ratios.length)]);
      samples.push(median(sample));
    }
    samples.sort((a, b) => a - b);
    return [percentile(samples, 0.025), percentile(samples, 0.975)];
  }
  function timerResolution(now = () => performance.now()) {
    const samples = [];
    let previous = now();
    for (let i = 0; i < 12000 && samples.length < 80; ++i) {
      const current = now(), delta = current - previous;
      if (delta > 0) samples.push(delta);
      previous = current;
    }
    return samples.length ? Math.min(...samples) : null;
  }
  function supportsSimd() {
    try {
      // () -> v128, v128.const zero. Validation only; no SIMD module is loaded.
      return WebAssembly.validate(new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,123,3,2,1,0,10,22,1,20,0,253,12,...Array(16).fill(0),11]));
    } catch (_) { return false; }
  }
  function protocol(mode, scenarios) {
    if (mode !== 'quick' && mode !== 'full') throw new Error('Unknown run mode');
    const selected = mode === 'quick' ? scenarios.filter(x => QUICK.includes(x.name)) : scenarios;
    if (mode === 'quick' && selected.length !== QUICK.length) throw new Error('Incomplete quick scene list');
    return {
      mode, profiles: ['smoke', 'reference'], scenarios: selected,
      primaryRounds: mode === 'quick' ? 4 : 6,
      controlRounds: mode === 'quick' ? 2 : 4,
      targetMs: 80, maxBatch: 2048, bootstrapDraws: 1500,
      checkpointSteps: '0,1,10,final', timing: 'existing MunkBench full trajectory, complete cpSpaceStep pipeline',
      order: 'adjacent alternating AB/BA; balanced primary and control counts',
      noRenderingDuringTiming: true, physicsThreads: 1,
      baseline: 'BASELINE in the same SIMD-enabled module',
      moduleControl: 'BASELINE in scalar-only vs SIMD-enabled modules',
      classification: 'bootstrap interval must clear A/A noise margin (at least 1%); exploratory, not a universal confidence guarantee'
    };
  }
  function plan(protocol, simd) {
    const cases = [];
    for (const profile of protocol.profiles) for (const scenario of protocol.scenarios) cases.push({ profile, ...scenario });
    return { cases, pairCount: cases.length * (simd ? protocol.primaryRounds + 2 * protocol.controlRounds : protocol.controlRounds) };
  }
  function parseTiming(lines, scenario) {
    const rows = lines.map(x => x.split(',')).filter(x => x.length === 7 && x[1] === scenario && Number.isFinite(Number(x[6])));
    if (rows.length !== 1) throw new Error('Expected one MunkBench CSV sample for ' + scenario);
    const r = rows[0], value = { version: r[0], scene: r[1], size: +r[2], sample: +r[3], batch: +r[4], initSeconds: +r[5], runSeconds: +r[6] };
    if (!Number.isInteger(value.batch) || value.batch < 1 || value.runSeconds < 0 || value.initSeconds < 0 || !Number.isFinite(value.initSeconds)) throw new Error('Invalid timing row');
    return value;
  }
  function validateSnapshot(value, scenario) {
    if (!value || !Array.isArray(value.benchmarks) || value.benchmarks.length !== 1 || value.benchmarks[0].benchmark !== scenario) throw new Error('Invalid checkpoint payload');
    const points = value.benchmarks[0].checkpoints;
    if (!Array.isArray(points) || !points.length) throw new Error('Missing checkpoints');
    for (const point of points) if (point.invalid_values !== 0) throw new Error('Nonfinite physics state in ' + scenario);
    return value;
  }
  function rowSummary(pairs, scenario, profile, seed = 13, draws = 1500) {
    const rows = pairs.filter(x => x.scene === scenario && x.profile === profile), primary = rows.filter(x => x.comparison === 'primary');
    const aa = rows.filter(x => x.comparison === 'aa' || x.comparison === 'scalar-aa').map(x => x.ratio);
    const moduleRatios = rows.filter(x => x.comparison === 'module').map(x => x.ratio);
    const ratios = primary.map(x => x.ratio), ratio = median(ratios), bounds = interval(ratios, seed, draws);
    const aaNoise = median(aa.map(x => Math.abs(x - 1))) || 0, noiseMargin = Math.max(0.01, aaNoise);
    let verdict = ratios.length ? 'inconclusive' : 'scalar-only';
    if (bounds && bounds[1] < 1 - noiseMargin) verdict = 'faster';
    if (bounds && bounds[0] > 1 + noiseMargin) verdict = 'slower';
    const referenceRows = primary.length ? primary : rows;
    return {
      scene: scenario, profile, count: primary.length, ratio, percent: ratio === null ? null : 100 * (ratio - 1), interval: bounds,
      verdict, noiseMargin, aaRatio: median(aa), aaNoise, moduleRatio: median(moduleRatios),
      originalMsPerStep: median(referenceRows.map(x => 1000 * x.base.runSeconds / x.base.batch / x.steps)),
      simdMsPerStep: primary.length ? median(primary.map(x => 1000 * x.candidate.runSeconds / x.candidate.batch / x.steps)) : null,
      includingInitRatio: median(primary.map(x => x.totalRatio)),
      minSampleMs: rows.length ? Math.min(...rows.flatMap(x => [x.base.runSeconds * 1000, x.candidate.runSeconds * 1000])) : null,
      minRatio: ratios.length ? Math.min(...ratios) : null, maxRatio: ratios.length ? Math.max(...ratios) : null,
      warnings: Array.from(new Set(rows.flatMap(x => x.warnings || [])))
    };
  }
  function summarize(pairs, seed = 13, draws = 1500) {
    const keys = Array.from(new Set(pairs.map(x => x.profile + '/' + x.scene)));
    const rows = keys.map((key, i) => { const [profile, scenario] = key.split('/'); return rowSummary(pairs, scenario, profile, seed + i, draws); });
    const suites = [];
    for (const profile of ['smoke', 'reference']) {
      const members = rows.filter(x => x.profile === profile && x.count), ratio = geometricMean(members.map(x => x.ratio));
      if (!members.length) continue;
      const rng = random(seed + (profile === 'smoke' ? 1001 : 2001)), samples = [];
      const raw = members.map(row => pairs.filter(x => x.profile === profile && x.scene === row.scene && x.comparison === 'primary').map(x => x.ratio));
      for (let i = 0; i < draws; ++i) samples.push(geometricMean(raw.map(values => median(values.map(() => values[Math.floor(rng() * values.length)])))));
      samples.sort((a, b) => a - b);
      const bounds = [percentile(samples, 0.025), percentile(samples, 0.975)];
      const aaRatio = geometricMean(members.map(x => x.aaRatio).filter(x => x !== null));
      const noiseMargin = Math.max(0.01, Math.abs((aaRatio || 1) - 1), median(members.map(x => x.aaNoise)) || 0);
      const verdict = bounds[1] < 1 - noiseMargin ? 'faster' : bounds[0] > 1 + noiseMargin ? 'slower' : 'inconclusive';
      suites.push({ profile, sceneCount: members.length, ratio, percent: 100 * (ratio - 1), interval: bounds, noiseMargin, aaRatio, verdict,
        moduleRatio: geometricMean(members.map(x => x.moduleRatio).filter(x => x !== null)),
        includingInitRatio: geometricMean(members.map(x => x.includingInitRatio)),
        method: 'geometric mean of per-scene median paired ratios; bootstrap within scenes; equal scene weights' });
    }
    return { rows, suites };
  }
  function validateConfig(value) {
    if (!value || !Array.isArray(value.profiles) || !value.profiles.length || value.profiles.some(x => !['smoke', 'reference'].includes(x))) throw new Error('Invalid profiles');
    if (!Array.isArray(value.scenarios) || !value.scenarios.length || value.scenarios.length > 26) throw new Error('Invalid scenes');
    for (const x of value.scenarios) if (!/^\w+$/.test(x.name) || !Number.isInteger(x.steps) || x.steps < 1) throw new Error('Invalid scenario');
    for (const key of ['primaryRounds', 'controlRounds']) if (!Number.isInteger(value[key]) || value[key] < 2 || value[key] > 12 || value[key] % 2) throw new Error('Round counts must be balanced/even');
    if (!Number.isFinite(value.targetMs) || value.targetMs < 5 || value.targetMs > 500 || !Number.isInteger(value.maxBatch) || value.maxBatch < 1 || value.maxBatch > 4096) throw new Error('Invalid calibration limits');
    return value;
  }
  root.MunkBenchCore = { SCHEMA, QUICK, median, geometricMean, interval, timerResolution, supportsSimd, protocol, plan, parseTiming, validateSnapshot, summarize, rowSummary, validateConfig };
})(globalThis);
