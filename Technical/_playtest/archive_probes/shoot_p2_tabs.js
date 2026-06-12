/* P2 evidence: begin a game, let the sim run, then screenshot ALL FOUR TABS. */
const puppeteer = require('puppeteer-core');
const path = require('path');
const fs = require('fs');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const OUT = path.join(__dirname, 'shots', 'p2');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function clickTab(page, n) {
  // Tabs are keyboard 1-4; also click the tab button by label as a fallback.
  await page.keyboard.press(String(n));
  await sleep(600);
}

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const browser = await puppeteer.launch({
    executablePath: EDGE, headless: 'new',
    args: ['--window-size=1680,950', '--disable-gpu'],
    defaultViewport: { width: 1680, height: 950 },
  });
  const page = await browser.newPage();
  await page.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 });
  await sleep(2000);

  // Begin the game.
  await page.evaluate(() => {
    const b = [...document.querySelectorAll('button')].find((x) => /begin/i.test(x.textContent));
    if (b) b.click();
  });
  await sleep(3000);

  // Let the sim run a bit so charts/markets have data; push to 4x via the speed key ']'.
  await page.keyboard.press(']');
  await page.keyboard.press(']');
  await sleep(18000);

  // Keep the world flowing if a resume bar appears.
  const keepFlowing = async () => {
    await page.evaluate(() => {
      const b = [...document.querySelectorAll('button')].find((x) => /resume/i.test(x.textContent));
      if (b) b.click();
    });
  };

  // Clear any pending decision memo so the right sidebar (funding-mix card,
  // annual report) isn't occluded by the global overlay in the shot. Submit the
  // currently-selected option a few times, then pause so nothing new pops in.
  const clearDecisions = async () => {
    for (let i = 0; i < 6; i++) {
      await page.keyboard.press('Enter');
      await sleep(400);
    }
    await page.keyboard.press('Space'); // pause so no fresh decision spawns
    await sleep(600);
  };

  const tabs = [
    [1, '1_economy'],
    [2, '2_hire'],
    [3, '3_bank'],
    [4, '4_financials'],
  ];
  // Resolve the opening decisions so right-rail content isn't hidden by the memo.
  await clearDecisions();

  for (const [n, name] of tabs) {
    await clickTab(page, n);
    await keepFlowing();
    await sleep(1200);
    await page.screenshot({ path: path.join(OUT, name + '.png') });
    console.log('SHOT', name);
  }

  // A second pass later in the game (more divisions/competitors) on Economy.
  await sleep(12000);
  await keepFlowing();
  await clickTab(page, 1);
  await page.screenshot({ path: path.join(OUT, '5_economy_later.png') });
  console.log('SHOT 5_economy_later');

  await browser.close();
  console.log('DONE');
})().catch((e) => { console.error('FATAL', e.message); process.exit(1); });
