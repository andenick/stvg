<script lang="ts">
  /*
   * Crisis modal — cinematic full-viewport take-over.
   *
   * Plan WP10 details:
   *   - Typewriter description reveal (~25ms/char)
   *   - Response buttons appear after a 1s delay so the player has to read first
   *   - Severity meter (horizontal bar) — 1..10
   *   - Border-pulse animation for severity > 7 (red)
   *   - Amber glow for severity > 4
   *   - Screen shake @keyframes for severity > 7
   *   - Type icon: skull for catastrophic, warning triangle for moderate
   *
   * Triggered: WebSocket quarter_boundary {phase:"crisis_response"} pushes a
   * CrisisArc into sim.activeCrises; we render the first one.
   *
   * Submit: POST /api/game/<id>/crisis/respond with {crisisId, responseType}
   * where responseType ∈ {aggressive, measured, gamble} (server-enforced).
   */

  import { sim } from '../stores/simulation.svelte';
  import { api, ApiError } from '../api/rest';
  import { toasts } from '../stores/toasts.svelte';
  import { telemetry } from '../telemetry';
  import { dwell } from '../actions/dwell';
  import type { CrisisArc, CrisisResponseType } from '../types/server';

  // Show first unresolved crisis (engine queues them one at a time anyway)
  let crisis = $derived<CrisisArc | undefined>(
    sim.activeCrises.find((c) => !c.resolved)
  );

  // ── Typewriter ───────────────────────────────────────────────────────
  let revealedChars = $state(0);
  let buttonsReady = $state(false);
  let typewriterTimer: ReturnType<typeof setInterval> | undefined;
  let buttonsTimer: ReturnType<typeof setTimeout> | undefined;
  let submitting = $state(false);

  // Reset typewriter whenever the active crisis changes
  let lastCrisisId = $state<string | null>(null);

  $effect(() => {
    const id = crisis?.id ?? null;
    if (id === lastCrisisId) return;

    // Lifecycle (P1): the previous crisis closed, and/or a new one opened. Log
    // crisis_end for the one leaving and crisis_start for the one arriving.
    if (lastCrisisId !== null) {
      telemetry.log('crisis_end', { crisisId: lastCrisisId });
    }
    if (id !== null && crisis) {
      telemetry.log('crisis_start', {
        crisisId: id, title: crisis.title, severity: crisis.severity,
      });
    }
    lastCrisisId = id;

    // clear pending
    clearInterval(typewriterTimer);
    clearTimeout(buttonsTimer);
    revealedChars = 0;
    buttonsReady = false;

    if (!crisis) return;

    const target = crisis.description?.length ?? 0;
    if (target === 0) {
      revealedChars = 0;
      buttonsReady = true;
      return;
    }
    typewriterTimer = setInterval(() => {
      revealedChars++;
      if (revealedChars >= target) {
        clearInterval(typewriterTimer);
        // 1-second delay before buttons after text completes (per plan)
        buttonsTimer = setTimeout(() => { buttonsReady = true; }, 1000);
      }
    }, 25);

    return () => {
      clearInterval(typewriterTimer);
      clearTimeout(buttonsTimer);
    };
  });

  async function respond(type: CrisisResponseType) {
    if (submitting || !crisis || !sim.gameId) return;
    submitting = true;
    telemetry.log('crisis_respond', { crisisId: crisis.id, choice: type, severity: crisis.severity });
    try {
      await api.respondCrisis(sim.gameId, crisis.id, type);
      toasts.success(`Crisis response: ${type}`);
      // Refetch full state so UI reflects post-resolution AP, capital, etc.
      const view = await api.getState(sim.gameId);
      sim.applyPlayerView(view);
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      toasts.error(`Crisis response failed: ${msg}`);
    } finally {
      submitting = false;
    }
  }

  function severityIcon(s: number): string {
    if (s >= 8) return '☠';     // skull
    if (s >= 5) return '⚠';     // warning
    return '◬';                 // attention
  }

  function severityLabel(s: number): string {
    if (s >= 9) return 'EXISTENTIAL';
    if (s >= 7) return 'CATASTROPHIC';
    if (s >= 5) return 'SEVERE';
    if (s >= 3) return 'MODERATE';
    return 'MINOR';
  }

  // Skip the typewriter on click anywhere on the description
  function skipTypewriter() {
    if (!crisis) return;
    revealedChars = crisis.description?.length ?? 0;
    clearInterval(typewriterTimer);
    clearTimeout(buttonsTimer);
    buttonsTimer = setTimeout(() => { buttonsReady = true; }, 400);
  }

  let visibleText = $derived(crisis?.description?.slice(0, revealedChars) ?? '');
  let isShake = $derived((crisis?.severity ?? 0) > 7);
  let isPulse = $derived((crisis?.severity ?? 0) > 7);
  let isGlow = $derived((crisis?.severity ?? 0) > 4);
</script>

{#if crisis}
  <div class="vignette" aria-hidden="true"></div>
  <div
    class="crisis-modal"
    role="alertdialog"
    aria-labelledby="crisis-title"
    aria-describedby="crisis-body"
    class:shake={isShake}
    class:pulse={isPulse}
    class:glow={isGlow}
    use:dwell={{ key: `crisis:${crisis.id}`, data: { title: crisis.title, severity: crisis.severity } }}
  >
    <header class="crisis-head">
      <span class="icon" aria-hidden="true">{severityIcon(crisis.severity)}</span>
      <div class="head-text">
        <p class="kind">{severityLabel(crisis.severity)} CRISIS</p>
        <h1 id="crisis-title">{crisis.title}</h1>
      </div>
    </header>

    <div class="severity">
      <div class="sev-track" aria-hidden="true">
        <span class="sev-fill" style="width: {Math.min(100, crisis.severity * 10)}%"></span>
      </div>
      <span class="sev-num">SEV {crisis.severity}/10</span>
      {#if crisis.quartersActive > 0}
        <span class="qtrs">{crisis.quartersActive} qtr{crisis.quartersActive === 1 ? '' : 's'} active</span>
      {/if}
    </div>

    <button
      type="button"
      id="crisis-body"
      class="body"
      onclick={skipTypewriter}
      title={revealedChars < (crisis.description?.length ?? 0) ? 'Click to reveal full text' : ''}
    >
      <p>{visibleText}<span class="caret" class:done={revealedChars >= (crisis.description?.length ?? 0)}>▌</span></p>
    </button>

    {#if buttonsReady}
      <div class="actions" aria-label="Choose response">
        <button
          class="response measured"
          onclick={() => respond('measured')}
          disabled={submitting}
          title="Measured response — balanced risk/reward"
        >
          <span class="r-label">Measured</span>
          <span class="r-desc">Balanced risk and reward.</span>
        </button>
        <button
          class="response aggressive"
          onclick={() => respond('aggressive')}
          disabled={submitting}
          title="Aggressive response — bigger downside, faster resolution"
        >
          <span class="r-label">Aggressive</span>
          <span class="r-desc">Decisive action. Higher cost, faster resolution.</span>
        </button>
        <button
          class="response gamble"
          onclick={() => respond('gamble')}
          disabled={submitting}
          title="Gamble — long shot for the biggest win or biggest loss"
        >
          <span class="r-label">Gamble</span>
          <span class="r-desc">Long shot. Could resolve cleanly or compound.</span>
        </button>
      </div>
    {:else}
      <p class="prompt" aria-live="polite">Read carefully&hellip;</p>
    {/if}
  </div>
{/if}

<style>
  .vignette {
    position: fixed;
    inset: 0;
    background:
      radial-gradient(circle at center, transparent 0%, rgba(0,0,0,0.8) 100%);
    z-index: 900;
    pointer-events: none;
    animation: crisisFlash 600ms ease-out, fadeIn 200ms var(--ease);
  }

  .crisis-modal {
    position: fixed;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: min(40rem, 92vw);
    background: var(--bg-card);
    border: 1px solid var(--accent-danger);
    border-radius: var(--card-radius);
    padding: 2rem 2.5rem;
    z-index: 920;
    color: var(--fg-primary);
    font-family: var(--font-body);
    animation: fadeIn 240ms var(--ease);
    box-shadow: 0 0 60px rgba(0, 0, 0, 0.6);
  }
  .crisis-modal.glow {
    box-shadow:
      0 0 60px rgba(0, 0, 0, 0.6),
      0 0 30px rgba(196, 137, 43, 0.35);
  }
  .crisis-modal.pulse {
    animation:
      fadeIn 240ms var(--ease),
      crisisPulse 1.4s ease-in-out infinite alternate;
    box-shadow:
      0 0 60px rgba(0, 0, 0, 0.6),
      0 0 36px rgba(168, 69, 58, 0.55);
  }
  .crisis-modal.shake {
    animation:
      fadeIn 240ms var(--ease),
      crisisPulse 1.4s ease-in-out infinite alternate,
      shake 0.45s ease-out 0.25s 1;
  }
  @keyframes crisisPulse {
    from { border-color: var(--accent-danger); }
    to   { border-color: rgba(168, 69, 58, 0.4); }
  }

  .crisis-head {
    display: flex;
    align-items: center;
    gap: 1rem;
    margin-bottom: 1rem;
  }
  .icon {
    font-size: 2.4rem;
    color: var(--accent-danger);
    line-height: 1;
  }
  .head-text { display: flex; flex-direction: column; gap: 0.25rem; }
  .kind {
    margin: 0;
    font-family: var(--font-chrome);
    color: var(--accent-danger);
    font-size: 0.7rem;
    letter-spacing: 0.4em;
  }
  .crisis-modal h1 {
    margin: 0;
    font-family: var(--font-display);
    font-size: 1.7rem;
    color: var(--fg-primary);
    letter-spacing: 0.02em;
    line-height: 1.15;
  }

  .severity {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    margin-bottom: 1.25rem;
  }
  .sev-track {
    flex: 1;
    height: 6px;
    background: var(--bg-base);
    border-radius: 2px;
    overflow: hidden;
  }
  .sev-fill {
    display: block;
    height: 100%;
    background: linear-gradient(to right, var(--accent-warning), var(--accent-danger));
  }
  .sev-num {
    font-family: var(--font-data);
    font-size: var(--fs-xs);
    color: var(--accent-danger);
    letter-spacing: 0.15em;
  }
  .qtrs {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.1em;
  }

  .body {
    display: block;
    background: transparent;
    border: none;
    padding: 0;
    margin: 0 0 1.5rem 0;
    color: inherit;
    text-align: left;
    cursor: pointer;
    width: 100%;
    font-family: inherit;
  }
  .body p {
    margin: 0;
    font-size: 1.05rem;
    line-height: 1.55;
    color: var(--fg-primary);
  }
  .caret {
    color: var(--accent-danger);
    margin-left: 0.15rem;
    animation: pulse 0.7s ease-in-out infinite;
  }
  .caret.done { display: none; }

  .prompt {
    text-align: center;
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.2em;
    margin: 0;
  }

  .actions {
    display: grid;
    grid-template-columns: 1fr;
    gap: 0.55rem;
    animation: fadeIn 320ms var(--ease);
  }
  @media (min-width: 30rem) {
    .actions { grid-template-columns: repeat(3, 1fr); }
  }
  .response {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 0.25rem;
    padding: 0.85rem 1rem;
    background: var(--bg-elevated);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    cursor: pointer;
    text-align: left;
    font-family: var(--font-body);
    transition: border-color var(--t-fast) var(--ease), background var(--t-fast) var(--ease);
  }
  .response:hover:not(:disabled) {
    background: rgba(196, 163, 90, 0.06);
  }
  .response.measured:hover:not(:disabled)   { border-color: var(--accent-primary); }
  .response.aggressive:hover:not(:disabled) { border-color: var(--accent-warning); }
  .response.gamble:hover:not(:disabled)     { border-color: var(--accent-danger); }
  .r-label {
    font-family: var(--font-chrome);
    font-size: 0.75rem;
    letter-spacing: 0.25em;
    color: var(--accent-primary);
    text-transform: uppercase;
  }
  .response.aggressive .r-label { color: var(--accent-warning); }
  .response.gamble .r-label     { color: var(--accent-danger); }
  .r-desc {
    font-size: var(--fs-sm);
    color: var(--fg-secondary);
  }
</style>
