/* Clean P4 card shots: force a long-text card and a crony card with no competing
   live card, to confirm legibility / no clipping after the layout fix. */
const puppeteer = require('puppeteer-core');
const path = require('path');
const fs = require('fs');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const OUT = path.join(__dirname, 'shots', 'p4');
const sleep = (ms) => new Promise(r => setTimeout(r, ms));

(async () => {
  const browser = await puppeteer.launch({
    executablePath: EDGE, headless: 'new',
    args: ['--window-size=1680,950', '--disable-gpu'],
    defaultViewport: { width: 1680, height: 950 },
  });
  const page = await browser.newPage();
  await page.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 });
  await sleep(1500);
  await page.evaluate(() => {
    const b = [...document.querySelectorAll('button')].find(x => /begin/i.test(x.textContent));
    if (b) b.click();
  });
  await sleep(2500);

  // Clear any queued/active card, then force a CRONY (long, wrapping) deal pitch.
  await page.evaluate(() => {
    const dbg = window.__stvgDebug;
    // long crony line — the worst case for wrapping/clipping.
    dbg.showCard('npc_ray_vance', 'Ray Vance', 'Prop Desk Lead', 'gunslinger',
      "I have to tell you, Permian Wildcat Oil is a layup. A LICENSE TO PRINT MONEY. People like us understand each other. If we blink, it's gone. (my desk happens to carry the other side, but never mind that).");
  });
  await sleep(3500);   // let it type out fully
  await page.screenshot({ path: path.join(OUT, 'clean_crony_long.png') });
  console.log('SHOT clean_crony_long');

  // Fresh load resets the card store (clears the 20s rate-limit window), then an
  // HONEST (short) card for contrast.
  await page.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 });
  await sleep(1500);
  await page.evaluate(() => {
    const b = [...document.querySelectorAll('button')].find(x => /begin/i.test(x.textContent));
    if (b) b.click();
  });
  await sleep(2500);
  await page.evaluate(() => {
    const dbg = window.__stvgDebug;
    dbg.showCard('npc_patricia_chen', 'Patricia Chen', 'Senior Credit Officer', 'lifer',
      "Acme Corp is sound at about 60%. No surprises. The risk is complacency — it's boring until it isn't.");
  });
  await sleep(2800);
  await page.screenshot({ path: path.join(OUT, 'clean_honest_short.png') });
  console.log('SHOT clean_honest_short');

  await browser.close();
  console.log('DONE');
  void fs;
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
