<script lang="ts">
  import { toasts } from '../stores/toasts.svelte';

  // Cap the visible stack at 3 (0.5). The newest toasts are kept visible; the
  // rest collapse into a "+N more" row. Dismissing a visible toast promotes
  // the next one. Showing the newest is right because TTL expiry drains the
  // tail first anyway.
  const MAX_VISIBLE = 3;
  let visible = $derived(toasts.items.slice(-MAX_VISIBLE));
  let hiddenCount = $derived(Math.max(0, toasts.items.length - MAX_VISIBLE));
</script>

<div class="toast-stack" role="status" aria-live="polite">
  {#if hiddenCount > 0}
    <button class="toast-more" onclick={() => toasts.clear()} title="Dismiss all notifications">
      +{hiddenCount} more — clear all
    </button>
  {/if}
  {#each visible as toast (toast.id)}
    <div class="toast toast-{toast.kind}">
      <span class="toast-msg">{toast.message}</span>
      <button class="toast-x" aria-label="Dismiss" onclick={() => toasts.dismiss(toast.id)}>×</button>
    </div>
  {/each}
</div>

<style>
  /*
   * Toasts sit ABOVE the character portrait card (P4): the bottom-right corner is
   * now owned by CharacterCard.svelte (a ~9rem-tall bust at bottom:1.5rem). The
   * toast stack is lifted to bottom:11rem so the two never overlap — toasts grow
   * upward from there and the bust keeps the corner. Right edge shared, but the
   * vertical bands are disjoint.
   */
  .toast-stack {
    position: fixed;
    bottom: 11rem;
    right: 1.5rem;
    display: flex;
    flex-direction: column-reverse;
    gap: 0.5rem;
    z-index: 1000;
    max-width: 22rem;
    pointer-events: none;
  }
  .toast {
    pointer-events: auto;
    background: var(--bg-elevated);
    border: 1px solid var(--border);
    border-left-width: 3px;
    border-radius: var(--card-radius);
    padding: 0.65rem 0.9rem;
    display: flex;
    align-items: center;
    gap: 0.5rem;
    font-size: var(--fs-sm);
    box-shadow: 0 6px 24px rgba(0, 0, 0, 0.4);
    animation: slideInRight 240ms var(--ease);
  }
  .toast-info    { border-left-color: var(--accent-primary); }
  .toast-success { border-left-color: var(--accent-success); }
  .toast-warning { border-left-color: var(--accent-warning); }
  .toast-error   { border-left-color: var(--accent-danger); }

  .toast-more {
    pointer-events: auto;
    align-self: flex-end;
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    color: var(--fg-secondary);
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    letter-spacing: 0.08em;
    padding: 0.3rem 0.6rem;
    cursor: pointer;
  }
  .toast-more:hover { color: var(--fg-primary); border-color: var(--accent-primary); }

  .toast-msg { flex: 1; color: var(--fg-primary); }

  .toast-x {
    background: transparent;
    border: none;
    color: var(--fg-secondary);
    font-size: 1.1rem;
    cursor: pointer;
    padding: 0 0.25rem;
  }
  .toast-x:hover { color: var(--fg-primary); }
</style>
