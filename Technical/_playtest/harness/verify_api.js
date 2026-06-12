#!/usr/bin/env node
/*
 * verify_api.js — STVG REST API smoke test (permanent keeper, W4.2).
 *
 * Drives a running `stvg_server` on http://localhost:8080 through its REST
 * surface and asserts each endpoint behaves: new game → state → decision →
 * hire → poach list → deal accept/reject/pass → trade buy/sell/limits →
 * macro-history → /api/archetypes → save/load round-trip with state spot-equality
 * → synthetic v3-blob graceful load → telemetry POST. Every check prints
 * PASS/FAIL; exits non-zero if anything FAILED so it can gate.
 *
 * Usage (server must already be up):
 *   cd Technical\_playtest
 *   node harness\verify_api.js               # default http://localhost:8080
 *   node harness\verify_api.js http://host:port
 *
 * No deps — uses Node 18+ global fetch. Pure HTTP; does not start the server.
 */

const BASE = (process.argv[2] || 'http://localhost:8080').replace(/\/$/, '');

let pass = 0, fail = 0;
const lines = [];
function rec(ok, label, detail) {
  const tag = ok ? 'PASS' : 'FAIL';
  const line = `[${tag}] ${label}${detail ? ' — ' + detail : ''}`;
  lines.push(line);
  console.log(line);
  if (ok) pass++; else fail++;
  return ok;
}

async function api(path, init) {
  const res = await fetch(BASE + path, {
    headers: { 'Content-Type': 'application/json', ...(init && init.headers) },
    ...init,
  });
  let body = null;
  try { body = await res.json(); } catch { /* non-json */ }
  return { status: res.status, ok: res.ok, body };
}
// unwrap the {success,data,error} envelope
function data(r) { return r.body && typeof r.body === 'object' ? r.body.data : undefined; }

async function main() {
  console.log(`=== STVG API smoke @ ${BASE} ===`);
  console.log(new Date().toISOString());

  // 1. health
  let r = await api('/api/health');
  rec(r.ok && data(r) && typeof data(r).status === 'string', 'GET /api/health', `status=${data(r) && data(r).status}`);

  // 2. new game
  r = await api('/api/game/new', { method: 'POST', body: JSON.stringify({ seed: 4242 }) });
  const gameId = data(r) && data(r).gameId;
  rec(r.ok && !!gameId, 'POST /api/game/new', `gameId=${gameId}`);
  if (!gameId) { return finish(); }
  const g = encodeURIComponent(gameId);

  // 3. state
  r = await api(`/api/game/${g}/state`);
  const st0 = data(r);
  rec(r.ok && st0 && st0.bank, 'GET /state', st0 && st0.bank ? `bank=${st0.bank.name} cap=${st0.bank.capital}` : '');
  const year0 = st0 && st0.state && st0.state.currentYear;
  const quarter0 = st0 && st0.state && st0.state.currentQuarter;
  const capital0 = st0 && st0.bank && st0.bank.capital;

  // 4. submit a decision (if any pending; else note none — engine may have none at t0)
  const decisions = (st0 && st0.pendingDecisions) || [];
  if (decisions.length && decisions[0].options.length) {
    const d = decisions[0];
    r = await api(`/api/game/${g}/decision/${encodeURIComponent(d.id)}`, {
      method: 'POST', body: JSON.stringify({ optionId: d.options[0].id }),
    });
    rec(r.ok && !!data(r), 'POST /decision (valid)', `${d.id} -> ${d.options[0].id}`);
  } else {
    rec(true, 'POST /decision', 'no pending decision at t0 (acceptable)');
  }

  // 5. hire — pick first candidate + first division, expect a sane 2xx/4xx (no crash)
  const pool = (st0 && st0.hiringPool) || [];
  const divs = (st0 && st0.bank && st0.bank.divisions) || [];
  if (pool.length && divs.length) {
    r = await api(`/api/game/${g}/hire`, {
      method: 'POST', body: JSON.stringify({ candidateId: pool[0].id, divisionId: divs[0].id }),
    });
    rec(r.status < 500, 'POST /hire', `cand=${pool[0].id} div=${divs[0].id} status=${r.status}`);
  } else {
    rec(true, 'POST /hire', 'no candidates/divisions to hire at t0 (acceptable)');
  }

  // 6. poach-offers list (read from state; offers may be empty early)
  r = await api(`/api/game/${g}/state`);
  const offers = (data(r) && data(r).poachOffers) || [];
  rec(r.ok, 'poach-offers (state.poachOffers)', `count=${offers.length}`);

  // 7. deal accept (valid), accept unknown-id (clean 4xx, no crash), pass
  const deals = (st0 && st0.availableDeals) || [];
  if (deals.length) {
    const dl = deals[0];
    const amt = dl.investmentRequired || dl.size || 1e9;
    r = await api(`/api/game/${g}/deal/${encodeURIComponent(dl.id)}/accept`, {
      method: 'POST', body: JSON.stringify({ investmentAmount: amt }),
    });
    rec(r.status < 500, 'POST /deal/accept (valid)', `status=${r.status}`);
  } else {
    rec(true, 'POST /deal/accept', 'no available deals at t0 (acceptable)');
  }
  r = await api(`/api/game/${g}/deal/${encodeURIComponent('does_not_exist_999')}/accept`, {
    method: 'POST', body: JSON.stringify({ investmentAmount: 1e9 }),
  });
  rec(r.status >= 400 && r.status < 500, 'POST /deal/accept (unknown id → clean 4xx)', `status=${r.status}`);
  // "pass" on a deal == not accepting; the API has no explicit pass route, so we
  // assert the unknown-id path didn't 5xx (above) and the server is still alive:
  r = await api('/api/health');
  rec(r.ok, 'server alive after unknown-deal', `status=${r.status}`);

  // 8. trade buy/sell + limit-rejection + short-rejection + unknown-market.
  // The engine seeds uppercase market ids (MarketSimulator): SP500, UST10Y,
  // GOLD, SPXOPT, EURUSD, BTC.
  const knownMarket = 'SP500';
  // The personal book starts with ~$50k cash — buy within it. buy small.
  r = await api(`/api/game/${g}/trade`, {
    method: 'POST', body: JSON.stringify({ marketId: knownMarket, side: 'buy', amount: 1e4 }),
  });
  const boughtOk = r.ok && data(r) && data(r).traded === true;
  rec(boughtOk, 'POST /trade buy (within cash)', `${knownMarket} status=${r.status} traded=${data(r) && data(r).traded}`);
  // sell some of what we bought
  r = await api(`/api/game/${g}/trade`, {
    method: 'POST', body: JSON.stringify({ marketId: knownMarket, side: 'sell', amount: 5e3 }),
  });
  rec(r.status < 500, 'POST /trade sell', `status=${r.status} traded=${data(r) && data(r).traded}`);
  // absurd buy (limit rejection) — far beyond personal book cash → expect rejection or traded:false, never 5xx
  r = await api(`/api/game/${g}/trade`, {
    method: 'POST', body: JSON.stringify({ marketId: knownMarket, side: 'buy', amount: 1e15 }),
  });
  rec(r.status < 500, 'POST /trade over-limit (no crash)', `status=${r.status} traded=${data(r) && data(r).traded}`);
  // short rejection — sell with no position
  r = await api(`/api/game/${g}/trade`, {
    method: 'POST', body: JSON.stringify({ marketId: 'GOLD', side: 'sell', amount: 1e9 }),
  });
  rec(r.status < 500, 'POST /trade short (no crash)', `status=${r.status} traded=${data(r) && data(r).traded}`);
  // unknown market
  r = await api(`/api/game/${g}/trade`, {
    method: 'POST', body: JSON.stringify({ marketId: 'not_a_market_zzz', side: 'buy', amount: 1e6 }),
  });
  rec(r.status >= 400 || (data(r) && data(r).traded === false), 'POST /trade unknown market (clean reject)', `status=${r.status}`);

  // 9. macro-history
  r = await api(`/api/game/${g}/macro-history`);
  const mh = data(r);
  rec(r.ok && mh && Array.isArray(mh.quarters), 'GET /macro-history', `quarters=${mh && mh.quarters && mh.quarters.length}`);

  // 10. archetypes (8 families / 33 specs). This route serves the canonical
  //     archetypes.json RAW (not wrapped in the {success,data} envelope).
  r = await api('/api/archetypes');
  const arch = r.body; // raw object
  const famCount = arch && Array.isArray(arch.families) ? arch.families.length : 0;
  const specCount = arch && Array.isArray(arch.specializations) ? arch.specializations.length : 0;
  rec(r.ok && famCount === 8 && specCount === 33, 'GET /api/archetypes', `families=${famCount} specs=${specCount}`);

  // 11. save → load round-trip → state spot-equality (year/quarter/capital).
  //     CONTRACT: /save returns the full save BLOB in `data`; /load takes that
  //     blob as the request body (NOT a slot) and returns {loaded:true}.
  r = await api(`/api/game/${g}/save`, { method: 'POST', body: JSON.stringify({}) });
  const blob = data(r);
  rec(r.ok && blob && blob.version != null, 'POST /save (returns save blob)', `version=${blob && blob.version}`);
  r = await api(`/api/game/${g}/load`, { method: 'POST', body: JSON.stringify(blob) });
  rec(r.ok && data(r) && data(r).loaded === true, 'POST /load (blob round-trip)', `loaded=${data(r) && data(r).loaded}`);
  // re-read state and spot-check it equals the pre-save snapshot.
  r = await api(`/api/game/${g}/state`);
  const stL = data(r);
  const yearL = stL && stL.state && stL.state.currentYear;
  const quarterL = stL && stL.state && stL.state.currentQuarter;
  const capitalL = stL && stL.bank && stL.bank.capital;
  rec(yearL === year0 && quarterL === quarter0,
      'load spot-equality (year/quarter)', `${year0}Q${quarter0} == ${yearL}Q${quarterL}`);
  rec(Math.abs((capitalL || 0) - (capital0 || 0)) < 1,
      'load spot-equality (capital)', `${capital0} == ${capitalL}`);

  // 12. synthetic v3 blob load → graceful. Take the real blob, STRIP personalBook
  //     (a field added after v3) and set version=3, then load it. A v3 save must
  //     load without crashing (the engine migrates a missing macroHistory/
  //     personalBook to empty defaults). Asserts backward-compat, not a 5xx.
  if (blob && typeof blob === 'object') {
    const v3 = JSON.parse(JSON.stringify(blob));
    delete v3.personalBook;
    v3.version = 3;
    r = await api(`/api/game/${g}/load`, { method: 'POST', body: JSON.stringify(v3) });
    rec(r.status < 500 && (data(r) ? data(r).loaded === true : r.status < 400),
        'POST /load synthetic v3 (no personalBook, version=3 → graceful)', `status=${r.status} loaded=${data(r) && data(r).loaded}`);
  } else {
    rec(false, 'synthetic v3 load', 'no blob to mutate');
  }
  // unknown/garbage blob → clean 4xx, no crash.
  r = await api(`/api/game/${g}/load`, { method: 'POST', body: JSON.stringify({ not: 'a save' }) });
  rec(r.status >= 400 && r.status < 500, 'POST /load (garbage blob → clean 4xx, no crash)', `status=${r.status}`);

  // 13. telemetry POST
  r = await api('/api/telemetry', {
    method: 'POST',
    body: JSON.stringify({ sessionId: 's_smoke_' + Date.now(), events: [{ t: 0, type: 'smoke_ping', data: {} }] }),
  });
  rec(r.status < 500, 'POST /api/telemetry', `status=${r.status}`);

  return finish();
}

function finish() {
  console.log('---');
  console.log(`SUMMARY: ${pass} PASS, ${fail} FAIL`);
  process.exitCode = fail === 0 ? 0 : 1;
}

main().catch((e) => {
  rec(false, 'FATAL', e && e.message);
  finish();
});
