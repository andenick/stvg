<script lang="ts">
  /*
   * Phase 2 layout — "The Office" (1960-1972).
   * Main column (CapitalChart + small-multiples market grid + bank stub) +
   * thin sidebar (annual report stub for now, panels arrive in WP8).
   */

  import GameHeader from './GameHeader.svelte';
  import SimControls from './SimControls.svelte';
  import BankOverview from './BankOverview.svelte';
  import DivisionGrid from './DivisionGrid.svelte';
  import DecisionDrawer from './DecisionDrawer.svelte';
  import NewsTicker from './NewsTicker.svelte';
  import PathPanel from '../sidebar/PathPanel.svelte';
  import AnnualReportPanel from '../sidebar/AnnualReportPanel.svelte';
  import PersonalityPanel from '../sidebar/PersonalityPanel.svelte';
  import CapitalChart from '../charts/CapitalChart.svelte';
  import MarketChart from '../charts/MarketChart.svelte';
  import { sim } from '../stores/simulation.svelte';

  let marketIds = $derived(Object.keys(sim.markets).slice(0, 6));

  function labelFor(id: string): string {
    const map: Record<string, string> = {
      SP500: 'S&P 500',
      TREASURY10Y: '10Y TREASURY',
      GOLD: 'GOLD',
      VIX: 'VIX',
      EURUSD: 'EUR/USD',
      OIL: 'OIL',
    };
    return map[id] ?? id;
  }
</script>

<div class="office" data-phase="2" data-ui-mode="light">
  <GameHeader />
  <SimControls />

  <div class="layout">
    <main class="main-col">
      <section class="capital-block"><CapitalChart /></section>
      <section class="market-grid" aria-label="Markets">
        {#each marketIds as id (id)}
          <MarketChart marketId={id} label={labelFor(id)} compact />
        {/each}
      </section>
      <BankOverview />
      <DivisionGrid />
      <NewsTicker />
    </main>
    <aside class="side-thin">
      <AnnualReportPanel />
      <PathPanel />
      <PersonalityPanel />
    </aside>
  </div>

  <DecisionDrawer />
</div>

<style>
  .office {
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    background: var(--bg-base);
  }
  .layout {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 16rem;
    gap: 1rem;
    padding: 1rem 1.25rem;
    flex: 1;
  }
  .main-col, .side-thin {
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }
  .capital-block { min-height: 16rem; display: flex; }
  .capital-block :global(.capital-chart-wrap) { flex: 1; }
  .market-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    grid-auto-rows: 9rem;
    gap: 0.75rem;
  }
  /* sidebar/panel styling lives inside the panel components */

  @media (max-width: 880px) {
    .layout { grid-template-columns: 1fr; }
    .market-grid { grid-template-columns: repeat(2, 1fr); }
  }
</style>
