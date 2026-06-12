<script lang="ts">
  /*
   * Expanded macro view (P3) — the MarketExpanded sibling for the living economy.
   * A large macro chart + a plain-language explainer of what the series is and
   * why it matters, with a chip row to flip between macro series (including the
   * unpromoted ones — Inflation, 10Y, GDP growth — and a disabled "coming soon"
   * Economy P&L chip so the IA shows the planned series without faking data).
   */
  import MacroChart from './MacroChart.svelte';
  import {
    macroLabel, macroBlurb, formatMacroValue, macroMeta,
    type MacroId,
  } from './macro-meta';
  import { sim } from '../stores/simulation.svelte';
  import { telemetry } from '../telemetry';
  import { dwell } from '../actions/dwell';
  import { onMount } from 'svelte';

  interface Props {
    macroId: MacroId;
    ids: MacroId[];          // chip row (real, promotable series)
    extraIds?: MacroId[];    // unpromoted chips (Inflation / 10Y / growth / coming-soon)
    onClose: () => void;
    onSelect: (id: MacroId) => void;
  }
  let { macroId, ids, extraIds = [], onClose, onSelect }: Props = $props();

  let series = $derived(sim.macro[macroId]);
  let last = $derived(series?.last ?? 0);

  onMount(() => { telemetry.log('market_opened', { id: `macro:${macroId}` }); });

  function selectMacro(id: MacroId) {
    if (macroMeta(id)?.comingSoon) return;        // disabled placeholder, no data
    if (id !== macroId) {
      telemetry.log('market_chip_switch', { from: `macro:${macroId}`, to: `macro:${id}` });
    }
    onSelect(id);
  }
</script>

<div
  class="mx-back"
  role="button"
  tabindex="0"
  aria-label="Close economy detail"
  onclick={onClose}
  onkeydown={(e) => { if (e.key === 'Escape' || e.key === 'Enter') onClose(); }}
></div>

<div class="mx-panel" role="dialog" aria-label="{macroLabel(macroId)} detail"
     use:dwell={{ key: `macro:${macroId}`, data: { id: `macro:${macroId}` } }}>
  <header class="mx-head">
    <div class="mx-titles">
      <h2 class="mx-title">{macroLabel(macroId)}</h2>
      <span class="mx-val num">{formatMacroValue(macroId, last)}</span>
    </div>
    <button class="mx-x" aria-label="Close" onclick={onClose}>&times;</button>
  </header>

  <p class="mx-blurb">{macroBlurb(macroId)}</p>

  <div class="mx-chart">
    {#key macroId}
      <MacroChart {macroId} label={macroLabel(macroId)} />
    {/key}
  </div>

  <div class="mx-chips" role="tablist" aria-label="Switch economic series">
    {#each ids as id (id)}
      <button class="mx-chip" class:active={id === macroId} onclick={() => selectMacro(id)}>
        {macroLabel(id)}
      </button>
    {/each}
    {#each extraIds as id (id)}
      {@const meta = macroMeta(id)}
      <button
        class="mx-chip"
        class:active={id === macroId}
        class:soon={meta?.comingSoon}
        disabled={meta?.comingSoon}
        title={meta?.comingSoon ? 'Coming soon — no data yet' : macroBlurb(id)}
        onclick={() => selectMacro(id)}
      >
        {macroLabel(id)}{#if meta?.comingSoon}<span class="soon-tag">soon</span>{/if}
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
  .mx-val { font-family: var(--font-data); font-size: 1.1rem; color: var(--fg-primary); font-variant-numeric: tabular-nums; }
  .mx-x { background: transparent; border: none; color: var(--fg-secondary); font-size: 1.6rem; cursor: pointer; line-height: 1; }
  .mx-x:hover { color: var(--fg-primary); }
  .mx-blurb { margin: 0; color: var(--fg-secondary); font-size: 0.95rem; line-height: 1.5; max-width: 46rem; }
  .mx-chart { height: min(48vh, 26rem); display: flex; }
  .mx-chart :global(.macro-chart) { flex: 1; }
  .mx-chips { display: flex; flex-wrap: wrap; gap: 0.4rem; }
  .mx-chip {
    font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.12em;
    padding: 0.3rem 0.7rem; color: var(--fg-secondary);
    background: var(--bg-elevated); border: 1px solid var(--border); border-radius: 999px;
    display: inline-flex; align-items: center; gap: 0.35rem;
  }
  .mx-chip.active { color: var(--accent-primary); border-color: var(--accent-primary); }
  .mx-chip:hover:not(.active):not(:disabled) { color: var(--fg-primary); }
  .mx-chip.soon { opacity: 0.55; cursor: not-allowed; }
  .soon-tag {
    font-size: 0.5rem; letter-spacing: 0.1em; text-transform: uppercase;
    padding: 0.05rem 0.25rem; border: 1px solid var(--border); border-radius: 3px;
    color: var(--fg-muted);
  }
</style>
