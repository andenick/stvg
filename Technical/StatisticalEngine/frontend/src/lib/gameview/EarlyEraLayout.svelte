<script lang="ts">
  /*
   * Phase 1 layout — "The Desk" (1945-1959).
   * Single column, centered. No sidebar.
   *
   * WP4 now wires the live charts. CapitalChart up top (the bank's "stock
   * price" the player nurses upward), then a compact 4-market mini-grid.
   * Bank summary placeholder until WP5; decision area until WP6.
   */

  import MiniHeader from './MiniHeader.svelte';
  import SimControls from './SimControls.svelte';
  import BankOverview from './BankOverview.svelte';
  import DivisionGrid from './DivisionGrid.svelte';
  import DecisionDrawer from './DecisionDrawer.svelte';
  import NewsTicker from './NewsTicker.svelte';
  import CapitalChart from '../charts/CapitalChart.svelte';
  import MarketChart from '../charts/MarketChart.svelte';
  import { sim } from '../stores/simulation.svelte';

  // Order matters: S&P, Treasury, Gold, VIX — the canonical phase-1 quartet
  // engineered into MarketSimulator. We fall back to whatever ids the server
  // actually emits so the grid is never empty.
  const PREFERRED_ORDER = ['SP500', 'TREASURY10Y', 'GOLD', 'VIX'];

  let marketIds = $derived(() => {
    const have = Object.keys(sim.markets);
    if (have.length === 0) return PREFERRED_ORDER;
    const inOrder = PREFERRED_ORDER.filter((id) => have.includes(id));
    const extra = have.filter((id) => !PREFERRED_ORDER.includes(id));
    return [...inOrder, ...extra].slice(0, 4);
  });

  function labelFor(id: string): string {
    const map: Record<string, string> = {
      SP500: 'S&P 500',
      TREASURY10Y: '10Y TREASURY',
      GOLD: 'GOLD',
      VIX: 'VIX',
    };
    return map[id] ?? id;
  }
</script>

<div class="desk" data-phase="1" data-ui-mode="minimal">
  <MiniHeader />
  <SimControls />

  <main class="desk-main">
    <section class="capital-block" aria-label="Bank capital">
      <CapitalChart />
    </section>

    <section class="market-grid" aria-label="Markets">
      {#each marketIds() as id (id)}
        <MarketChart marketId={id} label={labelFor(id)} compact />
      {/each}
    </section>

    <BankOverview />
    <DivisionGrid />
    <NewsTicker />
  </main>

  <DecisionDrawer />
</div>

<style>
  .desk {
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    background: var(--bg-base);
  }
  .desk-main {
    max-width: 52rem;
    width: 100%;
    margin: 1.5rem auto;
    padding: 0 1.5rem;
    display: flex;
    flex-direction: column;
    gap: 1.25rem;
    flex: 1;
  }
  .capital-block {
    min-height: 18rem;
    display: flex;
  }
  .capital-block :global(.capital-chart-wrap) {
    flex: 1;
  }
  .market-grid {
    display: grid;
    /* 2x2 grid for the canonical 4 markets — top_20 #6 */
    grid-template-columns: repeat(2, minmax(0, 1fr));
    grid-auto-rows: 10rem;
    gap: 0.75rem;
  }
  /* layout placeholder utility classes used by inner components live in their own files */

  @media (max-width: 640px) {
    .market-grid { grid-template-columns: 1fr; grid-auto-rows: 8rem; }
  }
</style>
