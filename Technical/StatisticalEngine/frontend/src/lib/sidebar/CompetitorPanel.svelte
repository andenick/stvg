<script lang="ts">
  /*
   * Competitor list. Visual_polish #1 (server emits competitors[] but old
   * React panel was a stub). Dead competitors render muted + strikethrough.
   * Visible from year 1965 — by then competitors have had time to differentiate.
   */

  import { sim } from '../stores/simulation.svelte';
  import { fmtMoney } from '../util/money';
  import { dwell, panelView } from '../actions/dwell';

  const fmtAssets = (v: number): string => fmtMoney(v);

  let sorted = $derived(
    [...sim.competitors].sort((a, b) => (b.totalAssets ?? 0) - (a.totalAssets ?? 0))
  );
</script>

{#if sim.competitors.length > 0 && sim.year >= 1965}
  <section class="panel" aria-label="Competitors" use:dwell={'panel:CompetitorPanel'} use:panelView={'CompetitorPanel'}>
    <header>
      <h3>Competitors</h3>
      <span class="count">{sim.competitors.filter((c) => c.alive).length} live</span>
    </header>
    <ul>
      {#each sorted as c (c.id)}
        <li class:dead={!c.alive} title={c.alive ? `${c.archetype}${c.ceoName ? ` · CEO ${c.ceoName}` : ''}` : `Failed ${c.failureYear ?? ''}${c.failureReason ? ` — ${c.failureReason}` : ''}`}>
          <span class="name">{c.name}</span>
          <span class="arch">{c.archetype}</span>
          <span class="num assets">{fmtAssets(c.totalAssets ?? 0)}</span>
          <span class="num share">{((c.marketShare ?? 0) * 100).toFixed(1)}%</span>
        </li>
      {/each}
    </ul>
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
  .count {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.12em;
  }
  ul { list-style: none; padding: 0; margin: 0; display: flex; flex-direction: column; gap: 0.35rem; }
  li {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto auto auto;
    column-gap: 0.5rem;
    align-items: baseline;
    padding: 0.3rem 0.4rem;
    border-left: 2px solid var(--border);
    cursor: help;
  }
  li.dead {
    color: var(--fg-muted);
    border-left-color: var(--accent-danger);
  }
  li.dead .name { text-decoration: line-through; }
  .name {
    color: var(--fg-primary);
    font-size: var(--fs-sm);
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  li.dead .name { color: var(--fg-muted); }
  .arch {
    font-family: var(--font-chrome);
    font-size: 0.6rem;
    letter-spacing: 0.1em;
    color: var(--fg-secondary);
    text-transform: uppercase;
  }
  .num {
    font-family: var(--font-data);
    font-variant-numeric: tabular-nums;
    font-size: var(--fs-xs);
    color: var(--fg-primary);
  }
  .assets { color: var(--accent-primary); }
  .share  { color: var(--fg-secondary); }
  li.dead .num { color: var(--fg-muted); }
</style>
