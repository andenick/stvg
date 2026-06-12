/*
 * Frontend archetype seed — the credibility/voice/portrait slice of the canonical
 * registry `data/archetypes/archetypes.json` (STAR_02 §4.2/§11).
 *
 * WHY a hand-mirrored TS file instead of `import archetypes.json`:
 *   - The canonical JSON lives in the ENGINE tree (`data/archetypes/`), OUTSIDE
 *     `frontend/src`. Importing across the tsconfig rootDir with
 *     verbatimModuleSyntax + resolveJsonModule is fragile and pulls a 700-line
 *     data file into the bundle for the ~5 fields the client actually needs.
 *   - We only consume the *family-level* credibilityTendency / voiceFlavor /
 *     portraitDescriptor (the credibility line-assembler seeds its slot pools and
 *     the portrait layer reads the descriptor). The full β/σ/statWeights live
 *     engine-side and never reach the client.
 *
 * If the canonical families change, mirror the 8 entries here. The specialization
 * roster (used to mint the recurring cast) lives in `registry.ts`.
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

/**
 * The 8 engine families, faithfully mirrored from archetypes.json. `color` is the
 * nameplate accent for the portrait card (not in the canonical JSON — a frontend
 * presentation concern, chosen to read distinctly against the era palettes).
 */
export const FAMILIES: Record<FamilyId, ArchetypeFamily> = {
  patrician: {
    id: 'patrician',
    displayName: 'The Patrician',
    credibilityTendency: 0.72,
    voiceFlavor: "Speaks in understatement and pedigree: 'We've banked the family since the war. The number is sound.'",
    portraitDescriptor: 'Silver-haired in a three-piece chalk-stripe, club tie, half-moon glasses; serene, a little bored.',
    color: '#9a8c98',
  },
  gunslinger: {
    id: 'gunslinger',
    displayName: 'The Gunslinger',
    credibilityTendency: 0.30,
    voiceFlavor: "Loud, certain, urgent: 'This is a layup. Size it up or get out of my way.'",
    portraitDescriptor: 'Sleeves rolled, tie loose, sweat at the collar, two phones; grinning like he already won.',
    color: '#c0563f',
  },
  quant: {
    id: 'quant',
    displayName: 'The Quant',
    credibilityTendency: 0.66,
    voiceFlavor: "Precise, hedged, falsifiable: 'Backtest says 1.4 Sharpe out of sample; it breaks if vol regimes shift.'",
    portraitDescriptor: 'Rumpled academic, lanyard, three monitors of Greek letters; talks to the screen, not you.',
    color: '#4a90b8',
  },
  dealmaker: {
    id: 'dealmaker',
    displayName: 'The Dealmaker',
    credibilityTendency: 0.45,
    voiceFlavor: "Smooth, flattering, name-dropping: 'I had the chairman on the phone this morning. This gets done by Friday.'",
    portraitDescriptor: 'Sharp suit, pocket square, mid-handshake; the smile reaches the eyes only sometimes.',
    color: '#c79a3c',
  },
  operator: {
    id: 'operator',
    displayName: 'The Operator',
    credibilityTendency: 0.68,
    voiceFlavor: "Plain, process-first: 'We can scale that to forty branches by Q3 if we fix settlement first.'",
    portraitDescriptor: 'Short sleeves, clipboard, ID badge; mid-sentence pointing at a flowchart.',
    color: '#6b8e6b',
  },
  rainmaker: {
    id: 'rainmaker',
    displayName: 'The Rainmaker',
    credibilityTendency: 0.50,
    voiceFlavor: "Warm, personal, unhurried: 'The Hartfords trust me, not the bank. They'll move when I move.'",
    portraitDescriptor: 'Tanned, golf-tan watch line, blazer no tie, a Rolodex of a smile.',
    color: '#c98a5a',
  },
  prodigy: {
    id: 'prodigy',
    displayName: 'The Prodigy',
    credibilityTendency: 0.55,
    voiceFlavor: "Eager, a little cocky, unpolished: 'I know it sounds crazy but the data is screaming it.'",
    portraitDescriptor: 'Too-big suit or a hoodie, energy-drink can, wide eyes; younger than everyone in the room.',
    color: '#a87fc0',
  },
  lifer: {
    id: 'lifer',
    displayName: 'The Lifer',
    credibilityTendency: 0.78,
    voiceFlavor: "Dry, institutional memory: 'We tried this in '74. It didn't end the way you think.'",
    portraitDescriptor: 'Cardigan over a shirt, reading glasses on a chain, a coffee mug older than the intern; unbothered.',
    color: '#8a8577',
  },
};

export const FAMILY_IDS = Object.keys(FAMILIES) as FamilyId[];

/** Nameplate accent for a family id, with a neutral fallback. */
export function familyColor(id: string | undefined | null): string {
  if (id && id in FAMILIES) return FAMILIES[id as FamilyId].color;
  return '#8a8577';
}
