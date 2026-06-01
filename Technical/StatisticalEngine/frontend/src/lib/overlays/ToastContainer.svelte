<script lang="ts">
  import { toasts } from '../stores/toasts.svelte';
</script>

<div class="toast-stack" role="status" aria-live="polite">
  {#each toasts.items as toast (toast.id)}
    <div class="toast toast-{toast.kind}">
      <span class="toast-msg">{toast.message}</span>
      <button class="toast-x" aria-label="Dismiss" onclick={() => toasts.dismiss(toast.id)}>×</button>
    </div>
  {/each}
</div>

<style>
  .toast-stack {
    position: fixed;
    bottom: 1.5rem;
    right: 1.5rem;
    display: flex;
    flex-direction: column;
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
