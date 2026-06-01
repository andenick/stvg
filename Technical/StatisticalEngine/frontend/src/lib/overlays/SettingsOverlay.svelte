<script lang="ts">
  /*
   * Advanced setup overlay — scenario, CEO, starting-position picker.
   * Hidden by default behind the Settings link on TitleScreen.
   * Resolves on Apply; cancel just closes.
   *
   * Endpoints (best-effort; gracefully degrades if 404):
   *   GET /api/scenarios
   *   GET /api/ceos
   *   GET /api/starting-positions
   */

  import { api } from '../api/rest';
  import { ui } from '../stores/ui.svelte';
  import { toasts } from '../stores/toasts.svelte';

  interface Props {
    seed?: number | null;
    ceoId?: string | null;
    startingPosition?: number | null;
    preset?: 'sandbox' | 'easy' | 'medium' | 'hard' | 'crisis' | null;
    onApply?: (opts: { seed?: number; ceoId?: string; startingPosition?: number; preset?: 'sandbox' | 'easy' | 'medium' | 'hard' | 'crisis' }) => void;
  }

  let {
    seed = $bindable(null),
    ceoId = $bindable(null),
    startingPosition = $bindable(null),
    preset = $bindable(null),
    onApply,
  }: Props = $props();

  let ceos = $state<Array<{ id: string; name: string; nickname?: string }>>([]);
  let positions = $state<Array<{ value: number; label: string; description?: string }>>([]);
  let scenarios = $state<Array<{ id: string; name: string; description?: string }>>([]);
  let loading = $state(true);
  let loadError = $state<string | null>(null);

  $effect(() => {
    if (ui.settingsOpen) load();
  });

  async function load() {
    loading = true;
    loadError = null;
    try {
      const [c, p, s] = await Promise.allSettled([
        api.listCeos(),
        api.listStartingPositions(),
        api.listScenarios(),
      ]);
      if (c.status === 'fulfilled') ceos = c.value as typeof ceos;
      if (p.status === 'fulfilled') positions = p.value as typeof positions;
      if (s.status === 'fulfilled') scenarios = s.value as typeof scenarios;
    } catch (e) {
      loadError = (e as Error).message;
    } finally {
      loading = false;
    }
  }

  function apply() {
    const opts: Parameters<NonNullable<Props['onApply']>>[0] = {};
    if (seed != null) opts.seed = seed;
    if (ceoId) opts.ceoId = ceoId;
    if (startingPosition != null) opts.startingPosition = startingPosition;
    if (preset) opts.preset = preset;
    onApply?.(opts);
    ui.settingsOpen = false;
    toasts.success('Settings applied');
  }

  function cancel() {
    ui.settingsOpen = false;
  }
</script>

{#if ui.settingsOpen}
  <div
    class="overlay-back"
    role="button"
    tabindex="0"
    aria-label="Close settings"
    onclick={cancel}
    onkeydown={(e) => { if (e.key === 'Escape') cancel(); }}
  ></div>
  <div class="overlay-panel" role="dialog" aria-modal="true" aria-labelledby="settings-title">
    <h2 id="settings-title" class="title">Advanced Setup</h2>

    {#if loading}
      <p class="muted">Loading…</p>
    {:else}
      {#if loadError}<p class="error">Could not load defaults: {loadError}</p>{/if}

      <label class="row">
        <span class="label">Preset</span>
        <select bind:value={preset}>
          <option value={null}>— Default —</option>
          <option value="sandbox">Sandbox</option>
          <option value="easy">Easy</option>
          <option value="medium">Medium</option>
          <option value="hard">Hard</option>
          <option value="crisis">Crisis</option>
        </select>
      </label>

      <label class="row">
        <span class="label">CEO</span>
        <select bind:value={ceoId}>
          <option value={null}>— No CEO —</option>
          {#each ceos as c (c.id)}
            <option value={c.id}>{c.name}{c.nickname ? ` "${c.nickname}"` : ''}</option>
          {/each}
        </select>
      </label>

      <label class="row">
        <span class="label">Starting position</span>
        <select bind:value={startingPosition}>
          <option value={null}>— Commercial Bank (default) —</option>
          {#each positions as p, i (p.value ?? i)}
            <option value={p.value ?? i}>{p.label ?? `Position ${i}`}</option>
          {/each}
        </select>
      </label>

      <label class="row">
        <span class="label">Seed</span>
        <input
          type="number"
          inputmode="numeric"
          placeholder="(random)"
          bind:value={seed}
        />
      </label>

      {#if scenarios.length}
        <p class="muted small">{scenarios.length} scenario(s) available; default = Full Game.</p>
      {/if}
    {/if}

    <div class="actions">
      <button class="btn-secondary" onclick={cancel}>Cancel</button>
      <button class="btn-primary" onclick={apply} disabled={loading}>Apply</button>
    </div>
  </div>
{/if}

<style>
  .overlay-back {
    position: fixed; inset: 0;
    background: rgba(0, 0, 0, 0.65);
    z-index: 900;
    border: none;
    padding: 0;
  }
  .overlay-panel {
    position: fixed;
    top: 50%; left: 50%;
    transform: translate(-50%, -50%);
    width: min(28rem, 92vw);
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 1.75rem 2rem;
    z-index: 950;
    display: flex;
    flex-direction: column;
    gap: 0.85rem;
    animation: fadeIn 200ms var(--ease);
  }
  .title {
    font-family: var(--font-display);
    color: var(--accent-primary);
    margin: 0 0 0.5rem 0;
    font-size: 1.4rem;
    letter-spacing: 0.05em;
  }
  .row {
    display: grid;
    grid-template-columns: 8rem 1fr;
    align-items: center;
    gap: 0.5rem;
  }
  .label {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    letter-spacing: 0.15em;
    color: var(--fg-secondary);
    text-transform: uppercase;
  }
  select, input {
    background: var(--bg-elevated);
    color: var(--fg-primary);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 0.4rem 0.55rem;
    font-family: var(--font-body);
    font-size: var(--fs-sm);
    width: 100%;
  }
  .muted { color: var(--fg-secondary); margin: 0; }
  .small { font-size: var(--fs-xs); }
  .error { color: var(--accent-danger); }
  .actions { display: flex; justify-content: flex-end; gap: 0.5rem; margin-top: 0.5rem; }
  .btn-primary {
    background: var(--accent-primary);
    color: var(--bg-base);
    border: 1px solid var(--accent-primary);
    font-weight: 600;
    letter-spacing: 0.1em;
  }
  .btn-primary:hover:not(:disabled) {
    background: var(--accent-secondary);
    border-color: var(--accent-secondary);
  }
  .btn-secondary { color: var(--fg-secondary); }
</style>
