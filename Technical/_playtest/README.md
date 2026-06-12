# STVG Playtest Harness

Permanent, *organized* browser-driving harness for the STVG banking-CEO sim
(process rule R4: organized, not deleted). Every tool here drives a **running
`stvg_server` on `http://localhost:8080/`** through a real browser (Microsoft
Edge via `puppeteer-core`, headless) and produces screenshots and/or exercises
the telemetry instrumentation.

## Prerequisites (every tool)

1. **Server running on :8080.** Build and launch `stvg_server` (serves the built
   Svelte frontend from its `static/` dir plus the REST/WS API). The harness does
   not start the server — start it yourself first (e.g. `play.ps1` at project root,
   or run the built `stvg_server` binary).
2. **Edge installed** at the hard-coded path
   `C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe`. The scripts use
   `puppeteer-core` (no bundled Chromium download) and point `executablePath` at
   this Edge. If Edge lives elsewhere, edit the `EDGE` const at the top of each
   script.
3. **`puppeteer-core` resolvable.** It is pinned in this directory's `package.json`
   and installed in the local `./node_modules`. Run the tools **from this
   `_playtest/` directory** (or from `harness/` — Node walks up to
   `_playtest/node_modules`) and **`puppeteer-core` resolves with `NODE_PATH`
   unset** — no env var needed:
   ```
   cd Technical\_playtest
   node harness\shoot_final.js
   ```
   (The sibling `StatisticalEngine\frontend\node_modules` does **not** contain
   `puppeteer-core` — only the local `_playtest\node_modules` does. Older script
   header comments mention setting `NODE_PATH` to the frontend modules; that is
   stale. Use the local install.)

### Install / refresh the dependency

The install is now managed by `package.json` (pins `puppeteer-core` 25.1.0):
```
cd Technical\_playtest
npm install        # restores ./node_modules from package.json
```

## `harness/` — the keeper tools (maintained)

| Tool | What it does |
|------|--------------|
| `harness/shoot.js [outDir]` | Generic walk-the-start-flow shooter. Loads the title screen, clicks the most plausible "advance" button (start/new game/begin/play/continue…), shoots each stage, and once in-game lets the sim run and takes timed shots (`10_game_12s`, `11_game_32s`). Default `outDir` = `shots/`. Robust to UI label changes. |
| `harness/shoot_live.js` | Shoots the sim actually **running**: clicks Begin, presses Space to start, pushes speed up, and shoots at 20s / 60s / 120s, clicking any "Resume" boundary bar to keep time flowing. Output → `shots/` (`20_live_20s` …). |
| `harness/shoot_final.js` | The 4-tab evidence set: Begin, let it auto-run ~45s to accumulate chart history, then shoot all four tabs via keys `1`–`4` (economy / hire / bank / financials), plus a later-era look. Output → `shots/final/`. |
| `harness/probe_telemetry.js` | ~3-minute scripted session that exercises the instrumented surfaces (begin → auto-run → open a market → hover tickers/division cards/balance-sheet lines → open deals invest/pass → let a decision lapse → toggle speed → race toward phase 2) so the produced `telemetry/session_*.jsonl` carries a broad event-type inventory. Verifies telemetry v2 (STAR_02 P1). Prints progress; flushes telemetry on exit. |
| `harness/verify_ready.js` | **The W4 full-readiness probe (keeper).** Drives every surface the owner touches and prints `CHECK [PASS/FAIL]` per step + a screenshot per step into `shots/ready/`: title online indicator → Begin → charts ≤3s → all 4 tabs → promote macro+market to hero + timescale switch → expanded explainer incl. the live **Economy P&L** chip → submit a decision → engineer a **major decision pause** (W5.1) + routine lapse → invest/pass a deal → loan outcome (Book strip) → hire a candidate → trade buy/sell → portrait card → annual report → save/reload/continue (chart backfill) → era transition → **funding-mix clear of an open memo on Financials** (W5.3). Captures `pageerror`s; exits non-zero on any hard FAIL or page error. Also produces the broad telemetry session the analyzer reads. Run with the server up: `node harness\verify_ready.js`. |
| `harness/verify_api.js` | **The W4 REST API smoke (keeper).** Pure-HTTP (Node 18+ `fetch`, no deps) sweep of every endpoint with `PASS/FAIL` per check: health → new game → state → decision → hire → poach-list → deal accept (valid + unknown-id clean 4xx) → trade buy/sell + over-limit/short/unknown-market rejections → macro-history → `/api/archetypes` (8 families / 33 specs) → **save→load blob round-trip** with year/quarter/capital spot-equality → **synthetic v3 blob** (strip `personalBook`, version=3 → graceful) → garbage-blob clean 4xx → telemetry POST. Exits non-zero if any check FAILs. `node harness\verify_api.js [http://host:port]`. **Save/load contract:** `/save` returns the full save **blob** in `data` (not a slot); `/load` takes that blob as the request body and returns `{loaded:true}` — the frontend's slot-based `api.saveGame/loadGame` wrappers are unused dead code (the real Continue flow re-attaches to the in-memory game by `lastGameId` + `getState`). |

Run any of them after the server is up:
```
cd Technical\_playtest
node harness\probe_telemetry.js
node harness\shoot_final.js
```
Or via the npm scripts: `npm run shoot:final`, `npm run probe:telemetry`, etc.

## `analyze_session.py` — the analyzer (stays at root)

Turns one telemetry session JSONL into a human/agent-readable markdown report
(timeline, dwell, engagement, pace, money curve). Stdlib only, no deps.
```
python analyze_session.py <session.jsonl> [-o report.md]
python analyze_session.py --selftest
```
`sample_session_report.md` is an example of its output.

## `shots/` — screenshot evidence (stays as-is)

Per-phase PNG evidence produced by the harness and the archived probes:
`shots/` root holds the generic/live shooter output (`01_title`, `10_game_12s`,
`20_live_20s` …); subdirs `final/`, `p2/`, `p3f/`, `p4/`, `p6/`, `p7/` hold the
per-phase STAR_02 megaplan evidence. These are written by the scripts' `OUT`
paths; keep them as the R2 "done = evidence" record.

`shots/ready/` holds the **W4 full-verification-sweep** evidence:
`verify_ready.js` step screenshots + `verify_ready.log` (18/18 CHECK PASS, 0
pageerror); `api_smoke.log` (23/23 PASS); `suite_summary.txt` (505 tests:
504 PASSED / 1 SKIPPED / 0 FAILED); `autoplay_quickmatrix.log` (12 bots × 3
games, 0 NaN, 0 bankruptcies); `session_report.md` (analyzer output, 32 event
types); `stability.log` (8× run, 0 pageerror, heap delta < 100 MB, annual
corner card engaged).

## `archive_probes/` — one-off phase probes (reference, NOT maintained)

Single-purpose scripts written for specific STAR_02 phases and kept only as a
reference for how a given surface was driven. They are **not** maintained against
the current UI and may break; pull patterns from them, don't rely on them:
`shoot_p2_tabs.js`, `shoot_p3f.js`, `shoot_p4_characters.js`,
`shoot_p4_clean.js`, `shoot_p6.js`, `shoot_p7.js`, `p4_final.js`,
`check_p4_lengths.js`.

## `archive_probe_sessions/` — archived agent-probe telemetry

The 30 `session_*.jsonl` files produced by agent probe runs (moved here from
`StatisticalEngine/telemetry/` during cleanup). They are the only forensic record
of agent-probe behavior; the owner's real telemetry must land in an empty
`StatisticalEngine/telemetry/`. Analyze any of them with `analyze_session.py`.

## `node_modules/`

Local install of `puppeteer-core` (+ its deps) per `package.json`. Not committed
elsewhere; restore with `npm install`.
