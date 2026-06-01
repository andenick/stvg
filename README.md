# STVG — Banking CEO Simulation (1945-2040)

A historically-calibrated banking CEO simulation built as a C++ statistical engine with a browser-based web demo. Manage a bank through 95 years of American financial history — from post-war stability through deregulation, the GFC, and into the AI age.

## Quick Start

```bash
cd Technical/StatisticalEngine

# Build
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build --parallel 4

# Run tests (356+ tests)
./build/stvg_tests.exe --gtest_brief=1

# Play in browser
./build/stvg_server.exe 8080 static
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
| AI competitor banks | 6 (persistent, personality-driven) |
| Autoplay bot strategies | 12 |
| Unit tests | 442 (all passing) |
| Regulatory frameworks | 7 (Glass-Steagall through AI/Climate regulation) |

## Architecture

```
Svelte 5 + Vite + TypeScript  ←→  Crow REST/WebSocket  ←→  C++ Statistical Engine
     ↓                                  ↓                         ↓
 Cinematic TitleScreen            30 REST endpoints         52 header files
 4-phase progressive UI           WebSocket broadcast       GARCH markets
 Decision memo / drawer           Game state management     OU economy
 7 era-themed palettes            Static file serving       Trader AI
 7 sidebar panels                 Save/Load endpoints       PersonalityDrivenBot
 lightweight-charts v5            SimulationRunner          Crisis / endgame
```

See [ARCHITECTURE.md](Technical/StatisticalEngine/docs/ARCHITECTURE.md) for the full layer diagram.

## Project Structure

```
STVG/
├── NORTH_STAR.md                    # Immutable founding vision (DO NOT MODIFY)
├── README.md                        # This file
├── .claude/instructions.md          # Agent onboarding instructions
├── Technical/
│   ├── StatisticalEngine/           # THE ENGINE
│   │   ├── include/stvg/            # 52 headers (core, game, math, simulation, api, autoplay)
│   │   ├── src/                     # main.cpp, autoplay_main.cpp
│   │   ├── tests/                   # 48 test files (413+ tests)
│   │   ├── frontend/               # Svelte 5 + Vite + TypeScript (runes-based stores, lightweight-charts v5)
│   │   ├── data/                    # JSON configs, events, scenarios, calibration
│   │   ├── static/                  # Vite build output (index.html + /assets/)
│   │   ├── docs/                    # ARCHITECTURE, PROJECT_HISTORY, FILE_CATALOG
│   │   └── CMakeLists.txt           # Build config (4 targets)
│   ├── Handoffs/                    # Session handoff documentation
│   └── PROGRESS_LOG.md              # Development session log
├── Inputs/                          # Source materials (read-only)
├── Outputs/                         # Final deliverables
└── Archive/                         # Deprecated: Legacy Engine, old docs, React prototype
```

## Key Documents

- **[NORTH_STAR.md](NORTH_STAR.md)** — Immutable vision: eras, gameplay feel, market simulation, AI endgame
- **[ARCHITECTURE.md](Technical/StatisticalEngine/docs/ARCHITECTURE.md)** — Full system architecture and layer diagram
- **[PROJECT_HISTORY.md](Technical/StatisticalEngine/docs/PROJECT_HISTORY.md)** — 20-phase development timeline
- **[FILE_CATALOG.md](Technical/StatisticalEngine/docs/FILE_CATALOG.md)** — Complete file inventory with line counts

## Current Status

**Engine ~97% complete** (May 2026). **Frontend rebuilt to Svelte 5 in May 2026** (see [SVELTE_FRONTEND_REBUILD.md](Technical/Plans/SVELTE_FRONTEND_REBUILD.md)). Core engine unchanged: SFC banking economics, diverse failure modes, 442 C++ tests passing, autoplay validated with 12 personality-driven bots.

**Frontend (Svelte 5)**: cinematic title screen, 4-phase progressive disclosure (Desk → Office → Trading Floor → Command Center), 7 era-themed palettes, lightweight-charts streaming, decision-memo (early) / drawer (late), 7 sidebar panels reading live server data (Path / Personality / Competitor / Leaderboard / AnnualReport / Political / Regulatory), CrisisModal with typewriter + severity escalation, GameOver cinematic with 14 reason-specific endings, AnnualReportModal at Q4, EraTransition overlay on era change. Bundle ~107 KB gzipped. Original React frontend archived to `Archive/Legacy_Frontend_React_20260523/`.

**What's done**: 7 eras, 16 divisions, 24 CEOs, SFC economics (NIM, credit standards, duration risk, Minsky dynamics, Basel RWA), regulatory gates, path dependence, crisis arcs, AI competitors, political engine, climate/AI endgame, historical calibration from FRED data, save/load (9 roundtrip tests), personality sensitivity analysis (11 axes), diverse failure modes (7 death types), distribution launcher.

**What's next**: Audio assets (era-appropriate ambient + stingers), itch.io upload.

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

---

*Last Updated: May 23, 2026 (Svelte 5 frontend rebuild)*
*Status: Engine ~97% Complete · Frontend rebuilt*
