# Workflow Templates — DRAFT / UNTESTED

<!-- STATUS: SCAFFOLDED 2026-06-12 — these JSONs have NEVER been loaded into ComfyUI. -->

> **Every JSON in this folder is a DRAFT SKELETON.** Node ids are symbolic
> (`"1"`, `"2"`, ...) and several node `inputs` are placeholders (model filenames,
> exact widget keys) that **must be validated live in session A0/E0** against the
> actual installed ComfyUI build. Do not assume any of these load as-is. They exist
> to (a) fix the *shape* of each graph, (b) fix the mutation handles (node
> `_meta.title`s), and (c) name the exact models to pull.

## Mutation contract (shared by all templates)

`comfy_loop.py` mutates nodes by their **`_meta.title`** (a stable human label),
never by numeric id. Every editable node in every template carries one of these
SCREAMING_SNAKE titles:

| Title | Node kind | Mutated by |
|---|---|---|
| `POSITIVE_PROMPT` | CLIP Text Encode (positive) | `set_prompt` |
| `NEGATIVE_PROMPT` | CLIP Text Encode (negative) | `set_negative` |
| `SEED` | KSampler (seed / noise_seed) | `set_seed` |
| `SAMPLER` | KSampler (denoise etc.) | `set_denoise` |
| `LORA_STRENGTH` | LoraLoader | `set_lora_strength` |
| `CHECKPOINT` | CheckpointLoaderSimple | (frozen per canon) |
| `SAVE` | SaveImage | (output naming) |

A0/E0 must confirm these titles survive a round-trip through the installed
ComfyUI's export and that the widget keys (`seed` vs `noise_seed`, `text`,
`strength_model`) match this build.

---

## Templates

### `portrait_base_sdxl.json` — hero/president base portrait
**Intent:** generate ONE canonical caricature portrait per character (the base that
emotion edits build on, and the render-once image for presidents).
**Graph shape:** CheckpointLoaderSimple → (LoraLoader) → CLIP encode ×2 →
EmptyLatentImage → KSampler → VAEDecode → SaveImage.
**Required models (pull in E0):**
- **Checkpoint:** an SDXL / **Illustrious** caricature-leaning checkpoint. Candidate
  from the research: a Two Point/Persona-band cartoon-business checkpoint on
  Civitai (exact pick is an E1 owner decision — the file goes in
  `ComfyUI/models/checkpoints/`). Placeholder in the JSON:
  `"PLACEHOLDER_sdxl_caricature.safetensors"`.
- **(Optional) per-hero LoRA:** a small caricature/style LoRA in
  `ComfyUI/models/loras/` to anchor the canon look. Placeholder:
  `"PLACEHOLDER_caricature_style_lora.safetensors"`.
**Validate live in A0/E0:** checkpoint loads on Blackwell (sm_120 / cu130 nightly);
resolution (1024×1024 SDXL native); sampler/cfg that yields the band; the LoraLoader
is wired only if a LoRA is chosen.

### `emotion_edit_kontext.json` — emotion variant by instruction-edit
**Intent:** take the frozen base portrait and EDIT it ("make him smug", "panicked,
tie loosened") with identity preserved — no re-roll. This is the 2026 unlock.
**Graph shape:** LoadImage(base) → FLUX Kontext model/clip/vae loaders →
text-encode(edit instruction) → Kontext conditioning/latent → KSampler(denoise) →
VAEDecode → SaveImage.
**Required model (pull in E0):**
- **FLUX.1 Kontext [dev] FP8** — the day-0 ComfyUI-supported instruction-edit model
  (12B, fits the 5090 in FP8). HF: `black-forest-labs/FLUX.1-Kontext-dev` (use the
  FP8 weight; the ComfyUI-packaged FP8 checkpoint, e.g.
  `flux1-kontext-dev-fp8` in `ComfyUI/models/diffusion_models/` or `checkpoints/`
  per the build's Kontext template). Placeholder in the JSON:
  `"PLACEHOLDER_flux1-kontext-dev-fp8.safetensors"`.
- Plus its text encoders / VAE per the official ComfyUI Kontext template (CLIP-L +
  T5xxl FP8, ae.safetensors) — names placeheld, confirm in E0.
**Mutation:** `POSITIVE_PROMPT` carries the edit instruction; `SEED`; `SAMPLER`
denoise tunes edit strength (identity-drift knob — a key E1/E2 finding to record).
**Validate live in A0/E0:** identity holds across a 4-edit chain on a *caricature*
(the research flag: Kontext may drift on stylized faces — measure it).

### `pixel_sprite.json` — static floor sprite (mini)
**Intent:** a tiny trading-floor worker sprite, pixel-art band, for the My Bank tab
strip. Static only — walk cycles come from the Blender route (A4), not diffusion.
**Graph shape:** CheckpointLoaderSimple(SDXL) → LoraLoader(pixel-art-xl) → encode ×2
→ EmptyLatentImage → KSampler → VAEDecode → (downscale/quantize is a post step,
nearest-neighbour, done outside the graph or via an image-resize node) → SaveImage.
**Required models (pull in E0):**
- **Base:** the same SDXL checkpoint (or stock SDXL base).
- **LoRA:** **`nerijs/pixel-art-xl`** (HF: `nerijs/pixel-art-xl`,
  `pixel-art-xl.safetensors` → `ComfyUI/models/loras/`). Placeholder:
  `"PLACEHOLDER_pixel-art-xl.safetensors"`.
**Validate live in A0/E0:** LoRA strength sweet spot; whether to downscale in-graph
(ImageScale nearest) or as a separate nearest-neighbour quantize pass.

---

## What A0/E0 must produce from this folder
1. Each template loaded successfully in the installed ComfyUI (node ids may renumber
   — re-anchor the `_meta.title`s, keep them stable).
2. Real model filenames substituted for every `PLACEHOLDER_*` (recorded in the
   character/provenance logs and in `../README.md` §2 once the canon is picked).
3. One approved end-to-end run per template (one SDXL gen, one Kontext edit, one
   pixel sprite), archived with its workflow JSON.
4. After E1, the chosen portrait graph is frozen as `style_canon_v1.json` here.
