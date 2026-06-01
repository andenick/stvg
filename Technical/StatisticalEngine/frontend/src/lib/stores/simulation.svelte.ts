/*
 * Simulation store — single source of truth for live game state.
 *
 * Backed by Svelte 5 runes. Updated by WebSocket events (market_tick,
 * quarter_boundary, game_over, state, simulation_state) and by REST
 * responses on initial connect. Components read fields via `sim.*`.
 *
 * Price history is kept as a per-market ring buffer (504 trading days =
 * ~2 game years) so charts can hydrate without server round-trips.
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
  PendingDecision,
  Progression,
  ActionPoints,
  Regime,
  GameEndState,
  AICEOState,
  ClimateState,
  Deal,
  HiringCandidate,
} from '../types/server';

const PRICE_HISTORY_LEN = 504; // ~2 years of trading days

export interface PricePoint { time: number; value: number; }
export interface MarketSeries {
  id: string;
  history: PricePoint[];        // ring buffer, oldest first
  lastPrice: number;
  lastReturn: number;
}

export interface CapitalPoint { time: number; value: number; }

function pushRing(buf: PricePoint[], pt: PricePoint, max = PRICE_HISTORY_LEN) {
  buf.push(pt);
  if (buf.length > max) buf.splice(0, buf.length - max);
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

  // Decisions & crises
  pendingDecisions = $state<PendingDecision[]>([]);
  activeCrises = $state<CrisisArc[]>([]);
  messages = $state<string[]>([]);
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
  hiringPool = $state<HiringCandidate[]>([]);

  // Game end
  gameEnd = $state<GameEndState | null>(null);

  /** Replace the whole snapshot from a PlayerView (initial REST + state msgs). */
  applyPlayerView(view: PlayerView) {
    this.bank = view.bank ?? null;
    if (view.bank) {
      const cap = view.bank.capital ?? 0;
      this.capitalDelta = cap - this.bankCapital;
      this.lastCapital = this.bankCapital;
      this.bankCapital = cap;
      this.bankAssets = view.bank.totalAssets ?? 0;
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
    this.hiringPool = view.hiringPool ?? [];
    this.era = view.era ?? null;
    this.year = view.state?.currentYear ?? this.year;
    this.quarter = view.state?.currentQuarter ?? this.quarter;
    this.gameEnd = view.gameEnd ?? null;
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
    pushRing(this.capitalHistory, { time: p.totalDay, value: p.bankCapital });

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
    this.capitalHistory = [];
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
    this.hiringPool = [];
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
