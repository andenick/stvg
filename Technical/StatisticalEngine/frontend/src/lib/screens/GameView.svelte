<script lang="ts">
  /*
   * Active-game screen. Picks the layout for the current UI phase derived
   * from sim.year:
   *   phase 1 (1945-1959): Desk (single column, no sidebar)
   *   phase 2 (1960-1972): Office (main column + thin sidebar)
   *   phase 3 (1973-1999): Trading Floor (full two-column)
   *   phase 4 (2000-2040): Command Center (full density, doom meters, AI panel)
   *
   * [data-era] on the root drives all the era CSS variable overrides in
   * lib/styles/app.css (background, palette, fonts, density).
   */

  import EarlyEraLayout from '../gameview/EarlyEraLayout.svelte';
  import OfficeLayout from '../gameview/OfficeLayout.svelte';
  import FullLayout from '../gameview/FullLayout.svelte';
  import { ui } from '../stores/ui.svelte';
</script>

<!-- data-era is hoisted to <html> in App.svelte so fixed overlays inherit it -->
<div class="game-root">
  {#if ui.phase === 1}
    <EarlyEraLayout />
  {:else if ui.phase === 2}
    <OfficeLayout />
  {:else}
    <FullLayout />
  {/if}
</div>

<style>
  .game-root {
    min-height: 100vh;
    background: var(--bg-base);
    color: var(--fg-primary);
    font-family: var(--font-body);
    transition:
      background var(--t-slow) var(--ease),
      color var(--t-slow) var(--ease);
  }
</style>
