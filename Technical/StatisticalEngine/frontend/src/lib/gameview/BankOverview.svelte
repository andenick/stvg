<script lang="ts">
  /*
   * Bank summary card — progressively expands by UI phase.
   *   phase 1: name, capital, quarterly P&L
   *   phase 2+: + division breakdown count, total assets, deposits
   *   phase 3+: + capital ratio, leverage, employee count
   *   phase 4:  + regulatory rule set summary line
   *
   * Capital change flashes green/red briefly on update (visual_polish #4 / #10).
   */

  import { sim } from '../stores/simulation.svelte';
  import { ui } from '../stores/ui.svelte';
  import { fmtMoney } from '../util/money';

  const fmt = (v: number | undefined): string => fmtMoney(v);

  function fmtPct(v: number | undefined): string {
    if (v == null) return '—';
    return `${(v * 100).toFixed(1)}%`;
  }

  // Local flash state: re-trigger animation when bankCapital changes.
  let flashKey = $state(0);
  let flashDir = $state<'up' | 'down' | 'none'>('none');
  let lastCap = sim.bankCapital;

  $effect(() => {
    const c = sim.bankCapital;
    if (c === lastCap) return;
    flashDir = c > lastCap ? 'up' : c < lastCap ? 'down' : 'none';
    flashKey++;
    lastCap = c;
  });

  let qtrPL = $derived(sim.bank?.netIncomeQuarterly ?? 0);
  let annPL = $derived(sim.bank?.netIncomeAnnual ?? 0);
</script>

<section class="bank-overview" aria-label="Bank overview">
  <header>
    <h2 class="title">{sim.bank?.name ?? 'Bank'}</h2>
    {#if ui.phase >= 2 && sim.bank}
      <span class="rep num">REP {Math.round(sim.bank.reputation ?? 0)}</span>
    {/if}
  </header>

  <div class="primary">
    <div class="metric capital">
      <span class="label">CAPITAL</span>
      {#key flashKey}
        <span class="num value flash-{flashDir}">{fmt(sim.bankCapital)}</span>
      {/key}
    </div>
    <div class="metric">
      <span class="label">QTR P&amp;L</span>
      <span class="num value" class:up={qtrPL > 0} class:down={qtrPL < 0}>{fmt(qtrPL)}</span>
    </div>
    {#if ui.phase >= 2}
      <div class="metric">
        <span class="label">ANN P&amp;L</span>
        <span class="num value" class:up={annPL > 0} class:down={annPL < 0}>{fmt(annPL)}</span>
      </div>
      <div class="metric">
        <span class="label">ASSETS</span>
        <span class="num value">{fmt(sim.bankAssets)}</span>
      </div>
    {/if}
  </div>

  {#if ui.phase >= 3 && sim.bank}
    <div class="secondary">
      <div class="metric small">
        <span class="label">DEPOSITS</span>
        <span class="num value">{fmt(sim.bank.totalDeposits)}</span>
      </div>
      <div class="metric small">
        <span class="label">LOANS</span>
        <span class="num value">{fmt(sim.bank.totalLoans)}</span>
      </div>
      <div class="metric small">
        <span class="label">CAP RATIO</span>
        <span class="num value">{fmtPct(sim.bank.capitalRatio)}</span>
      </div>
      <div class="metric small">
        <span class="label">LEVERAGE</span>
        <span class="num value">{fmtPct(sim.bank.leverageRatio)}</span>
      </div>
      <div class="metric small">
        <span class="label">VISIBILITY</span>
        <span class="num value">{fmtPct(sim.bank.visibilityPct)}</span>
      </div>
    </div>
  {/if}

  {#if ui.phase === 4 && sim.regulatory}
    <p class="regulatory">
      <span class="label">REG&middot;</span>
      {#if sim.regulatory.glassSteagallActive}Glass-Steagall{:else}post-GLB{/if}
      {#if sim.regulatory.baselIII}&middot; Basel III{:else if sim.regulatory.baselII}&middot; Basel II{:else if sim.regulatory.baselI}&middot; Basel I{/if}
      {#if sim.regulatory.doddFrank}&middot; Dodd-Frank{/if}
      {#if sim.regulatory.volckerRule}&middot; Volcker Rule{/if}
    </p>
  {/if}
</section>

<style>
  .bank-overview {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 1rem 1.25rem;
  }
  header {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    margin-bottom: 0.75rem;
  }
  .title {
    font-family: var(--font-display);
    color: var(--accent-primary);
    margin: 0;
    font-size: 1.1rem;
    letter-spacing: 0.04em;
  }
  .rep {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    letter-spacing: 0.2em;
    color: var(--fg-secondary);
  }

  .primary, .secondary {
    display: grid;
    gap: 0.85rem;
  }
  .primary {
    grid-template-columns: repeat(auto-fit, minmax(7rem, 1fr));
    margin-bottom: 0.5rem;
  }
  .secondary {
    grid-template-columns: repeat(auto-fit, minmax(7rem, 1fr));
    margin-top: 0.4rem;
    padding-top: 0.6rem;
    border-top: 1px solid var(--border-soft);
  }
  .metric {
    display: flex;
    flex-direction: column;
    gap: 0.15rem;
  }
  .label {
    font-family: var(--font-chrome);
    font-size: 0.66rem;
    letter-spacing: 0.22em;
    color: var(--fg-secondary);
  }
  .value {
    font-family: var(--font-data);
    font-variant-numeric: tabular-nums;
    color: var(--fg-primary);
    font-size: 1rem;
  }
  .metric.small .value { font-size: 0.85rem; }
  .value.up   { color: var(--accent-success); }
  .value.down { color: var(--accent-danger); }

  .flash-up {
    animation: flashUp 700ms ease-out;
  }
  .flash-down {
    animation: flashDown 700ms ease-out;
  }
  @keyframes flashUp {
    0%   { color: var(--accent-success); }
    100% { color: var(--fg-primary); }
  }
  @keyframes flashDown {
    0%   { color: var(--accent-danger); }
    100% { color: var(--fg-primary); }
  }

  .regulatory {
    margin: 0.75rem 0 0 0;
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.08em;
    border-top: 1px solid var(--border-soft);
    padding-top: 0.5rem;
  }
</style>
