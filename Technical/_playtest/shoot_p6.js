/* STAR_02 P6 evidence: begin a game, run the sim to accumulate trickled
 * candidates + poach offers + a reputation tag, then screenshot the Hire tab.
 * Captures: HireTab with candidates, a poach card, and the reputation tag (top
 * bar). Falls back to the REST truth view to confirm poach offers exist. */
const puppeteer = require('puppeteer-core');
const path = require('path');
const fs = require('fs');
const EDGE = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const OUT = path.join(__dirname, 'shots', 'p6');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const browser = await puppeteer.launch({
    executablePath: EDGE, headless: 'new',
    args: ['--window-size=1680,950', '--disable-gpu'],
    defaultViewport: { width: 1680, height: 950 },
  });
  const page = await browser.newPage();
  page.on('console', (m) => { if (/P6PROBE/.test(m.text())) console.log(m.text()); });
  await page.goto('http://localhost:8080/', { waitUntil: 'networkidle2', timeout: 30000 });
  await sleep(1800);

  // Begin the game.
  await page.evaluate(() => {
    const b = [...document.querySelectorAll('button')].find((x) => /begin/i.test(x.textContent));
    if (b) b.click();
  });
  await sleep(2500);

  // Push to 8x and let many game-years elapse so rivals build a track record
  // (poach offers need ~2+ years of rival operation) and candidates trickle in.
  await page.keyboard.press(']'); await page.keyboard.press(']'); await page.keyboard.press(']');
  // Keep the world flowing past every gate: resolve a decision memo (click the
  // first option), or click the Resume bar, each tick — this is what actually
  // advances the clock so poach offers can generate.
  // Drive the game forward via the REST API directly (UI timing for crisis
  // buttons / memo options is flaky in headless). Resolve crises + decisions,
  // then nudge the runner to resume so the clock advances.
  const keepFlowing = async () => {
    return await page.evaluate(async () => {
      const gid = 'game_42';
      const st = await fetch(`/api/game/${gid}/state`).then((r) => r.json()).then((j) => j.data).catch(() => null);
      if (!st) return 'nostate';
      // Resolve active crises (measured response).
      for (const c of (st.activeCrises || [])) {
        if (!c.resolved) {
          await fetch(`/api/game/${gid}/crisis/respond`, {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ crisisId: c.id, responseType: 'measured' }),
          }).catch(() => {});
        }
      }
      // Submit the first option of each pending decision.
      for (const d of (st.pendingDecisions || [])) {
        if (d.options && d.options.length) {
          await fetch(`/api/game/${gid}/decision/${encodeURIComponent(d.id)}`, {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ optionId: d.options[0].id }),
          }).catch(() => {});
        }
      }
      return st.state ? `${st.state.currentYear}Q${st.state.currentQuarter}/${st.phase}` : 'ok';
    });
  };
  // Keep the runner playing: click the transport play button whenever it shows
  // the paused (▶) glyph, so the clock keeps advancing between boundaries.
  const nudgePlay = async () => {
    await page.evaluate(() => {
      const play = document.querySelector('.play');
      if (play && /▶|▶/.test(play.textContent || '')) play.click();
    });
  };
  // Run flowing time so the sim covers a decade-plus of game-years (poach offers
  // need rivals to operate ≥2 years + a generation cycle).
  let lastTick = '';
  for (let i = 0; i < 90; i++) {
    const tick = await keepFlowing();
    await nudgePlay();
    await sleep(900);
    if (i % 6 === 0 && tick && tick !== lastTick) { console.log('P6PROBE', tick); lastTick = tick; }
  }

  // Pause the runner FIRST (so no new decisions spawn), then drain every
  // pending decision + crisis via the API so the memo/crisis overlay closes and
  // the Hire tab is unobstructed for the screenshots.
  await page.evaluate(() => { const p = document.querySelector('.play'); if (p && /⏸/.test(p.textContent || '')) p.click(); });
  await sleep(400);
  for (let i = 0; i < 12; i++) {
    const remaining = await page.evaluate(async () => {
      const gid = 'game_42';
      const st = await fetch(`/api/game/${gid}/state`).then((r) => r.json()).then((j) => j.data).catch(() => null);
      if (!st) return -1;
      for (const c of (st.activeCrises || [])) if (!c.resolved)
        await fetch(`/api/game/${gid}/crisis/respond`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ crisisId: c.id, responseType: 'measured' }) }).catch(() => {});
      for (const d of (st.pendingDecisions || [])) if (d.options && d.options.length)
        await fetch(`/api/game/${gid}/decision/${encodeURIComponent(d.id)}`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ optionId: d.options[0].id }) }).catch(() => {});
      return (st.pendingDecisions || []).length + (st.activeCrises || []).length;
    });
    // Refresh the client view so the now-empty pending list closes the memo.
    await page.evaluate(async () => { if (window.__stvgDebug && window.__stvgDebug.refresh) await window.__stvgDebug.refresh(); });
    if (remaining === 0) break;
    await sleep(300);
  }
  await sleep(800);

  // Pull health for the inspector.
  const info = await page.evaluate(async () => fetch('/api/health').then((r) => r.json()));
  console.log('P6PROBE health', JSON.stringify(info));
  // Report poach-offer count straight from the truth endpoint (ground truth).
  const offerCount = await page.evaluate(async () => {
    const h = await fetch('/api/health').then((r) => r.json());
    const gid = `game_${42}`; // default seed
    try {
      const t = await fetch(`/api/game/${gid}/poach-offers`).then((r) => r.json());
      return Array.isArray(t.data) ? t.data.length : -1;
    } catch { return -2; }
  });
  console.log('P6PROBE poachOffersFromApi', offerCount);

  // Switch to the Hire tab (keyboard 2) and let candidate/poach cards render.
  await page.keyboard.press('2');
  await sleep(1200);

  // Report what the client sees (candidates + offers + tag) for the inspector.
  const seen = await page.evaluate(() => {
    const text = document.body.innerText;
    return {
      hasPoachBoard: /Poach Board/i.test(text),
      hasTeamMade: /made .* over .* years/i.test(text),
      hasRepTag: /(gunslinger shop|fortress bank|balanced house|global house|tech-forward shop)/i.test(text),
      hasHiringDesk: /Hiring Desk/i.test(text),
    };
  });
  console.log('P6PROBE seen', JSON.stringify(seen));

  await page.screenshot({ path: path.join(OUT, '1_hire_tab.png') });
  console.log('P6PROBE SHOT 1_hire_tab');

  // Open a candidate's detail (self-pitch + specialization) for a close-up.
  await page.evaluate(() => {
    const b = document.querySelector('.cand .cand-summary');
    if (b) b.click();
  });
  await sleep(700);
  await page.screenshot({ path: path.join(OUT, '2_candidate_detail.png') });
  console.log('P6PROBE SHOT 2_candidate_detail');

  // Open a poach offer's confirm dialog (the "this team made $X" raid modal).
  const opened = await page.evaluate(() => {
    const b = document.querySelector('.offer .poach-btn');
    if (b) { b.click(); return true; }
    return false;
  });
  await sleep(700);
  if (opened) {
    await page.screenshot({ path: path.join(OUT, '3_poach_confirm.png') });
    console.log('P6PROBE SHOT 3_poach_confirm');
  } else {
    console.log('P6PROBE no poach offer card present yet');
  }

  // A topbar close-up of the reputation tag next to the era badge.
  await page.evaluate(() => { const d = document.querySelector('.poach-modal-wrap, .modal-scrim'); if (d) {} });
  await page.keyboard.press('Escape');
  await sleep(400);
  const bar = await page.$('.topbar');
  if (bar) { await bar.screenshot({ path: path.join(OUT, '4_topbar_reptag.png') }); console.log('P6PROBE SHOT 4_topbar_reptag'); }

  await browser.close();
  console.log('P6PROBE DONE');
})().catch((e) => { console.error('P6PROBE FATAL', e.message); process.exit(1); });
