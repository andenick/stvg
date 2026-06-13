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
  import { telemetry } from '../telemetry';
  import { hydrateArchetypes } from '../characters/archetype-store';
  import SettingsOverlay from '../overlays/SettingsOverlay.svelte';

  let starting = $state(false);
  let beginError = $state<string | null>(null);

  /**
   * P3: backfill macro ring buffers (+ market closes) from the per-quarter
   * macro-history endpoint so the Economy tab's charts aren't empty after a
   * reload/save-load. Best-effort — a fresh game's history is empty and the
   * live stream fills forward, so never let this block routing into the game.
   */
  async function backfillMacro(gameId: string) {
    try {
      const hist = await api.macroHistory(gameId);
      if (hist?.quarters?.length) sim.hydrateMacroHistory(hist.quarters);
    } catch { /* best-effort; forward-only is acceptable */ }
  }

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

  /*
   * W5.5: server-reachability indicator. The WebSocket only connects after
   * Begin, so `sim.connected` is ALWAYS false pre-game — the old footer always
   * said OFFLINE on the title screen, which was misleading. Probe /api/health
   * once on mount (and after a game is running, defer to the real WS state).
   *   'checking' → 'reachable' → server up; 'unreachable' → server down.
   */
  let serverReachable = $state<'checking' | 'reachable' | 'unreachable'>('checking');
  $effect(() => {
    let cancelled = false;
    serverReachable = 'checking';
    api.health()
      .then(() => { if (!cancelled) serverReachable = 'reachable'; })
      .catch(() => { if (!cancelled) serverReachable = 'unreachable'; });
    return () => { cancelled = true; };
  });
  // Pre-game we trust the health probe; once the WS is live, mirror it.
  let online = $derived(sim.connected || serverReachable === 'reachable');
  let connLabel = $derived(
    sim.connected ? 'CONNECTED'
    : serverReachable === 'checking' ? 'CHECKING…'
    : serverReachable === 'reachable' ? 'SERVER UP'
    : 'OFFLINE',
  );

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

      const { gameId, startingPosition } = await api.newGame(opts);

      // W3: fetch the canonical archetype roster ONCE before the game surface
      // renders, so family voice/color/credibility + specialization display all
      // read from the single source (GET /api/archetypes). Idempotent + cached;
      // best-effort (a failure leaves safe fallbacks, never blocks routing).
      await hydrateArchetypes();

      // Hydrate full PlayerView before we route — gives the game view real data
      // on first render rather than a flash of empty panels.
      const view = await api.getState(gameId);
      sim.reset();
      sim.gameId = gameId;
      sim.applyPlayerView(view);
      await backfillMacro(gameId);

      try { localStorage.setItem('stvg.lastGameId', gameId); } catch { /* ignore */ }

      // Lifecycle: game_start (P1). Logged after the view hydrates so the
      // context stamp carries the real opening year/capital.
      telemetry.log('game_start', {
        ceoId: sim.ceoId ?? advancedCeoId ?? null,
        scenario: advancedPreset ?? 'default',
        startingPosition: startingPosition ?? advancedStartingPosition ?? null,
      });

      wsClient.connect(gameId);
      ui.screen = 'game';
    } catch (e) {
      // Admission control: a 503 {error:'server_full'} means the box is at
      // capacity — show a friendly, non-alarming line rather than a raw error.
      if (e instanceof ApiError && (e.status === 503 || e.message === 'server_full')) {
        beginError = 'Server full — too many players right now. Try again soon.';
      } else if (e instanceof ApiError && (e.status === 429 || e.message === 'ip_limit')) {
        beginError = 'You already have a few games open from this connection. Close one and try again.';
      } else {
        const msg = e instanceof ApiError ? e.message : (e as Error).message;
        beginError = `Could not start game: ${msg}`;
      }
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
      // W3: hydrate the canonical archetype roster before the game renders
      // (same single source as the Begin path). Idempotent + best-effort.
      await hydrateArchetypes();
      const view = await api.getState(gameId);
      sim.reset();
      sim.gameId = gameId;
      sim.applyPlayerView(view);
      await backfillMacro(gameId);
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

    <p class="privacy">
      Playtest build — anonymous gameplay events are recorded to improve the game.
    </p>
  </div>

  <footer class="footer">
    <div class="conn">
      <span
        class="conn-dot"
        class:on={online}
        class:off={!online}
        aria-hidden="true"
      ></span>
      <span class="conn-label">{connLabel}</span>
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
  /*
   * Self-contained title-screen palette (0.1). The pre-game title must NOT
   * depend on the game-era CSS vars set on <html>/<main>, because those are
   * only applied once a game is running (and a prior dark-era game can leave
   * <html data-era> on a dark palette, rendering dark-on-dark / invisible
   * "The Banker"). We pin explicit, high-contrast tokens here so the title,
   * subtitle, and Begin button are always legible at a glance. WCAG AA: the
   * gold title (#5a4410) and ink subtitle on the warm beige bg clear 4.5:1.
   */
  .title-screen {
    /* local tokens — override anything inherited from a stale data-era */
    --ts-bg: #e7dcc1;          /* warm parchment */
    --ts-bg-edge: #d2c39c;     /* slightly darker vignette edge */
    --ts-ink: #241d10;         /* near-black ink */
    --ts-ink-soft: #4a3d24;    /* secondary ink */
    --ts-ink-muted: #6f6044;   /* muted */
    --ts-gold: #5a4410;        /* deep, legible gold (not the light accent) */
    --ts-gold-bright: #8a6a1f;
    --ts-danger: #8c2f22;

    position: relative;
    min-height: 100vh;
    display: grid;
    place-items: center;
    background:
      radial-gradient(ellipse at center, var(--ts-bg) 0%, var(--ts-bg-edge) 100%);
    color: var(--ts-ink);
    font-family: var(--font-body);
    overflow: hidden;
  }

  .ambient {
    position: absolute;
    inset: 0;
    background: radial-gradient(
      ellipse at center,
      rgba(138, 106, 31, 0.12) 0%,
      rgba(138, 106, 31, 0.05) 40%,
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
    color: var(--ts-gold-bright);
    margin: 0 0 2rem 0;
    text-indent: 0.5em;   /* offset for the wide letter-spacing on the last char */
  }

  .title {
    font-family: var(--font-display);
    margin: 0;
    color: var(--ts-ink);
    line-height: 1.05;
  }
  .title .italic {
    font-style: italic;
    font-weight: 400;
    font-size: 2.6rem;
    color: var(--ts-gold);
    letter-spacing: 0.05em;
  }
  .title .bold {
    font-weight: 700;
    font-size: 4.8rem;
    letter-spacing: 0.04em;
    color: var(--ts-ink);
  }

  .subtitle {
    font-family: var(--font-body);
    color: var(--ts-ink-soft);
    margin: 1.3rem 0 0.25rem 0;
    font-size: 0.95rem;
    letter-spacing: 0.32em;
    text-transform: uppercase;
    text-indent: 0.32em;
  }
  .years {
    font-family: var(--font-data);
    color: var(--ts-ink-muted);
    font-size: 0.72rem;
    letter-spacing: 0.4em;
    margin: 0 0 1.5rem 0;
  }

  .divider {
    color: var(--ts-gold);
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
    border: 1px solid var(--ts-gold);
    color: var(--ts-gold);
    background: transparent;
    border-radius: 0;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 0.5rem;
  }
  /* The Begin button is the primary call to action — make it solid + legible. */
  .actions .primary {
    border-width: 2px;
    background: var(--ts-gold);
    color: #f4ecd6;
  }
  .actions .primary:hover:not(:disabled) {
    background: var(--ts-gold-bright);
    border-color: var(--ts-gold-bright);
  }
  .actions .secondary:hover:not(:disabled) {
    background: rgba(138, 106, 31, 0.10);
  }
  .actions button:disabled { opacity: 0.5; }

  .spinner {
    width: 0.9rem;
    height: 0.9rem;
    border: 2px solid rgba(244, 236, 214, 0.35);
    border-top-color: #f4ecd6;
    border-radius: 50%;
    animation: spin 700ms linear infinite;
  }
  @keyframes spin { to { transform: rotate(360deg); } }

  .error {
    color: var(--ts-danger);
    font-family: var(--font-chrome);
    font-size: 0.78rem;
    margin-top: 1.5rem;
    letter-spacing: 0.05em;
  }

  .privacy {
    color: var(--ts-ink-muted);
    font-family: var(--font-body);
    font-size: 0.72rem;
    line-height: 1.4;
    max-width: 28rem;
    margin: 1.75rem auto 0 auto;
    opacity: 0.85;
  }

  .footer {
    position: absolute;
    bottom: 1.5rem;
    left: 0; right: 0;
    display: flex;
    justify-content: center;
    align-items: center;
    gap: 1.5rem;
    color: var(--ts-ink-muted);
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
  .conn-dot.on  { background: #4d7836; box-shadow: 0 0 6px #4d7836; }
  .conn-dot.off { background: var(--ts-danger); opacity: 0.65; }
  .conn-label { color: var(--ts-ink-muted); }

  .settings-link {
    background: transparent;
    border: none;
    color: var(--ts-ink-muted);
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    letter-spacing: 0.3em;
    cursor: pointer;
    text-transform: uppercase;
    padding: 0;
  }
  .settings-link:hover { color: var(--ts-gold); }

  .version {
    margin: 0;
    color: var(--ts-ink-muted);
  }
</style>
