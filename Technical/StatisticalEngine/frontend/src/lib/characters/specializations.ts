/*
 * Specialization display map (STAR_02 P6) — the display/flavor slice of the
 * canonical archetypes.json specializations roster, mirrored into the frontend.
 *
 * WHY a hand-mirrored TS file (same reasoning as archetypes-data.ts): the
 * canonical JSON lives in the ENGINE tree (`data/archetypes/`), outside
 * `frontend/src`. The candidate card only needs the DISPLAY fields — displayName,
 * family (for nameplate color), era window, two stat highlights, and the
 * period-appropriate voiceFlavor hint. The full β/σ/statWeights stay engine-side.
 *
 * If the canonical specializations change, regenerate this map from
 * data/archetypes/archetypes.json (the engine is the single source of truth).
 */

import type { FamilyId } from './archetypes-data';

export interface SpecializationDisplay {
  id: string;
  displayName: string;
  family: FamilyId;
  eraFrom: number;
  eraTo: number;
  /** The two stats this specialization leans into (for the detail view). */
  statHi: string[];
  /** A period-appropriate one-line voice hint (credibility flavor seed). */
  voiceFlavor: string;
}

export const SPECIALIZATIONS: Record<string, SpecializationDisplay> = {
  "relationship_man": { id: 'relationship_man', displayName: 'The Relationship Man', family: 'patrician', eraFrom: 1945, eraTo: 1985, statHi: ['persuasion', 'networking'], voiceFlavor: 'Golf-course dealflow: \'Made the loan on the back nine. Good family, they always pay.\'' },
  "trust_steward": { id: 'trust_steward', displayName: 'The Trust Steward', family: 'patrician', eraFrom: 1945, eraTo: 2040, statHi: ['judgment', 'analytical'], voiceFlavor: 'Fiduciary calm: \'We preserve, we do not chase. The estate will outlive us both.\'' },
  "credit_hawk": { id: 'credit_hawk', displayName: 'The Credit Hawk', family: 'lifer', eraFrom: 1945, eraTo: 2040, statHi: ['analytical', 'judgment'], voiceFlavor: 'The \'no\' man: \'Cash flow doesn\'t cover the debt service at a stress rate. We pass.\'' },
  "volume_lender": { id: 'volume_lender', displayName: 'The Volume Lender', family: 'operator', eraFrom: 1958, eraTo: 2040, statHi: ['persuasion', 'networking'], voiceFlavor: 'Throughput first: \'Approvals are up 40% quarter on quarter. Worry about defaults next year.\'' },
  "tape_reader": { id: 'tape_reader', displayName: 'The Tape Reader', family: 'gunslinger', eraFrom: 1945, eraTo: 1990, statHi: ['intuition', 'stamina'], voiceFlavor: 'Feel over model: \'I don\'t need a reason. The tape\'s heavy. I\'m short.\'' },
  "trading_gunslinger": { id: 'trading_gunslinger', displayName: 'The Gunslinger (Prop Desk)', family: 'gunslinger', eraFrom: 1975, eraTo: 2010, statHi: ['intuition', 'ambition'], voiceFlavor: 'Swagger: \'I made the desk eight figures last year. Don\'t ask me to wear a leash.\'' },
  "quant_trader": { id: 'quant_trader', displayName: 'The Quant', family: 'quant', eraFrom: 1985, eraTo: 2040, statHi: ['analytical'], voiceFlavor: 'Hedged precision: \'Edge is small but it\'s stable. We win on turnover, not on any one trade.\'' },
  "macro_tourist": { id: 'macro_tourist', displayName: 'The Macro Tourist', family: 'dealmaker', eraFrom: 1971, eraTo: 2040, statHi: ['intuition', 'networking'], voiceFlavor: 'Always has a thesis: \'The Fed\'s behind the curve. I\'m betting the whole curve steepens.\'' },
  "vol_harvester": { id: 'vol_harvester', displayName: 'The Vol Harvester', family: 'quant', eraFrom: 1990, eraTo: 2040, statHi: ['analytical', 'intuition'], voiceFlavor: 'Sells calm, fears the storm: \'We collect premium every quiet quarter. The tail is the whole game.\'' },
  "rainmaker_ib": { id: 'rainmaker_ib', displayName: 'The Rainmaker', family: 'rainmaker', eraFrom: 1980, eraTo: 2040, statHi: ['networking', 'persuasion'], voiceFlavor: 'Brings the mandate: \'I own this CEO\'s number. Where I go, the fee follows.\'' },
  "spreadsheet_surgeon": { id: 'spreadsheet_surgeon', displayName: 'The Spreadsheet Surgeon', family: 'quant', eraFrom: 1985, eraTo: 2040, statHi: ['analytical', 'judgment'], voiceFlavor: 'Model is gospel: \'At a 9.2x exit the IRR clears the hurdle. Anything north of that is gravy.\'' },
  "lbo_baron": { id: 'lbo_baron', displayName: 'The LBO Baron', family: 'dealmaker', eraFrom: 1982, eraTo: 2040, statHi: ['leadership', 'ambition'], voiceFlavor: 'Leverage is a feature: \'Cheap debt and a fat dividend recap. The equity is almost free.\'' },
  "junk_bond_evangelist": { id: 'junk_bond_evangelist', displayName: 'The Junk-Bond Evangelist', family: 'gunslinger', eraFrom: 1977, eraTo: 1990, statHi: ['persuasion', 'analytical'], voiceFlavor: 'Missionary zeal: \'High yield isn\'t risky, it\'s democratized capital. Everyone else just can\'t see it yet.\'' },
  "originator_cowboy": { id: 'originator_cowboy', displayName: 'The Originator Cowboy', family: 'gunslinger', eraFrom: 1999, eraTo: 2008, statHi: ['persuasion', 'networking'], voiceFlavor: 'Volume at any cost: \'Stated income, no doc, who cares? We sell it before it can default.\'' },
  "doc_checker": { id: 'doc_checker', displayName: 'The Doc Checker', family: 'operator', eraFrom: 1945, eraTo: 2040, statHi: ['analytical', 'judgment'], voiceFlavor: 'Belt and suspenders: \'The appraisal is stale and the title has a lien. This file doesn\'t close today.\'' },
  "securitization_engineer": { id: 'securitization_engineer', displayName: 'The Securitization Engineer', family: 'quant', eraFrom: 1985, eraTo: 2009, statHi: ['analytical'], voiceFlavor: 'Tranching as alchemy: \'Pool the risk, slice the waterfall, the senior tranche is AAA. Trust the correlation.\'' },
  "eurodollar_smoothie": { id: 'eurodollar_smoothie', displayName: 'The Eurodollar Smoothie', family: 'patrician', eraFrom: 1960, eraTo: 1995, statHi: ['networking', 'persuasion'], voiceFlavor: 'Cosmopolitan ease: \'London books it offshore, no reserve requirement. The spread is beautiful.\'' },
  "empire_builder": { id: 'empire_builder', displayName: 'The Empire Builder', family: 'dealmaker', eraFrom: 1965, eraTo: 2040, statHi: ['leadership', 'networking'], voiceFlavor: 'Maps and flags: \'A branch in Sao Paulo, a desk in Tokyo. We bank the whole hemisphere or none of it.\'' },
  "doom_prophet": { id: 'doom_prophet', displayName: 'The Doom Prophet', family: 'quant', eraFrom: 1980, eraTo: 2040, statHi: ['analytical', 'judgment'], voiceFlavor: 'Cassandra, often right too early: \'The whole thing is a house of cards. I\'ve been short for two years.\'' },
  "distressed_vulture": { id: 'distressed_vulture', displayName: 'The Distressed Vulture', family: 'dealmaker', eraFrom: 1985, eraTo: 2040, statHi: ['analytical', 'judgment'], voiceFlavor: 'Feeds on the wreckage: \'Their bankruptcy is my entry point. I buy the debt at forty and own the company.\'' },
  "box_ticker": { id: 'box_ticker', displayName: 'The Box Ticker', family: 'operator', eraFrom: 1980, eraTo: 2040, statHi: ['judgment', 'analytical'], voiceFlavor: 'Process over outcome: \'It\'s compliant. Whether it\'s a good idea is above my pay grade.\'' },
  "card_quant": { id: 'card_quant', displayName: 'The Card Quant', family: 'quant', eraFrom: 1985, eraTo: 2040, statHi: ['analytical', 'judgment'], voiceFlavor: 'Lives in the FICO bands: \'Charge-offs tick up at 680 and below. Price it, don\'t avoid it.\'' },
  "private_banker": { id: 'private_banker', displayName: 'The Private Banker', family: 'rainmaker', eraFrom: 1960, eraTo: 2040, statHi: ['judgment', 'networking'], voiceFlavor: 'Discreet and indispensable: \'I know what the family is worth and what they fear. That is the whole product.\'' },
  "fund_marketer": { id: 'fund_marketer', displayName: 'The Fund Marketer', family: 'rainmaker', eraFrom: 1975, eraTo: 2040, statHi: ['persuasion', 'networking'], voiceFlavor: 'Sells the sizzle: \'Past performance, four stars, and the story sells itself. AUM is destiny.\'' },
  "vc_partner": { id: 'vc_partner', displayName: 'The VC Partner', family: 'dealmaker', eraFrom: 1980, eraTo: 2040, statHi: ['intuition', 'networking'], voiceFlavor: 'Power-law gospel: \'Nine go to zero, one returns the fund. I only need to be right once, enormously.\'' },
  "tech_founder_whisperer": { id: 'tech_founder_whisperer', displayName: 'The Founder Whisperer', family: 'prodigy', eraFrom: 1995, eraTo: 2040, statHi: ['intuition', 'persuasion'], voiceFlavor: 'Speaks startup fluently: \'TAM is a trillion, burn is fine, they\'ll be default-alive by Series B.\'' },
  "prompt_whisperer": { id: 'prompt_whisperer', displayName: 'The Prompt Whisperer', family: 'prodigy', eraFrom: 2023, eraTo: 2040, statHi: ['analytical', 'intuition'], voiceFlavor: 'AI-native confidence: \'The model already wrote the strategy. I just decide when to trust it.\'' },
  "crypto_evangelist": { id: 'crypto_evangelist', displayName: 'The Crypto Evangelist', family: 'gunslinger', eraFrom: 2013, eraTo: 2040, statHi: ['persuasion', 'intuition'], voiceFlavor: 'Maximalist hype: \'Fiat is dying. We\'re not late, we\'re early. Have fun staying poor.\'' },
  "fintech_operator": { id: 'fintech_operator', displayName: 'The Fintech Operator', family: 'operator', eraFrom: 2010, eraTo: 2040, statHi: ['analytical', 'ambition'], voiceFlavor: 'Metrics-driven: \'CAC is down, LTV is up, churn\'s under 2%. The unit economics finally work.\'' },
  "compliance_lifer": { id: 'compliance_lifer', displayName: 'The Compliance Lifer', family: 'lifer', eraFrom: 1970, eraTo: 2040, statHi: ['judgment', 'analytical'], voiceFlavor: 'The institutional conscience: \'I\'ve watched three banks die doing exactly this. The answer is no.\'' },
  "branch_patriarch": { id: 'branch_patriarch', displayName: 'The Branch Patriarch', family: 'patrician', eraFrom: 1945, eraTo: 1980, statHi: ['networking', 'judgment'], voiceFlavor: '3-6-3 banker: \'Borrow at three, lend at six, on the golf course by three. Steady as she goes.\'' },
  "wartime_operator": { id: 'wartime_operator', displayName: 'The Wartime Operator', family: 'operator', eraFrom: 1945, eraTo: 1965, statHi: ['leadership', 'judgment'], voiceFlavor: 'Logistics discipline: \'I ran a supply depot in the Pacific. A bank branch is easy by comparison.\'' },
  "structured_products_alchemist": { id: 'structured_products_alchemist', displayName: 'The Structured-Products Alchemist', family: 'quant', eraFrom: 1994, eraTo: 2040, statHi: ['analytical', 'ambition'], voiceFlavor: 'Black-box salesman: \'The client doesn\'t need to understand the payoff. They need to like the coupon.\'' },
};

/** Look up a specialization's display record by id (null when absent). */
export function specializationFor(id: string | undefined | null): SpecializationDisplay | null {
  if (!id) return null;
  return SPECIALIZATIONS[id] ?? null;
}

/** A one-line era-flavor string ("1945–1985") for a specialization. */
export function eraFlavor(s: SpecializationDisplay): string {
  const to = s.eraTo >= 2040 ? 'today' : String(s.eraTo);
  return `${s.eraFrom}–${to}`;
}
