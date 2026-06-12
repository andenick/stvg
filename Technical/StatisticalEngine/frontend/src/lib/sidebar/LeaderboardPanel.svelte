<script lang="ts">
  /*
   * Mini-leaderboard with player rank highlighted (top_20 #16). Reads
   * sim.leaderboard (server already ranks; we just style).
   */

  import { sim } from '../stores/simulation.svelte';
  import { fmtMoney } from '../util/money';
  import { dwell, panelView } from '../actions/dwell';

  const fmtAssets = (v: number): string => fmtMoney(v);

  let sorted = $derived(
    [...sim.leaderboard].sort((a, b) => (a.rank ?? 999) - (b.rank ?? 999))
  );
</script>

{#if sim.leaderboard.length > 0 && sim.year >= 1965}
  <section class="panel" aria-label="Leaderboard" use:dwell={'panel:LeaderboardPanel'} use:panelView={'LeaderboardPanel'}>
    <header><h3>Leaderboard</h3></header>
    <ol>
      {#each sorted as row (row.id)}
        <li class:player={row.isPlayer} class:dead={!row.alive}>
          <span class="rank num">{row.rank}</span>
          <span class="name">{row.name}</span>
          <span class="num assets">{fmtAssets(row.totalAssets)}</span>
        </li>
      {/each}
    </ol>
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
  ol { list-style: none; padding: 0; margin: 0; display: flex; flex-direction: column; gap: 0.25rem; }
  li {
    display: grid;
    grid-template-columns: 1.5rem 1fr auto;
    column-gap: 0.5rem;
    padding: 0.25rem 0.4rem;
    border-radius: 2px;
  }
  li.player {
    background: rgba(196, 163, 90, 0.10);
    border-left: 2px solid var(--accent-primary);
  }
  li.dead { color: var(--fg-muted); }
  li.dead .name { text-decoration: line-through; }
  .rank {
    color: var(--fg-secondary);
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
  }
  .name { font-size: var(--fs-sm); color: var(--fg-primary); }
  li.player .name { color: var(--accent-primary); font-weight: 500; }
  .assets {
    font-family: var(--font-data);
    font-variant-numeric: tabular-nums;
    font-size: var(--fs-xs);
    color: var(--fg-primary);
  }
</style>
