<script lang="ts">
  /*
   * Full-screen cinematic when the era changes. 2.5-second auto-dismiss.
   * Era name + year range + brief description. Fade in / fade out.
   * Hidden on initial mount (only triggers on actual era change, not first load).
   */

  import { sim } from '../stores/simulation.svelte';
  import { telemetry } from '../telemetry';
  import type { Era } from '../types/server';

  let visible = $state(false);
  let shownEra = $state<Era | null>(null);
  let firstSeen = $state(false);
  let dismissTimer: ReturnType<typeof setTimeout> | undefined;

  $effect(() => {
    const era = sim.era;
    if (!era) return;

    // First time we see an era after game start — record but don't animate.
    if (!firstSeen) {
      shownEra = era;
      firstSeen = true;
      return;
    }

    // Era actually changed → fire the cinematic.
    if (shownEra && era.name !== shownEra.name) {
      // Lifecycle: era_transition (P1). Logged before we overwrite shownEra so
      // {from, to} are both accurate.
      telemetry.log('era_transition', { from: shownEra.name, to: era.name });
      shownEra = era;
      visible = true;
      clearTimeout(dismissTimer);
      dismissTimer = setTimeout(() => { visible = false; }, 2500);
    }

    return () => clearTimeout(dismissTimer);
  });

  function dismiss() {
    visible = false;
    clearTimeout(dismissTimer);
  }
</script>

{#if visible && shownEra}
  <div
    class="era-overlay"
    role="button"
    tabindex="0"
    aria-label="Era transition"
    onclick={dismiss}
    onkeydown={(e) => { if (e.key === 'Enter' || e.key === ' ' || e.key === 'Escape') dismiss(); }}
  >
    <div class="era-content">
      <p class="kind">A NEW ERA BEGINS</p>
      <h1 class="era-name">{shownEra.name}</h1>
      <p class="years">{shownEra.startYear}&ndash;{shownEra.endYear}</p>
      {#if shownEra.description}
        <p class="desc">{shownEra.description}</p>
      {/if}
    </div>
  </div>
{/if}

<style>
  .era-overlay {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.85);
    z-index: 940;
    display: grid;
    place-items: center;
    padding: 2rem;
    border: none;
    cursor: pointer;
    /* fade in + auto-fade-out via CSS keyframes that match the 2.5s timer */
    animation: eraFade 2.5s ease-in-out forwards;
  }
  @keyframes eraFade {
    0%   { opacity: 0; }
    10%  { opacity: 1; }
    85%  { opacity: 1; }
    100% { opacity: 0; }
  }
  .era-content {
    text-align: center;
    font-family: var(--font-body);
    color: var(--fg-primary);
    max-width: 36rem;
  }
  .kind {
    font-family: var(--font-chrome);
    font-size: 0.75rem;
    letter-spacing: 0.5em;
    color: var(--accent-primary);
    margin: 0 0 1.25rem 0;
  }
  .era-name {
    font-family: var(--font-display);
    font-size: 3rem;
    color: var(--accent-primary);
    margin: 0;
    line-height: 1.05;
    letter-spacing: 0.02em;
    text-shadow: 0 0 30px rgba(196, 163, 90, 0.55);
  }
  .years {
    font-family: var(--font-data);
    color: var(--fg-secondary);
    font-size: 0.95rem;
    letter-spacing: 0.3em;
    margin: 0.6rem 0 1rem 0;
  }
  .desc {
    color: var(--fg-secondary);
    font-size: 1.05rem;
    line-height: 1.55;
    margin: 0;
    font-style: italic;
  }
</style>
