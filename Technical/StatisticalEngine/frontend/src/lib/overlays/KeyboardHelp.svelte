<script lang="ts">
  /*
   * Keyboard shortcuts cheat sheet. Toggled by '?' — handled in SimControls'
   * keydown listener. Esc dismisses.
   */

  import { ui } from '../stores/ui.svelte';

  function onKey(e: KeyboardEvent) {
    if (e.key === 'Escape' && ui.keyboardHelpOpen) {
      ui.keyboardHelpOpen = false;
    }
  }

  $effect(() => {
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });

  const SHORTCUTS = [
    { keys: ['Space'],          label: 'Play / Pause simulation' },
    { keys: ['1', '2', '3', '4'], label: 'Set speed (1x / 2x / 4x / 8x)' },
    { keys: ['D'],              label: 'Toggle decision drawer (late eras)' },
    { keys: ['↑', '↓'],         label: 'Navigate decision options' },
    { keys: ['←', '→'],         label: 'Cycle pending decisions (drawer)' },
    { keys: ['Enter'],          label: 'Submit selected decision option' },
    { keys: ['?'],              label: 'Toggle this help' },
    { keys: ['Esc'],            label: 'Close overlay' },
  ] as const;
</script>

{#if ui.keyboardHelpOpen}
  <div
    class="back"
    role="button"
    tabindex="0"
    aria-label="Close help"
    onclick={() => ui.keyboardHelpOpen = false}
    onkeydown={(e) => { if (e.key === 'Enter' || e.key === ' ') ui.keyboardHelpOpen = false; }}
  ></div>
  <div class="panel" role="dialog" aria-labelledby="kb-title">
    <header class="head">
      <h2 id="kb-title">Keyboard</h2>
      <button class="x" aria-label="Close" onclick={() => ui.keyboardHelpOpen = false}>&times;</button>
    </header>
    <table class="shortcuts">
      <tbody>
        {#each SHORTCUTS as s}
          <tr>
            <td class="keys">
              {#each s.keys as k, i (k)}
                {#if i > 0}<span class="sep">/</span>{/if}
                <kbd>{k}</kbd>
              {/each}
            </td>
            <td class="label">{s.label}</td>
          </tr>
        {/each}
      </tbody>
    </table>
    <p class="foot">Press <kbd>?</kbd> any time to toggle this panel.</p>
  </div>
{/if}

<style>
  .back {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.6);
    z-index: 870;
    border: none;
    padding: 0;
  }
  .panel {
    position: fixed;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: min(24rem, 92vw);
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    z-index: 880;
    padding: 1.5rem 1.75rem;
    animation: fadeIn 200ms var(--ease);
  }
  .head { display: flex; align-items: center; margin-bottom: 1rem; }
  .head h2 {
    margin: 0;
    flex: 1;
    font-family: var(--font-display);
    color: var(--accent-primary);
    font-size: 1.2rem;
    letter-spacing: 0.04em;
  }
  .x {
    background: transparent;
    border: none;
    color: var(--fg-secondary);
    font-size: 1.4rem;
    cursor: pointer;
  }
  .x:hover { color: var(--fg-primary); }

  .shortcuts {
    width: 100%;
    border-collapse: collapse;
    margin-bottom: 1rem;
  }
  .shortcuts td {
    padding: 0.35rem 0.4rem;
    border-bottom: 1px solid var(--border-soft);
  }
  .shortcuts tr:last-child td { border-bottom: none; }
  .keys {
    width: 8rem;
    white-space: nowrap;
  }
  .label {
    font-size: var(--fs-sm);
    color: var(--fg-primary);
  }
  kbd {
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    background: var(--bg-elevated);
    border: 1px solid var(--border);
    border-bottom-width: 2px;
    border-radius: 3px;
    padding: 0.12rem 0.4rem;
    color: var(--accent-primary);
    margin: 0 0.1rem;
  }
  .sep {
    color: var(--fg-muted);
    margin: 0 0.05rem;
  }
  .foot {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.1em;
    margin: 0;
    text-align: center;
  }
</style>
