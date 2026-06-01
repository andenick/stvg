<script lang="ts">
  import TitleScreen from './lib/screens/TitleScreen.svelte';
  import GameView from './lib/screens/GameView.svelte';
  import GameOverScreen from './lib/screens/GameOverScreen.svelte';
  import ToastContainer from './lib/overlays/ToastContainer.svelte';
  import CrisisModal from './lib/overlays/CrisisModal.svelte';
  import AnnualReportModal from './lib/overlays/AnnualReportModal.svelte';
  import EraTransition from './lib/overlays/EraTransition.svelte';
  import KeyboardHelp from './lib/overlays/KeyboardHelp.svelte';
  import { ui } from './lib/stores/ui.svelte';
  import { sim } from './lib/stores/simulation.svelte';

  // Auto-route to game-over when the engine emits a gameEnd payload.
  $effect(() => {
    if (sim.gameEnd && ui.screen === 'game') {
      ui.screen = 'game-over';
    }
  });

  /*
   * Era theming — hoist [data-era] to <html> so every fixed/overlay element
   * (DecisionDrawer, SettingsOverlay, modals, toasts, NewsTicker event log)
   * inherits the era CSS variables. The title screen overrides this with its
   * own warm 1950s palette via [data-era="post-war"] on its root.
   *
   * Transition: the 1-second background ease declared in app.css on
   * html/body/#app gives the "shifting world" feel between eras.
   */
  $effect(() => {
    const era = ui.screen === 'title' ? 'post-war' : ui.eraSlug;
    document.documentElement.setAttribute('data-era', era);
  });
</script>

{#if ui.screen === 'title'}
  <TitleScreen />
{:else if ui.screen === 'game'}
  <GameView />
{:else if ui.screen === 'game-over'}
  <GameOverScreen />
{/if}

{#if ui.screen === 'game'}
  <CrisisModal />
  <AnnualReportModal />
  <EraTransition />
{/if}

<KeyboardHelp />
<ToastContainer />
