/*
 * NPC registry + recurring cast (STAR_02 §4.2/§4.3).
 *
 * A small cast of NAMED, recurring NPCs minted from the canonical archetype
 * specializations (one per family + a few wildcards). Each carries a HIDDEN
 * credibility ∈ [0,1] seeded from its family's credibilityTendency, jittered
 * per-game so trust is LEARNABLE within a run and stable across re-renders.
 *
 * PLUS US presidents 1945→2040 (real through the 2020s, then plausible
 * fictional-future placeholders) as registry entries with era ranges. Portraits
 * resolve through the same `portraitFor()` path; real portrait assets get
 * backfilled later (owner-confirmed STAR_02 Addendum A.5).
 *
 * Credibility is NEVER shown to the player. It biases the credibility
 * line-assembler (honest/terse vs crony/verbose) so the player vibe-reads trust.
 *
 * Determinism: the per-game jitter is drawn from a hash of (gameId, npcId) and
 * persisted in localStorage keyed by gameId, so the same NPC reads the same way
 * for the whole run and survives reloads.
 */

import { FAMILIES, FAMILY_IDS, type FamilyId } from './archetypes-data';

export type NpcKind = 'banker' | 'president' | 'rival';

export interface Npc {
  id: string;
  name: string;
  family: FamilyId | null;        // null for presidents (no archetype family)
  specialization: string | null;  // archetypes.json specialization id (banker cast)
  role: string;
  kind: NpcKind;
  /** Inclusive game-year window in which this NPC can appear. */
  eraFrom: number;
  eraTo: number;
  /** Family credibility prior (banker cast). Presidents: a neutral 0.5 default. */
  credibilityTendency: number;
  /** Portrait seed for DiceBear (deterministic, offline). */
  portraitSeed: string;
}

/*
 * The recurring banker cast (12). One representative per family plus wildcards —
 * the owner's named trio is covered: the Relationship Man (commercial banker),
 * the Gunslinger (trader), the Doom Prophet (risk officer), a crony fixer, and an
 * honest quant. `specialization` ties each to a row in archetypes.json so the
 * voice/credibility flavor stays consistent with the canonical roster.
 */
const BANKER_CAST: Omit<Npc, 'credibilityTendency' | 'portraitSeed' | 'kind'>[] = [
  { id: 'npc_walt_brennan',   name: 'Walt Brennan',    family: 'patrician',  specialization: 'relationship_man', role: 'Commercial Banker',     eraFrom: 1945, eraTo: 1990 },
  { id: 'npc_dutch_calloway', name: 'Dutch Calloway',  family: 'gunslinger', specialization: 'tape_reader',      role: 'Head Trader',           eraFrom: 1945, eraTo: 1995 },
  { id: 'npc_ray_vance',      name: 'Ray Vance',        family: 'gunslinger', specialization: 'trading_gunslinger', role: 'Prop Desk Lead',      eraFrom: 1975, eraTo: 2015 },
  { id: 'npc_ada_okafor',     name: 'Dr. Ada Okafor',  family: 'quant',      specialization: 'quant_trader',     role: 'Head of Systematic',    eraFrom: 1985, eraTo: 2040 },
  { id: 'npc_sol_perez',      name: 'Dr. Sol Perez',   family: 'quant',      specialization: 'doom_prophet',     role: 'Chief Risk Officer',    eraFrom: 1980, eraTo: 2040 },
  { id: 'npc_marcus_kane',    name: 'Marcus Kane',     family: 'dealmaker',  specialization: 'lbo_baron',        role: 'Head of M&A',           eraFrom: 1980, eraTo: 2040 },
  { id: 'npc_vivian_holt',    name: 'Vivian Holt',     family: 'rainmaker',  specialization: 'private_banker',   role: 'Head of Private Bank',  eraFrom: 1960, eraTo: 2040 },
  { id: 'npc_frank_boone',    name: 'Frank Boone',     family: 'operator',   specialization: 'doc_checker',      role: 'Head of Operations',    eraFrom: 1945, eraTo: 2040 },
  { id: 'npc_eleanor_bishop', name: 'Eleanor Bishop',  family: 'lifer',      specialization: 'compliance_lifer', role: 'Chief Compliance',      eraFrom: 1945, eraTo: 2040 },
  { id: 'npc_hank_ramirez',   name: 'Hank Ramirez',    family: 'prodigy',    specialization: 'tech_founder_whisperer', role: 'Ventures Associate', eraFrom: 1995, eraTo: 2040 },
  // Wildcards: the crony fixer (low credibility) and the honest credit hawk.
  { id: 'npc_buddy_grasso',   name: 'Buddy "the Senator\'s friend" Grasso', family: 'dealmaker', specialization: 'empire_builder', role: 'Government Affairs', eraFrom: 1950, eraTo: 2040 },
  { id: 'npc_patricia_chen',  name: 'Patricia Chen',   family: 'lifer',      specialization: 'credit_hawk',      role: 'Senior Credit Officer', eraFrom: 1955, eraTo: 2040 },
];

/*
 * US presidents 1945→2040. Real names through the current administration; the
 * 2030s+ entries are deliberately fictional placeholders (the game runs to 2040).
 * Era ranges are inclusive inauguration→end-of-term game-years.
 */
const PRESIDENTS: { id: string; name: string; from: number; to: number }[] = [
  { id: 'pres_truman',     name: 'Harry S. Truman',      from: 1945, to: 1953 },
  { id: 'pres_eisenhower', name: 'Dwight D. Eisenhower',  from: 1953, to: 1961 },
  { id: 'pres_kennedy',    name: 'John F. Kennedy',       from: 1961, to: 1963 },
  { id: 'pres_johnson',    name: 'Lyndon B. Johnson',     from: 1963, to: 1969 },
  { id: 'pres_nixon',      name: 'Richard Nixon',         from: 1969, to: 1974 },
  { id: 'pres_ford',       name: 'Gerald Ford',           from: 1974, to: 1977 },
  { id: 'pres_carter',     name: 'Jimmy Carter',          from: 1977, to: 1981 },
  { id: 'pres_reagan',     name: 'Ronald Reagan',         from: 1981, to: 1989 },
  { id: 'pres_bush41',     name: 'George H. W. Bush',     from: 1989, to: 1993 },
  { id: 'pres_clinton',    name: 'Bill Clinton',          from: 1993, to: 2001 },
  { id: 'pres_bush43',     name: 'George W. Bush',        from: 2001, to: 2009 },
  { id: 'pres_obama',      name: 'Barack Obama',          from: 2009, to: 2017 },
  { id: 'pres_trump',      name: 'Donald Trump',          from: 2017, to: 2021 },
  { id: 'pres_biden',      name: 'Joe Biden',             from: 2021, to: 2025 },
  { id: 'pres_trump2',     name: 'Donald Trump',          from: 2025, to: 2029 },
  // Fictional future placeholders (the sim runs past the present day).
  { id: 'pres_future_a',   name: 'President (48th)',      from: 2029, to: 2037 },
  { id: 'pres_future_b',   name: 'President (49th)',      from: 2037, to: 2041 },
];

/** djb2-style string hash → unsigned int. Deterministic, no Math.random. */
function hash(s: string): number {
  let h = 5381;
  for (let i = 0; i < s.length; i++) h = ((h << 5) + h + s.charCodeAt(i)) | 0;
  return h >>> 0;
}

/** Deterministic [0,1) from a string seed. */
function unit(seed: string): number {
  return (hash(seed) % 100000) / 100000;
}

/** Clamp helper. */
function clamp01(v: number): number {
  return v < 0 ? 0 : v > 1 ? 1 : v;
}

const JITTER_KEY = (gameId: string) => `stvg.npcCred.${gameId}`;

/**
 * Per-game credibility jitter, persisted in localStorage keyed by gameId. Each
 * NPC's realized credibility = familyTendency + jitter(±0.18), clamped to [0,1].
 * Drawn deterministically from hash(gameId, npcId) so a missing/cleared store
 * regenerates identically — the localStorage copy is a cache, not the source.
 */
function loadJitter(gameId: string): Record<string, number> {
  try {
    const raw = localStorage.getItem(JITTER_KEY(gameId));
    if (raw) return JSON.parse(raw) as Record<string, number>;
  } catch { /* ignore */ }
  return {};
}

function saveJitter(gameId: string, map: Record<string, number>) {
  try { localStorage.setItem(JITTER_KEY(gameId), JSON.stringify(map)); } catch { /* ignore */ }
}

/**
 * The realized per-NPC credibility for a given game. Deterministic per
 * (gameId, npcId); persisted so it is stable across reloads within a run.
 */
export function credibilityFor(gameId: string | null, npcId: string, tendency: number): number {
  const gid = gameId ?? 'default';
  const jitterMap = loadJitter(gid);
  if (npcId in jitterMap) return clamp01(jitterMap[npcId]);
  // ±0.18 jitter from a stable hash; widen the spread for wildcards via the seed.
  const j = (unit(`${gid}|${npcId}|cred`) - 0.5) * 0.36;
  const value = clamp01(tendency + j);
  jitterMap[npcId] = value;
  saveJitter(gid, jitterMap);
  return value;
}

/** Build a full Npc record (banker cast) with credibility prior + portrait seed. */
function bankerNpc(base: Omit<Npc, 'credibilityTendency' | 'portraitSeed' | 'kind'>): Npc {
  const fam = base.family ? FAMILIES[base.family] : null;
  return {
    ...base,
    kind: 'banker',
    credibilityTendency: fam?.credibilityTendency ?? 0.5,
    portraitSeed: base.id,
  };
}

function presidentNpc(p: { id: string; name: string; from: number; to: number }): Npc {
  return {
    id: p.id, name: p.name, family: null, specialization: null,
    role: 'President of the United States', kind: 'president',
    eraFrom: p.from, eraTo: p.to, credibilityTendency: 0.5, portraitSeed: p.id,
  };
}

/** The full immutable registry: 12 bankers + 17 presidents. */
export const REGISTRY: Npc[] = [
  ...BANKER_CAST.map(bankerNpc),
  ...PRESIDENTS.map(presidentNpc),
];

const BY_ID = new Map(REGISTRY.map((n) => [n.id, n]));

export function npcById(id: string): Npc | undefined {
  return BY_ID.get(id);
}

/** Banker cast members active in a given game-year. */
export function bankersInEra(year: number): Npc[] {
  return REGISTRY.filter((n) => n.kind === 'banker' && year >= n.eraFrom && year <= n.eraTo);
}

/** The sitting president for a game-year (first match), if any. */
export function presidentForYear(year: number): Npc | undefined {
  return REGISTRY.find((n) => n.kind === 'president' && year >= n.eraFrom && year < n.eraTo)
      ?? REGISTRY.find((n) => n.kind === 'president' && year >= n.eraFrom && year <= n.eraTo);
}

/**
 * Pick the recurring banker that best matches a deal's division + era. Used by
 * the DealBoard when the player has NO per-employee identity to attach a pitch
 * to (we never invent a fake "hired staff" name — see STAR_02 §4.2). Falls back
 * to a deterministic era-appropriate pick keyed by the deal id so the same deal
 * always draws the same NPC.
 */
export function bankerForDivision(division: string | undefined, year: number, dealId: string): Npc {
  const pool = bankersInEra(year);
  const usable = pool.length ? pool : REGISTRY.filter((n) => n.kind === 'banker');
  // Prefer a specialization whose canonical affinity contains the division.
  // We don't ship the full affinity map client-side, so approximate by a loose
  // family↔division heuristic; otherwise a stable hash pick keeps it deterministic.
  const divLc = (division ?? '').toLowerCase();
  const affinity: Record<string, FamilyId[]> = {
    trading: ['gunslinger', 'quant'], derivatives: ['quant', 'gunslinger'],
    commercial_lending: ['patrician', 'lifer', 'operator'], mortgage_lending: ['operator', 'gunslinger'],
    investment_banking: ['dealmaker', 'rainmaker'], private_equity: ['dealmaker'],
    restructuring: ['quant', 'dealmaker'], securitization: ['quant', 'gunslinger'],
    asset_management: ['quant', 'rainmaker'], wealth_management: ['rainmaker', 'patrician'],
    trust_custody: ['patrician', 'lifer'], international: ['dealmaker', 'patrician'],
    venture_capital: ['dealmaker', 'prodigy'], fintech: ['prodigy', 'operator'],
    crypto_custody: ['gunslinger'], credit_cards: ['operator', 'quant'],
  };
  const wantFamilies = affinity[divLc] ?? FAMILY_IDS;
  const matches = usable.filter((n) => n.family && wantFamilies.includes(n.family));
  const candidates = matches.length ? matches : usable;
  return candidates[hash(dealId) % candidates.length];
}
