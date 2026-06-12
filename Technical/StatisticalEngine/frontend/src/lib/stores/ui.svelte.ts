/*
 * UI store — screen routing, layout phase, contextual-hint dismissal,
 * keyboard-help visibility, settings overlay state. Pure UI; no server data.
 */

import { uiPhase, eraSlug, type EraSlug } from '../types/server';
import { sim } from './simulation.svelte';
import { telemetry } from '../telemetry';

export type Screen = 'title' | 'game' | 'game-over';

/**
 * Annual-report presentation mode (0.6):
 *   'modal' — always the full year-end modal.
 *   'card'  — always the compact corner card (click to expand).
 *   'auto'  — speed-dependent: full modal at 1x, compact card at 2x+.
 */
export type AnnualReportMode = 'modal' | 'card' | 'auto';

/**
 * Decision-pause behaviour (STAR_02 Addendum A.1):
 *   'pause-major' — (default) MAJOR non-crisis decisions pause the auto-continue
 *                   scheduler until the player acts on or dismisses them; routine
 *                   memos still lapse. Crises always hard-pause regardless.
 *   'lapse'       — nothing but a crisis pauses; every non-crisis decision lapses
 *                   on the auto-continue timer (the original "watch it flow" feel).
 */
export type DecisionPause = 'lapse' | 'pause-major';

/**
 * The four persistent tabs (P2). Phases now gate content RICHNESS within a tab,
 * not the whole layout. Keyboard 1-4 map to these in order.
 */
export type Tab = 'economy' | 'hire' | 'bank' | 'financials';
export const TABS: readonly Tab[] = ['economy', 'hire', 'bank', 'financials'] as const;
export const TAB_LABEL: Record<Tab, string> = {
  economy: 'Economy',
  hire: 'Hire',
  bank: 'My Bank',
  financials: 'Financials',
};

/**
 * Hero-chart time-scale windows (P2.3). MAX = whole game. Values are in trading
 * days (252/yr engine convention); the chart slices the existing ring buffers.
 */
export type Timescale = '1Y' | '5Y' | '10Y' | '20Y' | 'MAX';
export const TIMESCALES: readonly Timescale[] = ['1Y', '5Y', '10Y', '20Y', 'MAX'] as const;
export const TIMESCALE_DAYS: Record<Timescale, number> = {
  '1Y': 252, '5Y': 252 * 5, '10Y': 252 * 10, '20Y': 252 * 20, MAX: Infinity,
};

/**
 * Affordability heuristic for the TopBar HIRE pulse (P2.2). annualSalary is now
 * honest dollars (P0.9); the engine applies SALARY_COST_SCALE to the effective
 * hit. capital >= cheapest_salary * MULT is "comfortably affordable" — pulse on.
 */
export const HIRE_AFFORDABILITY_MULT = 2;

class UIStore {
  screen = $state<Screen>('title');
  settingsOpen = $state(false);
  decisionsOpen = $state(false);
  keyboardHelpOpen = $state(false);
  saveLoadOpen = $state(false);
  fullEventLogOpen = $state(false);

  /**
   * Deal-arrival badge counter (0.5). Deal arrivals increment this instead of
   * pushing a toast per deal (which flooded the stack). The Deal rail / a future
   * TopBar reads it; opening the loan book clears it via clearDealBadge().
   */
  dealBadge = $state(0);
  bumpDealBadge() { this.dealBadge += 1; }
  clearDealBadge() { this.dealBadge = 0; }

  /** Annual-report presentation mode (0.6). Default 'auto' = speed-dependent. */
  annualReportMode = $state<AnnualReportMode>('auto');

  /**
   * Decision-pause behaviour (STAR_02 Addendum A.1). Default 'pause-major'.
   * Persisted in localStorage so the owner's choice survives a reload. The
   * websocket auto-continue scheduler reads this to decide whether a MAJOR
   * non-crisis decision should freeze the world until acted on.
   */
  decisionPause = $state<DecisionPause>(this.loadDecisionPause());

  private loadDecisionPause(): DecisionPause {
    try {
      const raw = localStorage.getItem('stvg.decisionPause');
      if (raw === 'lapse' || raw === 'pause-major') return raw;
    } catch { /* ignore */ }
    return 'pause-major';
  }

  setDecisionPause(mode: DecisionPause) {
    this.decisionPause = mode;
    try { localStorage.setItem('stvg.decisionPause', mode); } catch { /* ignore */ }
  }

  // ── Tabs (P2) ────────────────────────────────────────────────────────────
  /** Active top-level tab. Default 'economy' (the day-trading hero view). */
  activeTab = $state<Tab>('economy');

  /**
   * Hero-chart selection on the Economy tab. `null` = the bank's own evolution
   * (CapitalChart); otherwise the engine market id promoted into the hero spot.
   */
  selectedHeroMarket = $state<string | null>(null);

  /** Hero-chart time-scale window (P2.3). */
  heroTimescale = $state<Timescale>('MAX');

  /** Right rail (Competitor/Leaderboard/Political/Regulatory) collapsed? */
  rightRailCollapsed = $state(false);
  toggleRightRail() { this.rightRailCollapsed = !this.rightRailCollapsed; }

  /** Switch tabs, emitting `tab_switch {from,to}` once (no-op if unchanged). */
  setTab(to: Tab) {
    if (this.activeTab === to) return;
    const from = this.activeTab;
    this.activeTab = to;
    telemetry.log('tab_switch', { from, to });
  }

  /** Promote a market into the hero spot (or `null` for the capital series). */
  setHeroMarket(id: string | null) {
    if (this.selectedHeroMarket === id) return;
    this.selectedHeroMarket = id;
    if (id) telemetry.log('chart_promote', { id });
  }

  /** Change the hero time-scale window, emitting `chart_timescale {scale}`. */
  setTimescale(scale: Timescale) {
    if (this.heroTimescale === scale) return;
    this.heroTimescale = scale;
    telemetry.log('chart_timescale', { scale });
  }

  /** Phase 1-4 derived from current sim year. */
  phase = $derived<1 | 2 | 3 | 4>(uiPhase(sim.year));

  /** Era CSS slug for [data-era=""]. */
  eraSlug = $derived<EraSlug>(eraSlug(sim.era?.name ?? 'Post-War Stability'));

  dismissedHints = $state<Set<string>>(this.loadDismissed());

  private loadDismissed(): Set<string> {
    try {
      const raw = localStorage.getItem('stvg.dismissedHints');
      if (raw) return new Set(JSON.parse(raw) as string[]);
    } catch { /* ignore */ }
    return new Set();
  }

  dismissHint(id: string) {
    this.dismissedHints.add(id);
    try {
      localStorage.setItem('stvg.dismissedHints', JSON.stringify([...this.dismissedHints]));
    } catch { /* ignore */ }
  }

  isHintDismissed(id: string): boolean {
    return this.dismissedHints.has(id);
  }
}

export const ui = new UIStore();
