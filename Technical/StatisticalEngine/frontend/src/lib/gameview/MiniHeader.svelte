<script lang="ts">
  /*
   * Minimal early-era header (phase 1, 1945-1959).
   * Per the rebuild plan: bank name in serif, date elegant, capital only.
   * No score, AP, level, era badge, connection dot — those layer in later.
   */
  import { sim } from '../stores/simulation.svelte';

  function fmtCapital(v: number): string {
    if (Math.abs(v) >= 1e12) return `$${(v / 1e12).toFixed(2)}T`;
    if (Math.abs(v) >= 1e9)  return `$${(v / 1e9).toFixed(2)}B`;
    if (Math.abs(v) >= 1e6)  return `$${(v / 1e6).toFixed(0)}M`;
    return `$${v.toFixed(0)}`;
  }
</script>

<header class="mini-header">
  <div class="bank-name">{sim.bank?.name ?? 'Bank'}</div>
  <div class="date num">{sim.year} Q{sim.quarter}</div>
  <div class="capital num">{fmtCapital(sim.bankCapital)}</div>
</header>

<style>
  .mini-header {
    display: grid;
    grid-template-columns: 1fr auto 1fr;
    align-items: baseline;
    padding: 1rem 2rem;
    border-bottom: 1px solid var(--border-soft);
  }
  .bank-name {
    font-family: var(--font-display);
    font-size: 1.25rem;
    color: var(--fg-primary);
    letter-spacing: 0.04em;
  }
  .date {
    font-family: var(--font-display);
    font-size: 1rem;
    color: var(--accent-primary);
    letter-spacing: 0.15em;
    text-align: center;
  }
  .capital {
    font-family: var(--font-data);
    font-size: 1.1rem;
    color: var(--fg-primary);
    text-align: right;
    font-variant-numeric: tabular-nums;
  }
</style>
