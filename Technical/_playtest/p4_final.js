const puppeteer = require('puppeteer-core');
const path = require('path');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const OUT = path.join(__dirname, 'shots', 'p4');
const sleep = ms => new Promise(r => setTimeout(r, ms));
(async () => {
  const b = await puppeteer.launch({ executablePath: EDGE, headless: 'new', args: ['--window-size=1680,950', '--disable-gpu', '--force-device-scale-factor=1'], defaultViewport: { width: 1680, height: 950, deviceScaleFactor: 1 } });
  const p = await b.newPage();
  await p.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 }); await sleep(1500);
  await p.evaluate(() => { const x = [...document.querySelectorAll('button')].find(z => /begin/i.test(z.textContent)); if (x) x.click(); }); await sleep(2500);
  await p.evaluate(() => window.__stvgDebug.showCard('npc_ray_vance', 'Ray Vance', 'Prop Desk Lead', 'gunslinger', "I have to tell you, Permian Wildcat Oil is a layup. A LICENSE TO PRINT MONEY. People like us understand each other. If we blink, it's gone. (my desk happens to carry the other side, but never mind that)."));
  await sleep(4000);
  await p.screenshot({ path: path.join(OUT, 'final_crony_closeup.png'), clip: { x: 1080, y: 560, width: 600, height: 385 } });
  console.log('SHOT final_crony_closeup');
  await b.close(); console.log('DONE');
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
