<script lang="ts">
  /*
   * AnnualReport sidebar — wires the server-generated headlines/narrative/
   * growth% (visual_polish #7 + #1). Visible after 1950 when there's a year
   * of history to summarize. Full Q4 modal lives in WP13.
   */

  import { sim } from '../stores/simulation.svelte';

  function fmt(v: number | undefined): string {
    if (v == null) return '—';
    const abs = Math.abs(v);
    const sign = v < 0 ? '-' : '';
    if (abs >= 1e9) return `${sign}$${(abs / 1e9).toFixed(2)}B`;
    if (abs >= 1e6) return `${sign}$${(abs / 1e6).toFixed(0)}M`;
    return `${sign}$${abs.toFixed(0)}`;
  }
</script>

{#if sim.annualReport && sim.year >= 1950}
  {@const r = sim.annualReport}
  <section class="panel" aria-label="Annual Report">
    <header>
      <h3>{r.year} Annual Report</h3>
      <span class="era">{r.eraName}</span>
    </header>
    <div class="kpis">
      <div class="kpi">
        <span class="label">REVENUE</span>
        <span class="num value">{fmt(r.totalRevenue)}</span>
      </div>
      <div class="kpi">
        <span class="label">NET INCOME</span>
        <span class="num value" class:up={r.netIncome > 0} class:down={r.netIncome < 0}>{fmt(r.netIncome)}</span>
      </div>
      <div class="kpi big">
        <span class="label">CAPITAL GROWTH</span>
        <span
          class="num value"
          class:up={r.capitalGrowthPct > 0}
          class:down={r.capitalGrowthPct < 0}
        >{r.capitalGrowthPct > 0 ? '+' : ''}{(r.capitalGrowthPct * 100).toFixed(1)}%</span>
      </div>
    </div>
    {#if r.headlines && r.headlines.length}
      <ul class="headlines">
        {#each r.headlines as h}
          <li>{h}</li>
        {/each}
      </ul>
    {/if}
    {#if r.narrative}
      <p class="narrative">{r.narrative}</p>
    {/if}
  </section>
{/if}

<style>
  .panel {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 0.85rem 1rem;
  }
  header { display: flex; align-items: baseline; gap: 0.5rem; margin-bottom: 0.5rem; }
  header h3 {
    font-family: var(--font-display);
    color: var(--accent-primary);
    margin: 0;
    font-size: 1rem;
    flex: 1;
  }
  .era {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    letter-spacing: 0.15em;
    color: var(--fg-secondary);
    text-transform: uppercase;
  }
  .kpis {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.5rem;
    margin-bottom: 0.6rem;
  }
  .kpi.big { grid-column: 1 / -1; }
  .label {
    display: block;
    font-family: var(--font-chrome);
    font-size: 0.62rem;
    letter-spacing: 0.2em;
    color: var(--fg-secondary);
  }
  .value {
    display: block;
    font-family: var(--font-data);
    font-size: 0.95rem;
    color: var(--fg-primary);
    font-variant-numeric: tabular-nums;
  }
  .value.up   { color: var(--accent-success); }
  .value.down { color: var(--accent-danger); }
  .headlines {
    list-style: none;
    padding: 0;
    margin: 0 0 0.5rem 0;
    border-top: 1px solid var(--border-soft);
    padding-top: 0.5rem;
  }
  .headlines li {
    color: var(--fg-primary);
    font-size: var(--fs-sm);
    padding: 0.2rem 0;
    line-height: 1.35;
  }
  .headlines li::before {
    content: '•';
    color: var(--accent-primary);
    margin-right: 0.4rem;
  }
  .narrative {
    margin: 0;
    font-size: var(--fs-sm);
    color: var(--fg-secondary);
    font-style: italic;
    line-height: 1.4;
  }
</style>
