<script lang="ts">
  /*
   * Hiring desk — the opening move. As a bank owner you hire bankers; they staff
   * your divisions and (in their archetype voice) bring you deals. Candidate
   * cards show archetype, headline stats, traits, and salary. Hiring spends a
   * signing bonus and drops them into your first division.
   */

  import { sim } from '../stores/simulation.svelte';
  import { toasts } from '../stores/toasts.svelte';
  import { api, ApiError } from '../api/rest';
  import { telemetry } from '../telemetry';
  import { dwell } from '../actions/dwell';
  import { fmtMoney } from '../util/money';
  import type { HiringCandidate } from '../types/server';

  let submitting = $state(false);
  let candidates = $derived(sim.hiringPool ?? []);

  // P1: a candidate "detail open" is the first time the player engages a card
  // (hover/focus). Log it once per candidate per mount; subsequent hovers just
  // feed the throttled hover() stream.
  let detailOpened = new Set<string>();
  function engageCandidate(c: HiringCandidate) {
    if (!detailOpened.has(c.id)) {
      detailOpened.add(c.id);
      telemetry.log('hire_detail_open', { id: c.id, archetype: c.archetype });
    }
    telemetry.hover(`candidate:${c.id}`, { id: c.id, archetype: c.archetype });
  }

  // 0.9: annualSalary from the engine is now an HONEST annual wage — display it
  // verbatim. The old SALARY_DISPLAY_SCALE=1000 hack (papering over an engine
  // ÷10,000 fudge) is gone; engine value === displayed value.
  const fmt = (v: number): string => fmtMoney(v);

  // Show the three strongest stats so the card reads at a glance.
  function topStats(c: HiringCandidate): Array<[string, number]> {
    const s = c.stats ?? {};
    return Object.entries(s)
      .filter(([, v]) => typeof v === 'number')
      .sort((a, b) => (b[1] as number) - (a[1] as number))
      .slice(0, 3) as Array<[string, number]>;
  }

  async function hire(c: HiringCandidate) {
    if (submitting || !sim.gameId) return;
    const div = sim.bank?.divisions?.[0];
    if (!div) { toasts.warn('No division to staff yet.'); return; }
    submitting = true;
    telemetry.log('hire_click', { id: c.id, archetype: c.archetype, salary: c.annualSalary });
    try {
      await api.hire(sim.gameId, c.id, div.id);
      const view = await api.getState(sim.gameId);
      sim.applyPlayerView(view);
      toasts.success(`Hired ${c.name} — ${c.archetype}`);
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      toasts.error(`Hire failed: ${msg}`);
    } finally {
      submitting = false;
    }
  }
</script>

{#if candidates.length}
  <aside class="hiring" aria-label="Hiring desk">
    <header class="hire-head">
      <span class="hire-title">Hiring Desk</span>
      <span class="hire-sub">{candidates.length} available</span>
    </header>
    <ul class="cand-list">
      {#each candidates as c (c.id)}
        <li
          class="cand"
          use:dwell={{ key: `candidate:${c.id}`, data: { archetype: c.archetype } }}
          onmouseenter={() => engageCandidate(c)}
          onfocusin={() => engageCandidate(c)}
        >
          <div class="cand-top">
            <span class="cand-name">{c.name}</span>
            <span class="cand-arch">{c.archetype}</span>
          </div>
          <div class="cand-stats">
            {#each topStats(c) as [k, v]}
              <span class="stat"><b>{k.slice(0, 3).toUpperCase()}</b> {v}</span>
            {/each}
          </div>
          {#if c.traits?.length}
            <div class="traits">
              {#each c.traits.slice(0, 2) as t}
                <span class="trait" class:bad={!t.positive} title={t.description}>{t.name}</span>
              {/each}
            </div>
          {/if}
          <div class="cand-foot">
            <span class="salary num">{fmt(c.annualSalary)}/yr</span>
            <button class="hire-btn" disabled={submitting} onclick={() => hire(c)}>Hire</button>
          </div>
        </li>
      {/each}
    </ul>
  </aside>
{/if}

<style>
  .hiring { background: var(--bg-card); border: 1px solid var(--border); border-radius: var(--card-radius);
    padding: 0.85rem 0.95rem 0.95rem; display: flex; flex-direction: column; gap: 0.7rem; }
  .hire-head { display: flex; align-items: baseline; justify-content: space-between;
    border-bottom: 1px solid var(--border-soft); padding-bottom: 0.45rem; }
  .hire-title { font-family: var(--font-display); color: var(--accent-primary); font-size: 1.05rem; }
  .hire-sub { font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.16em; color: var(--fg-secondary); text-transform: uppercase; }
  .cand-list { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: 0.5rem; }
  .cand { background: var(--bg-elevated); border: 1px solid var(--border-soft); border-radius: 2px; padding: 0.5rem 0.6rem; }
  .cand-top { display: flex; align-items: baseline; justify-content: space-between; gap: 0.5rem; }
  .cand-name { font-weight: 600; color: var(--fg-primary); font-size: 0.88rem; }
  .cand-arch { font-family: var(--font-chrome); font-size: 0.58rem; letter-spacing: 0.12em; text-transform: uppercase; color: var(--accent-secondary); }
  .cand-stats { display: flex; gap: 0.6rem; margin-top: 0.3rem; font-family: var(--font-data); font-size: 0.72rem; color: var(--fg-secondary); }
  .cand-stats b { color: var(--fg-muted); font-weight: 400; }
  .traits { display: flex; flex-wrap: wrap; gap: 0.3rem; margin-top: 0.35rem; }
  .trait { font-size: 0.6rem; letter-spacing: 0.04em; padding: 0.06rem 0.3rem; border-radius: 2px;
    border: 1px solid var(--accent-success); color: var(--accent-success); }
  .trait.bad { border-color: var(--accent-danger); color: var(--accent-danger); }
  .cand-foot { display: flex; align-items: center; justify-content: space-between; margin-top: 0.45rem; }
  .salary { font-family: var(--font-data); font-size: 0.74rem; color: var(--fg-data); }
  .hire-btn { background: transparent; color: var(--accent-primary); border: 1px solid var(--accent-primary);
    font-family: var(--font-chrome); font-size: 0.66rem; letter-spacing: 0.1em; padding: 0.3rem 0.8rem; border-radius: 2px; }
  .hire-btn:hover:not(:disabled) { background: rgba(135, 106, 44, 0.1); }
</style>
