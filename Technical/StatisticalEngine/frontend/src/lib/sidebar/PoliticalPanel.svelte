<script lang="ts">
  /*
   * Political engine readout (visual_polish #1). Visible from 1973 when
   * the deregulation era opens up real lobbying.
   */

  import { sim } from '../stores/simulation.svelte';
</script>

{#if sim.political && sim.year >= 1973}
  {@const p = sim.political}
  <section class="panel" aria-label="Political">
    <header><h3>Political</h3></header>
    <div class="bars">
      <div class="bar">
        <span class="label">CAPITAL</span>
        <div class="track"><span class="fill" style="width: {(p.currentCapital ?? 0) * 100}%"></span></div>
        <span class="num val">{((p.currentCapital ?? 0) * 100).toFixed(0)}</span>
      </div>
      <div class="bar">
        <span class="label">OPINION</span>
        <div class="track"><span class="fill" style="width: {(p.publicOpinion ?? 0) * 100}%"></span></div>
        <span class="num val">{((p.publicOpinion ?? 0) * 100).toFixed(0)}</span>
      </div>
      <div class="bar">
        <span class="label">LOBBY</span>
        <div class="track"><span class="fill" style="width: {(p.effectiveness ?? 0) * 100}%"></span></div>
        <span class="num val">{((p.effectiveness ?? 0) * 100).toFixed(0)}</span>
      </div>
    </div>
  </section>
{/if}

<style>
  .panel {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 0.85rem 1rem;
  }
  header h3 {
    font-family: var(--font-display);
    color: var(--accent-primary);
    margin: 0 0 0.5rem 0;
    font-size: 1rem;
  }
  .bars { display: flex; flex-direction: column; gap: 0.45rem; }
  .bar {
    display: grid;
    grid-template-columns: 4rem 1fr 2rem;
    align-items: center;
    gap: 0.5rem;
  }
  .label {
    font-family: var(--font-chrome);
    font-size: 0.62rem;
    letter-spacing: 0.2em;
    color: var(--fg-secondary);
  }
  .track {
    height: 6px;
    background: var(--bg-base);
    border-radius: 2px;
    overflow: hidden;
  }
  .fill {
    display: block;
    height: 100%;
    background: var(--accent-secondary);
  }
  .val {
    font-family: var(--font-data);
    font-size: var(--fs-xs);
    color: var(--fg-primary);
    font-variant-numeric: tabular-nums;
    text-align: right;
  }
</style>
