/*
 * STVG screenshot harness (rebuilt 2026-06-11 — keep committed; see HANDOFF_20260603 which
 * deleted the original at closeout).
 * Usage: set NODE_PATH to frontend/node_modules, server already running on :8080, then
 *   node shoot.js [outDir]
 * Drives Edge headless via puppeteer-core: shoots the title screen, then walks the
 * start-game flow by clicking primary buttons, shooting each stage.
 */
const puppeteer = require('puppeteer-core');
const path = require('path');
const fs = require('fs');

const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const BASE = 'http://localhost:8080/';
const OUT = process.argv[2] || path.join(__dirname, 'shots');

const CLICK_WORDS = ['start', 'new game', 'begin', 'play', 'continue', 'launch', 'confirm', 'select'];

async function shot(page, name) {
  fs.mkdirSync(OUT, { recursive: true });
  const f = path.join(OUT, name + '.png');
  await page.screenshot({ path: f, fullPage: false });
  console.log('SHOT', f);
}

async function clickNext(page) {
  // Click the most plausible "advance" button currently visible. Returns its label or null.
  return page.evaluate((words) => {
    const btns = [...document.querySelectorAll('button, [role="button"], a.btn')]
      .filter(b => b.offsetParent !== null && !b.disabled);
    for (const w of words) {
      const hit = btns.find(b => (b.textContent || '').trim().toLowerCase().includes(w));
      if (hit) { const label = hit.textContent.trim().slice(0, 60); hit.click(); return label; }
    }
    // Fallback: a lone prominent button
    if (btns.length === 1) { const l = btns[0].textContent.trim().slice(0, 60); btns[0].click(); return l; }
    return null;
  }, CLICK_WORDS);
}

(async () => {
  const browser = await puppeteer.launch({
    executablePath: EDGE,
    headless: 'new',
    args: ['--window-size=1680,950', '--disable-gpu'],
    defaultViewport: { width: 1680, height: 950 },
  });
  const page = await browser.newPage();
  page.on('pageerror', e => console.log('PAGE_ERROR', e.message));
  await page.goto(BASE, { waitUntil: 'networkidle2', timeout: 30000 });
  await new Promise(r => setTimeout(r, 2500));
  await shot(page, '01_title');

  for (let i = 0; i < 5; i++) {
    const clicked = await clickNext(page);
    if (!clicked) { console.log('No advance button found at stage', i); break; }
    console.log('CLICKED', JSON.stringify(clicked));
    await new Promise(r => setTimeout(r, 3000));
    await shot(page, `0${i + 2}_after_${clicked.toLowerCase().replace(/[^a-z0-9]+/g, '_').slice(0, 24)}`);
    const inGame = await page.evaluate(() =>
      !!document.querySelector('[class*="desk"], [class*="layout"], [class*="sim-controls"]'));
    if (inGame) {
      // Let the sim run so charts accumulate, then take timed shots of the live game.
      await new Promise(r => setTimeout(r, 12000));
      await shot(page, '10_game_12s');
      await new Promise(r => setTimeout(r, 20000));
      await shot(page, '11_game_32s');
      break;
    }
  }
  await browser.close();
  console.log('DONE');
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
