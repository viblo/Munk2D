// Targeted browser check of the existing solver tests and MunkBench, not a new
// benchmark suite. Install playwright in the checkout and its desired browser.
// node tests/run-wasm-browser.mjs BUILD_DIRECTORY/browser [chromium|firefox]
import { chromium, firefox } from 'playwright';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { resolve, sep } from 'node:path';
const root = resolve(process.argv[2]);
const kind = process.argv[3] || 'chromium';
const server = createServer(async (req, res) => {
  if (req.url === '/') { res.end('<!doctype html><title>Munk2D Wasm checks</title>'); return; }
  const path = resolve(root, '.' + new URL(req.url, 'http://localhost').pathname);
  if (!path.startsWith(root + sep)) { res.writeHead(403).end(); return; }
  try {
    res.setHeader('Content-Type', path.endsWith('.wasm') ? 'application/wasm' : 'text/javascript');
    res.end(await readFile(path));
  } catch { res.writeHead(404).end(); }
});
await new Promise(r => server.listen(0, '127.0.0.1', r));
let browser;
try {
  browser = await ({ chromium, firefox }[kind]).launch({ headless: true });
  const page = await browser.newPage();
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  const results = await page.evaluate(async (simdEnabled) => {
    const results = {};
    for (const name of ['contact_solver', 'contact_solver_alloc', 'contact_solver_quality', 'munkbench']) {
      await new Promise((yes, no) => {
        const script = document.createElement('script');
        script.src = name + '.js'; script.onload = yes; script.onerror = no;
        document.head.append(script);
      });
      const out = [], err = [];
      const m = await globalThis.createMunkModule({ print: x => out.push(x), printErr: x => err.push(x) });
      const args = name === 'munkbench'
        ? ['--contact-solver', 'original', '--profile', 'smoke', '-b', 'FrictionalPyramid', '--summary-json', '--checkpoints', '1,10'] : [];
      const savedStack = m.stackSave();
      let status;
      try { status = m.callMain([...args]); }
      finally { m.stackRestore(savedStack); }
      if (status || err.length || (name !== 'munkbench' && !out.some(x => x.startsWith('PASS:')))) {
        throw new Error(JSON.stringify({ name, status, out, err }));
      }
      if (name === 'munkbench') {
        JSON.parse(out.join('\n'));
        out.length = err.length = 0;
        const stack = m.stackSave();
        let simdStatus;
        try { simdStatus = m.callMain(['--contact-solver', 'wasm-simd', ...args.slice(2)]); }
        finally { m.stackRestore(stack); }
        if (simdEnabled) {
          if (simdStatus || err.length) throw new Error('SIMD CLI failed: ' + err.join('\n'));
          JSON.parse(out.join('\n'));
        } else if (simdStatus !== 2 || !err.some(x => x.includes('unavailable'))) {
          throw new Error('Disabled SIMD CLI was not rejected');
        }
        results[name] = { status, simdStatus, out, err };
      } else results[name] = { status, out };
    }
    return results;
  }, process.env.SIMD !== 'OFF');
  console.log(JSON.stringify({ browser: kind, version: browser.version(), results }, null, 2));
} finally {
  await browser?.close();
  await new Promise(r => server.close(r));
}
