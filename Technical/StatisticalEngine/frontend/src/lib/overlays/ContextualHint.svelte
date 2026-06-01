<script lang="ts">
  /*
   * Contextual hint — first-time-only tooltip attached to a newly-appeared
   * UI element. Replaces the React TutorialOverlay pattern (which blocked
   * the screen). This one floats next to the element it explains, the
   * game stays playable, and it self-dismisses with a flag in localStorage.
   *
   * Usage:
   *   <ContextualHint id="hint:sim-controls" position="below">
   *     Press Space to play; 1–4 to set speed.
   *   </ContextualHint>
   *
   * The id is opaque to the component — anything stable works. Once
   * dismissed, it never re-renders (per ui.dismissedHints set + localStorage).
   *
   * `position` controls placement relative to the parent element the hint
   * lives inside. Default: below.
   */

  import { ui } from '../stores/ui.svelte';
  import type { Snippet } from 'svelte';

  interface Props {
    id: string;
    position?: 'above' | 'below' | 'left' | 'right';
    delayMs?: number;
    children?: Snippet;
  }

  let { id, position = 'below', delayMs = 600, children }: Props = $props();

  let visible = $state(false);
  let timer: ReturnType<typeof setTimeout> | undefined;

  $effect(() => {
    if (ui.isHintDismissed(id)) return;
    timer = setTimeout(() => { visible = true; }, delayMs);
    return () => clearTimeout(timer);
  });

  function dismiss() {
    visible = false;
    ui.dismissHint(id);
  }
</script>

{#if visible}
  <div class="hint hint-{position}" role="note">
    <div class="hint-arrow" aria-hidden="true"></div>
    <div class="hint-body">
      {#if children}{@render children()}{:else}<em>Tip.</em>{/if}
    </div>
    <button class="hint-x" aria-label="Dismiss tip" onclick={dismiss}>×</button>
  </div>
{/if}

<style>
  .hint {
    position: absolute;
    z-index: 600;
    background: var(--bg-elevated);
    border: 1px solid var(--accent-primary);
    border-radius: var(--card-radius);
    padding: 0.5rem 0.75rem 0.5rem 0.85rem;
    max-width: 18rem;
    display: flex;
    align-items: flex-start;
    gap: 0.5rem;
    box-shadow: 0 6px 20px rgba(0, 0, 0, 0.4);
    font-family: var(--font-body);
    animation: fadeIn 220ms var(--ease);
  }
  .hint-body {
    flex: 1;
    color: var(--fg-primary);
    font-size: var(--fs-sm);
    line-height: 1.4;
  }
  .hint-x {
    flex: 0 0 auto;
    background: transparent;
    border: none;
    color: var(--fg-secondary);
    cursor: pointer;
    font-size: 1.05rem;
    padding: 0 0.25rem;
  }
  .hint-x:hover { color: var(--fg-primary); }
  .hint-arrow {
    position: absolute;
    width: 8px;
    height: 8px;
    background: var(--bg-elevated);
    border-top: 1px solid var(--accent-primary);
    border-left: 1px solid var(--accent-primary);
    transform: rotate(45deg);
  }
  .hint-below { top: calc(100% + 6px); left: 0.5rem; }
  .hint-below .hint-arrow { top: -5px; left: 1rem; }
  .hint-above { bottom: calc(100% + 6px); left: 0.5rem; }
  .hint-above .hint-arrow { bottom: -5px; left: 1rem; transform: rotate(225deg); }
  .hint-right { top: 0; left: calc(100% + 6px); }
  .hint-right .hint-arrow { top: 0.75rem; left: -5px; transform: rotate(-45deg); }
  .hint-left  { top: 0; right: calc(100% + 6px); }
  .hint-left .hint-arrow  { top: 0.75rem; right: -5px; transform: rotate(135deg); }
</style>
