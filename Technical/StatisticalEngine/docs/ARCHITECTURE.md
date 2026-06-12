# STVG Architecture

## Layer Diagram

```
┌──────────────────────────────────────────────────────────────┐
│  Frontend: Svelte 5 (runes) + Vite + TypeScript, 38 .svelte  │
│  TopBar + TabShell → Economy / Hire / My Bank / Financials   │
│  CharacterCard portrait overlay; macro + market charts;      │
│  DealBoard rail; telemetry v2 batches to /api/telemetry.     │
│  Runes stores, WebSocket auto-reconnect, builds to static/   │
└──────────────────────┬───────────────────────────────────────┘
                       │ REST (40 routes) + WebSocket (Crow)
┌──────────────────────┴───────────────────────────────────────┐
│  API Layer: api/WebSocketServer.h (~860 lines)               │
│  40 CROW_ROUTE routes + WebSocket broadcast + CORS + static, │
│  /assets/portraits/ static route, /macro-history, /trade,    │
│  /api/telemetry (batched events → telemetry/session_*.jsonl) │
└──────────────────────┬───────────────────────────────────────┘
                       │
┌──────────────────────┴───────────────────────────────────────┐
│  Game Orchestrator: core/QuarterlyTurnManager.h (~754 lines) │
│  + QuarterlyPhases.h (~806 lines, out-of-line phase methods) │
│  + EndgameResolver.h (~215 lines, game-end + doom resolution)│
│  6-phase quarterly turn loop, delegates to handlers below    │
├──────────────────────────────────────────────────────────────┤
│  Game Handlers (game/ namespace — extracted from QTM):       │
│  ├── DecisionHandler.h     (decision submission + parsing)   │
│  ├── CEOAbilityHandler.h   (special ability dispatch)        │
│  ├── CrisisHandler.h       (crisis response logic)           │
│  └── AnnualReportBuilder.h (annual report + accumulator)     │
├──────────────────────────────────────────────────────────────┤
│  Core Types (extracted from QTM for independent use):        │
│  ├── GameTypes.h      (TurnPhase, AnnualReport, GameEnd*,    │
│  │                     QuarterlyReport, CustomEconomyHook)   │
│  ├── Division.h       (DivisionType, Division, HeadHonesty)  │
│  ├── ActionPoints.h   (ActionPointBudget)                    │
│  └── BankConfig.h     (BankConfig, starting position factory)│
├──────────────────────────────────────────────────────────────┤
│  Game Systems:                                               │
│  ├── EventEngine.h          (weighted event pool, 70+ events)│
│  ├── HistoricalEventLoader.h (JSON → era-filtered, 111 events│
│  ├── DecisionEngine.h       (7 types + advisor biases)       │
│  ├── CharacterEngine.h      (5 advisors, bias-filtered recs) │
│  ├── CharacterGenerator.h   (RPG employees, 10 stats, 53    │
│  │                           traits, era-appropriate names)  │
│  ├── CrisisEngine.h         (5 types, multi-quarter arcs)   │
│  ├── ConsequenceTracker.h   (delayed decision outcomes)      │
│  ├── ScoringEngine.h        (score, XP, 40 levels)          │
│  ├── DealPortfolio.h        (JSON templates + procedural,   │
│  │                           12 division-specific deal types)│
│  └── NarrativeEngine.h      (reactive prose generation)     │
├──────────────────────────────────────────────────────────────┤
│  Progression & Strategy:                                     │
│  ├── EraEngine.h            (7 eras, division/market gating) │
│  ├── RegulatoryEngine.h     (7 regulations, enact/repeal)   │
│  ├── PathEngine.h           (5-axis strategic DNA)           │
│  ├── ReputationLens.h       (fortress/gunslinger/global tags│
│  │                           tilt pools, deal risk, pricing) │
│  ├── ArchetypeRegistry.h    (archetypes.json: 8 families +  │
│  │                           33 specs → β/σ P&L distribution)│
│  ├── CEOProfile.h           (24 CEOs, special abilities)     │
│  ├── PersonalityProfile.h   (11-axis personality model)      │
│  └── PoliticalEngine.h      (lobbying, political capital)    │
├──────────────────────────────────────────────────────────────┤
│  Competition & Endgame:                                      │
│  ├── CompetitorEngine.h     (6 rival AI banks, leaderboard) │
│  ├── AICEOEngine.h          (AI threat, singularity)        │
│  └── ClimateEngine.h        (temperature, tipping points)   │
├──────────────────────────────────────────────────────────────┤
│  Simulation:                                                 │
│  ├── EconomicEngine.h       (7 OU indicators, gdpLevel,     │
│  │                           creditImpulse)                  │
│  ├── core/MacroHistory.h    (saved macro series, base 100   │
│  │                           @1945 → GET /macro-history)     │
│  ├── MarketSimulator.h      (6 correlated GARCH markets)    │
│  ├── PersonalBook.h         (CEO off-balance-sheet trading  │
│  │                           account, SFC-safe → /trade)     │
│  └── TraderAI.h             (personality-driven positions)   │
├──────────────────────────────────────────────────────────────┤
│  Math:                                                       │
│  ├── RandomEngine.h         (thread-safe deterministic RNG)  │
│  ├── Distributions.h        (Normal, Student-t, Mixture)     │
│  ├── GARCH.h                (GJR-GARCH(1,1) with leverage)  │
│  ├── JumpDiffusion.h        (Merton crash events)           │
│  └── CorrelationEngine.h    (Cholesky dynamic correlations) │
├──────────────────────────────────────────────────────────────┤
│  Data:                                                       │
│  ├── core/GameConfig.h      (70+ configurable parameters)   │
│  ├── core/BankState.h       (Bank struct + SFC balance sheet)│
│  ├── core/Types.h           (shared enums and structs)       │
│  └── simulation/RiskReturnProfile.h (standardized risk/return│
├──────────────────────────────────────────────────────────────┤
│  Autoplay:                                                   │
│  ├── autoplay/BotStrategy.h (7 base + 5 era-aware bots)     │
│  ├── autoplay/GameRunner.h  (batch game execution)           │
│  ├── autoplay/EraAwareBots.h (Historical, Speedrun, etc.)   │
│  ├── autoplay/Telemetry.h   (per-game CSV output)           │
│  └── autoplay/SensitivityAnalysis.h (11-axis sweep)         │
└──────────────────────────────────────────────────────────────┘
```

**Total: 59 header files across 6 namespaces (core 25, game 4, math 5, simulation 19, api 1, autoplay 5). The STAR_02 overhaul (2026-06-12) added MacroHistory, ArchetypeRegistry, ReputationLens (core) and PersonalBook (simulation).**

## Game Lifecycle

```
1. Splash Screen
   ├── Fetch /api/scenarios → 5 scenario cards
   ├── Fetch /api/ceos → 5 CEO profiles
   ├── Fetch /api/starting-positions → 5 positions
   └── Player selects scenario + CEO + starting position

2. Game Creation
   ├── POST /api/game/new → creates QuarterlyTurnManager
   ├── Config: seed, startYear, quartersPerGame, ceoId, startingPosition
   ├── BankConfig from startingPosition (capital, employees, divisions)
   ├── CEO overrides applied (capital, reputation, multipliers)
   └── WebSocket subscription for real-time updates

3. Quarterly Loop (6 phases × 380 quarters max)
   ├── See "Data Flow Per Quarter" below
   └── Annual report modal at Q1 of each year

4. Game End
   ├── Win/Loss condition detected (see below)
   ├── GameEndState with reason, narrative, stats
   └── Victory/defeat screen with "Play Again"
```

## Win/Loss Conditions (GameEndReason)

**Loss conditions:**
- `Bankrupt` — Capital <= 0
- `RegulatorySeizure` — Capital ratio < 2%
- `ReputationCollapse` — Reputation <= 5
- `BoardFired` — 4 consecutive poor quarters or 8 mediocre quarters
- `LiquidityCrisis` — Deposit-to-asset ratio < 0.4
- `FraudScandal` — Probabilistic when hidden risk > 3x capital

**Win conditions:**
- `GrowthTarget` — Total assets >= $2T
- `LegendStatus` — Level >= 40
- `SurvivorWin` — Survived all quarters with avg score > 35

**Endgame conditions:**
- `AISingularity` — AI CEO captures >60% market at efficiency >5
- `ClimateCollapse` — 5 climate tipping points reached

**Endgame resolution paths** (from EndgameResolver.h):
- `EndgameReconstruction` — Doom meter forces systemic rebuild
- `EndgameFortress` — Player builds resilient institution
- `EndgameManagedDecline` — Graceful wind-down
- `EndgameTransformation` — AI partnership transforms banking
- `EndgameAbolition` — Voluntary self-dissolution
- `EndgameCollapse` — Catastrophic failure from doom meters

## Era System (7 Eras)

| Era | Years | Name | Key Unlocks |
|-----|-------|------|-------------|
| 1 | 1945-1958 | Post-War Stability | Commercial Lending, Mortgage Lending |
| 2 | 1958-1971 | Expansion & Innovation | Trust & Custody, Credit Cards |
| 3 | 1971-1985 | Volatility & Deregulation | International Banking, Trading Desk, EUR/USD |
| 4 | 1985-2000 | Big Bang & Excess | Asset Management, IB, Private Equity, Derivatives |
| 5 | 2000-2010 | Shadow Banking & Hubris | Restructuring, Securitization, Venture Capital |
| 6 | 2010-2025 | Crisis & Reform | Wealth Management, Fintech, Bitcoin |
| 7 | 2025-2040 | Modern & Endgame | Crypto Custody, AI CEO threat, Climate decisions |

Each era defines: available divisions, tradeable instruments, economy OU targets, GARCH regime, initial economy state, and scripted historical events.

## Data Flow Per Quarter

```
1. QuarterStart
   ├── EraEngine: check era transition, gate divisions/markets
   ├── RegulatoryEngine: check regulation enact/repeal dates
   ├── HistoricalEventLoader: filter events by game year
   ├── EventEngine: weight events by bank state, draw 4-5
   │     (draws sign-weighted to engine macro direction; market
   │      coupling applied when eventMarketCoupling is ON)
   ├── DecisionEngine: generate decision options from events
   │   ├── Standard decisions (autonomy, capital, personnel, deals, investigation)
   │   ├── Climate decisions (annual, 2020+)
   │   ├── AI counter-decisions (annual, post-emergence)
   │   └── Competitor acquisition offers (on rival failure)
   ├── CharacterEngine: add advisor recommendations + messages
   ├── DealPortfolio: generate new deal opportunities (JSON templates + procedural)
   ├── CompetitorEngine: simulate rival bank quarters
   └── Output: events, decisions, messages, deals → REST/WebSocket

2. DecisionPhase (player input via REST API)
   ├── Player submits decisions (POST /api/game/:id/decision/:did)
   ├── Action points consumed (5 AP per quarter)
   ├── ConsequenceTracker: seed delayed consequences (1-4 quarters)
   ├── PoliticalEngine: process lobbying decisions, track political capital
   └── Output: resolved decisions, remaining AP

3. Simulation (63 daily ticks)
   ├── EconomicEngine: OU processes update 7 indicators (gdpLevel, creditImpulse)
   ├── MarketSimulator: GARCH + correlations + jumps update 6 markets
   ├── MacroHistory: append macro series points (GDP/rate/credit), saved + served via /macro-history
   ├── TraderAI: positions generate P&L, hidden positions accumulate
   ├── PersonalBook: CEO off-balance-sheet trades mark-to-market (BUY/SELL via /trade)
   └── Output: per-tick `econ` block (incl. economyRevenue/economyProfit) + market updates via WebSocket

4. QuarterEnd
   ├── ConsequenceTracker: resolve delayed consequences
   ├── Division revenues computed via ArchetypeRegistry P&L distribution:
   │     expected · clamp(1 + Σwβ·x, .4, 1.8) · (1 + σZ), one RNG draw per
   │     division per quarter (lockstep-preserved; gated by archetypePnlVariance)
   ├── Division costs computed (honest salary via SALARY_COST_SCALE + infra + compliance)
   ├── ReputationLens: recompute bank reputation tags (tilt pools, deal risk, poach pricing)
   ├── Bank.endQuarter(): aggregate financials, hidden risk accumulation
   ├── CrisisEngine: escalate active crises, check new triggers
   ├── AICEOEngine: update AI efficiency, check singularity
   ├── ClimateEngine: update temperature, check tipping points
   ├── PathEngine: update 5-axis strategic DNA
   ├── ScoringEngine: compute quarterly score + XP + progression
   ├── NarrativeEngine: generate quarter summary + headlines
   └── Output: QuarterlyReport, annual report (at Q4)

5. CrisisResponse (if triggered)
   ├── Crisis event presented with severity and description
   ├── Player chooses response (Aggressive / Measured / Gamble)
   └── Crisis severity adjusted, financial impact applied

6. QuarterComplete → loops to QuarterStart
```

## Information Filtering

The game implements organizational complexity through two JSON views:

- **toPlayerJson()** — Filtered by visibility (5-100% based on bank size + CEO bonus). Division data is fog-obscured at low visibility. Hidden risk concealed unless audited. Division head reports filtered by honesty level.

- **toGodJson()** — Full truth view with all hidden risk, true division performance, and unfiltered data. Used for post-mortem analysis and autoplay telemetry.

## Key Design Patterns

- **Header-only library**: All engine code in .h files (template-heavy C++20)
- **JSON-driven data**: Events, config, calibration, scenarios, deal templates in editable JSON files
- **Standardized RiskReturnProfile**: One struct for all risk/reward calculations across events, deals, and investments
- **Dynamic divisions**: `std::vector<Division>` allows 1-16 business lines, era-gated
- **Deterministic replay**: Master seed → all RNG derived deterministically
- **Autoplay testing**: Separate executable (12 bot strategies) runs thousands of games for balance analysis
- **Starting positions**: 5 configurations (Commercial, Trading, IB, Community, Universal) with factory methods
- **CEO abilities**: Special powers with cooldowns, consuming AP or political capital
- **Era-gating**: Divisions, markets, instruments, and deal templates locked to historical periods
- **Archetype P&L distribution**: hire archetypes (8 families + 33 specializations, `data/archetypes/archetypes.json` via ArchetypeRegistry) drive per-division revenue dispersion through macro betas + idiosyncratic σ — the owner's "core equation"
- **Portrait / credibility layer (frontend)**: CharacterCard overlay shows recurring bankers + presidents; the credibility line-assembler encodes honesty in language (honest = short/specific/falsifiable; crony = padded/vague/urgent) so length itself is a tell. DiceBear placeholders behind `portraitFor()` (PNG manifest can override later) served from `/assets/portraits/`
- **Telemetry pipeline**: frontend batches 42 event types (incl. dwell + hover) → `POST /api/telemetry` → `telemetry/session_*.jsonl`; `_playtest/analyze_session.py` renders a session report for tuning
