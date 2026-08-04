# Stars — STVG Vision Documents

This directory holds the project's **star documents**: durable captures of the design
vision, given as long-form direction during development. Each star preserves that
direction in something close to its original words, lightly structured and annotated, so
the *actual intent* stays available rather than a paraphrase that drifted over time.

## Convention

- `STAR_01` is the founding vision: **`../NORTH_STAR.md`** (immutable, lives at project
  root, never moves, never edited). It is listed here for completeness only.
- Each subsequent star is `STAR_NN_<SLUG>.md`, numbered in the order received.
- A star records: the date, the near-verbatim direction (cleaned of dictation artifacts
  only — no paraphrasing of substance), and an **annotation layer** that maps each
  directive to concrete systems and files.
- Stars are **append-only**: when a direction is refined later, a new star supersedes
  specific sections and cross-references both ways. Old stars are never rewritten to
  match new intent.
- Any substantial change should start from NORTH_STAR.md plus the most recent star(s)
  touching the systems in scope.

## Why this exists

A 2026-06-11 development audit found a recurring failure mode: **open design decisions
silently resolved in passing**, and direction details lost between one work session and
the next (e.g. hover telemetry claimed done but never wired; decision-lapse vs
hard-pause chosen by fiat). Stars fix the root cause by giving that direction a durable
home in the repository.

## Index

| Star | Date | Subject |
|------|------|---------|
| STAR_01 → `../NORTH_STAR.md` | 2026-04-02 | Founding vision: eras, progressive complexity, path dependence, AI endgame |
| `STAR_02_DAY_TRADING_FEEL_AND_CHARACTERS.md` | 2026-06-11 | The day-trading feel, tab structure, Hades-style character pop-ups, credibility-through-language, archetype hiring economy, telemetry-driven iteration, sprite roadmap — executed 2026-06-12. |
