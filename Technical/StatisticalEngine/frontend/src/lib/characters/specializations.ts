/*
 * Specialization display accessor (W3 — single source of truth).
 *
 * This file used to hand-mirror all 33 specialization display records
 * (displayName / family / era window / statHi / voiceFlavor) from the canonical
 * `data/archetypes/archetypes.json`. That mirror could DRIFT from the engine.
 *
 * It is now a THIN, DATA-LITERAL-FREE accessor over `archetype-store.ts`, which
 * fetches the canonical roster once from `GET /api/archetypes` at game bootstrap.
 * `statHi` is derived from each spec's canonical `statWeightDeltas` (the two
 * largest positive deltas), so the two-stat lean shown in the hire detail view
 * can never disagree with the engine's stat math.
 *
 * The exported surface (`SpecializationDisplay`, `SPECIALIZATIONS`,
 * `specializationFor`, `eraFlavor`) is preserved so existing consumers compile
 * unchanged. `SPECIALIZATIONS` is a LIVE reference to the store map, populated
 * before any hire surface renders.
 */

import {
  getSpecializations,
  getSpecialization,
  type SpecializationDisplay,
} from './archetype-store';

export type { SpecializationDisplay };

/**
 * The full specialization display map, sourced from the canonical roster via the
 * fetch store. Live reference: populated by `hydrateArchetypes()` at bootstrap.
 */
export const SPECIALIZATIONS: Record<string, SpecializationDisplay> = getSpecializations();

/** Look up a specialization's display record by id (null when absent). */
export function specializationFor(id: string | undefined | null): SpecializationDisplay | null {
  return getSpecialization(id);
}

/** A one-line era-flavor string ("1945–1985") for a specialization. */
export function eraFlavor(s: SpecializationDisplay): string {
  const to = s.eraTo >= 2040 ? 'today' : String(s.eraTo);
  return `${s.eraFrom}–${to}`;
}
