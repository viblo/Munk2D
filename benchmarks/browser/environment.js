/* Browser-reported hints, not an attempt to infer hidden CPU or OS identity. */
export function deadline(promise, ms = 1000) {
  let timer;
  return Promise.race([promise, new Promise((_, reject) => { timer = setTimeout(() => reject(new Error('API timed out')), ms); })])
    .finally(() => clearTimeout(timer));
}
function unavailable(reason) { return { available: false, reason }; }
export async function batteryInfo() {
  if (!navigator.getBattery) return unavailable('Battery Status API not exposed');
  try {
    const b = await deadline(navigator.getBattery());
    return { available: true, source: 'Battery Status API; browser-reported, not proof of wall power', charging: b.charging, level: b.level,
      chargingTimeSeconds: Number.isFinite(b.chargingTime) ? b.chargingTime : null,
      dischargingTimeSeconds: Number.isFinite(b.dischargingTime) ? b.dischargingTime : null };
  } catch (e) { return unavailable(e.message); }
}
function gpuInfo() {
  try {
    const canvas = document.createElement('canvas'), gl = canvas.getContext('webgl', { powerPreference: 'low-power', failIfMajorPerformanceCaveat: false });
    if (!gl) return unavailable('WebGL unavailable/blocked');
    const debug = gl.getExtension('WEBGL_debug_renderer_info');
    const value = { available: true, source: 'WebGL renderer; GPU information, NOT CPU model',
      vendor: gl.getParameter(gl.VENDOR), renderer: gl.getParameter(gl.RENDERER), version: gl.getParameter(gl.VERSION),
      shadingLanguageVersion: gl.getParameter(gl.SHADING_LANGUAGE_VERSION),
      unmaskedVendor: debug ? gl.getParameter(debug.UNMASKED_VENDOR_WEBGL) : null,
      unmaskedRenderer: debug ? gl.getParameter(debug.UNMASKED_RENDERER_WEBGL) : null,
      note: debug ? 'Driver/virtualization/software rendering can obscure hardware identity' : 'Unmasked renderer not exposed' };
    gl.getExtension('WEBGL_lose_context')?.loseContext();
    return value;
  } catch (e) { return unavailable(e.message); }
}
function browserLabel(ua, data) {
  const brands = data?.highEntropy?.fullVersionList || data?.lowEntropy?.brands || [];
  const brand = brands.find(x => !/not.?a.?brand|chromium/i.test(x.brand));
  if (brand) return { name: brand.brand, version: brand.version, source: 'User-Agent Client Hints (can be reduced or spoofed)' };
  for (const [name, pattern] of [['Edge', /Edg(?:A|iOS)?\/([\d.]+)/], ['Firefox', /(?:Firefox|FxiOS)\/([\d.]+)/], ['Chrome', /(?:Chrome|CriOS)\/([\d.]+)/], ['Safari', /Version\/([\d.]+).*Safari\//]]) {
    const match = ua.match(pattern); if (match) return { name, version: match[1], source: 'User-Agent string (can be reduced or spoofed)' };
  }
  return { name: 'Unknown browser', version: null, source: 'Not exposed' };
}
function osLabel(ua, data) {
  const h = data?.highEntropy, low = data?.lowEntropy;
  if (low?.platform || h?.platform) return { name: h?.platform || low.platform, version: h?.platformVersion || null, source: 'User-Agent Client Hints; raw version, not translated into an OS edition' };
  if (/iPhone|iPad|iPod/.test(ua)) return { name: 'iOS/iPadOS', version: (ua.match(/OS ([\d_]+)/) || [])[1]?.replaceAll('_', '.') || null, source: 'User-Agent string' };
  if (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1) return { name: 'Possibly iPadOS in desktop mode', version: null, source: 'Heuristic; not verified' };
  for (const [name, pattern] of [['Windows', /Windows NT ([\d.]+)/], ['Android', /Android ([\d.]+)/], ['macOS', /Mac OS X ([\d_]+)/], ['Linux', /Linux/]]) {
    const m = ua.match(pattern); if (m) return { name, version: m[1]?.replaceAll('_', '.') || null, source: name === 'Windows' ? 'User-Agent NT version; cannot reliably distinguish Windows 10/11' : 'User-Agent string' };
  }
  return { name: navigator.platform || 'Unknown OS', version: null, source: 'Legacy platform hint' };
}
export async function collectEnvironment() {
  const ua = navigator.userAgent;
  let uaData = null;
  if (navigator.userAgentData) {
    uaData = { lowEntropy: navigator.userAgentData.toJSON ? navigator.userAgentData.toJSON() : { brands: navigator.userAgentData.brands, mobile: navigator.userAgentData.mobile, platform: navigator.userAgentData.platform } };
    try { uaData.highEntropy = await deadline(navigator.userAgentData.getHighEntropyValues(['architecture', 'bitness', 'model', 'platformVersion', 'uaFullVersion', 'fullVersionList', 'wow64'])); }
    catch (e) { uaData.highEntropyError = e.message; }
  }
  const c = navigator.connection || navigator.mozConnection || navigator.webkitConnection;
  const memory = performance.memory;
  return {
    capturedAt: new Date().toISOString(), browser: browserLabel(ua, uaData), os: osLabel(ua, uaData),
    userAgent: ua, userAgentData: uaData, appVersion: navigator.appVersion, platform: navigator.platform, vendor: navigator.vendor,
    language: navigator.language, languages: Array.from(navigator.languages || []),
    hardware: { cpuModel: null, cpuModelReason: 'Browsers do not reliably expose the CPU model; optional user entry is recorded separately',
      logicalProcessorsHint: navigator.hardwareConcurrency ?? null, deviceMemoryGiBHint: navigator.deviceMemory ?? null,
      note: 'Logical processor/RAM values may be capped or bucketed; they are not physical core counts or exact installed RAM',
      maxTouchPoints: navigator.maxTouchPoints ?? null, gpu: gpuInfo() },
    screen: { width: screen.width, height: screen.height, availableWidth: screen.availWidth, availableHeight: screen.availHeight,
      colorDepth: screen.colorDepth, pixelDepth: screen.pixelDepth, devicePixelRatio: window.devicePixelRatio,
      viewportWidth: innerWidth, viewportHeight: innerHeight, orientation: screen.orientation?.type || null },
    runtime: { secureContext: window.isSecureContext, crossOriginIsolated: window.crossOriginIsolated,
      webAssembly: typeof WebAssembly !== 'undefined', simd: globalThis.MunkBenchCore.supportsSimd(),
      worker: typeof Worker !== 'undefined', sharedArrayBufferExposed: typeof SharedArrayBuffer !== 'undefined', automationHint: navigator.webdriver ?? null,
      physicsThreadsUsed: 1, wasmSharedMemoryUsed: false, benchmarkRunsInDedicatedWorker: true,
      timerResolutionMs: globalThis.MunkBenchCore.timerResolution(), visibility: document.visibilityState,
      standalone: window.matchMedia('(display-mode: standalone)').matches, timezoneOffsetMinutes: new Date().getTimezoneOffset() },
    battery: await batteryInfo(),
    networkHint: c ? { effectiveType: c.effectiveType || null, downlinkMbps: c.downlink ?? null, rttMs: c.rtt ?? null, saveData: c.saveData ?? null } : unavailable('Network Information API not exposed'),
    jsHeapHint: memory ? { usedBytes: memory.usedJSHeapSize, totalBytes: memory.totalJSHeapSize, limitBytes: memory.jsHeapSizeLimit,
      note: 'Nonstandard Chromium JS heap estimates, not physical or Wasm memory' } : unavailable('JS heap API not exposed'),
    limitations: ['Browser/OS strings can be reduced or spoofed', 'CPU temperature, throttling, true CPU model, OS power mode and other running tasks are not reliably observable',
      'Most iOS browser apps use WebKit; different app names do not necessarily mean different engines', 'No geolocation, IP lookup, account data or device enumeration is requested'],
    privacy: 'No automatic result upload. Only same-origin static assets are fetched. Results stay in this browser until you download/share them.'
  };
}
