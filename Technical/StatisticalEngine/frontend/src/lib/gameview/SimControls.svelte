<script lang="ts">
  /*
   * Play/Pause + speed selector + always-visible date readout.
   *
   * Sends simulation_control over WebSocket (engine wires this to
   * SimulationRunner::play/pause/setSpeed).
   *
   * Bugs fixed:
   *   - top_20 #7: era badge + regime indicator visible from phase 1
   *   - top_20 #12: date is always visible at every viewport width
   *
   * Keyboard: Space=play/pause, 1=1x, 2=2x, 3=4x, 4=8x
   */

  import { sim } from '../stores/simulation.svelte';
  import { ui } from '../stores/ui.svelte';
  import { wsClient } from '../ws/websocket';
  import ContextualHint from '../overlays/ContextualHint.svelte';

  const SPEEDS = [1, 2, 4, 8] as const;
  type Speed = (typeof SPEEDS)[number];

  function toggle() {
    if (!sim.gameId) return;
    if (sim.paused) wsClient.play();
    else wsClient.pause();
    // Optimistic local update; server will broadcast confirmation
    sim.paused = !sim.paused;
  }

  function setSpeed(s: Speed) {
    if (!sim.gameId) return;
    wsClient.setSpeed(s);
    sim.speed = s;
  }

  // Whether the play button is gated. Engine refuses to advance into a
  // simulation phase while there are pending decisions or unresolved
  // crises — surface that as a disabled state with a tooltip.
  let gated = $derived(
    sim.pendingDecisions.length > 0 || sim.activeCrises.length > 0
  );

  let regimeLabel = $derived(
    sim.regime === 'normal'   ? 'Calm' :
    sim.regime === 'volatile' ? 'Volatile' :
    sim.regime === 'stressed' ? 'Stressed' :
    sim.regime === 'crisis'   ? 'Crisis' :
    String(sim.regime ?? 'Calm')
  );

  function onKey(e: KeyboardEvent) {
    // Don't steal keys while typing in an input
    const t = e.target as HTMLElement;
    if (t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.isContentEditable)) return;

    if (e.code === 'Space') { e.preventDefault(); toggle(); }
    else if (e.key === '1') setSpeed(1);
    else if (e.key === '2') setSpeed(2);
    else if (e.key === '3') setSpeed(4);
    else if (e.key === '4') setSpeed(8);
    else if (e.key === '?' && !ui.keyboardHelpOpen) ui.keyboardHelpOpen = true;
  }

  $effect(() => {
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });
</script>

<div class="sim-controls" role="toolbar" aria-label="Simulation controls">
  <ContextualHint id="hint:sim-controls" position="below" delayMs={1200}>
    Press <kbd>Space</kbd> to start the simulation. Keys <kbd>1</kbd>–<kbd>4</kbd> set speed. Press <kbd>?</kbd> for all shortcuts.
  </ContextualHint>
  <button
    class="play"
    class:running={!sim.paused && !gated}
    aria-label={sim.paused ? 'Play' : 'Pause'}
    aria-pressed={!sim.paused}
    onclick={toggle}
    disabled={gated || !sim.gameId}
    title={gated ? 'Resolve pending decisions / crises first' : (sim.paused ? 'Play (Space)' : 'Pause (Space)')}
  >
    {#if sim.paused}
      <span aria-hidden="true">&#9654;</span>
    {:else}
      <span aria-hidden="true">&#9208;</span>
    {/if}
  </button>

  <div class="speeds" role="group" aria-label="Simulation speed">
    {#each SPEEDS as s}
      <button
        class="speed"
        class:active={sim.speed === s}
        onclick={() => setSpeed(s)}
        disabled={!sim.gameId}
        title="Speed {s}x ({SPEEDS.indexOf(s) + 1})"
      >{s}&times;</button>
    {/each}
  </div>

  <div class="sim-date" aria-live="polite">
    <span
      class="pulse"
      class:on={!sim.paused && !gated}
      aria-hidden="true"
    ></span>
    <span class="date-text num">
      {sim.year} Q{sim.quarter}
    </span>
    <span class="day-text num">Day {sim.day}</span>
  </div>

  <div class="era-regime" aria-label="Era and market regime">
    <span class="era">{sim.era?.name ?? 'Post-War Stability'}</span>
    <span class="regime regime-{sim.regime}">
      <span class="regime-dot" aria-hidden="true"></span>
      {regimeLabel}
    </span>
  </div>
</div>

<style>
  .sim-controls {
    /* relative so ContextualHint can absolutely-position itself against it */
    position: relative;
    display: flex;
    align-items: center;
    gap: 1rem;
    padding: 0.6rem 1rem;
    background: var(--bg-card);
    border-bottom: 1px solid var(--border);
    font-family: var(--font-chrome);
    flex-wrap: wrap;
  }
  kbd {
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    background: var(--bg-elevated);
    border: 1px solid var(--border);
    border-bottom-width: 2px;
    border-radius: 3px;
    padding: 0.05rem 0.32rem;
    color: var(--accent-primary);
  }

  .play {
    width: 2.5rem;
    height: 2.5rem;
    border-radius: 50%;
    border: 2px solid var(--accent-primary);
    color: var(--accent-primary);
    font-size: 1.1rem;
    padding: 0;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    transition: all var(--t-fast) var(--ease);
  }
  .play:hover:not(:disabled) {
    background: rgba(196, 163, 90, 0.12);
  }
  .play.running {
    box-shadow: 0 0 0 0 var(--accent-success);
    animation: simPulse 1.6s ease-in-out infinite;
  }

  .speeds {
    display: inline-flex;
    gap: 0.25rem;
  }
  .speed {
    padding: 0.35rem 0.65rem;
    font-size: var(--fs-xs);
    letter-spacing: 0.1em;
    color: var(--fg-secondary);
    background: transparent;
    border: 1px solid var(--border);
  }
  .speed.active {
    color: var(--accent-primary);
    border-color: var(--accent-primary);
    background: rgba(196, 163, 90, 0.08);
  }
  .speed:hover:not(:disabled):not(.active) {
    color: var(--fg-primary);
    border-color: var(--fg-secondary);
  }

  .sim-date {
    display: inline-flex;
    align-items: center;
    gap: 0.5rem;
    /* Bug fix top_20 #12: never hide via media queries */
    flex: 0 0 auto;
  }
  .pulse {
    width: 0.55rem;
    height: 0.55rem;
    border-radius: 50%;
    background: var(--fg-muted);
  }
  .pulse.on {
    background: var(--accent-success);
    animation: pulse 1.4s ease-in-out infinite;
  }
  .date-text {
    color: var(--fg-primary);
    font-size: var(--fs-sm);
    letter-spacing: 0.1em;
  }
  .day-text {
    color: var(--fg-secondary);
    font-size: var(--fs-xs);
    letter-spacing: 0.08em;
  }

  .era-regime {
    margin-left: auto;
    display: inline-flex;
    align-items: center;
    gap: 0.75rem;
    color: var(--fg-secondary);
    font-size: var(--fs-xs);
    letter-spacing: 0.2em;
    text-transform: uppercase;
  }
  .era {
    color: var(--accent-primary);
  }
  .regime {
    display: inline-flex;
    align-items: center;
    gap: 0.35rem;
    border: 1px solid var(--border);
    border-radius: 999px;
    padding: 0.15rem 0.55rem;
  }
  .regime-dot {
    width: 0.45rem;
    height: 0.45rem;
    border-radius: 50%;
    background: var(--accent-success);
  }
  .regime-volatile .regime-dot { background: var(--accent-warning); }
  .regime-stressed .regime-dot { background: var(--accent-danger); }
  .regime-crisis   .regime-dot { background: var(--accent-danger); animation: pulse 1s ease-in-out infinite; }
</style>
