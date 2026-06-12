/* Final megaplan evidence set: begin game, let it run, shoot all four tabs (keys 1-4). */
const puppeteer = require('puppeteer-core');
const path = require('path');
const fs = require('fs');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const OUT = path.join(__dirname, 'shots', 'final');

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const browser = await puppeteer.launch({
    executablePath: EDGE, headless: 'new',
    args: ['--window-size=1680,950', '--disable-gpu'],
    defaultViewport: { width: 1680, height: 950 },
  });
  const page = await browser.newPage();
  await page.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 });
  await new Promise(r => setTimeout(r, 2500));
  await page.screenshot({ path: path.join(OUT, '00_title.png') });
  await page.evaluate(() => {
    const b = [...document.querySelectorAll('button')].find(x => /begin/i.test(x.textContent));
    if (b) b.click();
  });
  await new Promise(r => setTimeout(r, 4000));
  // Let the sim run (auto-starts since P0) and accumulate chart history.
  await new Promise(r => setTimeout(r, 45000));
  const tabs = [['1', '01_economy'], ['2', '02_hire'], ['3', '03_bank'], ['4', '04_financials']];
  for (const [key, name] of tabs) {
    await page.keyboard.press(key);
    await new Promise(r => setTimeout(r, 2500));
    await page.screenshot({ path: path.join(OUT, name + '.png') });
    console.log('SHOT', name);
  }
  // Back to economy, run longer for a portrait card + later-era look.
  await page.keyboard.press('1');
  await new Promise(r => setTimeout(r, 40000));
  await page.screenshot({ path: path.join(OUT, '05_economy_later.png') });
  await browser.close();
  console.log('DONE');
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
