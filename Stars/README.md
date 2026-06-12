# Stars — STVG Vision Documents

This directory holds the project's **star documents**: durable captures of the owner's
vision, given as long-form feedback during development. Each star preserves the owner's
direction in something close to their own words, lightly structured and annotated, so
that future agents can refer back to the *actual intent* rather than a paraphrase that
drifted through three handoffs.

## Convention

- `STAR_01` is the founding vision: **`../NORTH_STAR.md`** (immutable, lives at project
  root, never moves, never edited). It is listed here for completeness only.
- Each subsequent star is `STAR_NN_<SLUG>.md`, numbered in the order received.
- A star records: the date, the verbatim-or-near-verbatim owner direction (cleaned of
  dictation artifacts only — no paraphrasing of substance), and an **annotation layer**
  that maps each directive to concrete systems/files and to the plan that executes it.
- Stars are **append-only**: if the owner refines a direction later, write a new star
  that supersedes specific sections and cross-reference both ways. Never rewrite an old
  star to match new intent.
- When planning any substantial work, agents MUST re-read NORTH_STAR.md plus the most
  recent star(s) touching the systems in scope. Plans cite stars by section.

## Why this exists

The 2026-06-11 development audit found a recurring failure mode: **open user decisions
silently resolved by agents** and direction details lost between plan and handoff
(e.g., hover telemetry claimed done but never wired; decision-lapse vs hard-pause chosen
by fiat). Stars fix the root cause: the owner's words stop living only in a chat session
that gets compacted away.

## Index

| Star | Date | Subject |
|------|------|---------|
| STAR_01 → `../NORTH_STAR.md` | 2026-04-02 | Founding vision: eras, progressive complexity, path dependence, AI endgame |
| `STAR_02_DAY_TRADING_FEEL_AND_CHARACTERS.md` | 2026-06-11 | The day-trading feel, tab structure, Hades-style character pop-ups, credibility-through-language, archetype hiring economy, telemetry-driven iteration, sprite roadmap |
