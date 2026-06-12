/* Shoot the sim actually RUNNING: begin game, press Space to start, shots at 20s/60s/120s. */
const puppeteer = require('puppeteer-core');
const path = require('path');
const fs = require('fs');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const OUT = path.join(__dirname, 'shots');

(async () => {
  const browser = await puppeteer.launch({
    executablePath: EDGE, headless: 'new',
    args: ['--window-size=1680,950', '--disable-gpu'],
    defaultViewport: { width: 1680, height: 950 },
  });
  const page = await browser.newPage();
  await page.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 });
  await new Promise(r => setTimeout(r, 2000));
  await page.evaluate(() => {
    const b = [...document.querySelectorAll('button')].find(x => /begin/i.test(x.textContent));
    if (b) b.click();
  });
  await new Promise(r => setTimeout(r, 3000));
  // Dismiss hint, start sim, push speed to 4x
  await page.keyboard.press('Space');
  await new Promise(r => setTimeout(r, 1000));
  await page.keyboard.press('3');
  fs.mkdirSync(OUT, { recursive: true });
  for (const [delay, name] of [[20000, '20_live_20s'], [40000, '21_live_60s'], [60000, '22_live_120s']]) {
    await new Promise(r => setTimeout(r, delay));
    // If a resume bar appeared (quarter boundary), click it to keep flowing
    await page.evaluate(() => {
      const b = [...document.querySelectorAll('button')].find(x => /resume/i.test(x.textContent));
      if (b) b.click();
    });
    await new Promise(r => setTimeout(r, 1500));
    await page.screenshot({ path: path.join(OUT, name + '.png') });
    console.log('SHOT', name);
  }
  await browser.close();
  console.log('DONE');
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
