/*
 * Mirrors QuarterlyTurnManager::toPlayerJson() and SimulationRunner WebSocket
 * payloads. The C++ schema is the single source of truth; if the engine adds
 * fields, add them here. Optional fields use `?` (server may omit them when
 * not present, e.g. ceo, gameEnd).
 */

// ── Enums / unions ──────────────────────────────────────────────────────

export type TurnPhase =
  | 'QuarterStart'
  | 'DecisionPhase'
  | 'Simulation'
  | 'CrisisResponse'
  | 'QuarterEnd'
  | 'QuarterComplete';

export type EraName =
  | 'Post-War Stability'
  | 'Expansion & Innovation'
  | 'Volatility & Deregulation'
  | 'Big Bang & Excess'
  | 'Shadow Banking & Hubris'
  | 'Crisis & Reform'
  | 'Modern & Endgame';

export type EraSlug =
  | 'post-war'
  | 'expansion'
  | 'volatility'
  | 'big-bang'
  | 'shadow'
  | 'crisis'
  | 'modern';

export type Regime = 'normal' | 'volatile' | 'stressed' | 'crisis' | string;

export type GameEndReason =
  | 'None'
  | 'Bankruptcy'
  | 'BoardFired'
  | 'RegulatorySeizure'
  | 'LiquidityCrisis'
  | 'FraudScandal'
  | 'AISingularity'
  | 'ClimateCatastrophe'
  | 'Reconstruction'
  | 'ShadowEmperor'
  | 'AIPartnership'
  | 'VoluntaryAbolition'
  | 'Catastrophe'
  | 'Fortress'
  | 'SurvivorWin'
  | string;

// ── Core entities ───────────────────────────────────────────────────────

export interface Division {
  id: string;
  type: string;
  name: string;
  capital: number;
  revenue: number;
  cost: number;
  netIncome: number;
  employees: number;
  riskWeightedAssets: number;
  hiddenRisk: number;          // 0..1 (player sees only revealed risk)
  revealedRisk: number;
  autonomy: number;
  morale: number;
  commentary?: string;
  [key: string]: unknown;
}

export interface Bank {
  name: string;
  capital: number;
  totalAssets: number;
  totalLoans: number;
  totalDeposits: number;
  reputation: number;
  divisions: Division[];
  netIncomeQuarterly?: number;
  netIncomeAnnual?: number;
  capitalRatio?: number;
  leverageRatio?: number;
  visibilityPct?: number;
  [key: string]: unknown;
}

export interface ActionPoints {
  total: number;
  spent: number;
}

export function apRemaining(ap: ActionPoints | null | undefined): number {
  if (!ap) return 0;
  return Math.max(0, (ap.total ?? 0) - (ap.spent ?? 0));
}

export interface Progression {
  level: number;
  levelName: string;
  xp: number;
  xpToNext: number;
  totalScore: number;
}

export interface PendingDecision {
  id: string;
  title: string;
  description: string;
  type?: string;
  urgency?: string | number;     // engine emits DecisionUrgency as enum
  actionPointCost: number;       // server uses actionPointCost, not apCost
  sourceEventId?: string;
  options: DecisionOption[];
  resolved?: boolean;
  expiresAt?: number;
}

export interface CharacterRecommendation {
  characterId?: string;
  characterName?: string;
  // engine emits capitalized "Support"/"Oppose"/"Neutral"
  recommendation: 'support' | 'oppose' | 'neutral' | 'Support' | 'Oppose' | 'Neutral' | string;
  reasoning?: string;
  argument?: string;     // engine field name for the advisor's reasoning
  confidence?: number;
}

export interface RiskImpactDelta {
  hiddenRisk?: number;
  revealedRisk?: number;
  reputationRisk?: number;
  regulatoryRisk?: number;
}

/**
 * The engine sends a structured financial impact (NOT a scalar):
 *   immediateCost  — one-off cost now
 *   recurringCost  — cost per quarter for `timelineQuarters`
 *   expectedRevenue — expected upside over the horizon
 * Net headline value = expectedRevenue - immediateCost.
 */
export interface FinancialImpact {
  immediateCost?: number;
  recurringCost?: number;
  expectedRevenue?: number;
  timelineQuarters?: number;
  [key: string]: unknown;
}

export interface DecisionOption {
  id: string;
  title: string;                 // option headline ("Approve loan", "Decline")
  description?: string;          // body copy
  successProbability?: number;   // 0..1
  financialImpact?: FinancialImpact | number;  // engine emits a struct; tolerate a legacy number
  riskImpact?: RiskImpactDelta;
  recommendations?: CharacterRecommendation[];
  longTermConsequences?: string[];
}

/**
 * Quarterly-brief / advisor message. The engine emits these as OBJECTS
 * (not strings): a named character speaks with a role, sentiment and tone.
 * Some legacy code paths may still surface plain strings, so consumers
 * should accept `string | CharacterMessage`.
 */
export interface CharacterMessage {
  characterId?: string;
  characterName?: string;
  content: string;
  role?: string;
  sentiment?: number;
  tone?: string;
  [key: string]: unknown;
}

export type FeedMessage = string | CharacterMessage;

export interface GameEvent {
  id: string;
  title: string;
  description: string;
  type?: string;
  severity?: number;
  year?: number;
  quarter?: number;
  [key: string]: unknown;
}

export interface CrisisArc {
  id: string;
  type: string;
  title: string;
  description: string;
  severity: number;          // 1..10, escalates each unresolved quarter
  startQuarter: number;
  quartersActive: number;
  resolved: boolean;
}

export type CrisisResponseType = 'aggressive' | 'measured' | 'gamble';

export interface SimulationDigestEntry {
  day: number;
  description: string;
  type?: string;
}

export interface Era {
  name: EraName | string;
  startYear: number;
  endYear: number;
  description?: string;
  [key: string]: unknown;
}

export interface RegulatoryState {
  glassSteagallActive?: boolean;
  regulationQ?: boolean;
  baselI?: boolean;
  baselII?: boolean;
  baselIII?: boolean;
  doddFrank?: boolean;
  volckerRule?: boolean;
  capitalRequirement?: number;
  [key: string]: unknown;
}

export interface Competitor {
  id: string;
  name: string;
  ceoName?: string;
  archetype: string;
  totalAssets: number;
  reputation: number;
  marketShare: number;
  alive: boolean;
  failureReason?: string;
  failureYear?: number;
  recentDecisions?: string[];
}

export interface LeaderboardEntry {
  id: string;
  name: string;
  archetype: string;
  totalAssets: number;
  marketShare: number;
  reputation: number;
  alive: boolean;
  rank: number;
  isPlayer?: boolean;
}

export interface AICEOState {
  active?: boolean;
  marketCap?: number;
  efficiency?: number;
  threatLevel?: number;
  [key: string]: unknown;
}

export interface ClimateState {
  physicalRisk?: number;
  transitionRisk?: number;
  tippingPointsCrossed?: number;
  [key: string]: unknown;
}

export interface PoliticalState {
  currentCapital: number;
  publicOpinion: number;
  effectiveness: number;
  [key: string]: unknown;
}

export interface PathState {
  riskCulture: number;             // 0..1
  internationalFocus: number;      // 0..1
  techInvestment: number;          // 0..1
  politicalCapital: number;        // 0..1
  innovationBias: number;          // 0..1
  archetype?: string;
  unlockedPaths?: string[];
  closedPaths?: string[];
  majorDecisions?: Array<{ year: number; quarter: number; label: string }>;
}

export interface AnnualReport {
  year: number;
  totalRevenue: number;
  totalCosts: number;
  netIncome: number;
  capitalStart: number;
  capitalEnd: number;
  capitalGrowthPct: number;
  capitalRatio: number;
  leverageRatio: number;
  avgQuarterScore: number;
  eraName: string;
  headlines?: string[];
  narrative?: string;
  keyEvents?: string[];
  quartersPlayed: number;
}

export interface CEOProfile {
  id: string;
  name: string;
  nickname?: string;
  backstory?: string;
  archetype?: string;
  specialAbility?: { id: string; name: string; description: string; apCost: number };
  personality: CEOPersonality;
}

export interface CEOPersonality {
  riskTolerance: number;
  growthAmbition: number;
  complianceFocus: number;
  politicalSavvy: number;
  analyticalAbility: number;
  persuasion: number;
  leadership: number;
  adaptability: number;
  integrity: number;
  stressResponse: number;
  longTermFocus: number;
}

export interface DealRisk {
  duration?: number;
  failureLoss?: number;
  returnRange?: [number, number];
  riskLevel?: number;
  successProbability?: number;
  windfallMultiplier?: number;
  windfallProbability?: number;
  [key: string]: unknown;
}

export interface Deal {
  id: string;
  title: string;
  type?: string;
  // Real engine shape (commercial-lending prospects etc.)
  clientName?: string;          // the firm you'd lend to
  description?: string;
  investmentRequired?: number;  // size of the loan/commitment
  requiredLine?: string;        // division/business line that can take it
  risk?: DealRisk | number;     // engine emits a rich object; older code expected a number
  // Legacy/optional
  size?: number;
  expectedReturn?: number;
  [key: string]: unknown;
}

/**
 * STAR_02 P7 — a resolved-loan outcome event. Emitted by the engine when an
 * accepted deal matures (DealPortfolio::resolveDeals). The client uses these to
 * bark the credit officer, fill the Loan Book strip, and list recent results.
 */
export interface DealOutcome {
  dealId: string;
  title: string;
  clientName: string;
  outcome: 'paid_off' | 'defaulted' | 'windfall';
  investedCapital: number;
  realizedPnl: number;     // net P&L on the stake (positive = win)
  quartersHeld: number;
  resolvedQuarter: number;
}

/** STAR_02 P7 — cumulative loan-book scoreboard (the "loany" heart). */
export interface LoanBookStats {
  accepted: number;
  paidOff: number;
  defaulted: number;
  windfalls: number;
  realizedPnl: number;
}

/** STAR_02 P7 — one long position in the CEO's personal trading account. */
export interface PersonalPosition {
  marketId: string;
  qty: number;
  avgCost: number;
  price?: number;   // current mark (player-view shape)
  value?: number;   // qty * price
  pnl?: number;     // qty * (price - avgCost)
}

/** STAR_02 P7 — the CEO's off-balance-sheet personal trading account. */
export interface TradeBook {
  cash: number;
  seedCapital: number;
  costBasis: number;
  value: number;     // cash + marked positions
  pnl: number;       // total value − seed
  positions: PersonalPosition[];
}

export interface HiringCandidate {
  id: string;
  name: string;
  archetype: string;
  /** STAR_02 P6: registry specialization id (e.g. "relationship_man"). */
  specializationId?: string;
  level: number;
  annualSalary: number;
  yearsExperience: number;
  stats: Record<string, number>;
  traits: Array<{ name: string; description: string; positive: boolean }>;
  expectedSharpe?: number;
}

/**
 * STAR_02 P6 poach offer — a rival's whole division put up for poaching.
 * Mirrors simulation::PoachOffer (CompetitorEngine.h).
 */
export interface PoachOffer {
  id: string;
  rivalId: string;
  rivalName: string;
  divisionType: string;
  divisionName: string;
  teamSize: number;
  trackRecord: number;       // their cumulative revenue ("made $X")
  yearsTrackRecord: number;  // "over Y years"
  askPrice: number;          // cost to poach
  archetypeMix: string[];    // family ids the imported team carries
  active: boolean;
  offeredQuarter: number;
  expiresQuarter: number;
}

export interface GameEndState {
  reason: GameEndReason;
  isVictory: boolean;
  title: string;
  narrative: string;
}

/**
 * Per-tick macro block streamed inside `market_tick` (engine: QuarterlyTurnManager
 * ::marketTickPayload, key `econ`). All rates are DECIMALS (0.05 = 5%); gdpLevel
 * is an index (100.0 in 1945); gdpGrowth is an annualized decimal growth rate.
 * Exact keys mirror EconomicEngine state — keep in lockstep with the engine.
 */
export interface EconTick {
  gdpLevel: number;       // index, base 100 at 1945
  gdpGrowth: number;      // annualized real growth, decimal (0.03 = 3%)
  unemployment: number;   // decimal (0.04 = 4%)
  fedFundsRate: number;   // decimal (0.05 = 5%)
  cpiInflation: number;   // decimal (0.02 = 2%)
  creditSpread: number;   // decimal (0.015 = 1.5%)
}

/**
 * Full macro indicator snapshot — the richer per-quarter shape returned by
 * GET /api/game/<id>/macro-history (and serialized by the engine's
 * EconomicIndicators). The intraday `econ` tick block is a subset of this.
 */
export interface EconomicIndicators {
  fedFundsRate: number;
  cpiInflation: number;
  gdpGrowth: number;
  unemployment: number;
  vix: number;
  sp500Return: number;
  treasuryYield10Y: number;
  creditSpread: number;
  gdpLevel: number;
  creditImpulse: number;
}

/** One per-quarter macro snapshot from the macro-history endpoint. */
export interface MacroSnapshot {
  year: number;
  quarter: number;
  econ: EconomicIndicators;
  closes: Record<string, number>;   // marketId → quarter-close price
}

/** GET /api/game/<id>/macro-history response (data field of the envelope). */
export interface MacroHistoryResponse {
  quarters: MacroSnapshot[];
}

export interface GameState {
  currentYear: number;
  currentQuarter: number;
  /**
   * Live macro indicators (engine `EconomicIndicators`, serialized in every
   * PlayerView under `economics`). Previously discarded by the client (P3.1).
   */
  economics?: EconomicIndicators;
  [key: string]: unknown;
}

// ── Composite payload (matches toPlayerJson) ────────────────────────────

export interface PlayerView {
  phase: TurnPhase;
  state: GameState;
  bank: Bank;
  actionPoints: ActionPoints;
  progression: Progression;
  pendingDecisions: PendingDecision[];
  messages: FeedMessage[];
  events: GameEvent[];
  activeCrises: CrisisArc[];
  simulationDigest: SimulationDigestEntry[];
  resolvedConsequences: unknown[];
  pendingConsequenceCount: number;
  hiringPool: HiringCandidate[];
  poachOffers?: PoachOffer[];   // STAR_02 P6
  reputationTag?: string;       // STAR_02 P6 ReputationLens
  availableDeals: Deal[];
  activeDeals: Deal[];
  dealOutcomes?: DealOutcome[];   // STAR_02 P7
  loanBookStats?: LoanBookStats;  // STAR_02 P7
  tradeBook?: TradeBook;          // STAR_02 P7
  era: Era;
  regulatory: RegulatoryState;
  competitors: Competitor[];
  leaderboard: LeaderboardEntry[];
  aiCeo: AICEOState;
  climate: ClimateState;
  political: PoliticalState;
  path: PathState;
  annualReport: AnnualReport | null;
  ceo?: CEOProfile;
  specialAbilityCooldown?: number;
  gameEnd?: GameEndState;
}

// ── WebSocket message types ─────────────────────────────────────────────

export interface MarketTickMsg {
  type: 'market_tick';
  day: number;
  year: number;
  quarter: number;
  totalDay: number;
  prices: Array<{ id: string; price: number; dailyReturn: number }>;
  bankCapital: number;
  bankAssets: number;
  // Live balance-sheet stream (added for the real-time financial dashboard)
  bankLoans?: number;
  bankSecurities?: number;
  bankReserves?: number;
  bankDeposits?: number;
  bankInterbank?: number;
  availableDeals?: Deal[];
  // STAR_02 P6: live hiring pool + poach offers stream so candidates/offers
  // arrive intra-quarter (the frontend animates new ids).
  hiringPool?: HiringCandidate[];
  poachOffers?: PoachOffer[];
  // STAR_02 P7: live loan-outcome feed + scoreboard, and the personal trading
  // book's mark-to-market value/P&L (so the Book strip + position pills update
  // against the ticking tape).
  dealOutcomes?: DealOutcome[];
  loanBookStats?: LoanBookStats;
  personalBookValue?: number;
  personalBookPnl?: number;
  // P3.2: per-day macro stream so macro lines move intraday like markets do.
  econ?: EconTick;
  regime: Regime;
}

export interface QuarterBoundaryMsg {
  type: 'quarter_boundary';
  year: number;
  quarter: number;
  phase: 'decision_phase' | 'crisis_response' | 'quarter_complete';
  pendingDecisions?: PendingDecision[];
  messages?: FeedMessage[];
  events?: GameEvent[];
  activeCrises?: CrisisArc[];
  report?: AnnualReport;
  state: PlayerView;
}

export interface GameOverMsg {
  type: 'game_over';
  reason: GameEndReason;
  isVictory: boolean;
  title: string;
  narrative: string;
  state: PlayerView;
}

export interface StateMsg {
  type: 'state';
  state: PlayerView;
}

export interface EraChangeMsg {
  type: 'era_change';
  era: Era;
}

// Client -> server
export interface SimulationControlMsg {
  type: 'simulation_control';
  action: 'play' | 'pause' | 'speed';
  speed?: 1 | 2 | 4 | 8;
}

export interface DecisionMadeMsg {
  type: 'decision_made';
  decisionId: string;
  optionId: string;
}

export interface CrisisResponseMsg {
  type: 'crisis_response';
  crisisId: string;
  responseType: CrisisResponseType;
}

export type ServerMsg =
  | MarketTickMsg
  | QuarterBoundaryMsg
  | GameOverMsg
  | StateMsg
  | EraChangeMsg;

export type ClientMsg =
  | SimulationControlMsg
  | DecisionMadeMsg
  | CrisisResponseMsg;

// ── REST response envelope ──────────────────────────────────────────────

export interface ApiEnvelope<T> {
  success: boolean;
  data?: T;
  error?: string;
  timestamp: string;
}

export interface NewGameResponse {
  gameId: string;
  state: PlayerView;
}

// ── Era helpers ─────────────────────────────────────────────────────────

export function eraSlug(name: string): EraSlug {
  if (name.startsWith('Post-War')) return 'post-war';
  if (name.startsWith('Expansion')) return 'expansion';
  if (name.startsWith('Volatility')) return 'volatility';
  if (name.startsWith('Big Bang')) return 'big-bang';
  if (name.startsWith('Shadow')) return 'shadow';
  if (name.startsWith('Crisis')) return 'crisis';
  if (name.startsWith('Modern')) return 'modern';
  return 'post-war';
}

/** UI phase per the rebuild plan progressive-disclosure schedule. */
export function uiPhase(year: number): 1 | 2 | 3 | 4 {
  if (year < 1960) return 1;
  if (year < 1973) return 2;
  if (year < 2000) return 3;
  return 4;
}
