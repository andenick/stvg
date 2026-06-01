<script lang="ts">
  /*
   * Full header used phase 2 and later. Grouped into Identity / Financials /
   * Game (per visual_polish #5). Era badge has tooltip-on-hover with date range.
   */
  import { sim } from '../stores/simulation.svelte';
  import { apRemaining } from '../types/server';

  function fmtCap(v: number): string {
    if (Math.abs(v) >= 1e12) return `$${(v / 1e12).toFixed(2)}T`;
    if (Math.abs(v) >= 1e9)  return `$${(v / 1e9).toFixed(2)}B`;
    if (Math.abs(v) >= 1e6)  return `$${(v / 1e6).toFixed(0)}M`;
    return `$${v.toFixed(0)}`;
  }

  function fmtDelta(v: number): string {
    const sign = v >= 0 ? '+' : '−';
    return `${sign}${fmtCap(Math.abs(v))}`;
  }

  let eraTooltip = $derived(
    sim.era
      ? `${sim.era.name} · ${sim.era.startYear}–${sim.era.endYear}${sim.era.description ? ` · ${sim.era.description}` : ''}`
      : ''
  );
</script>

<header class="game-header">
  <div class="group identity">
    <span class="bank-name">{sim.bank?.name ?? 'Bank'}</span>
    <span class="era-badge" title={eraTooltip}>{sim.era?.name ?? 'Post-War Stability'}</span>
  </div>

  <div class="group financials">
    <span class="label">CAPITAL</span>
    <span class="num value">{fmtCap(sim.bankCapital)}</span>
    {#if sim.capitalDelta !== 0}
      <span
        class="num delta"
        class:positive={sim.capitalDelta > 0}
        class:negative={sim.capitalDelta < 0}
        aria-label="capital change"
      >{fmtDelta(sim.capitalDelta)}</span>
    {/if}
    <span class="divider" aria-hidden="true"></span>
    <span class="label">REPUTATION</span>
    <span class="num value">{Math.round(sim.bank?.reputation ?? 0)}</span>
  </div>

  <div class="group game">
    {#if sim.progression}
      <span class="label">LVL {sim.progression.level}</span>
      <span class="value-soft">{sim.progression.levelName}</span>
      <span class="divider" aria-hidden="true"></span>
      <span class="label">SCORE</span>
      <span class="num value">{sim.progression.totalScore.toFixed(0)}</span>
    {/if}
    {#if sim.actionPoints}
      <span class="divider" aria-hidden="true"></span>
      <span class="label">AP</span>
      <span class="num value">{apRemaining(sim.actionPoints)}/{sim.actionPoints.total}</span>
    {/if}
  </div>
</header>

<style>
  .game-header {
    display: flex;
    align-items: center;
    gap: 1.5rem;
    padding: 0.6rem 1.25rem;
    background: var(--bg-card);
    border-bottom: 1px solid var(--border);
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    letter-spacing: 0.12em;
    flex-wrap: wrap;
  }
  .group {
    display: inline-flex;
    align-items: center;
    gap: 0.55rem;
  }
  .financials { margin-left: auto; }

  .bank-name {
    font-family: var(--font-display);
    font-size: 1.05rem;
    letter-spacing: 0.04em;
    color: var(--fg-primary);
    text-transform: none;
  }
  .era-badge {
    color: var(--accent-primary);
    border: 1px solid var(--border);
    padding: 0.15rem 0.55rem;
    border-radius: 2px;
    cursor: help;
  }

  .label { color: var(--fg-secondary); text-transform: uppercase; }
  .value, .value-soft {
    color: var(--fg-primary);
    text-transform: none;
    letter-spacing: 0.04em;
  }
  .value-soft { color: var(--fg-secondary); }
  .num { font-variant-numeric: tabular-nums; }

  .delta {
    padding: 0.1rem 0.4rem;
    border-radius: 2px;
    font-size: 0.7rem;
  }
  .delta.positive {
    color: var(--accent-success);
    background: rgba(107, 142, 78, 0.12);
  }
  .delta.negative {
    color: var(--accent-danger);
    background: rgba(168, 69, 58, 0.12);
  }

  .divider {
    width: 1px;
    height: 1rem;
    background: var(--border);
  }
</style>
