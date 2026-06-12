/*
 * Banker voices — COMPATIBILITY SHIM (STAR_02 §4.2/§4.3).
 *
 * The old hand-authored phrasebook + deal-id-hash archetype assignment is gone.
 * Pitches now come from a REAL source: a recurring registry NPC matched to the
 * deal's division and era (`registry.bankerForDivision`), spoken through the
 * credibility line-assembler (`voice/credibility.assemblePitch`) at that NPC's
 * hidden, per-game credibility. The same deal always draws the same NPC and the
 * same line (deterministic seed), and an honest NPC's pitch is SHORTER than a
 * crony's — length signals evasion.
 *
 * This module stays as a thin shim so existing call sites keep working while the
 * substance moved to registry.ts + voice/credibility.ts. New code should call
 * those directly.
 */

import { bankerForDivision, credibilityFor, type Npc } from './characters/registry';
import { assemblePitch } from './voice/credibility';
import { FAMILIES } from './characters/archetypes-data';

export interface Banker {
  archetype: string;
  name: string;
  npc: Npc;
}

/** Resolve the recurring NPC that pitches a given deal (division + era aware). */
export function bankerForDeal(dealId: string, division?: string, year = 1945): Banker {
  const npc = bankerForDivision(division, year, dealId);
  const fam = npc.family ? FAMILIES[npc.family] : null;
  return { archetype: fam?.displayName ?? 'Advisor', name: npc.name, npc };
}

/**
 * Compose a deal pitch in the sourcing banker's voice at their hidden credibility.
 * Signature preserved (dealId, firm, line, riskLevel) + optional (division, year,
 * gameId, successPct, stake) so the credibility/era machinery can engage.
 */
export function pitchForDeal(
  dealId: string,
  firm: string,
  line: string,
  riskLevel: number,
  opts: { division?: string; year?: number; gameId?: string | null; successPct?: number | null; stake?: string } = {},
): { banker: Banker; pitch: string; credibility: number } {
  const year = opts.year ?? 1945;
  const banker = bankerForDeal(dealId, opts.division ?? line, year);
  const fam = banker.npc.family ?? 'dealmaker';
  const credibility = credibilityFor(opts.gameId ?? null, banker.npc.id, banker.npc.credibilityTendency);
  const { text } = assemblePitch(
    fam,
    credibility,
    { firm, line, riskLevel, successPct: opts.successPct ?? null, stake: opts.stake },
    banker.npc.id,
    dealId,
  );
  return { banker, pitch: text, credibility };
}
