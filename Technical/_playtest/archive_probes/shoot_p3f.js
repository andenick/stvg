/* P3F: Economy tab living-economy — macro charts moving, one macro promoted to
 * hero, and the expanded explainer view. Shots into shots/p3f/. */
const puppeteer = require('puppeteer-core');
const path = require('path');
const fs = require('fs');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const OUT = path.join(__dirname, 'shots', 'p3f');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const browser = await puppeteer.launch({
    executablePath: EDGE, headless: 'new',
    args: ['--window-size=1680,950', '--disable-gpu'],
    defaultViewport: { width: 1680, height: 950 },
  });
  const page = await browser.newPage();
  page.on('console', (m) => { if (m.type() === 'error') console.log('PAGE-ERR', m.text()); });
  await page.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 });
  await sleep(2000);

  // Begin
  await page.evaluate(() => {
    const b = [...document.querySelectorAll('button')].find((x) => /begin/i.test(x.textContent));
    if (b) b.click();
  });
  await sleep(3500);

  // Push speed up via ']' (speed step) — NOT Space (that toggles pause now).
  await page.keyboard.press(']');
  await page.keyboard.press(']');
  await sleep(1000);

  // Let the economy run so macro series accumulate several quarters.
  for (let i = 0; i < 4; i++) {
    await sleep(15000);
    // Keep flowing if a resume bar appears at a quarter boundary.
    await page.evaluate(() => {
      const b = [...document.querySelectorAll('button')].find((x) => /resume/i.test(x.textContent));
      if (b) b.click();
    });
  }

  // Shot 1: Economy tab with macro strip moving.
  await page.screenshot({ path: path.join(OUT, '01_economy_macro.png') });
  console.log('SHOT 01_economy_macro');

  // Promote a macro series (GDP) to the hero spot — click its ★ promote button.
  const promoted = await page.evaluate(() => {
    const btns = [...document.querySelectorAll('.macro-cell .promote')];
    if (btns.length) { btns[0].click(); return true; }
    return false;
  });
  console.log('promoted macro to hero:', promoted);
  await sleep(2500);
  await page.screenshot({ path: path.join(OUT, '02_macro_hero.png') });
  console.log('SHOT 02_macro_hero');

  // Open the expanded explainer view — click the macro chart body.
  const opened = await page.evaluate(() => {
    const c = document.querySelector('.macro-cell .macro-chart.clickable');
    if (c) { c.click(); return true; }
    return false;
  });
  console.log('opened expanded macro:', opened);
  await sleep(2000);
  await page.screenshot({ path: path.join(OUT, '03_macro_expanded.png') });
  console.log('SHOT 03_macro_expanded');

  // Close, then open the credit-spread chip's expanded view.
  await page.keyboard.press('Escape');
  await sleep(800);
  await page.evaluate(() => {
    const c = document.querySelector('.market-chip');
    if (c) c.click();
  });
  await sleep(1800);
  await page.screenshot({ path: path.join(OUT, '04_spread_expanded.png') });
  console.log('SHOT 04_spread_expanded');

  await browser.close();
  console.log('DONE');
})().catch((e) => { console.error('FATAL', e.message); process.exit(1); });
