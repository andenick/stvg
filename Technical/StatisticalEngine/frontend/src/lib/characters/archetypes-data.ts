/*
 * Frontend archetype family accessor (W3 — single source of truth).
 *
 * This file used to hand-mirror the 8 family records (credibilityTendency /
 * voiceFlavor / portraitDescriptor / nameplate color) from the canonical
 * `data/archetypes/archetypes.json`. That mirror could DRIFT from the engine.
 *
 * It is now a THIN, DATA-LITERAL-FREE accessor over `archetype-store.ts`, which
 * fetches the canonical roster once from `GET /api/archetypes` during game
 * bootstrap. The family color moved INTO archetypes.json (a per-family `color`
 * field), so even the nameplate accent now comes from the single source.
 *
 * The exported surface (`FamilyId`, `ArchetypeFamily`, `FAMILIES`, `FAMILY_IDS`,
 * `familyColor`) is preserved so existing consumers compile unchanged. `FAMILIES`
 * and `FAMILY_IDS` are LIVE references to the store's maps — they are populated
 * by `hydrateArchetypes()` before any game surface renders, so reads at use-time
 * (inside functions / Svelte render) always see the real data.
 */

import {
  getFamilies,
  getFamilyIds,
  NEUTRAL_COLOR,
  type ArchetypeFamily,
  type FamilyId,
} from './archetype-store';

export type { FamilyId, ArchetypeFamily };

/**
 * The 8 engine families, sourced from the canonical roster via the fetch store.
 * Live reference: empty until `hydrateArchetypes()` resolves at bootstrap, fully
 * populated for every game-surface read thereafter.
 */
export const FAMILIES: Record<string, ArchetypeFamily> = getFamilies();

/** Live list of family ids (populated by hydrateArchetypes at bootstrap). */
export const FAMILY_IDS: FamilyId[] = getFamilyIds();

/** Nameplate accent for a family id, with a neutral fallback. */
export function familyColor(id: string | undefined | null): string {
  if (id && id in FAMILIES) return FAMILIES[id].color;
  return NEUTRAL_COLOR;
}
