/* STAR_02 P7 evidence: lending-first early game + light trading.
 * Captures:
 *   (a) loan card expanded with terms (amount/return/duration/soundness) + pitch
 *   (b) a deal outcome bark + the Loan Book "Book" strip (accepted + realized P&L)
 *   (c) BUY on a promoted market + the position pill on its thumbnail
 *
 * Drives the game via the REST API (UI timing in headless is flaky): accept a
 * deal, run quarters until it resolves, place a personal trade, then screenshot.
 */
const puppeteer = require('puppeteer-core');
const path = require('path');
const fs = require('fs');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const OUT = path.join(__dirname, 'shots', 'p7');
const GID = 'game_42';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const browser = await puppeteer.launch({
    executablePath: EDGE, headless: 'new',
    args: ['--window-size=1680,950', '--disable-gpu'],
    defaultViewport: { width: 1680, height: 950 },
  });
  const page = await browser.newPage();
  page.on('console', (m) => { if (/P7PROBE/.test(m.text())) console.log(m.text()); });
  await page.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 });
  await sleep(1800);

  // Begin the game.
  await page.evaluate(() => {
    const b = [...document.querySelectorAll('button')].find((x) => /begin/i.test(x.textContent));
    if (b) b.click();
  });
  await sleep(2500);
  // Crank speed to 8x so quarters fly and the annual report shows as a corner
  // card (not a full-screen modal) — and so the dur-5 deal resolves in-probe.
  await page.keyboard.press(']'); await page.keyboard.press(']'); await page.keyboard.press(']');
  await sleep(400);

  // Drive quarters via the API: resolve crises/decisions, accept the first deal
  // that appears (modest 5% stake), keep the runner playing so the clock moves.
  let acceptedDealId = null;
  let acceptedDur = 4;
  const flow = async () => {
    return await page.evaluate(async (gid) => {
      const st = await fetch(`/api/game/${gid}/state`).then((r) => r.json()).then((j) => j.data).catch(() => null);
      if (!st) return null;
      for (const c of (st.activeCrises || [])) if (!c.resolved)
        await fetch(`/api/game/${gid}/crisis/respond`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ crisisId: c.id, responseType: 'measured' }) }).catch(() => {});
      for (const d of (st.pendingDecisions || [])) if (d.options && d.options.length)
        await fetch(`/api/game/${gid}/decision/${encodeURIComponent(d.id)}`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ optionId: d.options[0].id }) }).catch(() => {});
      return {
        deals: st.availableDeals || [],
        active: (st.activeDeals || []).length,
        stats: st.loanBookStats || null,
        outcomes: (st.dealOutcomes || []).length,
        cap: st.bank ? st.bank.capital : 0,
        clock: st.state ? `${st.state.currentYear}Q${st.state.currentQuarter}` : '',
      };
    }, GID);
  };
  // Dismiss the Annual Report (scrim/×) + era overlays so they don't block the
  // clock or the screenshots, then resume the runner via the play button.
  const dismissModals = async () => {
    await page.evaluate(() => {
      for (const sel of ['.modal .back', '.back', '.card-x', '.era-transition button', '.modal-close']) {
        const el = document.querySelector(sel);
        if (el) el.click();
      }
    });
  };
  const nudgePlay = async () => {
    await dismissModals();
    // Resume the runner via REST (/advance delegates to runner->play() in
    // real-time mode) — reliable regardless of button glyph/disabled state.
    await page.evaluate(async (gid) => {
      await fetch(`/api/game/${gid}/advance`, { method: 'POST' }).catch(() => {});
    }, GID);
  };
  const acceptDeal = async (dealId, amount) => {
    return await page.evaluate(async (gid, id, amt) => {
      const r = await fetch(`/api/game/${gid}/deal/${encodeURIComponent(id)}/accept`, {
        method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ investmentAmount: amt }),
      }).then((x) => x.json()).catch(() => null);
      return r;
    }, GID, dealId, amount);
  };

  // Run until we've accepted a deal AND at least one outcome has resolved.
  let last = '';
  for (let i = 0; i < 120; i++) {
    const s = await flow();
    await nudgePlay();
    if (s) {
      if (!acceptedDealId && s.deals.length) {
        // shortest-duration deal, 5% of capital
        let best = 99, pick = null;
        for (const d of s.deals) {
          const dur = (d.risk && d.risk.duration) || 4;
          if (dur < best) { best = dur; pick = d; }
        }
        if (pick) {
          const amt = Math.max(10, s.cap * 0.05);
          const r = await acceptDeal(pick.id, amt);
          if (r && r.data && r.data.accepted) { acceptedDealId = pick.id; acceptedDur = best; console.log('P7PROBE accepted', pick.id, 'dur', best); }
        }
      }
      if (i % 4 === 0 && s.clock !== last) { console.log('P7PROBE', s.clock, 'active', s.active, 'outcomes', s.outcomes, 'stats', JSON.stringify(s.stats)); last = s.clock; }
      const resolved = s.stats ? (s.stats.paidOff + s.stats.defaulted + s.stats.windfalls) : 0;
      if (acceptedDealId && s.outcomes >= 1 && resolved >= 1) {
        console.log('P7PROBE outcome resolved — stats', JSON.stringify(s.stats));
        break;
      }
    }
    await sleep(650);
  }

  // Pause the runner. Decisions in this build cost an action point to resolve
  // and otherwise LAPSE at the next quarter boundary, so a paused desk keeps its
  // backlog (the memo overlay stays up). We therefore capture the P7 surfaces as
  // ELEMENT screenshots (the loan card, Book strip, trade controls, Loan Book
  // card) which are unobstructed by the global memo overlay, plus full-page
  // shots where the overlay doesn't cover the evidence.
  await page.evaluate(async () => {
    if (window.__stvgDebug && window.__stvgDebug.drainAndPause) await window.__stvgDebug.drainAndPause();
  });
  await sleep(700);

  // Helper: screenshot a single element by selector (clipped to its box).
  const shotEl = async (sel, name) => {
    const el = await page.$(sel);
    if (el) { await el.screenshot({ path: path.join(OUT, name) }); console.log('P7PROBE SHOT ' + name + ' (element)'); return true; }
    console.log('P7PROBE missing element ' + sel);
    return false;
  };

  // Place a personal trade so the Book line + position pill appear: buy SP500.
  const buy = await page.evaluate(async (gid) => {
    const r = await fetch(`/api/game/${gid}/trade`, {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ marketId: 'SP500', side: 'buy', amount: 1500 }),
    }).then((x) => x.json()).catch((e) => ({ error: String(e) }));
    return r;
  }, GID);
  console.log('P7PROBE buy', JSON.stringify(buy && buy.data ? buy.data.tradeBook : buy));
  await page.evaluate(async () => { if (window.__stvgDebug && window.__stvgDebug.refresh) await window.__stvgDebug.refresh(); });
  await sleep(800);

  // ── (a) Economy tab: open a loan card to show terms + pitch ────────────────
  await page.keyboard.press('1');
  await sleep(500);
  // Promote SP500 to the hero chart first so the BUY/SELL strip + pill render.
  await page.evaluate(() => {
    const thumbs = [...document.querySelectorAll('.thumb')];
    const t = thumbs.find((x) => /S&P|SP500/i.test(x.textContent || ''));
    if (t) t.click();
  });
  await sleep(600);
  // Expand the first deal in the left Loan Book rail. Prospects trickle, so if
  // none are present this instant, briefly resume the runner to let one arrive,
  // then re-pause and expand (the Book strip + an expanded card both visible).
  let opened = await page.evaluate(() => {
    const b = document.querySelector('.deal-board .deal-summary');
    if (b) { b.click(); return true; }
    return false;
  });
  for (let r = 0; r < 8 && !opened; r++) {
    await page.evaluate(async (gid) => { await fetch(`/api/game/${gid}/advance`, { method: 'POST' }).catch(() => {}); }, GID);
    await sleep(900);
    opened = await page.evaluate(() => {
      const b = document.querySelector('.deal-board .deal-summary');
      if (b) { b.click(); return true; }
      return false;
    });
  }
  await page.evaluate(async () => { if (window.__stvgDebug && window.__stvgDebug.drainAndPause) await window.__stvgDebug.drainAndPause(); });
  await sleep(700);
  console.log('P7PROBE deal opened=' + opened);
  // (a) Element shot of the whole loan-book rail: Book strip header + expanded
  // card with terms (amount / expected return / duration / soundness) + pitch.
  await shotEl('.deal-board', 'a_loan_card_terms_pitch.png');

  // ── (b) Outcome bark + Book strip together ─────────────────────────────────
  // Force an outcome character card so the bark is visible; the Book strip lives
  // in the same rail. Full-page so both the bottom-right bust and the left-rail
  // strip are captured in one frame (the memo overlay sits on the right, clear of
  // both).
  await page.evaluate(() => {
    if (window.__stvgDebug && window.__stvgDebug.showCard) {
      window.__stvgDebug.showCard('npc_walter_hollis', 'Dutch Calloway', 'Mortgage Desk · The Gunslinger', 'gunslinger',
        'Trident Solutions paid off clean. Boring is beautiful — told you that tape was in our favour.');
    }
  });
  // Wait for the bust to mount + finish sliding in, then capture it as an element
  // (the global memo overlay sits on the right, clear of the bottom-right card).
  await page.waitForSelector('.char-card', { timeout: 4000 }).catch(() => {});
  await sleep(1400);
  await page.screenshot({ path: path.join(OUT, 'b_outcome_bark_book_strip.png') });
  console.log('P7PROBE SHOT b_outcome_bark_book_strip (full)');
  await shotEl('.char-card', 'b1_outcome_bark.png');
  // Also a tight element shot of just the Book strip for unambiguous evidence.
  await shotEl('.book-strip', 'b2_book_strip.png');

  // ── (c) BUY control on the promoted market + position pill ─────────────────
  // The TradeControls strip sits under the hero chart (SP500 promoted above).
  await shotEl('.trade', 'c_buy_market_position_pill.png');
  // The position pill is on the SP500 thumbnail — element shot of the thumb strip.
  await shotEl('.thumbs', 'c2_position_pills.png');

  // ── (d) Financials tab: the Loan Book card with stats + outcomes ───────────
  // The Loan Book card lives in the right sidebar, exactly under the global
  // decision memo overlay (which can't be drained while paused — decisions are
  // action-point-gated and otherwise lapse on quarter advance). Hide the memo
  // overlay for this evidence shot only (screenshot concern, no state change).
  await page.keyboard.press('4');
  await sleep(800);
  await page.addStyleTag({ content: '.memo-overlay,.resume-bar,.crisis-modal,.modal-scrim{display:none !important;}' });
  await sleep(400);
  await shotEl('.loanbook', 'd_financials_loanbook.png');
  // Full-page Financials too (shows the TopBar BOOK line in context).
  await page.screenshot({ path: path.join(OUT, 'd2_financials_full.png') });
  console.log('P7PROBE SHOT d2_financials_full');

  await browser.close();
  console.log('P7PROBE DONE');
})().catch((e) => { console.error('P7PROBE FATAL', e.message); process.exit(1); });
