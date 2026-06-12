<script lang="ts">
  /*
   * Expanded market view — a large chart + a plain-language explainer of what
   * the market is and why it matters. Opened by clicking a small-multiple.
   * Lets the player "click between markets" via the chip row.
   */
  import MarketChart from './MarketChart.svelte';
  import { marketLabel, marketBlurb } from './markets-meta';
  import { sim } from '../stores/simulation.svelte';
  import { telemetry } from '../telemetry';
  import { dwell } from '../actions/dwell';
  import { onMount } from 'svelte';

  interface Props {
    marketId: string;
    ids: string[];
    onClose: () => void;
    onSelect: (id: string) => void;
  }
  let { marketId, ids, onClose, onSelect }: Props = $props();

  let series = $derived(sim.markets[marketId]);
  let lastReturn = $derived(series?.lastReturn ?? 0);

  // P1: opening the expanded view is itself a signal (which market drew a look).
  onMount(() => { telemetry.log('market_opened', { id: marketId }); });

  // P1: clicking a chip swaps the focused market — record the from→to switch so
  // the analyzer can see how the player grazed across markets.
  function selectMarket(id: string) {
    if (id !== marketId) telemetry.log('market_chip_switch', { from: marketId, to: id });
    onSelect(id);
  }
</script>

<div
  class="mx-back"
  role="button"
  tabindex="0"
  aria-label="Close market detail"
  onclick={onClose}
  onkeydown={(e) => { if (e.key === 'Escape' || e.key === 'Enter') onClose(); }}
></div>

<div class="mx-panel" role="dialog" aria-label="{marketLabel(marketId)} detail"
     use:dwell={{ key: `market:${marketId}`, data: { id: marketId } }}>
  <header class="mx-head">
    <div class="mx-titles">
      <h2 class="mx-title">{marketLabel(marketId)}</h2>
      <span class="mx-ret num" class:up={lastReturn > 0} class:down={lastReturn < 0}>
        {lastReturn >= 0 ? '+' : ''}{(lastReturn * 100).toFixed(2)}% today
      </span>
    </div>
    <button class="mx-x" aria-label="Close" onclick={onClose}>&times;</button>
  </header>

  <p class="mx-blurb">{marketBlurb(marketId)}</p>

  <div class="mx-chart">
    {#key marketId}
      <MarketChart {marketId} label={marketLabel(marketId)} />
    {/key}
  </div>

  <div class="mx-chips" role="tablist" aria-label="Switch market">
    {#each ids as id (id)}
      <button class="mx-chip" class:active={id === marketId} onclick={() => selectMarket(id)}>
        {marketLabel(id)}
      </button>
    {/each}
  </div>
</div>

<style>
  .mx-back {
    position: fixed; inset: 0; z-index: 880;
    background: rgba(20, 16, 8, 0.35);
    border: none; padding: 0;
    animation: fadeIn 180ms var(--ease);
  }
  .mx-panel {
    position: fixed;
    top: 50%; left: 50%; transform: translate(-50%, -50%);
    width: min(56rem, 94vw); max-height: 88vh;
    z-index: 890;
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
    padding: 1.25rem 1.5rem 1.5rem;
    display: flex; flex-direction: column; gap: 0.75rem;
    animation: fadeIn 200ms var(--ease);
  }
  .mx-head { display: flex; align-items: flex-start; justify-content: space-between; }
  .mx-titles { display: flex; align-items: baseline; gap: 0.85rem; flex-wrap: wrap; }
  .mx-title { font-family: var(--font-display); color: var(--accent-primary); margin: 0; font-size: 1.6rem; }
  .mx-ret { font-family: var(--font-data); font-size: 0.9rem; }
  .mx-ret.up { color: var(--accent-success); }
  .mx-ret.down { color: var(--accent-danger); }
  .mx-x { background: transparent; border: none; color: var(--fg-secondary); font-size: 1.6rem; cursor: pointer; line-height: 1; }
  .mx-x:hover { color: var(--fg-primary); }
  .mx-blurb { margin: 0; color: var(--fg-secondary); font-size: 0.95rem; line-height: 1.5; max-width: 46rem; }
  .mx-chart { height: min(48vh, 26rem); display: flex; }
  .mx-chart :global(.market-chart) { flex: 1; }
  .mx-chips { display: flex; flex-wrap: wrap; gap: 0.4rem; }
  .mx-chip {
    font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.12em;
    padding: 0.3rem 0.7rem; color: var(--fg-secondary);
    background: var(--bg-elevated); border: 1px solid var(--border); border-radius: 999px;
  }
  .mx-chip.active { color: var(--accent-primary); border-color: var(--accent-primary); }
  .mx-chip:hover:not(.active) { color: var(--fg-primary); }
</style>
