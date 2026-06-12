/* Verify honest < crony pitch length across ALL families via the in-page hook. */
const puppeteer = require('puppeteer-core');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

(async () => {
  const browser = await puppeteer.launch({ executablePath: EDGE, headless: 'new', args: ['--disable-gpu'] });
  const page = await browser.newPage();
  await page.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 });
  await sleep(1500);
  const res = await page.evaluate(() => {
    const dbg = window.__stvgDebug;
    const fams = ['patrician','gunslinger','quant','dealmaker','operator','rainmaker','prodigy','lifer'];
    const out = [];
    let allOk = true;
    for (const f of fams) {
      const honest = dbg.pitch(f, 0.85, 'Acme Corp', 'Trading', 60);
      const mixed  = dbg.pitch(f, 0.45, 'Acme Corp', 'Trading', 60);
      const crony  = dbg.pitch(f, 0.10, 'Acme Corp', 'Trading', 60);
      const ok = honest.length < crony.length && honest.length <= mixed.length;
      if (!ok) allOk = false;
      out.push({ f, h: honest.length, m: mixed.length, c: crony.length, ok });
    }
    return { out, allOk };
  });
  console.log('ALL_OK=' + res.allOk);
  for (const r of res.out) console.log(`${r.ok?'OK ':'BAD'} ${r.f.padEnd(11)} honest=${r.h}  mixed=${r.m}  crony=${r.c}`);
  await browser.close();
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
