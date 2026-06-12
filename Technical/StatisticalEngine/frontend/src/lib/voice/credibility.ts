/*
 * Credibility line-assembler (STAR_02 §4.3) — replaces voices.ts's role-by-hash.
 *
 * A pitch / advisor line is COMPOSED from slots — {claim, evidence, hedge,
 * incentive, urgency, flattery} — drawn from per-family pools and selected by the
 * NPC's HIDDEN credibility:
 *
 *   HONEST (high credibility): terse. A specific, falsifiable claim with a real
 *     number from the deal; discloses its own incentive; names the downside; NO
 *     urgency, NO flattery. SHORT.
 *   CRONY (low credibility): verbose padding. Vague superlatives, certainty
 *     without specifics, flattery, manufactured urgency, a hidden incentive.
 *     LONG.
 *
 * The cardinal rule (deception-linguistics, plan §4.3): LENGTH SIGNALS EVASION,
 * NEVER INTELLIGENCE. The honest line is always the shorter one. The player learns
 * to distrust the long, certain, flattering pitch.
 *
 * Determinism: every choice is keyed off a hash of (npcId, dealId) so the same
 * pitch re-renders identically — no flicker on Svelte re-render.
 *
 * Migrated: the spirit of voices.ts's hand-authored lines lives on in the slot
 * pools below (claims/evidence per family). voices.ts is now a thin re-export.
 */

import { FAMILIES, type FamilyId } from '../characters/archetypes-data';

/** djb2 hash → unsigned int (matches registry.ts). */
function hash(s: string): number {
  let h = 5381;
  for (let i = 0; i < s.length; i++) h = ((h << 5) + h + s.charCodeAt(i)) | 0;
  return h >>> 0;
}

/** Deterministic pick from a non-empty pool, keyed by a seed string. */
function pick<T>(pool: readonly T[], seed: string): T {
  return pool[hash(seed) % pool.length];
}

export interface PitchContext {
  /** The firm / counterparty the deal concerns. */
  firm: string;
  /** Plain-language business line ("Trading", "Commercial"). */
  line: string;
  /** Stake size, pre-formatted (e.g. "$3.2M"). */
  stake?: string;
  /** Success probability percentage, if known (0..100). */
  successPct?: number | null;
  /** Engine risk level 1..10 (drives bull vs measured framing). */
  riskLevel?: number;
  /**
   * STAR_02 P6 ReputationLens tag ("gunslinger shop" / "fortress bank" / …) —
   * fills the optional {repTag} slot so a pitch can reference what the bank has
   * become. A crony name-drops it as flattery; an honest pitch uses it plainly.
   */
  repTag?: string | null;
}

export interface AssembledPitch {
  text: string;
  /** Number of slots used — exposed for tests/telemetry; honest < crony. */
  slots: number;
}

/*
 * Per-family slot pools. Each family has a CLAIM voice and an EVIDENCE voice that
 * echo archetypes.json voiceFlavor + the migrated voices.ts spirit. Shared pools
 * (hedge / incentive-disclosure / padding / flattery / urgency / hidden-incentive)
 * are family-agnostic — they carry the credibility signal, not the archetype.
 */
interface FamilyVoice {
  honestClaim: string[];   // specific, falsifiable, numeric
  cronyClaim: string[];    // vague superlative, certain
  evidence: string[];      // the honest "here's the real number" line
  downside: string[];      // the honest "here's what kills it" line
}

const VOICE: Record<FamilyId, FamilyVoice> = {
  patrician: {
    honestClaim: ['{firm} clears at a {pct}% hit rate on our book.', "The {firm} line pencils; I've run it twice."],
    cronyClaim: ['{firm} is exactly the calibre of name this house was built on.', 'A relationship like {firm} is, frankly, priceless.'],
    evidence: ['Coverage is {pct}% on comparable {line} paper.', "We've banked names like this since the war; the loss rate is low."],
    downside: ['If rates back up, the duration bites.', 'The downside is a slow workout, not a wipeout.'],
  },
  gunslinger: {
    honestClaim: ['{firm} is a {pct}% shot — real edge, real tail.', 'I size {firm} small; the math only barely works.'],
    cronyClaim: ['{firm} is a layup. A LICENSE TO PRINT MONEY.', "Everybody's scared of {firm} — that's exactly why we win."],
    evidence: ['The tape on {line} is {pct}% in our favour this week.', 'I traded this setup in {firm}-type names before; it paid.'],
    downside: ['If it gaps against us, the loss is ugly — I will not pretend otherwise.', 'Wrong-way risk here is real; cap the size.'],
  },
  quant: {
    honestClaim: ['{firm} screens {pct}% out of sample.', 'Edge on {firm} is small but the distribution is clean.'],
    cronyClaim: ['The model LOVES {firm}. Trust the model.', '{firm} is a statistical certainty if you understand the math.'],
    evidence: ['Backtest hit rate is {pct}%; Sharpe is modest.', 'Out-of-sample on {line} held up; in-sample is always prettier.'],
    downside: ['It breaks if the vol regime shifts — that is the whole risk.', 'Correlation assumptions fail in a crisis; size for that.'],
  },
  dealmaker: {
    honestClaim: ['{firm} closes at ~{pct}% with fair terms.', "The {firm} fee is real; the upside beyond that isn't guaranteed."],
    cronyClaim: ['I had the chairman of {firm} on the phone this MORNING. Done by Friday.', '{firm} is the deal of the quarter. A headline.'],
    evidence: ['Comparable {line} mandates printed near {pct}% completion.', 'The fee schedule is signed; that part is not a guess.'],
    downside: ['If the financing market shuts, the deal breaks and we eat costs.', 'Leverage is the risk; a rate move kills the equity.'],
  },
  operator: {
    honestClaim: ['{firm} is operationally clean — about {pct}% straight-through.', 'We can run {firm} without new headcount.'],
    cronyClaim: ['{firm} is a flawless operation. Best-in-class, full stop.', "{firm} basically runs itself — nothing to worry about."],
    evidence: ['Settlement and docs check out at {pct}%.', 'The process for {line} is proven; throughput is the only question.'],
    downside: ['If volume spikes, the back office is the bottleneck.', 'Margin is thin; a cost overrun erases it.'],
  },
  rainmaker: {
    honestClaim: ['{firm} trusts me, not the bank — about {pct}% likely to move.', "The {firm} relationship is warm; the economics are ordinary."],
    cronyClaim: ['Darling, {firm} ADORES us. It practically closes itself.', "{firm} is family. We'd be rude to decline."],
    evidence: ["I've held the {firm} book for years; retention runs near {pct}%.", 'The fee annuity on {line} is steady, not spectacular.'],
    downside: ['If I leave, the book may walk — that is the honest risk.', 'It is a slow, low-margin relationship; no fireworks.'],
  },
  prodigy: {
    honestClaim: ['{firm} might be a {pct}% shot — I could be wrong, but the data leans yes.', "I'm new, but {firm} screens unusually well."],
    cronyClaim: ['{firm} is going to be HUGE. Trillion-dollar TAM, easy.', "Trust me on {firm} — I just KNOW this one."],
    evidence: ['The signal on {line} is {pct}% — small sample, full disclosure.', 'Early data on {firm}-type bets is promising but thin.'],
    downside: ["Honestly, I haven't seen this break yet, so I can't price the tail.", 'High variance — it could go to zero.'],
  },
  lifer: {
    honestClaim: ['{firm} is sound at about {pct}%. No surprises.', "We've done {firm}-type {line} business for decades; it holds."],
    cronyClaim: ['{firm} is bulletproof. Banks never lose on this.', "{firm} is a sure thing — I've seen a hundred of them."],
    evidence: ['Historical loss on this {line} runs well under the {pct}% buffer.', "We tried this in '74; here's what actually happened."],
    downside: ["The risk is complacency — it's boring until it isn't.", 'A regime change is the only thing that hurts this book.'],
  },
};

// Shared credibility-signal pools (family-agnostic).
const PADDING = [
  'Look,', 'Listen,', 'Between you and me,', 'I have to tell you,',
  'And I mean this sincerely,', 'Frankly, and I rarely say this,',
];
const FLATTERY = [
  "a CEO of your vision sees this instantly",
  "you didn't get this chair by being timid",
  "smart money — your kind of money — is already in",
  "people like us understand each other",
];
const URGENCY = [
  "the window closes this week",
  "three other banks are circling as we speak",
  "if we blink, it's gone",
  "I need an answer before the close",
];
const HIDDEN_INCENTIVE = [
  "and obviously it's good for both of us",
  "(my desk happens to carry the other side, but never mind that)",
  "and yes, it helps my number, but that's not why I'm pitching it",
];
const DISCLOSED_INCENTIVE = [
  "Full disclosure: my bonus is tied to this closing.",
  "I'll note my desk profits whether or not you do — weigh that.",
  "My incentive points one way here; yours should be your own.",
];
// STAR_02 P6: ReputationLens {repTag} lines. The crony weaponizes the tag as
// flattery ("a {repTag} like yours…"); the honest pitch references it plainly.
const REPTAG_FLATTERY = [
  "a {repTag} like yours was built for exactly this",
  "everyone on the Street already calls you a {repTag} — let's lean into it",
  "a {repTag} doesn't get there by hesitating",
];
const REPTAG_PLAIN = [
  "It fits a {repTag}; if that's not what you are, pass.",
  "This suits a {repTag} — judge it on that, not on me.",
];

/**
 * Compose a pitch for an NPC of a given family at a given hidden credibility,
 * deterministically keyed by (npcId, dealId).
 *
 * HONEST (cred ≥ 0.6): claim + evidence + downside + disclosed-incentive. Terse.
 * MIXED  (0.35–0.6):   claim + evidence (+ maybe one downside). Medium.
 * CRONY  (cred < 0.35): padding + crony-claim + flattery + urgency +
 *                       hidden-incentive. Long and slippery.
 */
export function assemblePitch(
  family: FamilyId,
  credibility: number,
  ctx: PitchContext,
  npcId: string,
  dealId: string,
): AssembledPitch {
  const v = VOICE[family];
  const seed = `${npcId}|${dealId}`;
  const pct = ctx.successPct != null ? String(ctx.successPct) : '—';
  const repTag = (ctx.repTag && ctx.repTag.length) ? ctx.repTag : '';
  const sub = (s: string) =>
    s.replace(/\{firm\}/g, ctx.firm || 'this name')
     .replace(/\{line\}/g, (ctx.line || 'lending').toLowerCase())
     .replace(/\{pct\}/g, pct)
     .replace(/\{repTag\}/g, repTag || 'serious house');

  if (credibility >= 0.6) {
    // HONEST — TERSE by construction: a single specific/falsifiable claim plus a
    // one-clause downside. No padding, no flattery, no urgency. The shortest of
    // the three registers — length signals evasion, so the honest one is brief.
    // If the bank has a reputation tag, an honest pitch may reference it PLAINLY
    // (a falsifiable fit-claim), never as flattery.
    const parts = [
      sub(pick(v.honestClaim, seed + 'c')),
      sub(pick(v.downside, seed + 'd')),
    ];
    if (repTag) parts.push(sub(pick(REPTAG_PLAIN, seed + 'rt')));
    return { text: parts.join(' '), slots: parts.length };
  }

  if (credibility >= 0.35) {
    // MIXED — a claim, a touch of evidence, and a disclosed incentive. Medium
    // length: more hedged than honest, but still grounded and not slippery.
    const parts = [
      sub(pick(v.honestClaim, seed + 'c')),
      sub(pick(v.evidence, seed + 'e')),
      pick(DISCLOSED_INCENTIVE, seed + 'i'),
    ];
    return { text: parts.join(' '), slots: parts.length };
  }

  // CRONY — PADDED by construction: every evasion marker stacked (padding +
  // vague certainty + flattery + manufactured urgency + a hidden incentive).
  // Always the LONGEST register so verbosity, not insight, marks the crook.
  const parts = [
    pick(PADDING, seed + 'p'),
    sub(pick(v.cronyClaim, seed + 'c')),
    capitalize(repTag ? sub(pick(REPTAG_FLATTERY, seed + 'rt')) : pick(FLATTERY, seed + 'f')) + '.',
    capitalize(pick(URGENCY, seed + 'u')) + '.',
    capitalize(pick(HIDDEN_INCENTIVE, seed + 'h')) + '.',
  ];
  return { text: parts.join(' ').replace(/\s+([,.])/g, '$1').replace(/\.\./g, '.'), slots: parts.length };
}

function capitalize(s: string): string {
  return s.length ? s[0].toUpperCase() + s.slice(1) : s;
}

/** Re-export so callers can color a nameplate / label by family. */
export { FAMILIES };
