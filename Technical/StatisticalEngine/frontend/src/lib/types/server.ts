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
  recommendation: 'support' | 'oppose' | 'neutral' | string;
  reasoning?: string;
  confidence?: number;
}

export interface RiskImpactDelta {
  hiddenRisk?: number;
  revealedRisk?: number;
  reputationRisk?: number;
  regulatoryRisk?: number;
}

export interface DecisionOption {
  id: string;
  title: string;                 // option headline ("Approve loan", "Decline")
  description?: string;          // body copy
  successProbability?: number;   // 0..1
  financialImpact?: number;
  riskImpact?: RiskImpactDelta;
  recommendations?: CharacterRecommendation[];
  longTermConsequences?: string[];
}

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

export interface Deal {
  id: string;
  title: string;
  type?: string;
  size?: number;
  expectedReturn?: number;
  risk?: number;
  [key: string]: unknown;
}

export interface HiringCandidate {
  id: string;
  name: string;
  archetype: string;
  level: number;
  annualSalary: number;
  yearsExperience: number;
  stats: Record<string, number>;
  traits: Array<{ name: string; description: string; positive: boolean }>;
  expectedSharpe?: number;
}

export interface GameEndState {
  reason: GameEndReason;
  isVictory: boolean;
  title: string;
  narrative: string;
}

export interface GameState {
  currentYear: number;
  currentQuarter: number;
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
  messages: string[];
  events: GameEvent[];
  activeCrises: CrisisArc[];
  simulationDigest: SimulationDigestEntry[];
  resolvedConsequences: unknown[];
  pendingConsequenceCount: number;
  hiringPool: HiringCandidate[];
  availableDeals: Deal[];
  activeDeals: Deal[];
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
  regime: Regime;
}

export interface QuarterBoundaryMsg {
  type: 'quarter_boundary';
  year: number;
  quarter: number;
  phase: 'decision_phase' | 'crisis_response' | 'quarter_complete';
  pendingDecisions?: PendingDecision[];
  messages?: string[];
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
