/*
 * Per-market display metadata — friendly labels + a one-line "what is this and
 * why do I care" explainer shown on the expanded market view. Keyed by the
 * ACTUAL engine market ids (SP500, UST10Y, GOLD, SPXOPT, EURUSD, BTC).
 */

export interface MarketMeta {
  label: string;
  blurb: string;
}

const META: Record<string, MarketMeta> = {
  SP500: {
    label: 'S&P 500',
    blurb: 'The broad US stock market. Rises in booms, falls in panics — the mood of the economy.',
  },
  UST10Y: {
    label: '10Y Treasury',
    blurb: 'Price of the 10-year US government bond. The safe haven — it rises when yields fall and money runs scared.',
  },
  GOLD: {
    label: 'Gold',
    blurb: 'The old-money hedge. Climbs when inflation bites or confidence in paper money wobbles.',
  },
  SPXOPT: {
    label: 'VIX',
    blurb: "The market's fear gauge — implied S&P volatility. Low and calm in good times; it spikes hard in a crisis.",
  },
  EURUSD: {
    label: 'EUR/USD',
    blurb: 'Euro vs dollar. Opens up once Bretton Woods breaks (1971) — currency risk and a new place to make (or lose) money.',
  },
  BTC: {
    label: 'Bitcoin',
    blurb: 'Digital wildcard (2009+). Enormous swings — generational upside or a fast way to blow up.',
  },
};

export function marketLabel(id: string): string {
  return META[id]?.label ?? id;
}

export function marketBlurb(id: string): string {
  return META[id]?.blurb ?? '';
}

/** Preferred small-multiples ordering using the real engine ids. */
export const PREFERRED_MARKET_ORDER = ['SP500', 'UST10Y', 'GOLD', 'SPXOPT', 'EURUSD', 'BTC'];
