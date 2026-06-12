<script lang="ts">
  /*
   * Active-game screen (P2). The phase-as-layout model (EarlyEra/Office/Full) is
   * gone — there is now ONE persistent shell: a sticky TopBar over a 4-tab
   * TabShell (Economy | Hire | My Bank | Financials). Phases gate content
   * RICHNESS within each tab (panels self-gate by data + year), not the layout.
   *
   * [data-era] is hoisted to <html> in App.svelte so the era palettes/textures
   * and all fixed overlays inherit the era CSS variables.
   */

  import TopBar from '../gameview/TopBar.svelte';
  import TabShell from '../gameview/TabShell.svelte';
</script>

<div class="game-root">
  <TopBar />
  <TabShell />
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
