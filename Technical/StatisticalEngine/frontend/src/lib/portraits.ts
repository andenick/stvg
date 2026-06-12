/*
 * Portrait abstraction layer (STAR_02 §4.4).
 *
 * portraitFor(npcId, emotion?) resolves a portrait URL in priority order:
 *   1. A real generated PNG at `/assets/portraits/<id>_<emotion>.png` — IF the
 *      manifest lists it. We probe ONE manifest (`/assets/portraits/manifest.json`)
 *      once, not a 404-per-image storm. The P9 RTX-5090 pipeline backfills hero
 *      portraits here without touching game code.
 *   2. DiceBear LOCAL generation — a deterministic, OFFLINE SVG data-URI seeded by
 *      npcId (`@dicebear/personas`, the cartoon-business band the owner confirmed).
 *      No network, no asset pipeline required for the playtest.
 *
 * Emotions are advisory: if a PNG variant for the emotion isn't in the manifest we
 * fall back to the neutral PNG, then to DiceBear (which is emotion-agnostic for
 * now — DiceBear seeds are stable per-npc so the same face always appears).
 */

import { createAvatar } from '@dicebear/core';
import * as personas from '@dicebear/personas';
import { npcById } from './characters/registry';

export type Emotion = 'neutral' | 'happy' | 'worried' | 'angry' | 'smug';

const MANIFEST_URL = '/assets/portraits/manifest.json';

/** Manifest shape: { "<id>_<emotion>.png": true, ... } or a string[] of filenames. */
let manifest: Set<string> | null = null;
let manifestPromise: Promise<Set<string>> | null = null;

/**
 * Fetch the portrait manifest once. The server ships `{}` until real assets land,
 * so the common path is an empty set → DiceBear everywhere. Best-effort: any
 * failure yields an empty manifest (DiceBear fallback), never throws.
 */
function loadManifest(): Promise<Set<string>> {
  if (manifest) return Promise.resolve(manifest);
  if (manifestPromise) return manifestPromise;
  manifestPromise = fetch(MANIFEST_URL)
    .then((r) => (r.ok ? r.json() : {}))
    .then((j: unknown) => {
      const set = new Set<string>();
      if (Array.isArray(j)) for (const f of j) if (typeof f === 'string') set.add(f);
      else if (j && typeof j === 'object') for (const k of Object.keys(j as object)) set.add(k);
      manifest = set;
      return set;
    })
    .catch(() => {
      manifest = new Set<string>();
      return manifest;
    });
  return manifestPromise;
}

// Kick the manifest fetch eagerly so the first card has it ready.
if (typeof fetch !== 'undefined') void loadManifest();

/** DiceBear cache — generating SVG is cheap but we cache per (id, emotion). */
const diceCache = new Map<string, string>();

function diceBearUri(seed: string): string {
  const cached = diceCache.get(seed);
  if (cached) return cached;
  // `personas` is passed as the style object directly (DiceBear v9 API).
  const uri = createAvatar(personas, { seed, radius: 50, backgroundColor: ['transparent'] }).toDataUri();
  diceCache.set(seed, uri);
  return uri;
}

/**
 * Synchronous best-effort portrait URL. Returns a real PNG path if the manifest
 * (already loaded) lists it, else a DiceBear data-URI. Synchronous so a Svelte
 * `$derived` can call it without await; the manifest is fetched eagerly at module
 * load, so by the time any card shows it is almost always resolved.
 */
export function portraitFor(npcId: string, emotion: Emotion = 'neutral'): string {
  const npc = npcById(npcId);
  const seed = npc?.portraitSeed ?? npcId;

  if (manifest && manifest.size) {
    const want = `${seed}_${emotion}.png`;
    const neutral = `${seed}_neutral.png`;
    if (manifest.has(want)) return `/assets/portraits/${want}`;
    if (manifest.has(neutral)) return `/assets/portraits/${neutral}`;
  }
  // DiceBear fallback (offline, deterministic). Emotion folded into the seed only
  // if a real asset would have differed — for DiceBear we keep one stable face per
  // npc so the character is recognisable across appearances.
  return diceBearUri(seed);
}

/**
 * For ad-hoc speakers that aren't in the registry (engine CharacterMessage /
 * CharacterRecommendation with only a name), derive a stable seed from the name
 * so the same advisor keeps the same face.
 */
export function portraitForName(name: string, emotion: Emotion = 'neutral'): string {
  void emotion;
  return diceBearUri(`name:${name}`);
}
