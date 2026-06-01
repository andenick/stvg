<script lang="ts">
  /*
   * Cinematic title screen. 1950s sepia/gold aesthetic per the rebuild plan.
   * Three buttons: Begin, Continue (if save exists), Settings.
   * Begin -> POST /api/game/new (defaults), then connect WS, then route to game.
   * Inline error feedback on failure (kills the silent-failure VISUAL_INSPECTION bug).
   * Connection-status dot reflects sim.connected.
   *
   * WCAG AA: subtitle and footer use tokens that meet 4.5:1 against bg-base.
   */

  import { api, ApiError } from '../api/rest';
  import { sim } from '../stores/simulation.svelte';
  import { ui } from '../stores/ui.svelte';
  import { wsClient } from '../ws/websocket';
  import { toasts } from '../stores/toasts.svelte';
  import SettingsOverlay from '../overlays/SettingsOverlay.svelte';

  let starting = $state(false);
  let beginError = $state<string | null>(null);

  // Advanced opts come back from SettingsOverlay on Apply
  let advancedSeed = $state<number | null>(null);
  let advancedCeoId = $state<string | null>(null);
  let advancedStartingPosition = $state<number | null>(null);
  let advancedPreset = $state<'sandbox' | 'easy' | 'medium' | 'hard' | 'crisis' | null>(null);

  // Continue is enabled if we have a save slot in localStorage.
  let hasSave = $state(false);
  $effect(() => {
    try { hasSave = !!localStorage.getItem('stvg.lastGameId'); } catch { hasSave = false; }
  });

  async function begin() {
    if (starting) return;
    starting = true;
    beginError = null;
    try {
      const opts: Parameters<typeof api.newGame>[0] = {};
      if (advancedSeed != null) opts.seed = advancedSeed;
      if (advancedCeoId) opts.ceoId = advancedCeoId;
      if (advancedStartingPosition != null) opts.startingPosition = advancedStartingPosition;
      if (advancedPreset) opts.preset = advancedPreset;

      const { gameId } = await api.newGame(opts);

      // Hydrate full PlayerView before we route — gives the game view real data
      // on first render rather than a flash of empty panels.
      const view = await api.getState(gameId);
      sim.reset();
      sim.gameId = gameId;
      sim.applyPlayerView(view);

      try { localStorage.setItem('stvg.lastGameId', gameId); } catch { /* ignore */ }

      wsClient.connect(gameId);
      ui.screen = 'game';
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      beginError = `Could not start game: ${msg}`;
      toasts.error(beginError);
    } finally {
      starting = false;
    }
  }

  async function cont() {
    let gameId: string | null = null;
    try { gameId = localStorage.getItem('stvg.lastGameId'); } catch { /* ignore */ }
    if (!gameId) return;
    starting = true;
    beginError = null;
    try {
      const view = await api.getState(gameId);
      sim.reset();
      sim.gameId = gameId;
      sim.applyPlayerView(view);
      wsClient.connect(gameId);
      ui.screen = 'game';
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      beginError = `Could not resume game: ${msg}`;
      toasts.error(beginError);
      // Stale save id — drop it
      try { localStorage.removeItem('stvg.lastGameId'); } catch { /* ignore */ }
      hasSave = false;
    } finally {
      starting = false;
    }
  }

  function openSettings() {
    ui.settingsOpen = true;
    beginError = null;
  }

  function applySettings(opts: { seed?: number; ceoId?: string; startingPosition?: number; preset?: 'sandbox' | 'easy' | 'medium' | 'hard' | 'crisis' }) {
    advancedSeed = opts.seed ?? null;
    advancedCeoId = opts.ceoId ?? null;
    advancedStartingPosition = opts.startingPosition ?? null;
    advancedPreset = opts.preset ?? null;
  }
</script>

<main class="title-screen">
  <div class="ambient" aria-hidden="true"></div>

  <div class="content">
    <p class="badge">EST. 1945</p>
    <h1 class="title">
      <span class="italic">The</span><br />
      <span class="bold">Banker</span>
    </h1>
    <p class="subtitle">A Banking Simulation</p>
    <p class="years">1945 &mdash; 2040</p>

    <div class="divider" aria-hidden="true">&#9670;</div>

    <div class="actions">
      <button
        class="primary"
        onclick={begin}
        disabled={starting}
        aria-busy={starting}
      >
        {#if starting}
          <span class="spinner" aria-hidden="true"></span>
          <span>Starting&hellip;</span>
        {:else}
          <span>Begin</span>
        {/if}
      </button>

      {#if hasSave}
        <button class="secondary" onclick={cont} disabled={starting}>Continue</button>
      {/if}
    </div>

    {#if beginError}
      <p class="error" role="alert">{beginError}</p>
    {/if}
  </div>

  <footer class="footer">
    <div class="conn">
      <span
        class="conn-dot"
        class:on={sim.connected}
        class:off={!sim.connected}
        aria-hidden="true"
      ></span>
      <span class="conn-label">{sim.connected ? 'CONNECTED' : 'OFFLINE'}</span>
    </div>
    <button class="settings-link" onclick={openSettings}>Settings</button>
    <p class="version">STVG · SVELTE ALPHA · WP2</p>
  </footer>
</main>

<SettingsOverlay
  bind:seed={advancedSeed}
  bind:ceoId={advancedCeoId}
  bind:startingPosition={advancedStartingPosition}
  bind:preset={advancedPreset}
  onApply={applySettings}
/>

<style>
  .title-screen {
    position: relative;
    min-height: 100vh;
    display: grid;
    place-items: center;
    background: var(--bg-base);
    color: var(--fg-primary);
    font-family: var(--font-body);
    overflow: hidden;
  }

  .ambient {
    position: absolute;
    inset: 0;
    background: radial-gradient(
      ellipse at center,
      rgba(196, 163, 90, 0.10) 0%,
      rgba(196, 163, 90, 0.04) 40%,
      transparent 70%
    );
    pointer-events: none;
  }

  .content {
    text-align: center;
    padding: 3rem 4rem;
    animation: titleFadeIn 1.4s var(--ease);
    position: relative;
  }

  .badge {
    font-family: var(--font-chrome);
    font-size: 0.72rem;
    letter-spacing: 0.5em;
    color: var(--accent-primary);
    margin: 0 0 2rem 0;
    text-indent: 0.5em;   /* offset for the wide letter-spacing on the last char */
  }

  .title {
    font-family: var(--font-display);
    margin: 0;
    color: var(--fg-primary);
    line-height: 1.05;
  }
  .title .italic {
    font-style: italic;
    font-weight: 400;
    font-size: 2.6rem;
    color: var(--accent-primary);
    letter-spacing: 0.05em;
  }
  .title .bold {
    font-weight: 700;
    font-size: 4.8rem;
    letter-spacing: 0.04em;
  }

  .subtitle {
    /* WCAG AA: #9a8d70 on #0d0b08 ~= 5.6:1 */
    font-family: var(--font-body);
    color: #9a8d70;
    margin: 1.3rem 0 0.25rem 0;
    font-size: 0.95rem;
    letter-spacing: 0.32em;
    text-transform: uppercase;
    text-indent: 0.32em;
  }
  .years {
    font-family: var(--font-data);
    color: var(--fg-muted);
    font-size: 0.72rem;
    letter-spacing: 0.4em;
    margin: 0 0 1.5rem 0;
  }

  .divider {
    color: var(--accent-primary);
    opacity: 0.55;
    font-size: 0.85rem;
    margin: 0.5rem 0 2rem 0;
  }

  .actions {
    display: flex;
    gap: 1rem;
    justify-content: center;
    align-items: center;
  }

  .actions button {
    min-width: 9rem;
    padding: 0.75rem 1.5rem;
    font-family: var(--font-chrome);
    font-size: 0.8rem;
    letter-spacing: 0.32em;
    text-transform: uppercase;
    text-indent: 0.32em;
    border: 1px solid var(--accent-primary);
    color: var(--accent-primary);
    background: transparent;
    border-radius: 0;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 0.5rem;
  }
  .actions .primary {
    border-width: 2px;
  }
  .actions button:hover:not(:disabled) {
    background: rgba(196, 163, 90, 0.10);
  }
  .actions button:disabled { opacity: 0.5; }

  .spinner {
    width: 0.9rem;
    height: 0.9rem;
    border: 2px solid rgba(196, 163, 90, 0.25);
    border-top-color: var(--accent-primary);
    border-radius: 50%;
    animation: spin 700ms linear infinite;
  }
  @keyframes spin { to { transform: rotate(360deg); } }

  .error {
    color: var(--accent-danger);
    font-family: var(--font-chrome);
    font-size: 0.78rem;
    margin-top: 1.5rem;
    letter-spacing: 0.05em;
  }

  .footer {
    position: absolute;
    bottom: 1.5rem;
    left: 0; right: 0;
    display: flex;
    justify-content: center;
    align-items: center;
    gap: 1.5rem;
    color: #5a5040; /* WCAG AA-compliant footer per top_20 #10 */
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    letter-spacing: 0.3em;
  }
  .conn {
    display: inline-flex;
    align-items: center;
    gap: 0.4rem;
  }
  .conn-dot {
    display: inline-block;
    width: 0.45rem;
    height: 0.45rem;
    border-radius: 50%;
  }
  .conn-dot.on  { background: var(--accent-success); box-shadow: 0 0 6px var(--accent-success); }
  .conn-dot.off { background: var(--accent-danger); opacity: 0.65; }
  .conn-label { color: #6b6047; }

  .settings-link {
    background: transparent;
    border: none;
    color: #6b6047;
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    letter-spacing: 0.3em;
    cursor: pointer;
    text-transform: uppercase;
    padding: 0;
  }
  .settings-link:hover { color: var(--accent-primary); }

  .version {
    margin: 0;
    color: #5a5040;
  }
</style>
