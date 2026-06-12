/*
 * P4 evidence probe — character portrait cards (Persona bustup × Hades cadence).
 *
 *   1. Begin a game, start the sim, run a couple of minutes at speed.
 *   2. Repeatedly OPEN deals (fires the deal-pitch bust) + click resume bars so
 *      the quarter keeps flowing (advisor / crisis / era / report cards may also
 *      proc). Screenshot whenever a `.char-card` is on screen.
 *   3. Force-show a card via the debug hook for a guaranteed bust screenshot.
 *   4. Render an HONEST (cred 0.85) vs CRONY (cred 0.10) pitch for the SAME deal
 *      class (gunslinger / Trading) and write both to a text file + a side card.
 */
const puppeteer = require('puppeteer-core');
const path = require('path');
const fs = require('fs');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const OUT = path.join(__dirname, 'shots', 'p4');

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

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

  // Begin.
  await page.evaluate(() => {
    const b = [...document.querySelectorAll('button')].find(x => /begin/i.test(x.textContent));
    if (b) b.click();
  });
  await sleep(3000);
  await page.keyboard.press('Space');   // dismiss hint / start
  await sleep(800);
  await page.keyboard.press('3');        // speed up

  let shotN = 0;
  async function tryShotIfCard(tag) {
    const has = await page.evaluate(() => !!document.querySelector('.char-card'));
    if (has) {
      shotN++;
      const name = `card_${String(shotN).padStart(2, '0')}_${tag}`;
      await page.screenshot({ path: path.join(OUT, name + '.png') });
      const info = await page.evaluate(() => {
        const c = document.querySelector('.char-card');
        return c ? { name: c.querySelector('.name')?.textContent, role: c.querySelector('.role')?.textContent, line: c.querySelector('.line')?.textContent } : null;
      });
      console.log('CARD SHOT', name, JSON.stringify(info));
      return true;
    }
    return false;
  }

  // Drive the game: open deals, click resume bars, watch for cards (~2 min).
  const tEnd = Date.now() + 120000;
  while (Date.now() < tEnd) {
    // Open the first available deal to fire a deal-pitch bark.
    await page.evaluate(() => {
      const d = document.querySelector('.deal-summary');
      if (d) d.click();
    });
    await sleep(700);
    await tryShotIfCard('live');
    // Keep the quarter flowing.
    await page.evaluate(() => {
      const r = [...document.querySelectorAll('button')].find(x => /resume/i.test(x.textContent));
      if (r) r.click();
    });
    await sleep(1800);
    await tryShotIfCard('live');
  }

  // Guaranteed bust: force-show a card via the debug hook (deal-pitch style).
  await page.evaluate(() => {
    const dbg = window.__stvgDebug;
    if (dbg) dbg.showCard('npc_dutch_calloway', 'Dutch Calloway', 'Head Trader', 'gunslinger',
      'I size this small; the math only barely works. The tape on trading is 58% in our favour this week. If it gaps against us, the loss is ugly — I will not pretend otherwise. Full disclosure: my bonus is tied to this closing.');
  });
  await sleep(900);
  shotN++;
  await page.screenshot({ path: path.join(OUT, `card_${String(shotN).padStart(2,'0')}_forced.png`) });
  console.log('FORCED CARD SHOT');

  // Honest vs crony pitch for the SAME deal class (gunslinger / Trading).
  const pair = await page.evaluate(() => {
    const dbg = window.__stvgDebug;
    if (!dbg) return null;
    return {
      honest: dbg.pitch('gunslinger', 0.85, 'Permian Wildcat Oil', 'Trading', 58),
      crony:  dbg.pitch('gunslinger', 0.10, 'Permian Wildcat Oil', 'Trading', 58),
    };
  });
  if (pair) {
    fs.writeFileSync(path.join(OUT, 'honest_vs_crony.txt'),
      `SAME DEAL CLASS (Gunslinger / Trading / Permian Wildcat Oil / 58% success)\n\n` +
      `HONEST (credibility 0.85), ${pair.honest.length} chars:\n${pair.honest}\n\n` +
      `CRONY  (credibility 0.10), ${pair.crony.length} chars:\n${pair.crony}\n`);
    console.log('PAIR honest_len=' + pair.honest.length + ' crony_len=' + pair.crony.length);
    console.log('HONEST:', pair.honest);
    console.log('CRONY :', pair.crony);
  } else {
    console.log('NO DEBUG HOOK');
  }

  await browser.close();
  console.log('DONE shots=' + shotN);
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });
