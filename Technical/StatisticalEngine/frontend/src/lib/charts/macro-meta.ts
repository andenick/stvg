/*
 * Macro-series display metadata (P3) — the "living economy" counterpart to
 * markets-meta.ts. Each macro series carries a friendly label, a plain-language
 * "what is this and why do I care" explainer (same voice as the market blurbs),
 * an axis format (percent for decimal rates, index for GDP level), an unlock
 * phase per STAR_02 §3.3, and a promotion/strip flag.
 *
 * Hero promotion namespaces macro ids as `macro:<MacroId>` so the existing
 * ui.selectedHeroMarket (a string) can carry either a market id or a macro id.
 */

export type MacroId =
  | 'GDP'         // nominal GDP level index, base 100 at 1945
  | 'UNEMP'       // unemployment rate (decimal)
  | 'FEDFUNDS'    // fed funds policy rate (decimal)
  | 'CPI'         // CPI inflation (decimal)
  | 'SPREAD'      // credit spread (decimal)
  | 'TREAS10Y'    // 10Y treasury yield (decimal) — chip only
  | 'GDPGROWTH'   // real GDP growth (decimal) — derived, chip only
  | 'ECONREV'     // economy-wide revenue index (engine econ.economyRevenue)
  | 'ECONPROFIT'; // economy-wide profit index (engine econ.economyProfit)

/** Axis/value formatting per series. */
export type MacroFormat = 'percent' | 'index';

export interface MacroMeta {
  label: string;
  blurb: string;
  format: MacroFormat;
  /** UI phase at which this series unlocks (1=1945). */
  unlockPhase: 1 | 2 | 3 | 4;
  /**
   * Where the series shows by default:
   *   'strip'  — always in the default macro strip (GDP, Unemployment, Fed Funds).
   *   'chip'   — present but unpromoted; only in the expanded chip row (Inflation,
   *              10Y, GDP growth — owner: "maybe too complicated").
   *   'market' — a market-adjacent chip (credit spread).
   *   'soon'   — disabled "coming soon" placeholder (none active; the engine now
   *              streams economy-wide P&L, so Economy P&L is a real chip).
   */
  placement: 'strip' | 'chip' | 'market' | 'soon';
  /** Disabled placeholder — render greyed-out, never charts (no faked data). */
  comingSoon?: boolean;
}

const META: Record<MacroId, MacroMeta> = {
  GDP: {
    label: 'GDP',
    blurb:
      "The whole economy's output. When this grows, businesses borrow, build, and repay — " +
      'good times for a bank. When it stalls or shrinks, loans go bad and deals dry up.',
    format: 'index',
    unlockPhase: 1,
    placement: 'strip',
  },
  UNEMP: {
    label: 'Unemployment',
    blurb:
      'The share of people out of work. Low and falling means households spend and pay their ' +
      'mortgages; spiking unemployment is the warning light before loan losses hit your books.',
    format: 'percent',
    unlockPhase: 1,
    placement: 'strip',
  },
  FEDFUNDS: {
    label: 'Fed Funds',
    blurb:
      'The interest rate the central bank sets — the price of money itself. Raise it and lending ' +
      'gets dear and the economy cools; cut it and credit gushes. It moves everything you trade.',
    format: 'percent',
    unlockPhase: 1,
    placement: 'strip',
  },
  CPI: {
    label: 'Inflation',
    blurb:
      'How fast prices are rising. A little is healthy; too much forces the Fed to slam the ' +
      'brakes (see Fed Funds), which is usually how the good times end.',
    format: 'percent',
    unlockPhase: 1,
    placement: 'chip',
  },
  SPREAD: {
    label: 'Credit Spread',
    blurb:
      'The extra yield risky borrowers must pay over safe government debt — the market’s fear of ' +
      'default. It widens before a crisis and screams when one arrives. Watch it like a hawk.',
    format: 'percent',
    unlockPhase: 1,
    placement: 'market',
  },
  TREAS10Y: {
    label: '10Y Treasury',
    blurb:
      'The yield on 10-year government debt — the benchmark long rate that prices mortgages and ' +
      'corporate loans. The gap between this and Fed Funds (the "curve") foreshadows recessions.',
    format: 'percent',
    unlockPhase: 1,
    placement: 'chip',
  },
  GDPGROWTH: {
    label: 'GDP Growth',
    blurb:
      'How fast output is expanding, year over year. Positive is expansion; a dip below zero is ' +
      'recession — the moment lending turns from profit engine to loss machine.',
    format: 'percent',
    unlockPhase: 1,
    placement: 'chip',
  },
  ECONREV: {
    label: 'Economy Revenue',
    blurb:
      "The whole economy's nominal activity — the top line everyone is competing for. It's an index " +
      '(100 in 1945) that climbs with output and prices. When it stalls, the lending pie stops growing ' +
      'and every bank fights harder for the same business.',
    format: 'index',
    unlockPhase: 2,
    placement: 'chip',
  },
  ECONPROFIT: {
    label: 'Economy Profit',
    blurb:
      "Profit across the economy after the squeeze of costs — revenue times the corporate margin. " +
      'Margins fatten in booms and compress when credit gets dear, so this leads loan demand: fat ' +
      'profits mean confident borrowers; a profit slump is the early tell that defaults are coming.',
    format: 'index',
    unlockPhase: 2,
    placement: 'chip',
  },
};

/** All real (data-backed) macro ids in display order. */
export const MACRO_IDS: readonly MacroId[] = [
  'GDP', 'UNEMP', 'FEDFUNDS', 'CPI', 'SPREAD', 'TREAS10Y', 'GDPGROWTH', 'ECONREV', 'ECONPROFIT',
];

/** The default macro strip (always-visible, promotable to hero). */
export const MACRO_STRIP_IDS: readonly MacroId[] = (
  Object.entries(META) as [MacroId, MacroMeta][]
).filter(([, m]) => m.placement === 'strip').map(([id]) => id);

/** Expanded-only chip row (present but unpromoted) + the coming-soon placeholder. */
export const MACRO_CHIP_IDS: readonly MacroId[] = (
  Object.entries(META) as [MacroId, MacroMeta][]
).filter(([, m]) => m.placement === 'chip' || m.placement === 'soon').map(([id]) => id);

export function macroMeta(id: MacroId): MacroMeta | undefined {
  return META[id];
}
export function macroLabel(id: string): string {
  return META[id as MacroId]?.label ?? id;
}
export function macroBlurb(id: string): string {
  return META[id as MacroId]?.blurb ?? '';
}
export function macroFormat(id: string): MacroFormat {
  return META[id as MacroId]?.format ?? 'index';
}
export function isMacroId(id: string | null | undefined): id is MacroId {
  return !!id && id in META;
}

// ── Hero-promotion namespacing ────────────────────────────────────────────
// Macro ids are promoted to the hero spot as `macro:<id>` so the single
// ui.selectedHeroMarket string can carry markets OR macro series unambiguously.
export const MACRO_HERO_PREFIX = 'macro:';
export function macroHeroId(id: MacroId): string {
  return MACRO_HERO_PREFIX + id;
}
/** If `heroId` is a namespaced macro id, return the bare MacroId, else null. */
export function heroMacroId(heroId: string | null): MacroId | null {
  if (heroId && heroId.startsWith(MACRO_HERO_PREFIX)) {
    const bare = heroId.slice(MACRO_HERO_PREFIX.length);
    return isMacroId(bare) ? bare : null;
  }
  return null;
}

/** Format a macro value for axis/labels. percent: decimal→"5.0%". index: "1,250". */
export function formatMacroValue(id: string, v: number): string {
  if (macroFormat(id) === 'percent') return `${(v * 100).toFixed(1)}%`;
  // index (GDP level): thousands-separated, no decimals once large.
  const abs = Math.abs(v);
  if (abs >= 100) return v.toLocaleString('en-US', { maximumFractionDigits: 0 });
  return v.toFixed(1);
}
