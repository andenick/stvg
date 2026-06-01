<script lang="ts">
  /*
   * Phase 3/4 layout — "The Trading Floor" / "The Command Center" (1973+).
   * Two-column full density. Left sidebar (path, competitors), right sidebar
   * (annual report, personality, regulatory), main chart grid.
   */

  import GameHeader from './GameHeader.svelte';
  import SimControls from './SimControls.svelte';
  import BankOverview from './BankOverview.svelte';
  import DivisionGrid from './DivisionGrid.svelte';
  import DecisionDrawer from './DecisionDrawer.svelte';
  import NewsTicker from './NewsTicker.svelte';
  import PathPanel from '../sidebar/PathPanel.svelte';
  import PersonalityPanel from '../sidebar/PersonalityPanel.svelte';
  import CompetitorPanel from '../sidebar/CompetitorPanel.svelte';
  import LeaderboardPanel from '../sidebar/LeaderboardPanel.svelte';
  import AnnualReportPanel from '../sidebar/AnnualReportPanel.svelte';
  import PoliticalPanel from '../sidebar/PoliticalPanel.svelte';
  import RegulatoryPanel from '../sidebar/RegulatoryPanel.svelte';
  import CapitalChart from '../charts/CapitalChart.svelte';
  import MarketChart from '../charts/MarketChart.svelte';
  import { sim } from '../stores/simulation.svelte';
  import { ui } from '../stores/ui.svelte';

  let marketIds = $derived(Object.keys(sim.markets).slice(0, 6));

  function labelFor(id: string): string {
    const map: Record<string, string> = {
      SP500: 'S&P 500',
      TREASURY10Y: '10Y TREASURY',
      GOLD: 'GOLD',
      VIX: 'VIX',
      EURUSD: 'EUR/USD',
      OIL: 'OIL',
      BITCOIN: 'BITCOIN',
    };
    return map[id] ?? id;
  }
</script>

<div class="floor" data-phase={ui.phase} data-ui-mode={ui.phase === 4 ? 'dense' : 'full'}>
  <GameHeader />
  <SimControls />

  <div class="grid">
    <aside class="left">
      <PathPanel />
      <LeaderboardPanel />
      <CompetitorPanel />
      {#if ui.phase === 4}
        <section class="placeholder"><h2>Doom</h2><p class="muted">WP9 / WP13</p></section>
      {/if}
    </aside>

    <main class="center">
      <section class="capital-block"><CapitalChart /></section>
      <section class="market-grid" aria-label="Market charts">
        {#each marketIds as id (id)}
          <MarketChart marketId={id} label={labelFor(id)} />
        {/each}
      </section>
      <BankOverview />
      <DivisionGrid />
      <NewsTicker />
    </main>

    <aside class="right">
      <AnnualReportPanel />
      <PersonalityPanel />
      <PoliticalPanel />
      <RegulatoryPanel />
      {#if ui.phase === 4}
        <section class="placeholder"><h2>AI / Climate</h2><p class="muted">WP9</p></section>
      {/if}
    </aside>
  </div>

  <DecisionDrawer />
</div>

<style>
  .floor {
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    background: var(--bg-base);
  }
  .grid {
    display: grid;
    grid-template-columns: 14rem minmax(0, 1fr) 16rem;
    gap: 0.75rem;
    padding: 0.75rem;
    flex: 1;
  }
  .left, .center, .right {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
    min-width: 0;
  }
  .placeholder {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 0.9rem 1rem;
    min-height: 4rem;
  }
  .placeholder h2 {
    font-family: var(--font-display);
    color: var(--accent-primary);
    margin: 0 0 0.3rem 0;
    font-size: 0.95rem;
    letter-spacing: 0.05em;
  }
  .muted { color: var(--fg-secondary); margin: 0; font-size: var(--fs-xs); }

  .capital-block { min-height: 18rem; display: flex; }
  .capital-block :global(.capital-chart-wrap) { flex: 1; }
  .market-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    grid-auto-rows: 10rem;
    gap: 0.75rem;
  }

  @media (max-width: 1100px) {
    .grid { grid-template-columns: 1fr; }
    .market-grid { grid-template-columns: repeat(2, 1fr); }
  }
</style>
