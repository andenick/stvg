<script lang="ts">
  /*
   * My Bank tab (P2) — the bank's own surfaces: overview, divisions, path &
   * personality, plus a Trading Floor placeholder strip (the P9 floor view).
   * Path/Personality self-gate by year so phase-1 simply shows the basics.
   */

  import BankOverview from './BankOverview.svelte';
  import DivisionGrid from './DivisionGrid.svelte';
  import PathPanel from '../sidebar/PathPanel.svelte';
  import PersonalityPanel from '../sidebar/PersonalityPanel.svelte';
</script>

<div class="bank-tab">
  <main class="bank-main">
    <BankOverview />
    <DivisionGrid />

    <!-- P9: TradingFloor -->
    <section class="floor-placeholder" aria-label="Trading floor">
      <span class="floor-title">Trading Floor</span>
      <span class="floor-sub">coming soon</span>
    </section>
  </main>

  <aside class="bank-side">
    <PathPanel />
    <PersonalityPanel />
  </aside>
</div>

<style>
  .bank-tab {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 18rem;
    gap: 1.25rem;
    padding: 1.25rem 1.5rem;
    align-items: start;
  }
  .bank-main, .bank-side { min-width: 0; display: flex; flex-direction: column; gap: 1rem; }

  .floor-placeholder {
    display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 0.3rem;
    min-height: 9rem;
    background: var(--bg-card);
    border: 1px dashed var(--border);
    border-radius: var(--card-radius);
    color: var(--fg-secondary);
  }
  .floor-title { font-family: var(--font-display); color: var(--accent-primary); font-size: 1.2rem; letter-spacing: 0.05em; }
  .floor-sub { font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.22em; text-transform: uppercase; color: var(--fg-muted); }

  @media (max-width: 900px) {
    .bank-tab { grid-template-columns: 1fr; }
    .bank-side { order: 2; }
  }
</style>
