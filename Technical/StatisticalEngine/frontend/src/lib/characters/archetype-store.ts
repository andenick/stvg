/*
 * Archetype roster store (W3 — single source of truth).
 *
 * The canonical roster lives in the ENGINE tree (`data/archetypes/archetypes.json`)
 * and is served verbatim by `GET /api/archetypes`. This module fetches it ONCE
 * during game bootstrap (TitleScreen begin/continue, before routing into the
 * game) and exposes the family + specialization display slice the frontend
 * consumes. There are NO data literals here or in archetypes-data.ts /
 * specializations.ts anymore — they are thin typed accessors over this store, so
 * the engine math and the client voice/portrait/hire UI can never drift.
 *
 * Shape: a plain module object populated synchronously once the fetch resolves.
 * The data is immutable for the session and is loaded BEFORE any game surface
 * renders, so it needs no Svelte reactivity — the accessors read it directly.
 *
 * Resilience: `hydrateArchetypes()` is idempotent and best-effort. If the fetch
 * fails (offline / server down), the maps stay empty and the accessors return
 * safe fallbacks (neutral color, empty roster) so the title flow never blocks.
 */

export type FamilyId =
  | 'patrician' | 'gunslinger' | 'quant' | 'dealmaker'
  | 'operator' | 'rainmaker' | 'prodigy' | 'lifer';

export interface ArchetypeFamily {
  id: FamilyId;
  displayName: string;
  /** 0..1 prior for the per-NPC hidden credibility (high = honest/terse). */
  credibilityTendency: number;
  /** Seed line for the credibility assembler's slot pools. */
  voiceFlavor: string;
  /** Hero-portrait descriptor (informs the P9 art pipeline; unused by DiceBear). */
  portraitDescriptor: string;
  /** Nameplate accent — color-codes the portrait card by family. */
  color: string;
}

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

/** Neutral fallback color (matches the old familyColor default). */
export const NEUTRAL_COLOR = '#8a8577';

// ── Module-level store (populated once by hydrateArchetypes) ───────────────
const families: Record<string, ArchetypeFamily> = {};
const familyIds: FamilyId[] = [];
const specializations: Record<string, SpecializationDisplay> = {};
let hydrated = false;
let inflight: Promise<void> | null = null;

// The 10-stat order from the canonical JSON (stat_order), used to derive each
// specialization's two stat highlights from its statWeightDeltas.
const DEFAULT_STAT_ORDER = [
  'analytical', 'intuition', 'judgment', 'persuasion', 'networking',
  'leadership', 'ambition', 'resilience', 'stamina', 'adaptability',
];

interface RawFamily {
  id: string;
  displayName?: string;
  color?: string;
  credibilityTendency?: number;
  voiceFlavor?: string;
  portraitDescriptor?: string;
}

interface RawSpec {
  id: string;
  family: string;
  displayName?: string;
  eraRange?: { from?: number; to?: number };
  statWeightDeltas?: number[];
  voiceFlavor?: string;
}

interface RawRoster {
  stat_order?: string[];
  families?: RawFamily[];
  specializations?: RawSpec[];
}

/**
 * Derive the two stat highlights a specialization leans into from its
 * statWeightDeltas (the two largest positive deltas, in stat_order). This is the
 * same display intent the old hand-mirror baked by hand — now computed from the
 * canonical deltas so it can never disagree with the engine.
 */
function deriveStatHi(deltas: number[] | undefined, order: string[]): string[] {
  if (!deltas?.length) return [];
  const pairs = deltas
    .map((v, i) => [order[i] ?? `stat${i}`, v] as [string, number])
    .filter(([, v]) => v > 0)
    .sort((a, b) => b[1] - a[1]);
  if (pairs.length === 0) {
    // No positive delta — fall back to the single largest magnitude so the
    // detail view still shows a lean rather than nothing.
    const top = deltas
      .map((v, i) => [order[i] ?? `stat${i}`, Math.abs(v)] as [string, number])
      .sort((a, b) => b[1] - a[1])[0];
    return top ? [top[0]] : [];
  }
  return pairs.slice(0, 2).map(([k]) => k);
}

function ingest(raw: RawRoster): void {
  const order = raw.stat_order?.length ? raw.stat_order : DEFAULT_STAT_ORDER;

  for (const f of raw.families ?? []) {
    if (!f.id) continue;
    const fam: ArchetypeFamily = {
      id: f.id as FamilyId,
      displayName: f.displayName ?? f.id,
      credibilityTendency: f.credibilityTendency ?? 0.5,
      voiceFlavor: f.voiceFlavor ?? '',
      portraitDescriptor: f.portraitDescriptor ?? '',
      color: f.color ?? NEUTRAL_COLOR,
    };
    families[fam.id] = fam;
    familyIds.push(fam.id);
  }

  for (const s of raw.specializations ?? []) {
    if (!s.id) continue;
    specializations[s.id] = {
      id: s.id,
      displayName: s.displayName ?? s.id,
      family: s.family as FamilyId,
      eraFrom: s.eraRange?.from ?? 1945,
      eraTo: s.eraRange?.to ?? 2040,
      statHi: deriveStatHi(s.statWeightDeltas, order),
      voiceFlavor: s.voiceFlavor ?? '',
    };
  }
  hydrated = true;
}

/**
 * Fetch + ingest the canonical roster from `GET /api/archetypes`. Idempotent:
 * concurrent or repeat calls share one in-flight promise and a populated store
 * is never re-fetched. Best-effort — a failed fetch leaves the store empty and
 * the accessors fall back, so the title flow is never blocked.
 */
export function hydrateArchetypes(): Promise<void> {
  if (hydrated) return Promise.resolve();
  if (inflight) return inflight;
  inflight = (async () => {
    try {
      const res = await fetch('/api/archetypes');
      if (res.ok) {
        const raw = (await res.json()) as RawRoster;
        ingest(raw);
      }
    } catch {
      /* offline / server down — accessors fall back to safe defaults */
    } finally {
      inflight = null;
    }
  })();
  return inflight;
}

/** True once the canonical roster has been ingested at least once. */
export function archetypesReady(): boolean {
  return hydrated;
}

// ── Accessors (consumed by archetypes-data.ts / specializations.ts) ─────────
export function getFamilies(): Record<string, ArchetypeFamily> {
  return families;
}
export function getFamilyIds(): FamilyId[] {
  return familyIds;
}
export function getFamily(id: string | undefined | null): ArchetypeFamily | undefined {
  return id ? families[id] : undefined;
}
export function getSpecializations(): Record<string, SpecializationDisplay> {
  return specializations;
}
export function getSpecialization(id: string | undefined | null): SpecializationDisplay | null {
  if (!id) return null;
  return specializations[id] ?? null;
}
