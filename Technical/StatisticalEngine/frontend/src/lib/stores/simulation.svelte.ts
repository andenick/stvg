/*
 * Simulation store — single source of truth for live game state.
 *
 * Backed by Svelte 5 runes. Updated by WebSocket events (market_tick,
 * quarter_boundary, game_over, state, simulation_state) and by REST
 * responses on initial connect. Components read fields via `sim.*`.
 *
 * Price history is kept as a per-market ring buffer sized to the whole game
 * (~95 years × 252 trading days) so the hero chart's 1Y/5Y/10Y/20Y/MAX
 * time-scale toggle can slice real history without server round-trips. The old
 * 504-day (~2y) cap silently discarded everything older than the MAX window.
 */

import type {
  PlayerView,
  Era,
  Bank,
  Competitor,
  LeaderboardEntry,
  PathState,
  CEOPersonality,
  AnnualReport,
  PoliticalState,
  RegulatoryState,
  GameEvent,
  CrisisArc,
  SimulationDigestEntry,
  FeedMessage,
  PendingDecision,
  Progression,
  ActionPoints,
  Regime,
  GameEndState,
  AICEOState,
  ClimateState,
  Deal,
  DealOutcome,
  LoanBookStats,
  TradeBook,
  PersonalPosition,
  HiringCandidate,
  PoachOffer,
  EconTick,
  EconomicIndicators,
  MacroSnapshot,
} from '../types/server';
import { MACRO_IDS, type MacroId } from '../charts/macro-meta';

// Whole-game length: ~95 game-years × 252 trading days, plus headroom. The hero
// chart's MAX time-scale needs the full series; smaller windows slice this.
const PRICE_HISTORY_LEN = 252 * 96; // ~24,000 trading days

export interface PricePoint { time: number; value: number; }
export interface MarketSeries {
  id: string;
  history: PricePoint[];        // ring buffer, oldest first
  lastPrice: number;
  lastReturn: number;
}

export interface CapitalPoint { time: number; value: number; }

export interface MacroSeries {
  id: MacroId;
  history: PricePoint[];   // ring buffer, oldest first; time = totalDay
  last: number;
}

function pushRing(buf: PricePoint[], pt: PricePoint, max = PRICE_HISTORY_LEN) {
  buf.push(pt);
  if (buf.length > max) buf.splice(0, buf.length - max);
}

/**
 * Map a macro id to its raw value out of the per-tick `econ` block. Returns
 * `undefined` when the field isn't present in this stream shape so we skip it
 * rather than push a zero (which would dent the axis on quarterly-only fields).
 */
function econTickValue(id: MacroId, e: EconTick): number | undefined {
  switch (id) {
    case 'GDP': return e.gdpLevel;
    case 'UNEMP': return e.unemployment;
    case 'FEDFUNDS': return e.fedFundsRate;
    case 'CPI': return e.cpiInflation;
    case 'SPREAD': return e.creditSpread;
    default: return undefined;   // TREAS10Y / GDPGROWTH only on quarterly history
  }
}

/** Map a macro id to its value out of a full EconomicIndicators snapshot. */
function econFullValue(id: MacroId, e: EconomicIndicators): number | undefined {
  switch (id) {
    case 'GDP': return e.gdpLevel;
    case 'UNEMP': return e.unemployment;
    case 'FEDFUNDS': return e.fedFundsRate;
    case 'CPI': return e.cpiInflation;
    case 'SPREAD': return e.creditSpread;
    case 'TREAS10Y': return e.treasuryYield10Y;
    case 'GDPGROWTH': return e.gdpGrowth;
    default: return undefined;
  }
}

class SimulationStore {
  // Connection
  connected = $state(false);
  reconnecting = $state(false);

  // Identity
  gameId = $state<string | null>(null);

  // Clock
  day = $state(0);              // day within quarter (1..63)
  totalDay = $state(0);         // since start of game
  year = $state(1945);
  quarter = $state(1);

  // Simulation control
  paused = $state(true);
  speed = $state<1 | 2 | 4 | 8>(1);
  regime = $state<Regime>('normal');

  // Bank
  bank = $state<Bank | null>(null);
  bankCapital = $state(0);
  bankAssets = $state(0);
  capitalDelta = $state(0);
  lastCapital = $state(0);
  actionPoints = $state<ActionPoints | null>(null);
  progression = $state<Progression | null>(null);

  // Era
  era = $state<Era | null>(null);

  // Markets — keyed by market id
  markets = $state<Record<string, MarketSeries>>({});
  capitalHistory = $state<CapitalPoint[]>([]);

  // Macro series — keyed by MacroId (GDP, UNEMP, FEDFUNDS, CPI, SPREAD, …). Per-day
  // ring buffers fed by the `econ` block on each market_tick, full-game length like
  // markets, and backfilled with quarterly points from /macro-history on load (P3).
  macro = $state<Record<string, MacroSeries>>({});
  // The most recent full macro snapshot (from PlayerView.state.economics) — carries
  // the quarterly-only fields (treasuryYield10Y, gdpGrowth, vix, creditImpulse).
  economics = $state<EconomicIndicators | null>(null);

  // Live balance sheet (streamed each tick) + history for the combined chart
  bankLoans = $state(0);
  bankSecurities = $state(0);
  bankReserves = $state(0);
  bankDeposits = $state(0);
  bankInterbank = $state(0);
  loanHistory = $state<CapitalPoint[]>([]);
  assetHistory = $state<CapitalPoint[]>([]);
  // Funding-mix history (P2 Financials tab): deposits vs wholesale/interbank,
  // plus securities. Buffered here so the funding-mix card can chart what it
  // displays (the raw fields stream each tick but were never kept as series).
  depositHistory = $state<CapitalPoint[]>([]);
  interbankHistory = $state<CapitalPoint[]>([]);
  securitiesHistory = $state<CapitalPoint[]>([]);

  // Decisions & crises
  pendingDecisions = $state<PendingDecision[]>([]);
  activeCrises = $state<CrisisArc[]>([]);
  messages = $state<FeedMessage[]>([]);
  events = $state<GameEvent[]>([]);
  simulationDigest = $state<SimulationDigestEntry[]>([]);

  // Strategic / meta state
  path = $state<PathState | null>(null);
  ceoPersonality = $state<CEOPersonality | null>(null);
  ceoId = $state<string | null>(null);
  ceoName = $state<string | null>(null);
  competitors = $state<Competitor[]>([]);
  leaderboard = $state<LeaderboardEntry[]>([]);
  political = $state<PoliticalState | null>(null);
  regulatory = $state<RegulatoryState | null>(null);
  aiCeo = $state<AICEOState | null>(null);
  climate = $state<ClimateState | null>(null);
  annualReport = $state<AnnualReport | null>(null);
  availableDeals = $state<Deal[]>([]);
  activeDeals = $state<Deal[]>([]);
  // P7 pacing: wall-clock (ms) of the most recent fresh loan-card arrival. The
  // auto-continue scheduler reads this to extend the boundary dwell modestly when
  // a deal just popped in, so the player gets a beat to read it. Not reactive UI.
  lastDealArrivalAt = 0;
  // STAR_02 P7: loan-outcome feed (most-recent-last, capped) + cumulative
  // scoreboard. The Loan Book rail Book strip + Financials card read these.
  dealOutcomes = $state<DealOutcome[]>([]);
  loanBookStats = $state<LoanBookStats | null>(null);
  // STAR_02 P7: the CEO's personal trading account. tradeBook is the full
  // snapshot (positions + cost basis); value/pnl mirror the live tick stream so
  // the TopBar Book line + position pills move with the tape between snapshots.
  tradeBook = $state<TradeBook | null>(null);
  personalBookValue = $state(0);
  personalBookPnl = $state(0);
  hiringPool = $state<HiringCandidate[]>([]);
  // STAR_02 P6: rival-division poach offers + the ReputationLens tag.
  poachOffers = $state<PoachOffer[]>([]);
  reputationTag = $state<string | null>(null);

  // Game end
  gameEnd = $state<GameEndState | null>(null);

  /** Replace the whole snapshot from a PlayerView (initial REST + state msgs). */
  applyPlayerView(view: PlayerView) {
    this.bank = view.bank ?? null;
    if (view.bank) {
      const b = view.bank as Record<string, number | undefined>;
      const cap = view.bank.capital ?? 0;
      this.capitalDelta = cap - this.bankCapital;
      this.lastCapital = this.bankCapital;
      this.bankCapital = cap;
      this.bankAssets = view.bank.totalAssets ?? 0;
      // server may use `loans`/`totalLoans` etc. — accept either
      this.bankLoans = b.loans ?? b.totalLoans ?? this.bankLoans;
      this.bankSecurities = b.securities ?? this.bankSecurities;
      this.bankReserves = b.reserves ?? this.bankReserves;
      this.bankDeposits = b.totalDeposits ?? b.deposits ?? this.bankDeposits;
      this.bankInterbank = b.interbankBorrowing ?? this.bankInterbank;
    }
    this.actionPoints = view.actionPoints ?? null;
    this.progression = view.progression ?? null;
    this.pendingDecisions = view.pendingDecisions ?? [];
    this.messages = view.messages ?? [];
    this.events = view.events ?? [];
    this.activeCrises = view.activeCrises ?? [];
    this.simulationDigest = view.simulationDigest ?? [];
    this.path = view.path ?? null;
    if (view.ceo) {
      this.ceoPersonality = view.ceo.personality ?? null;
      this.ceoId = view.ceo.id ?? null;
      this.ceoName = view.ceo.name ?? null;
    }
    this.competitors = view.competitors ?? [];
    this.leaderboard = view.leaderboard ?? [];
    this.political = view.political ?? null;
    this.regulatory = view.regulatory ?? null;
    this.aiCeo = view.aiCeo ?? null;
    this.climate = view.climate ?? null;
    this.annualReport = view.annualReport ?? null;
    this.availableDeals = view.availableDeals ?? [];
    this.activeDeals = view.activeDeals ?? [];
    // P7: loan-outcome feed + scoreboard + personal trade book snapshot.
    if (view.dealOutcomes) this.dealOutcomes = view.dealOutcomes;
    if (view.loanBookStats) this.loanBookStats = view.loanBookStats;
    if (view.tradeBook) {
      this.tradeBook = view.tradeBook;
      this.personalBookValue = view.tradeBook.value;
      this.personalBookPnl = view.tradeBook.pnl;
    }
    this.hiringPool = view.hiringPool ?? [];
    this.poachOffers = view.poachOffers ?? [];
    if (view.reputationTag != null) this.reputationTag = view.reputationTag;
    this.era = view.era ?? null;
    this.year = view.state?.currentYear ?? this.year;
    this.quarter = view.state?.currentQuarter ?? this.quarter;
    // P3.1: stop discarding state.economics. Keep the latest full macro snapshot
    // so the quarterly-only fields (10Y, gdpGrowth, vix) are available to charts.
    if (view.state?.economics) this.economics = view.state.economics;
    this.gameEnd = view.gameEnd ?? null;
  }

  /**
   * Direction of a macro series read straight from its ring buffer — last value
   * vs the value ~`lookback` trading days earlier (default ~1 quarter). Returns
   * 'flat' when there isn't enough history. NEVER fabricated; this is the only
   * source the NewsTicker glyphs and thumbnails use for up/down coloring.
   */
  macroDir(id: MacroId, lookback = 63): 'up' | 'down' | 'flat' {
    const h = this.macro[id]?.history;
    if (!h || h.length < 2) return 'flat';
    const last = h[h.length - 1];
    const cutoff = last.time - lookback;
    // Find the most recent point at/under the cutoff; fall back to the oldest.
    let prev = h[0];
    for (let i = h.length - 1; i >= 0; i--) {
      if (h[i].time <= cutoff) { prev = h[i]; break; }
    }
    const d = last.value - prev.value;
    const eps = Math.abs(prev.value) * 1e-4;
    return d > eps ? 'up' : d < -eps ? 'down' : 'flat';
  }

  /** Direction of a market series (1 quarter) from its ring buffer. */
  marketDir(id: string, lookback = 63): 'up' | 'down' | 'flat' {
    const h = this.markets[id]?.history;
    if (!h || h.length < 2) return 'flat';
    const last = h[h.length - 1];
    const cutoff = last.time - lookback;
    let prev = h[0];
    for (let i = h.length - 1; i >= 0; i--) {
      if (h[i].time <= cutoff) { prev = h[i]; break; }
    }
    const d = last.value - prev.value;
    const eps = Math.abs(prev.value) * 1e-4;
    return d > eps ? 'up' : d < -eps ? 'down' : 'flat';
  }

  /** P7: the open personal position in a market, or null. */
  positionFor(marketId: string): PersonalPosition | null {
    return this.tradeBook?.positions.find((p) => p.marketId === marketId) ?? null;
  }

  /** Append a value to a macro ring buffer, creating the series on first sight. */
  private pushMacro(id: MacroId, time: number, value: number) {
    const existing = this.macro[id];
    if (existing) {
      // Avoid duplicate x-values (a backfilled quarter then its first live tick
      // can share a totalDay); update-in-place keeps lightweight-charts happy.
      const h = existing.history;
      if (h.length && h[h.length - 1].time === time) {
        h[h.length - 1].value = value;
      } else {
        pushRing(h, { time, value });
      }
      existing.last = value;
    } else {
      this.macro[id] = { id, history: [{ time, value }], last: value };
    }
  }

  /**
   * Backfill macro ring buffers from GET /api/game/<id>/macro-history so charts
   * aren't empty after a reload/save-load. Quarter (year,quarter) is mapped to a
   * totalDay at the quarter's start so the points line up with the live stream.
   */
  hydrateMacroHistory(quarters: MacroSnapshot[], startYear = 1945) {
    for (const q of quarters) {
      // 252 trading days/year, 63/quarter — match chart-utils' time model.
      const totalDay = (q.year - startYear) * 252 + (q.quarter - 1) * 63;
      for (const id of MACRO_IDS) {
        const v = econFullValue(id, q.econ);
        if (v != null && Number.isFinite(v)) this.pushMacro(id, totalDay, v);
      }
      // Also seed market closes the same way, so a reloaded game's market charts
      // have history before the first live tick arrives.
      for (const [mid, px] of Object.entries(q.closes)) {
        if (!Number.isFinite(px)) continue;
        const existing = this.markets[mid];
        if (existing) {
          const h = existing.history;
          if (!(h.length && h[h.length - 1].time === totalDay)) {
            pushRing(h, { time: totalDay, value: px });
            existing.lastPrice = px;
          }
        } else {
          this.markets[mid] = {
            id: mid, history: [{ time: totalDay, value: px }], lastPrice: px, lastReturn: 0,
          };
        }
      }
      if (q.econ) this.economics = q.econ;
    }
  }

  /** Apply a market_tick payload — append to ring buffers, no full replace. */
  applyMarketTick(p: {
    day: number;
    year: number;
    quarter: number;
    totalDay: number;
    prices: Array<{ id: string; price: number; dailyReturn: number }>;
    bankCapital: number;
    bankAssets: number;
    bankLoans?: number;
    bankSecurities?: number;
    bankReserves?: number;
    bankDeposits?: number;
    bankInterbank?: number;
    availableDeals?: Deal[];
    hiringPool?: HiringCandidate[];
    poachOffers?: PoachOffer[];
    dealOutcomes?: DealOutcome[];
    loanBookStats?: LoanBookStats;
    personalBookValue?: number;
    personalBookPnl?: number;
    econ?: EconTick;
    regime: Regime;
  }) {
    this.day = p.day;
    this.totalDay = p.totalDay;
    this.year = p.year;
    this.quarter = p.quarter;
    this.regime = p.regime;

    const newDelta = p.bankCapital - this.bankCapital;
    this.capitalDelta = newDelta;
    this.lastCapital = this.bankCapital;
    this.bankCapital = p.bankCapital;
    this.bankAssets = p.bankAssets;
    if (p.bankLoans != null) this.bankLoans = p.bankLoans;
    if (p.bankSecurities != null) this.bankSecurities = p.bankSecurities;
    if (p.bankReserves != null) this.bankReserves = p.bankReserves;
    if (p.bankDeposits != null) this.bankDeposits = p.bankDeposits;
    if (p.bankInterbank != null) this.bankInterbank = p.bankInterbank;
    pushRing(this.capitalHistory, { time: p.totalDay, value: p.bankCapital });
    pushRing(this.loanHistory, { time: p.totalDay, value: this.bankLoans });
    pushRing(this.assetHistory, { time: p.totalDay, value: p.bankAssets });
    pushRing(this.depositHistory, { time: p.totalDay, value: this.bankDeposits });
    pushRing(this.interbankHistory, { time: p.totalDay, value: this.bankInterbank });
    pushRing(this.securitiesHistory, { time: p.totalDay, value: this.bankSecurities });
    // Live deal flow (trickle) — replace the available list as it changes.
    // P7 pacing: note a fresh arrival (list grew) so the scheduler can give a beat.
    if (p.availableDeals) {
      const prevIds = new Set(this.availableDeals.map((d) => d.id));
      if (p.availableDeals.some((d) => !prevIds.has(d.id))) {
        this.lastDealArrivalAt = Date.now();
      }
      this.availableDeals = p.availableDeals;
    }
    // STAR_02 P6: live hiring-pool + poach-offer streams (candidates/offers
    // arrive intra-quarter; HireTab diffs ids to animate arrivals).
    if (p.hiringPool) this.hiringPool = p.hiringPool;
    if (p.poachOffers) this.poachOffers = p.poachOffers;
    // P7: live loan-outcome feed + scoreboard, and the personal book's
    // mark-to-market value/P&L (against the ticking tape). The DealBoard diffs
    // dealOutcomes to fire outcome barks + bump the Book strip.
    if (p.dealOutcomes) this.dealOutcomes = p.dealOutcomes;
    if (p.loanBookStats) this.loanBookStats = p.loanBookStats;
    if (p.personalBookValue != null) this.personalBookValue = p.personalBookValue;
    if (p.personalBookPnl != null) this.personalBookPnl = p.personalBookPnl;

    // P3.2: macro lines move intraday like markets. Each `econ` field with a
    // mapping gets a point on its ring buffer at this totalDay.
    if (p.econ) {
      for (const id of MACRO_IDS) {
        const v = econTickValue(id, p.econ);
        if (v != null && Number.isFinite(v)) this.pushMacro(id, p.totalDay, v);
      }
    }

    for (const px of p.prices) {
      const existing = this.markets[px.id];
      if (existing) {
        pushRing(existing.history, { time: p.totalDay, value: px.price });
        existing.lastPrice = px.price;
        existing.lastReturn = px.dailyReturn;
      } else {
        this.markets[px.id] = {
          id: px.id,
          history: [{ time: p.totalDay, value: px.price }],
          lastPrice: px.price,
          lastReturn: px.dailyReturn,
        };
      }
    }
  }

  reset() {
    this.gameId = null;
    this.bank = null;
    this.bankCapital = 0;
    this.bankAssets = 0;
    this.capitalDelta = 0;
    this.markets = {};
    this.macro = {};
    this.economics = null;
    this.capitalHistory = [];
    this.loanHistory = [];
    this.assetHistory = [];
    this.depositHistory = [];
    this.interbankHistory = [];
    this.securitiesHistory = [];
    this.bankLoans = 0;
    this.bankSecurities = 0;
    this.bankReserves = 0;
    this.bankDeposits = 0;
    this.bankInterbank = 0;
    this.pendingDecisions = [];
    this.activeCrises = [];
    this.messages = [];
    this.events = [];
    this.simulationDigest = [];
    this.path = null;
    this.ceoPersonality = null;
    this.ceoId = null;
    this.ceoName = null;
    this.competitors = [];
    this.leaderboard = [];
    this.political = null;
    this.regulatory = null;
    this.aiCeo = null;
    this.climate = null;
    this.annualReport = null;
    this.availableDeals = [];
    this.activeDeals = [];
    this.dealOutcomes = [];
    this.loanBookStats = null;
    this.tradeBook = null;
    this.personalBookValue = 0;
    this.personalBookPnl = 0;
    this.hiringPool = [];
    this.poachOffers = [];
    this.reputationTag = null;
    this.gameEnd = null;
    this.year = 1945;
    this.quarter = 1;
    this.day = 0;
    this.totalDay = 0;
    this.paused = true;
    this.speed = 1;
    this.regime = 'normal';
  }
}

export const sim = new SimulationStore();
