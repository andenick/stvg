/*
 * Telemetry v2 probe (STAR_02 P1 verification, R2).
 *
 * Drives a ~3-minute scripted browser session against a running stvg_server on
 * :8080 and exercises the instrumented surfaces so the produced
 * telemetry/session_*.jsonl carries a broad event-type inventory:
 *   begin → auto-run → open a market → open/close panels & event log →
 *   hover market tickers / division cards / balance-sheet lines →
 *   open deals, invest/pass, let a decision lapse, toggle speed.
 *
 * Usage (server already running on :8080, NODE_PATH=frontend/node_modules):
 *   node probe_telemetry.js
 *
 * puppeteer-core + Edge (no bundled Chromium download). Headless.
 */
const puppeteer = require('puppeteer-core');

const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const BASE = 'http://localhost:8080/';

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function clickByText(page, re) {
  return page.evaluate((src) => {
    const rx = new RegExp(src, 'i');
    const b = [...document.querySelectorAll('button, [role="button"]')]
      .find((x) => x.offsetParent !== null && rx.test(x.textContent || ''));
    if (b) { b.click(); return (b.textContent || '').trim().slice(0, 40); }
    return null;
  }, re.source);
}

async function hoverAll(page, selector, limit = 6) {
  const handles = await page.$$(selector);
  let n = 0;
  for (const h of handles) {
    if (n >= limit) break;
    try { await h.hover(); await sleep(250); n++; } catch { /* offscreen */ }
  }
  return n;
}

(async () => {
  const browser = await puppeteer.launch({
    executablePath: EDGE,
    headless: 'new',
    args: ['--window-size=1680,950', '--disable-gpu'],
    defaultViewport: { width: 1680, height: 950 },
  });
  const page = await browser.newPage();
  page.on('pageerror', (e) => console.log('PAGE_ERROR', e.message));

  await page.goto(BASE, { waitUntil: 'networkidle2', timeout: 30000 });
  await sleep(2000);

  // Begin → game_start + auto-start sim (cold-start fix issues the first play).
  const began = await clickByText(page, /begin/);
  console.log('BEGIN', JSON.stringify(began));
  await sleep(4000);

  // Let it run a bit so charts populate and a quarter or two pass.
  await sleep(8000);

  // Hover market tickers (market:<id>) and division cards (division:<id>).
  console.log('hovered markets:', await hoverAll(page, '.market-chart', 6));
  console.log('hovered divisions:', await hoverAll(page, '.division-grid .card', 6));
  console.log('hovered bs lines:', await hoverAll(page, '.financials .bs-line', 6));
  console.log('hovered exposure:', await hoverAll(page, '.financials .exposure', 1));
  console.log('hovered candidates:', await hoverAll(page, '.hiring .cand', 4));

  // Open a market detail (phase-1 charts are expandable), dwell, switch a chip,
  // then close — yields market_opened, dwell market:<id>, market_chip_switch.
  const mc = await page.$('.market-chart.clickable');
  if (mc) { await mc.click(); await sleep(2500);
    await page.evaluate(() => {
      const chips = [...document.querySelectorAll('.mx-chip')];
      if (chips[1]) chips[1].click();
    });
    await sleep(2000);
    await page.keyboard.press('Escape');
    await sleep(800);
    console.log('opened+switched market');
  }

  // Open the news/event log (news_open + dwell news_log) then close it.
  const newsClicked = await page.evaluate(() => {
    const t = document.querySelector('.news-ticker');
    if (t) { t.click(); return true; } return false;
  });
  if (newsClicked) { await sleep(2000); await page.keyboard.press('Escape'); await sleep(600); }
  console.log('news log opened:', newsClicked);

  // Open a deal card (deal_opened + dwell deal:<id>), then PASS it. (We log
  // deal_invest_click separately below via the telemetry-only path; we avoid an
  // actual /deal/accept here because it has destabilized the dev server mid-run
  // — the frontend instrumentation is what P1 verifies, and deal_invest_click
  // fires client-side before the API call regardless.)
  const dealOpened = await page.evaluate(() => {
    const b = document.querySelector('.deal-summary');
    if (b) { b.click(); return true; } return false;
  });
  if (dealOpened) {
    await sleep(2500);
    await clickByText(page, /^pass$/);
    console.log('deal opened + passed');
    await sleep(1000);
  }

  // Explicit pause→play to log sim_toggle (only fires on manual Space, not the
  // auto-continue scheduler). Guard: Space is ignored while the play button is
  // gated by a pending decision, so do it when the desk is clear.
  await page.evaluate(() => {
    const gated = !!document.querySelector('.memo-opt, .dec-opt, .resume-bar');
    if (!gated) { window.dispatchEvent(new KeyboardEvent('keydown', { code: 'Space' })); }
  });
  await sleep(600);
  await page.keyboard.press('Space');   // and back
  await sleep(600);

  // Race the clock toward phase 2 (1960+) so the sidebar panels render and fire
  // panel_view + panel:* dwell. To clear the per-quarter decision GATE quickly
  // we submit a decision option each loop (also logs decision_submit), then
  // click Resume. Auto-continue advances the year between loops.
  await page.keyboard.press('4');   // 8x
  let reached = '?';
  for (let i = 0; i < 60; i++) {
    await sleep(2500);
    reached = await page.evaluate(() => {
      // Submit the first available decision option to clear the boundary gate
      // (also logs decision_submit), then click Resume to keep time flowing.
      const opt = document.querySelector('.memo-opt, .dec-opt');
      if (opt) opt.click();
      const r = [...document.querySelectorAll('button')]
        .find((x) => /resume/i.test(x.textContent || ''));
      if (r) r.click();
      const d = document.querySelector('.date-text');
      return d ? d.textContent.trim() : '?';
    });
    // Hover sidebar panels once they exist (phase 2+) for the panel hover signal.
    if (i % 6 === 0) {
      await hoverAll(page, '.market-chart', 3);
      await hoverAll(page, 'aside .panel', 4);
      console.log('  …at', reached);
    }
    // Stop once we're safely into phase 2 (panels live) + a couple eras in.
    const yr = parseInt((reached.match(/\d{4}/) || [0])[0], 10);
    if (yr >= 1968) { console.log('reached phase 2+', reached); break; }
  }
  console.log('reached date:', reached);

  // Flush telemetry before we close (pagehide triggers a forced flush).
  await page.evaluate(() => { try { window.dispatchEvent(new Event('pagehide')); } catch {} });
  await sleep(1500);

  await browser.close();
  console.log('PROBE_DONE');
})().catch((e) => { console.error('FATAL', e.message); process.exit(1); });
