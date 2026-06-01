# STVG Statistical Engine

Banking CEO Simulation (1945-2040) — a C++20 header-only game engine with a browser-based web demo. 39 headers, 253 tests, 29 REST endpoints, 12 bot strategies, 7 historical eras, 16 bank divisions, 111 historical events.

## Quick Start

```bash
# Build
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build --parallel 4

# Run tests (253 tests: 232 pass, 21 skipped)
./build/stvg_tests.exe --gtest_brief=1

# Run server + web demo
./build/stvg_server.exe 8080 static
# Open http://localhost:8080/ in browser

# Autoplay validation
./build/stvg_autoplay.exe              # 5 strategies x 200 games
./build/stvg_autoplay.exe --full-matrix # 12 strategies x 10 seeds
```

## Architecture

```
Math Layer (pure math, no game logic)
  RandomEngine, Distributions, GJR-GARCH, JumpDiffusion, CorrelationEngine
      |
Simulation Layer (game simulation systems)
  EconomicEngine, MarketSimulator, TraderAI, EventEngine,
  DecisionEngine, CharacterEngine, CrisisEngine, StorytellerAI
      |
Core Layer (game infrastructure)
  GameConfig, BankState, QuarterlyTurnManager, ConsequenceTracker,
  ScoringEngine, SimulationClock, Types
      |
API Layer (server)
  WebSocketServer (Crow REST + WebSocket)
      |
Frontend (browser)
  static/index.html (Alpine.js + Lightweight Charts)
```

## Game Loop

Each game spans **380 quarters (95 years, 1945-2040)** across 7 historical eras. Each quarter has 6 phases:

1. **QuarterStart** — Events generated, advisor messages delivered
2. **DecisionPhase** — Player makes 3-7 decisions using action points
3. **Simulation** — 63 trading days simulated (markets, traders, economy)
4. **QuarterEnd** — Results calculated, consequences from prior decisions resolve
5. **CrisisResponse** — If crisis triggered, player must respond immediately
6. **QuarterComplete** — Ready for next quarter

## REST API (29 endpoints)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Web demo |
| GET | `/api/health` | Server status |
| POST | `/api/game/new` | Create new game (body: `{"seed":42}`) |
| GET | `/api/game/:id/state` | Full game state |
| POST | `/api/game/:id/advance` | Advance to next turn phase |
| GET | `/api/game/:id/decisions` | Pending decisions |
| POST | `/api/game/:id/decision/:did` | Submit decision (`{"optionId":"..."}`) |
| GET | `/api/game/:id/messages` | Advisor messages |
| GET | `/api/game/:id/report` | Quarterly report |
| GET | `/api/game/:id/annual-report` | Annual report |
| GET | `/api/game/:id/bank` | Bank state |
| GET | `/api/game/:id/traders` | Trader roster |
| GET | `/api/game/:id/events` | Quarter events |
| GET | `/api/game/:id/competitors` | AI competitor standings |
| GET | `/api/game/:id/political` | Political engine state |
| GET | `/api/game/:id/climate` | Climate engine state |
| GET | `/api/game/:id/ai-ceo` | AI CEO threat state |
| GET | `/api/game/:id/path` | Path dependence (5-axis) |
| GET | `/api/game/:id/era` | Current era info |
| GET | `/api/game/:id/god` | Full game state JSON (debug) |
| GET | `/api/ceos` | Available CEO profiles |
| POST | `/api/game/:id/special-ability` | Use CEO special ability |
| GET | `/api/scenarios` | Available scenarios |
| GET | `/api/config` | Current config |
| POST | `/api/config` | Update config (partial merge) |
| POST | `/api/config/preset/:name` | Load preset (sandbox/easy/normal/hard/crisis) |
| GET | `/api/config/validate` | Validate config |

WebSocket: `ws://localhost:8080/ws` — subscribe to game updates.

## Configuration

All 70+ game parameters are adjustable via `data/config/game_config.json` or REST API.

**Difficulty presets:** Sandbox, Easy, Normal, Hard, Crisis.

**Scenarios:** Tutorial, Crisis Mode, Full Game (95yr), Speedrun, AI Endgame.

Key adjustable values: quarter duration (10-252 days), simulation speed, starting capital, crisis probabilities, trader personality distribution, scoring weights, visibility decay, era parameters, OU economic process calibration, and more. Historical calibration loaded from `data/calibration/` (7 eras x 7 OU parameters from FRED/Volcker data).

## Key Systems

### Math Layer
| System | Description |
|--------|-------------|
| **GJR-GARCH** | Asymmetric volatility clustering with leverage effect |
| **Jump Diffusion** | Sudden market crash events (~2/year, 6/year in crisis) |
| **DCC Correlation** | Cholesky-decomposed multi-asset returns, dynamic crisis correlations |
| **Ornstein-Uhlenbeck Economy** | 7 mean-reverting indicators (Fed Funds, GDP, VIX, etc.) |

### Game Systems
| System | Description |
|--------|-------------|
| **EraEngine** | 7 historical eras (1945-2040), division/market/instrument gating |
| **RegulatoryEngine** | 7 regulations (Glass-Steagall, Bretton Woods, Basel I-III, Dodd-Frank, Reg Q) |
| **PathEngine** | 5-axis path dependence (risk culture, international, tech, political, innovation) |
| **CEOProfile** | 5 playable CEOs with unique abilities and modifiers |
| **PoliticalEngine** | Lobbying, political capital, era modifiers |
| **CompetitorEngine** | 6 persistent AI rival banks (Civilization-style) |
| **AICEOEngine** | AI singularity endgame threat (2030-2040) |
| **ClimateEngine** | Climate risk, temperature tracking, tipping points |
| **Trader AI** | 4 personalities (Conservative/Aggressive/Rogue/Analytical) with hidden positions |
| **HistoricalEventLoader** | 111 era-filtered events from JSON |
| **Weighted Event Pool** | Events drawn based on bank state (Reigns-style) |
| **Character Advisors** | 5 advisors with conflicting biases and quarterly briefings |
| **CharacterGenerator** | RPG employees (10 stats, 53 traits, 8 archetypes) |
| **NarrativeEngine** | Reactive prose generation for quarter summaries |
| **DealPortfolio** | JSON deal templates + procedural deals, 12 division-specific types |
| **Consequence Tracker** | Decisions create delayed consequences (1-3 quarters later) |
| **Crisis Arcs** | Multi-quarter crises with escalation and three response types |
| **Organizational Complexity** | Visibility and control decay exponentially with bank size |

## File Structure

```
StatisticalEngine/
├── include/stvg/
│   ├── core/        12 headers (config, types, bank, scoring, turn loop, consequences,
│   │                 CEO profiles, era engine, regulatory, path, employee, clock)
│   ├── math/         5 headers (RNG, distributions, GARCH, jumps, DCC correlations)
│   ├── simulation/  15 headers (markets, economy, traders, events, decisions,
│   │                 characters, crises, narrative, historical events, competitors,
│   │                 AI CEO, climate, political, deals, character generator)
│   ├── autoplay/     4 headers (game runner, bot strategy, era-aware bots, telemetry)
│   └── api/          1 header (REST + WebSocket server, ~500 lines)
├── src/              main.cpp (server) + autoplay_main.cpp + placeholder.cpp
├── tests/           25 test files (253 tests across core, math, simulation, autoplay)
├── data/            config JSON, calibration data, scenarios, deal templates, events
├── static/          index.html (Alpine.js web demo, ~1700 lines)
└── docs/            ARCHITECTURE.md, PROJECT_HISTORY.md, FILE_CATALOG.md
```

## Dependencies

Fetched automatically via CMake FetchContent:
- [Crow](https://github.com/CrowCpp/Crow) — REST + WebSocket server
- [nlohmann/json](https://github.com/nlohmann/json) — JSON serialization
- [spdlog](https://github.com/gabime/spdlog) — Logging
- [GoogleTest](https://github.com/google/googletest) — Unit testing
- [Asio](https://github.com/chriskohlhoff/asio) — Networking (standalone, no Boost)
