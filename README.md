# STVG — Banking CEO Simulation (1945-2040)

A historically-calibrated banking CEO simulation built as a C++ statistical engine with a browser-based web demo. Manage a bank through 95 years of American financial history — from post-war stability through deregulation, the GFC, and into the AI age.

## Quick Start

```bash
cd Technical/StatisticalEngine

# Build (MSVC 2022 / Visual Studio 17 — kill zombies first; reconfigure after new test files)
taskkill /F /IM stvg_server.exe /IM stvg_tests.exe /IM stvg_autoplay.exe
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build --config Debug --parallel 4

# Run tests (505 tests: 504 pass / 1 skip / 0 fail)
build\Debug\stvg_tests.exe --gtest_brief=1

# Play in browser
build\Debug\stvg_server.exe 8080 static
# Open http://localhost:8080/
```

## The Game

**Core Innovation**: Organizational complexity. As your bank grows from $10B to $2T, visibility drops from 100% to 5%. Division heads may lie. Hidden risks accumulate. Can you build a banking empire without it blowing up?

**7 Historical Eras** with progressive complexity:
1. **Post-War Stability** (1945-1959) — Simple commercial bank, "3-6-3 banking"
2. **Expansion & Innovation** (1960-1972) — Credit cards, international banking
3. **Volatility & Deregulation** (1973-1986) — Trading desk unlocks, Volcker shock
4. **Big Bang & Excess** (1987-1999) — Glass-Steagall falls, investment banking
5. **Shadow Banking & Hubris** (2000-2007) — CDOs, maximum complexity and danger
6. **Crisis & Reform** (2008-2019) — GFC survival, Dodd-Frank constraints
7. **Modern & Endgame** (2020-2040) — AI CEO threat, climate tipping points

## By the Numbers

| Feature | Count |
|---------|-------|
| Historical eras | 7 (1945-2040) |
| Bank divisions | 16 (era-gated) |
| Playable CEOs | 24 (unique abilities) |
| Starting positions | 5 (Commercial, Trading, IB, Community, Universal) |
| Scenarios | 5 (Tutorial, Crisis, Full Game, Speedrun, AI Endgame) |
| Historical events | 111 (Korean War through AI disruption) |
| Correlated markets | 6 (S&P, Treasury, Gold, EUR/USD, VIX, Bitcoin) |
| Living-economy macro series | 8 (GDP, Unemployment, Fed Funds, CPI, Credit Spread, 10Y, GDP-growth, Econ P&L) |
| AI competitor banks | 6 (persistent, personality-driven) |
| Autoplay bot strategies | 12 |
| UI tabs | 4 (Economy, Hire, My Bank, Financials) |
| NPC character cast | 29 (12 recurring bankers + 17 presidents) |
| Hire archetypes | 8 families + 33 specializations |
| Telemetry event types | 42 (incl. dwell + hover) |
| Engine header files | 59 (core, game, math, simulation, api, autoplay) |
| REST endpoints | 40 routes (Crow) + WebSocket |
| Unit tests | 505 (504 pass / 1 skip / 0 fail) |
| Regulatory frameworks | 7 (Glass-Steagall through AI/Climate regulation) |

## Architecture

```
Svelte 5 + Vite + TypeScript  ←→  Crow REST/WebSocket  ←→  C++ Statistical Engine
     ↓                                  ↓                         ↓
 TopBar + TabShell                40 REST endpoints         59 header files
 4 tabs: Economy/Hire/Bank/Fin    WebSocket broadcast       GARCH markets + econ block
 CharacterCard portraits          Game state management     MacroHistory / ArchetypeRegistry
 Macro + market charts            /macro-history, /trade    ReputationLens / PersonalBook
 DealBoard rail (loan/deal flow)  Telemetry → JSONL         Trader AI / archetype P&L
 lightweight-charts v5            Save/Load (kSaveVersion 4) Crisis / endgame
```

See [ARCHITECTURE.md](Technical/StatisticalEngine/docs/ARCHITECTURE.md) for the full layer diagram.

## Project Structure

```
STVG/
├── NORTH_STAR.md                    # Immutable founding vision = STAR_01 (DO NOT MODIFY)
├── README.md                        # This file
├── .claude/instructions.md          # Agent onboarding instructions
├── Stars/                           # Vision documents (STAR_01→NORTH_STAR, STAR_02 day-trading
│   │                                #   overhaul + append-only addenda); see Stars/README.md
│   └── STAR_02_DAY_TRADING_FEEL_AND_CHARACTERS.md
├── Technical/
│   ├── StatisticalEngine/           # THE ENGINE
│   │   ├── include/stvg/            # 59 headers (core, game, math, simulation, api, autoplay)
│   │   ├── src/                     # main.cpp, autoplay_main.cpp, game handler .cpp
│   │   ├── tests/                   # 62 test files (505 tests)
│   │   ├── frontend/               # Svelte 5 + Vite + TypeScript (runes stores, tab shell, lightweight-charts v5)
│   │   ├── data/                    # JSON configs, events, scenarios, calibration, archetypes/, ceos/
│   │   ├── static/                  # Vite build output (index.html + /assets/ + /assets/portraits/)
│   │   ├── telemetry/               # Owner playtest session_*.jsonl land here (analyze_session.py)
│   │   ├── docs/                    # ARCHITECTURE, PROJECT_HISTORY, FILE_CATALOG
│   │   └── CMakeLists.txt           # Build config (4 targets)
│   ├── Content/kb_mining/           # STAGED KB-mined events/characters (owner review before merge)
│   ├── Plans/                       # STAR_02_OVERHAUL_PLAN, READY_FOR_PLAY_PLAN, roster design, …
│   ├── _playtest/                   # PERMANENT screenshot/telemetry harness — being reorganized
│   │                                #   into harness/ (keepers) + archive_probes/ (one-off probes)
│   │                                #   + shots/ (evidence) + analyze_session.py + README
│   ├── megaplan_state.json          # Canonical megaplan execution state
│   ├── Handoffs/                    # Session handoff documentation
│   └── PROGRESS_LOG.md              # Development session log
├── Inputs/                          # Source materials (read-only)
├── Outputs/                         # Final deliverables
└── Archive/                         # Deprecated: Legacy Engine, old docs, React prototype
```

## Key Documents

- **[NORTH_STAR.md](NORTH_STAR.md)** — Immutable vision: eras, gameplay feel, market simulation, AI endgame
- **[ARCHITECTURE.md](Technical/StatisticalEngine/docs/ARCHITECTURE.md)** — Full system architecture and layer diagram
- **[PROJECT_HISTORY.md](Technical/StatisticalEngine/docs/PROJECT_HISTORY.md)** — 22-phase development timeline (through the STAR_02 overhaul)
- **[FILE_CATALOG.md](Technical/StatisticalEngine/docs/FILE_CATALOG.md)** — Complete file inventory with line counts

## Current Status

**STAR_02 "day-trading feel & characters" overhaul complete (2026-06-12)** — executed as a
9-subagent megaplan against [STAR_02](Stars/STAR_02_DAY_TRADING_FEEL_AND_CHARACTERS.md); see
[HANDOFF_20260612_094900.md](Technical/Handoffs/HANDOFF_20260612_094900.md) and
[megaplan_state.json](Technical/megaplan_state.json). Headlines: a **4-tab Svelte UI**
(Economy / Hire / My Bank / Financials), a **portrait + credibility system** (Hades-style
character pop-ups whose language signals honesty), an **archetype × macro P&L engine**
(division revenue distributed by per-archetype betas + variance), a **lending-first early
game** with a CEO **personal trading book**, a **hiring economy** with poach offers + a
reputation lens, **living-economy macro charts** (GDP/Unemployment/Fed Funds + macro history),
and **telemetry v2**. **Gate: 505 C++ tests (504 pass / 1 skip / 0 fail), svelte-check 0/0.**
Engine was ~97% complete before the overhaul; this work was net-additive on top.

**Frontend (Svelte 5)**: persistent **TopBar** (identity / date / capital / play+speed / era /
regime / HIRE pulse) + **TabShell** with 4 tabs (keys 1-4). **Economy** tab — macro strip +
chips, promotable hero macro/market charts with 1Y/5Y/10Y/20Y/MAX timescales and expanded
explainers, news synced to real series direction. **Hire** tab — candidate trickle, poach
offers, per-employee roster. **My Bank** tab — divisions, loan book, personal trading book.
**Financials** tab — income statement / balance sheet / funding mix / loan book. **CharacterCard**
portrait overlay (DiceBear placeholders, typewriter, never blocks input), **DealBoard** rail
for loan/deal flow, **CrisisModal**, **AnnualReportModal** (full + corner-card at speed),
**EraTransition** + president card. Telemetry batches to `/api/telemetry` → JSONL. The 4-phase
layout components (EarlyEra/Office/Full layouts, MiniHeader, GameHeader, SimControls) were
**deleted**; phases now gate content richness inside the tabs.

**What's done**: 7 eras, 16 divisions, 24 CEOs, SFC economics (NIM, credit standards, duration
risk, Minsky dynamics, Basel RWA), regulatory gates, path dependence, crisis arcs, AI competitors,
political engine, climate/AI endgame, historical calibration from FRED data, save/load
(`kSaveVersion=4`), personality sensitivity (11 axes), diverse failure modes (7 death types);
**plus STAR_02**: MacroHistory, ArchetypeRegistry (8 families + 33 specializations), ReputationLens,
PersonalBook, event→market sign-weighting, portrait/credibility cast (12 bankers + 17 presidents).

**What's next**: owner **playtest loop** (P10 — play sessions; telemetry lands in
`StatisticalEngine/telemetry/`; `analyze_session.py` → iterate tunables); **KB merge review**
(`Technical/Content/kb_mining/` — staged events/characters await owner sign-off); **sprite
generation on the RTX 5090 + tiny-tower floor view** (P9; placeholder strip in My Bank now);
a **leverage death-spiral balance pass** (pre-existing: bots die ~Q304 at 19.4× leverage,
archetype-independent); then audio assets + itch.io packaging.

## How to Play

```bash
cd Technical/StatisticalEngine

# Build (requires MSVC 2022 / Visual Studio 17)
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build --config Debug --parallel 4

# Play in browser
build\Debug\stvg_server.exe 8080 static
# Open http://localhost:8080/

# Run tests
build\Debug\stvg_tests.exe --gtest_brief=1

# Autoplay validation
build\Debug\stvg_autoplay.exe --quick-matrix --parallel
```

**Play flow:** title → **Begin** (auto-starts the sim) → the four tabs fill in as you go —
take loans and watch the Loan Book and personal trading book move, hire and poach staff,
click a chart to promote it to the hero slot, let routine decisions lapse or act on the
big ones, and watch portraits pop on era/credibility beats. Everything you do is recorded:
playtest **telemetry lands in `Technical/StatisticalEngine/telemetry/` as `session_*.jsonl`**,
and `python Technical/_playtest/analyze_session.py <session.jsonl> -o report.md` renders a
session report for tuning pace, the money curve, and portrait cadence.

---

*Last Updated: June 12, 2026 (STAR_02 day-trading & characters overhaul)*
*Status: STAR_02 overhaul complete · 505 tests (504/1 skip) · svelte-check 0/0 · ready for owner playtest*
