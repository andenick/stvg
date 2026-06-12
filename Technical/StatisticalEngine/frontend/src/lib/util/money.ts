/*
 * Single money formatter for the whole frontend. Auto-scales to a sensible
 * unit and is negative-aware, so it reads honestly at the $1M-bank world scale
 * (where a quarter's net income might be $850, $12.5K, $3.2M, or $1.1B).
 *
 * Replaces the per-component `fmt()` helpers that each re-derived this badly —
 * the old ones truncated sub-$1M values to "$0M" (the headline bug) by doing
 * `(v / 1e6).toFixed(0)` and never reaching a K/plain branch.
 *
 * Scale ladder: T / B / M / K / plain dollars. Each tier keeps ~3 significant
 * figures: $1.1B, $12.5K, $850. Whole-thousands collapse cleanly ($12K not
 * $12.0K). Mirrors the engine-side fmtMoney() in QuarterlyPhases.h so the
 * server-built headlines and the client agree.
 */

export interface FmtMoneyOpts {
  /** Force a leading '+' on positive values (deltas). Default false. */
  signed?: boolean;
  /** Value to show when v is null/undefined/NaN. Default '—'. */
  placeholder?: string;
}

/** Trim a trailing ".0" so "$12.0K" reads as "$12K" but "$12.5K" survives. */
function trim(n: number, digits: number): string {
  const s = n.toFixed(digits);
  return s.replace(/\.0+$/, '');
}

export function fmtMoney(v: number | null | undefined, opts: FmtMoneyOpts = {}): string {
  const placeholder = opts.placeholder ?? '—';
  if (v == null || Number.isNaN(v)) return placeholder;

  const abs = Math.abs(v);
  const neg = v < 0;
  const lead = neg ? '-' : (opts.signed ? '+' : '');

  let body: string;
  if (abs >= 1e12)      body = `$${trim(abs / 1e12, 2)}T`;
  else if (abs >= 1e9)  body = `$${trim(abs / 1e9, 2)}B`;
  else if (abs >= 1e6)  body = `$${trim(abs / 1e6, 2)}M`;
  else if (abs >= 1e3)  body = `$${trim(abs / 1e3, 1)}K`;
  else                  body = `$${Math.round(abs)}`;

  return lead + body;
}
