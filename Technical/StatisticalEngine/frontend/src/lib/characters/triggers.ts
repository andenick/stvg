/*
 * Character-card trigger helpers (STAR_02 §4.1).
 *
 * Turn engine events / messages into CharacterCard payloads for the queue store.
 * App.svelte calls these from the places where events & messages are already
 * iterated, plus on quarter boundaries. Keeping the mapping here keeps App.svelte
 * readable and gives one place to tune cadence.
 *
 * Speaker resolution: prefer a registry NPC (named recurring cast / president);
 * fall back to an ad-hoc speaker derived from an engine-provided name so an
 * advisor message with only a characterName still gets a stable face + plate.
 */

import { characters, type CardTrigger } from '../stores/characters.svelte';
import { REGISTRY, npcById, presidentForYear, bankerForDivision, credibilityFor, type Npc } from './registry';
import { FAMILIES, familyColor, type FamilyId } from './archetypes-data';
import { assemblePitch } from '../voice/credibility';
import { fmtMoney } from '../util/money';
import type { Emotion } from '../portraits';

/** Map an engine tone/sentiment to a portrait emotion. */
function emotionFor(tone?: string, sentiment?: number): Emotion {
  const t = (tone ?? '').toLowerCase();
  if (t.includes('worried') || t.includes('alarm') || t.includes('fear')) return 'worried';
  if (t.includes('angry') || t.includes('furious')) return 'angry';
  if (t.includes('smug') || t.includes('boast') || t.includes('confident')) return 'smug';
  if (t.includes('happy') || t.includes('pleased') || t.includes('optimistic')) return 'happy';
  if (sentiment != null) {
    if (sentiment < -0.3) return 'worried';
    if (sentiment > 0.3) return 'happy';
  }
  return 'neutral';
}

/** Resolve a speaker (registry NPC or ad-hoc by name) → card identity fields. */
function speakerFields(name: string | undefined, role: string | undefined, npc?: Npc) {
  if (npc) {
    const fam = npc.family ? FAMILIES[npc.family] : null;
    return {
      npcId: npc.id,
      speaker: npc.name,
      role: role ?? npc.role,
      family: npc.family,
      color: fam?.color ?? familyColor(null),
    };
  }
  // Ad-hoc: stable id derived from the name so the same advisor keeps one face.
  const id = `name:${name ?? 'advisor'}`;
  return { npcId: id, speaker: name ?? 'Advisor', role, family: null as FamilyId | null, color: familyColor(null) };
}

/** A character message (quarterly brief / advisor) with a named speaker. */
export function cardFromMessage(m: {
  characterName?: string; role?: string; content?: string; tone?: string; sentiment?: number;
}): boolean {
  const text = (m.content ?? '').trim();
  if (!text || !m.characterName) return false;
  const npc = findNpcByName(m.characterName);
  const f = speakerFields(m.characterName, m.role, npc);
  return characters.enqueue({
    ...f,
    text,
    emotion: emotionFor(m.tone, m.sentiment),
    trigger: 'message',
    dedupKey: `msg:${m.characterName}:${text.slice(0, 40)}`,
  });
}

/** A decision advisor (bull/bear) — characterName provided by the engine. */
export function cardFromAdvisor(rec: {
  characterName?: string; reasoning?: string; argument?: string; recommendation?: string;
}, decisionId: string): boolean {
  const text = (rec.reasoning ?? rec.argument ?? '').trim();
  if (!text || !rec.characterName) return false;
  const npc = findNpcByName(rec.characterName);
  const f = speakerFields(rec.characterName, undefined, npc);
  const k = String(rec.recommendation ?? '').toLowerCase();
  const emotion: Emotion = k === 'oppose' ? 'worried' : k === 'support' ? 'smug' : 'neutral';
  return characters.enqueue({
    ...f, text, emotion, trigger: 'advisor',
    dedupKey: `adv:${decisionId}:${rec.characterName}`,
  });
}

/** Crisis start / end — preempts the queue. Voiced by the risk officer if present. */
export function cardForCrisis(phase: 'start' | 'end', title: string, year: number): boolean {
  const npc = npcById('npc_sol_perez') ?? riskOfficerFallback(year);
  const f = speakerFields(npc.name, npc.role, npc);
  const text = phase === 'start'
    ? `${title}. This is the scenario I flagged. We act now or we explain it to the board later.`
    : `${title} is behind us. We held. Don't mistake survival for safety.`;
  return characters.enqueue({
    ...f, text,
    emotion: phase === 'start' ? 'worried' : 'neutral',
    trigger: phase === 'start' ? 'crisis_start' : 'crisis_end',
    dedupKey: `crisis:${phase}:${title}`,
  });
}

/** Era transition — voiced by the sitting president (felt political texture). */
export function cardForEra(eraName: string, year: number): boolean {
  const pres = presidentForYear(year);
  if (!pres) return false;
  const f = speakerFields(pres.name, pres.role, pres);
  return characters.enqueue({
    ...f,
    text: `A new chapter for the country — and for American finance. ${eraName}. The decisions you make now will be remembered.`,
    emotion: 'neutral', trigger: 'era',
    dedupKey: `era:${eraName}`,
  });
}

/** Annual-report headline moment — voiced by the head of operations. */
export function cardForAnnualReport(year: number, headline: string | undefined): boolean {
  if (!headline) return false;
  const npc = npcById('npc_frank_boone')!;
  const f = speakerFields(npc.name, npc.role, npc);
  return characters.enqueue({
    ...f, text: `${year} is in the books. ${headline}`,
    emotion: 'neutral', trigger: 'annual_report', dedupKey: `report:${year}`,
  });
}

/** Big capital swing (>±10% in a quarter) — voiced by the risk officer (down) or M&A (up). */
export function cardForCapitalSwing(pct: number, year: number): boolean {
  const up = pct > 0;
  const npc = up ? (npcById('npc_marcus_kane') ?? riskOfficerFallback(year)) : (npcById('npc_sol_perez') ?? riskOfficerFallback(year));
  const f = speakerFields(npc.name, npc.role, npc);
  const text = up
    ? `Capital's up ${Math.round(pct)}% this quarter. Good. Now do it again without betting the house.`
    : `We're down ${Math.round(Math.abs(pct))}% this quarter. I'd like to know which desk did that — and so would the regulator.`;
  return characters.enqueue({
    ...f, text, emotion: up ? 'smug' : 'worried',
    trigger: 'capital_swing', dedupKey: `swing:${year}:${Math.round(pct)}`,
  });
}

/** A deal pitch (deal_opened) — voiced by the deal's sourcing banker at credibility. */
export function cardForDealPitch(deal: {
  id: string; clientName?: string; title?: string; requiredLine?: string;
}, year: number, gameId: string | null, successPct: number | null, riskLevel: number): boolean {
  const npc = bankerForDivision(deal.requiredLine, year, deal.id);
  const fam = (npc.family ?? 'dealmaker') as FamilyId;
  const cred = credibilityFor(gameId, npc.id, npc.credibilityTendency);
  const { text } = assemblePitch(
    fam, cred,
    { firm: deal.clientName ?? deal.title ?? 'this name', line: deal.requiredLine ?? 'lending', successPct, riskLevel },
    npc.id, deal.id,
  );
  const f = speakerFields(npc.name, npc.role, npc);
  return characters.enqueue({
    ...f, text, emotion: cred < 0.35 ? 'smug' : 'neutral',
    trigger: 'deal_pitch', dedupKey: `pitch:${deal.id}`,
  });
}

/**
 * A loan outcome (STAR_02 P7) — the sourcing banker crows on a win or eats crow
 * on a default. Voiced by the same division banker the pitch came from (so the
 * "told you so" lands on the right face). realizedPnl is the net P&L on the stake.
 */
export function cardForDealOutcome(o: {
  dealId: string; title?: string; clientName?: string; requiredLine?: string;
  outcome: 'paid_off' | 'defaulted' | 'windfall'; realizedPnl: number;
}, year: number): boolean {
  const npc = bankerForDivision(o.requiredLine, year, o.dealId);
  const f = speakerFields(npc.name, npc.role, npc);
  const firm = o.clientName ?? o.title ?? 'the borrower';
  const amt = fmtMoney(Math.abs(o.realizedPnl));
  const text =
    o.outcome === 'windfall'
      ? `${firm} blew the doors off — ${amt} clear over the stake. Told you that one had legs.`
    : o.outcome === 'paid_off'
      ? `${firm} paid off clean. ${amt} to the book. Boring is beautiful.`
      : `${firm} went bad on us — ${amt} down the drain. I'll own that one.`;
  return characters.enqueue({
    ...f, text,
    emotion: o.outcome === 'defaulted' ? 'worried' : o.outcome === 'windfall' ? 'smug' : 'happy',
    trigger: 'deal_outcome', dedupKey: `outcome:${o.dealId}`,
  });
}

/** A poach offer arriving (STAR_02 P6) — the rival's head pitches their own
 * team in their family's voice. Credibility comes from the team's lead family;
 * a low-credibility rival oversells the track record, an honest one is terse. */
export function cardForPoachOffer(offer: {
  id: string; rivalName: string; divisionName: string; teamSize: number;
  trackRecord: number; yearsTrackRecord: number; archetypeMix?: string[];
}, gameId: string | null): boolean {
  const fam = ((offer.archetypeMix && offer.archetypeMix[0]) ?? 'dealmaker') as FamilyId;
  const family = (fam in FAMILIES) ? fam : 'dealmaker';
  const npcId = `rival:${offer.rivalName}`;
  const cred = credibilityFor(gameId, npcId, FAMILIES[family].credibilityTendency);
  // A short, in-character bark — the rival head pitching their bench.
  const text = cred < 0.4
    ? `My ${offer.divisionName} team is the best on the Street — ${offer.teamSize} killers who printed money for ${offer.yearsTrackRecord} years. Name your price, they're yours.`
    : `${offer.teamSize} of my ${offer.divisionName} people are available. Real book, ${offer.yearsTrackRecord} years of it. Diligence it — they're good, not magic.`;
  return characters.enqueue({
    npcId, speaker: offer.rivalName, role: 'Rival CEO',
    family, color: familyColor(family),
    text, emotion: cred < 0.4 ? 'smug' : 'neutral',
    trigger: 'message', dedupKey: `poach:${offer.id}`,
  });
}

// ── Name → registry NPC resolution ─────────────────────────────────────────
// registry.ts has no import edge back to this module, so a direct import is safe
// (no cycle). The cast is tiny (≈29) — a linear, case-insensitive match is fine.
function findNpcByName(name: string): Npc | undefined {
  const n = name.toLowerCase();
  return REGISTRY.find((x) => x.name.toLowerCase() === n)
      ?? REGISTRY.find((x) => n.includes(x.name.toLowerCase()) || x.name.toLowerCase().includes(n));
}

function riskOfficerFallback(year: number): Npc {
  return npcById('npc_sol_perez')
      ?? bankerForDivision('restructuring', year, 'crisis')
      ?? REGISTRY[0];
}

export type { CardTrigger };
