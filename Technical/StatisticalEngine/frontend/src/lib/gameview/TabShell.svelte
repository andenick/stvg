<script lang="ts">
  /*
   * Tab shell (P2) — hosts the active tab's content beneath the persistent
   * TopBar. Phases now gate content RICHNESS inside each tab (the panels
   * self-gate by data + year); the shell itself is era-agnostic.
   *
   *   • Persistent LEFT rail = the Loan Book (DealBoard), visible on the Economy
   *     and Hire tabs — lending is the heartbeat of the early game.
   *   • The active tab fills the rest. Each tab root carries use:dwell={'tab:<name>'}
   *     so read-time per section lands in telemetry.
   *   • DecisionDrawer (memo / drawer / resume bar) is a fixed-position global
   *     overlay and is mounted here once, across all tabs.
   */

  import DealBoard from './DealBoard.svelte';
  import DecisionDrawer from './DecisionDrawer.svelte';
  import EconomyTab from './EconomyTab.svelte';
  import HireTab from './HireTab.svelte';
  import BankTab from './BankTab.svelte';
  import FinancialsTab from './FinancialsTab.svelte';
  import { ui } from '../stores/ui.svelte';
  import { sim } from '../stores/simulation.svelte';
  import { dwell } from '../actions/dwell';

  // The Loan Book left rail rides along on the lending-first tabs.
  let showLoanRail = $derived(ui.activeTab === 'economy' || ui.activeTab === 'hire');

  // W5.3: the decision memo/drawer is a fixed overlay pinned to the right edge.
  // When one is open it visually covers the right rail (Economy) and the
  // funding-mix / annual-report side column (Financials). Reserve right gutter
  // on the tab content so those cards slide clear of the card. Works on every
  // tab because it pads the shared content wrapper, not a per-tab rail.
  let decisionOpen = $derived(sim.pendingDecisions.length > 0);
</script>

<div class="shell" class:with-rail={showLoanRail} class:decision-open={decisionOpen}>
  {#if showLoanRail}
    <aside class="loan-rail" aria-label="Loan book">
      <DealBoard />
    </aside>
  {/if}

  <div class="tab-content">
    {#if ui.activeTab === 'economy'}
      <div class="tab-root" use:dwell={'tab:economy'}><EconomyTab /></div>
    {:else if ui.activeTab === 'hire'}
      <div class="tab-root" use:dwell={'tab:hire'}><HireTab /></div>
    {:else if ui.activeTab === 'bank'}
      <div class="tab-root" use:dwell={'tab:bank'}><BankTab /></div>
    {:else if ui.activeTab === 'financials'}
      <div class="tab-root" use:dwell={'tab:financials'}><FinancialsTab /></div>
    {/if}
  </div>
</div>

<!-- Global decision overlay (fixed-position) — one instance across every tab. -->
<DecisionDrawer />

<style>
  .shell {
    display: grid;
    grid-template-columns: 1fr;
  }
  .shell.with-rail {
    grid-template-columns: minmax(20rem, 23rem) minmax(0, 1fr);
    align-items: start;
  }
  /* The rail gets its own framing padding; tab content owns its internal
     padding, so we don't double-pad the boundary between them. */
  .loan-rail { min-width: 0; padding: 1rem 0 0 1.25rem; }
  .tab-content { min-width: 0; }
  .tab-root { min-width: 0; }

  /* W5.3: reserve room for the right-pinned decision card so right-side content
     (Economy right rail; Financials funding-mix / annual report) isn't covered.
     The memo/drawer is min(30rem,94vw) / min(28rem,92vw) wide — ~31rem reserve
     clears both with a small margin. Smooth so it doesn't jump. */
  .tab-content { transition: padding-right var(--t-med, 0.24s) var(--ease, ease); }
  .shell.decision-open .tab-content { padding-right: 31rem; }

  @media (max-width: 1100px) {
    .shell.with-rail { grid-template-columns: 1fr; }
    .loan-rail { order: 2; padding: 0 1.25rem; }
    /* On narrow viewports the memo/drawer is near-full-width; don't double-pad. */
    .shell.decision-open .tab-content { padding-right: 0; }
  }
</style>
