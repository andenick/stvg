# STVG Art Pipeline — Operating Manual

<!-- STATUS: SCAFFOLDED 2026-06-12 — NOTHING HERE HAS RUN. A0 requires owner go-ahead. -->

> **HARD STATUS: this is files-only scaffolding.** No ComfyUI, no models, no Blender,
> no generation, and no GPU process have been touched. Everything below is the
> *design* of the pipeline. The first executable session (A0, "rig the loop") may
> begin **only when the owner says go** (see
> `../Plans/ART_EXPLORATION_PLAN.md` for the gated execution plan).

This is the operating manual for STVG's visual asset pipeline: how Claude drives a
local image-generation loop, how generated assets enter the game with zero code
changes, how provenance is recorded, and the hard GPU-coexistence rule. It is the
companion reference to:

- **`../Plans/ART_PIPELINE_PLAN.md`** — the strategic plan (research verdicts,
  asset-class table, workstreams A0–A5).
- **`../Plans/ART_EXPLORATION_PLAN.md`** — the owner-gated session-by-session
  execution plan (E0 environment → E4 cards/flourishes).

---

## 1. The agent generation loop (ComfyUI HTTP API)

The substrate is **ComfyUI's HTTP API**, driven by `comfy_loop.py` (in this dir).
The loop is the tightest possible *generate → inspect → refine* cycle, with Claude
inspecting the rendered PNG natively (Read tool on the file) and mutating the
workflow JSON between iterations:

```
            ┌─────────────────────────────────────────────────────────┐
            │  1. Load a workflow template JSON (workflows/*.json)      │
            │  2. Mutate node inputs by node TITLE:                     │
            │       - positive prompt text   (set_prompt)               │
            │       - seed                    (set_seed)                │
            │       - LoRA strength / denoise (set_lora / set_denoise)  │
            │  3. POST the workflow dict to  /prompt   → {prompt_id}    │
            │  4. Poll  /history/{prompt_id}  until outputs appear      │
            │  5. Download the output PNG(s) to an out dir             │
            │  6. *** Claude reads the PNG and judges it ***            │
            │  7. Decide: ship it, or mutate (step 2) and resubmit      │
            └─────────────────────────────────────────────────────────┘
```

`comfy_loop.py` implements steps 1–5 (submit / poll / fetch) and the step-2 mutate
helpers. Steps 6–7 are Claude's judgement in the conversation — there is no
"quality model"; the agent looks at the image and decides. MCP (artokun/comfyui-mcp)
is an optional ergonomic wrapper over the same HTTP endpoints; the stdlib client is
the canonical, dependency-free path.

**Endpoints used** (ComfyUI default `127.0.0.1:8188`):
- `POST /prompt` — body `{"prompt": <workflow_dict>}` → `{"prompt_id": "..."}`.
- `GET /history/{prompt_id}` — returns node outputs once the run completes; the
  `images` entries name `filename` / `subfolder` / `type`.
- `GET /view?filename=...&subfolder=...&type=output` — fetches the PNG bytes.

### Mutation by node *title*, not numeric id
Workflow node ids are brittle (they change when a graph is re-exported). The mutate
helpers therefore locate nodes by their `_meta.title` (the human label you set in
the ComfyUI graph editor), e.g. a node titled `POSITIVE_PROMPT`, `SEED`,
`LORA_STRENGTH`. **Convention: every editable node in a shipped workflow template
carries a stable, SCREAMING_SNAKE title.** See `workflows/README_workflows.md`.

---

## 2. Style-canon convention

Once the owner picks the canonical look (session E1), the winning workflow JSON is
frozen as **`workflows/style_canon_v1.json`** and every later asset is generated
from a copy of it (prompt/seed swapped, structure untouched). This guarantees a
coherent cast — same checkpoint, same LoRA mix, same sampler/cfg, same caricature
band (Two Point Hospital ⇄ Persona, per STAR_02 §4/§9).

- The canon is **versioned**: a deliberate style change mints `style_canon_v2.json`;
  v1 is never edited in place (old assets must stay regenerable from their recorded
  workflow).
- Per-asset prompt text comes from the character briefs in `characters/<id>.md`
  (the `portraitDescriptor` seed + the expanded generation brief), not from editing
  the canon.
- Emotion variants do **not** re-roll: they are Kontext *edits* of the frozen base
  portrait (see `workflows/emotion_edit_kontext.json`), preserving identity.

---

## 3. Provenance rules (every shipped asset is regenerable)

**No asset ships without a recorded provenance entry.** Regenerability and honest
attribution are mandatory (consistent with the workspace's no-placeholder / full-
provenance discipline). For every PNG that lands in the manifest, record:

| Field | Meaning |
|---|---|
| `asset` | output filename, e.g. `npc_dutch_calloway_smug.png` |
| `npcId` / slot | which registry id (or card/bg slot) it fills |
| `emotion` | for portraits: neutral / confident / smug / worried / panicked / delighted |
| `workflow` | the exact workflow JSON used (path + version, e.g. `style_canon_v1`) |
| `checkpoint` / `lora` | model + LoRA names and strengths |
| `prompt` | the full positive (and negative) prompt text |
| `seed` | the integer seed (so the base is reproducible bit-for-bit) |
| `edit_chain` | for Kontext variants: the ordered list of edit instructions applied to the base, with the base asset named |
| `tool` | `comfyui-local` / `kontext-local` / `blender-mcp` / a named cloud fallback |
| `date` / `session` | when, and which E-session produced it |

Each character's provenance lives in the **"Provenance" stub section of its brief**
(`characters/<id>.md`) and is filled in as assets are produced. A base portrait plus
its emotion edit-chain is enough to regenerate the whole emotion set deterministically.

Cloud-fallback assets (GPT-image-mini, Ideogram, Recraft) record the service, model,
and prompt in the same table; they are not bit-reproducible but the prompt + service
are logged so a near-equivalent can be re-pulled.

---

## 4. Manifest integration contract (ALREADY SHIPPED — read this exactly)

The game already resolves portraits through a manifest indirection, so generated art
drops in with **zero game-code changes**. The contract is defined by
`frontend/src/lib/portraits.ts` (at
`Technical/StatisticalEngine/frontend/src/lib/portraits.ts`). Its behaviour, exactly:

**`portraitFor(npcId, emotion = 'neutral'): string`**
1. Looks up the NPC via `npcById(npcId)` and takes `npc.portraitSeed` (which equals
   the npc `id` for every cast member — bankers use their `id`, presidents use their
   `id`). Falls back to the raw `npcId` if not in the registry.
2. If the manifest has been loaded **and is non-empty**, it checks for
   `` `${seed}_${emotion}.png` `` first, then `` `${seed}_neutral.png` ``. A hit
   returns `/assets/portraits/<that-file>`.
3. Otherwise it returns a **DiceBear `personas`** data-URI seeded by `portraitSeed`
   (deterministic, offline, emotion-agnostic) — the current playtest placeholder.

**The manifest** is fetched **once** from **`/assets/portraits/manifest.json`**
(eagerly at module load). Accepted shapes (the loader handles both):
- an **object** whose KEYS are filenames: `{ "npc_dutch_calloway_neutral.png": true, ... }`
- a **string array** of filenames: `[ "npc_dutch_calloway_neutral.png", ... ]`

A failed/missing fetch yields an empty set → DiceBear everywhere (never throws).

**`Emotion`** type in code today is the union
`'neutral' | 'happy' | 'worried' | 'angry' | 'smug'`. **NOTE the divergence:** the
art plan's richer emotion set is *neutral, confident, smug, worried, panicked,
delighted*. Filenames must match what `portraitFor` will *ask for*. Practical rule
for A2/E2:
- Always ship `<id>_neutral.png` (the universal fallback target).
- Ship `<id>_smug.png` and `<id>_worried.png` — these map 1:1 to existing `Emotion`
  members and will resolve immediately.
- `confident` / `panicked` / `delighted` (and `happy`/`angry`) require **either**
  generating those filenames **or** a one-line `Emotion`-union widening +
  call-site mapping in the frontend. Until then, any emotion the manifest lacks
  falls back to `<id>_neutral.png`. **Do not silently ship emotion files whose names
  the code will never request** — pick the filename set against the `Emotion` union
  (or widen the union first). This is the single integration decision E2 must make
  with the owner; record it here when made.

**`portraitForName(name, emotion)`** exists for ad-hoc, non-registry speakers and is
DiceBear-only (no manifest path) — out of scope for the asset pipeline.

### To put a real portrait in the game (the entire integration step)
1. Generate `/<id>_<emotion>.png` per the canon, record provenance.
2. Drop the PNG into the site's `static/assets/portraits/` (served at
   `/assets/portraits/<file>`).
3. Add its filename to `static/assets/portraits/manifest.json`.
4. Done. `portraitFor` now returns the real path; no component changes.

The seeds are pre-wired: `portraitSeed === id` for all 12 bankers and all 17
presidents (`frontend/src/lib/characters/registry.ts`). The `portraitDescriptor`
fields in `data/archetypes/archetypes.json` plus the per-character briefs in
`characters/` are the prompt seeds.

### Card art (A3/E4) — a parallel, additive contract
The `Deal` interface (`frontend/src/lib/types/server.ts`) already has an index
signature (`[key: string]: unknown`). Card art adds an **optional** `art?: string`
field (a `/assets/cards/<slot>.png` path) plus a small DealBoard tweak to render it
when present — a ~1 h frontend change scoped in E4. No engine change.

---

## 5. GPU-coexistence rule (HARD)

**The RTX 5090 runs Hopper VLM jobs too, and Hopper's engine blanket-kills GPU
server processes and shares ports.** Therefore:

- **Art-generation sessions and Hopper jobs are MUTUALLY EXCLUSIVE bookings.**
  Before any A0–A5 / E0–E4 GPU session, verify the local VLM engine is idle (no
  `llama-server.exe`, no active extraction drain) and treat the GPU booking as a lock.
- **Never** run ComfyUI / Hunyuan3D concurrently with a Hopper drain.
- **Mid-drain art needs → cloud fallbacks only** (GPT-image-mini, Ideogram, Recraft).
  No local GPU generation while Hopper holds the card.
- Schedule local generation in Hopper idle windows; treat the booking as a lock.

---

## 6. What exists in this directory

```
Technical/art/
├── README.md                  ← this file (operating manual)
├── comfy_loop.py              ← stdlib ComfyUI HTTP client + mutate helpers (untested-by-design)
├── workflows/                 ← DRAFT/untested workflow JSON templates
│   ├── README_workflows.md
│   ├── portrait_base_sdxl.json
│   ├── emotion_edit_kontext.json
│   └── pixel_sprite.json
└── characters/                ← per-cast-member generation briefs (29 files)
    ├── npc_*.md               (12 bankers)
    └── pres_*.md              (17 presidents)
```

Future sessions add: `workflows/style_canon_v1.json` (frozen in E1), filled-in
provenance sections in each `characters/<id>.md`, and the produced PNGs (which live
in the site's `static/assets/`, not here).

---

## 7. Reminder

Nothing in this directory has been executed. The code is a readable spec; the
workflows are skeletons with symbolic node ids; the briefs are prompt seeds. A0/E0
is the first time any of it runs, and only on the owner's go-ahead.
