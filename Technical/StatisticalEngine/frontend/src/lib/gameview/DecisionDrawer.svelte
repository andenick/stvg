<script lang="ts">
  /*
   * Decision UI. Two visual variants picked by UI phase:
   *
   *   phase 1 (1945-1959): MEMORANDUM — full-viewport, serif memo style.
   *     One decision at a time, big text, two option buttons below.
   *
   *   phase 2+ (1960-2040+): Side drawer — stacked cards, urgency = glow.
   *     Multiple decisions visible at once, denser layout.
   *
   * Both variants:
   *   - Show advisor recommendations as small pill icons
   *   - Keyboard: D toggles drawer (phase 2+), Enter confirms selected option,
   *     ArrowUp/Down navigates options, ArrowLeft/Right cycles decisions
   *   - After all decisions cleared -> Resume Simulation button
   *     (sends simulation_control: play via WebSocket)
   *
   * Fixes (top_20 #2): never clip headings at narrow viewports — drawer has
   * min-width and consistent left padding; memo variant uses centered max-width.
   */

  import { sim } from '../stores/simulation.svelte';
  import { ui } from '../stores/ui.svelte';
  import { api, ApiError } from '../api/rest';
  import { wsClient } from '../ws/websocket';
  import { toasts } from '../stores/toasts.svelte';
  import type { PendingDecision, DecisionOption } from '../types/server';

  // Currently focused decision index + option index (for keyboard nav)
  let selectedDecisionIdx = $state(0);
  let selectedOptionIdx = $state(0);
  let submitting = $state(false);

  // Whether the late-era drawer is showing (auto-open when decisions arrive)
  let drawerOpen = $state(true);

  // Clamp selection on decisions changing
  $effect(() => {
    if (sim.pendingDecisions.length === 0) {
      selectedDecisionIdx = 0;
      selectedOptionIdx = 0;
      return;
    }
    if (selectedDecisionIdx >= sim.pendingDecisions.length) {
      selectedDecisionIdx = sim.pendingDecisions.length - 1;
    }
    const dec = sim.pendingDecisions[selectedDecisionIdx];
    if (dec && selectedOptionIdx >= dec.options.length) {
      selectedOptionIdx = 0;
    }
  });

  // Open the drawer whenever new decisions arrive
  let lastCount = sim.pendingDecisions.length;
  $effect(() => {
    if (sim.pendingDecisions.length > lastCount) drawerOpen = true;
    lastCount = sim.pendingDecisions.length;
  });

  let currentDecision = $derived<PendingDecision | undefined>(
    sim.pendingDecisions[selectedDecisionIdx]
  );

  async function submit(decision: PendingDecision, option: DecisionOption) {
    if (submitting || !sim.gameId) return;
    submitting = true;
    try {
      const view = await api.submitDecision(sim.gameId, decision.id, option.id);
      sim.applyPlayerView(view);
      toasts.success(`"${option.title}" submitted`);
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      toasts.error(`Decision failed: ${msg}`);
    } finally {
      submitting = false;
    }
  }

  function resumeSim() {
    if (!sim.gameId) return;
    wsClient.play();
    sim.paused = false;
    toasts.info('Simulation resumed');
  }

  function onKey(e: KeyboardEvent) {
    const t = e.target as HTMLElement;
    if (t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.isContentEditable)) return;

    if (e.key.toLowerCase() === 'd' && ui.phase >= 2) {
      drawerOpen = !drawerOpen;
      return;
    }

    if (!currentDecision) return;

    if (e.key === 'ArrowUp') {
      e.preventDefault();
      selectedOptionIdx = (selectedOptionIdx - 1 + currentDecision.options.length) % currentDecision.options.length;
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      selectedOptionIdx = (selectedOptionIdx + 1) % currentDecision.options.length;
    } else if (e.key === 'ArrowLeft' && ui.phase >= 2) {
      e.preventDefault();
      selectedDecisionIdx = (selectedDecisionIdx - 1 + sim.pendingDecisions.length) % sim.pendingDecisions.length;
    } else if (e.key === 'ArrowRight' && ui.phase >= 2) {
      e.preventDefault();
      selectedDecisionIdx = (selectedDecisionIdx + 1) % sim.pendingDecisions.length;
    } else if (e.key === 'Enter') {
      const opt = currentDecision.options[selectedOptionIdx];
      if (opt) submit(currentDecision, opt);
    }
  }

  $effect(() => {
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });

  function fmtImpact(v: number | undefined): string {
    if (v == null || v === 0) return '';
    const abs = Math.abs(v);
    const sign = v > 0 ? '+' : '-';
    if (abs >= 1e9) return `${sign}$${(abs / 1e9).toFixed(2)}B`;
    if (abs >= 1e6) return `${sign}$${(abs / 1e6).toFixed(0)}M`;
    if (abs >= 1e3) return `${sign}$${(abs / 1e3).toFixed(0)}K`;
    return `${sign}$${abs.toFixed(0)}`;
  }

  function recIcon(rec: 'support' | 'oppose' | 'neutral' | string): string {
    if (rec === 'support') return '▲';   // ▲
    if (rec === 'oppose')  return '▼';   // ▼
    return '●';                          // ●
  }
  function recClass(rec: string): string {
    if (rec === 'support') return 'rec-support';
    if (rec === 'oppose')  return 'rec-oppose';
    return 'rec-neutral';
  }
</script>

{#if sim.pendingDecisions.length === 0 && sim.paused && sim.gameId && sim.connected}
  <!-- Quarter boundary cleared: prompt to resume -->
  <div class="resume-bar">
    <p>All decisions resolved for {sim.year} Q{sim.quarter}.</p>
    <button class="resume-btn" onclick={resumeSim}>Resume Simulation</button>
  </div>
{/if}

{#if sim.pendingDecisions.length > 0}
  {#if ui.phase === 1}
    <!-- ── MEMORANDUM (phase 1) ─────────────────────────────────────── -->
    <div class="memo-overlay" role="dialog" aria-labelledby="memo-title">
      <article class="memo">
        <header class="memo-head">
          <p class="memo-stamp">MEMORANDUM TO THE BOARD</p>
          <p class="memo-meta">{sim.year} &middot; Q{sim.quarter}</p>
        </header>
        {#if currentDecision}
          <h1 id="memo-title" class="memo-title">{currentDecision.title}</h1>
          <p class="memo-body">{currentDecision.description}</p>

          {#if sim.pendingDecisions.length > 1}
            <p class="memo-counter">Item {selectedDecisionIdx + 1} of {sim.pendingDecisions.length}</p>
          {/if}

          <div class="memo-options">
            {#each currentDecision.options as opt, i (opt.id)}
              <button
                class="memo-opt"
                class:selected={i === selectedOptionIdx}
                disabled={submitting}
                onclick={() => { selectedOptionIdx = i; submit(currentDecision!, opt); }}
                onmouseenter={() => selectedOptionIdx = i}
              >
                <span class="opt-num">{i + 1}.</span>
                <span class="opt-body">
                  <span class="opt-title">{opt.title}</span>
                  {#if opt.description}<span class="opt-desc">{opt.description}</span>{/if}
                </span>
                <span class="opt-meta">
                  {#if opt.financialImpact}<span class="opt-impact">{fmtImpact(opt.financialImpact)}</span>{/if}
                  {#if opt.successProbability != null}<span class="opt-prob">{Math.round(opt.successProbability * 100)}%</span>{/if}
                </span>
              </button>
            {/each}
          </div>

          {#if currentDecision.options[selectedOptionIdx]?.recommendations?.length}
            <div class="memo-recs" aria-label="Advisor recommendations">
              {#each currentDecision.options[selectedOptionIdx].recommendations ?? [] as r}
                <span class="rec-pill {recClass(r.recommendation)}" title={r.reasoning ?? ''}>
                  <span class="rec-icon" aria-hidden="true">{recIcon(r.recommendation)}</span>
                  {r.characterName ?? r.characterId ?? 'Advisor'}
                </span>
              {/each}
            </div>
          {/if}
        {/if}
        <footer class="memo-foot">
          <p class="memo-cost">
            {currentDecision?.actionPointCost ?? 1} action point{(currentDecision?.actionPointCost ?? 1) === 1 ? '' : 's'} ·
            Use number keys or arrows to select · Enter to submit
          </p>
        </footer>
      </article>
    </div>
  {:else}
    <!-- ── DRAWER (phase 2+) ─────────────────────────────────────────── -->
    {#if drawerOpen}
      <aside class="drawer" aria-label="Pending decisions">
        <header class="drawer-head">
          <h2>Decisions</h2>
          <span class="counter">{sim.pendingDecisions.length} pending</span>
          <button class="drawer-x" aria-label="Hide drawer" onclick={() => drawerOpen = false}>&times;</button>
        </header>
        <div class="drawer-list">
          {#each sim.pendingDecisions as dec, i (dec.id)}
            <div
              class="dec-card urgency-{String(dec.urgency ?? 'standard').toLowerCase()}"
              class:active={i === selectedDecisionIdx}
              onclick={() => { selectedDecisionIdx = i; selectedOptionIdx = 0; }}
              onkeydown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); selectedDecisionIdx = i; selectedOptionIdx = 0; } }}
              role="button"
              tabindex="0"
            >
              <header class="dec-head">
                <span class="dec-title">{dec.title}</span>
                <span class="dec-ap">{dec.actionPointCost} AP</span>
              </header>
              <p class="dec-desc">{dec.description}</p>
              <div class="dec-opts">
                {#each dec.options as opt, oi (opt.id)}
                  <button
                    class="dec-opt"
                    class:selected={i === selectedDecisionIdx && oi === selectedOptionIdx}
                    disabled={submitting}
                    onclick={(e) => { e.stopPropagation(); selectedDecisionIdx = i; selectedOptionIdx = oi; submit(dec, opt); }}
                  >
                    <span class="dec-opt-title">{opt.title}</span>
                    <span class="dec-opt-meta">
                      {#if opt.financialImpact}<span class="opt-impact">{fmtImpact(opt.financialImpact)}</span>{/if}
                      {#if opt.successProbability != null}<span class="opt-prob">{Math.round(opt.successProbability * 100)}%</span>{/if}
                    </span>
                  </button>
                {/each}
              </div>
              {#if dec.options[selectedOptionIdx]?.recommendations?.length && i === selectedDecisionIdx}
                <div class="dec-recs">
                  {#each dec.options[selectedOptionIdx].recommendations ?? [] as r}
                    <span class="rec-pill {recClass(r.recommendation)}" title={r.reasoning ?? ''}>
                      <span class="rec-icon" aria-hidden="true">{recIcon(r.recommendation)}</span>
                      {r.characterName ?? r.characterId ?? 'Advisor'}
                    </span>
                  {/each}
                </div>
              {/if}
            </div>
          {/each}
        </div>
        <footer class="drawer-foot">
          <p class="hint">D toggles drawer · &larr;/&rarr; decisions · &uarr;/&darr; options · Enter submits</p>
        </footer>
      </aside>
    {:else}
      <button class="drawer-tab" onclick={() => drawerOpen = true} title="Show decisions (D)">
        <span class="tab-count">{sim.pendingDecisions.length}</span>
        <span class="tab-label">DECISIONS</span>
      </button>
    {/if}
  {/if}
{/if}

<style>
  /* ── Resume bar (between quarters when no decisions outstanding) ───── */
  .resume-bar {
    position: fixed;
    bottom: 1.5rem;
    left: 50%;
    transform: translateX(-50%);
    background: var(--bg-elevated);
    border: 1px solid var(--accent-primary);
    border-radius: var(--card-radius);
    padding: 0.75rem 1.25rem;
    display: flex;
    align-items: center;
    gap: 1rem;
    font-family: var(--font-chrome);
    z-index: 800;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
    animation: fadeIn 200ms var(--ease);
  }
  .resume-bar p { margin: 0; color: var(--fg-secondary); font-size: var(--fs-sm); }
  .resume-btn {
    background: var(--accent-primary);
    color: var(--bg-base);
    border: 1px solid var(--accent-primary);
    font-weight: 600;
    letter-spacing: 0.15em;
    padding: 0.5rem 1rem;
    font-size: var(--fs-xs);
    text-transform: uppercase;
  }
  .resume-btn:hover { background: var(--accent-secondary); border-color: var(--accent-secondary); }

  /* ── MEMORANDUM (phase 1) ──────────────────────────────────────────── */
  .memo-overlay {
    position: fixed;
    inset: 0;
    background: rgba(13, 11, 8, 0.93);
    display: grid;
    place-items: center;
    padding: 2rem;
    z-index: 850;
    animation: fadeIn 280ms var(--ease);
    overflow-y: auto;
  }
  .memo {
    /* top_20 #2: never clip — generous max-width + consistent padding */
    max-width: 44rem;
    width: 100%;
    min-width: 18rem;
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 2.5rem 3rem;
    box-shadow: 0 16px 60px rgba(0, 0, 0, 0.5);
  }
  .memo-head {
    display: flex;
    justify-content: space-between;
    border-bottom: 1px solid var(--border-soft);
    padding-bottom: 1rem;
    margin-bottom: 1.5rem;
  }
  .memo-stamp {
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    letter-spacing: 0.4em;
    color: var(--accent-primary);
    margin: 0;
  }
  .memo-meta {
    font-family: var(--font-data);
    font-size: 0.75rem;
    color: var(--fg-secondary);
    margin: 0;
  }
  .memo-title {
    font-family: var(--font-display);
    font-size: 1.9rem;
    color: var(--fg-primary);
    margin: 0 0 1rem 0;
    letter-spacing: 0.02em;
    line-height: 1.15;
  }
  .memo-body {
    font-family: var(--font-body);
    font-size: 1.05rem;
    color: var(--fg-primary);
    line-height: 1.55;
    margin: 0 0 1.5rem 0;
  }
  .memo-counter {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.2em;
    margin: 0 0 1rem 0;
  }
  .memo-options {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
    margin-bottom: 1.25rem;
  }
  .memo-opt {
    display: grid;
    grid-template-columns: 2rem 1fr auto;
    align-items: baseline;
    gap: 0.75rem;
    padding: 1rem 1.25rem;
    background: var(--bg-elevated);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    text-align: left;
    transition: border-color var(--t-fast) var(--ease), background var(--t-fast) var(--ease);
    font-family: var(--font-body);
    cursor: pointer;
  }
  .memo-opt:hover:not(:disabled),
  .memo-opt.selected {
    border-color: var(--accent-primary);
    background: rgba(196, 163, 90, 0.06);
  }
  .opt-num {
    font-family: var(--font-display);
    font-size: 1.3rem;
    color: var(--accent-primary);
  }
  .opt-body { display: flex; flex-direction: column; gap: 0.2rem; min-width: 0; }
  .opt-title {
    color: var(--fg-primary);
    font-size: 1rem;
    font-weight: 500;
  }
  .opt-desc {
    color: var(--fg-secondary);
    font-size: var(--fs-sm);
  }
  .opt-meta {
    display: flex;
    flex-direction: column;
    gap: 0.2rem;
    align-items: flex-end;
    font-family: var(--font-data);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
  }
  .opt-impact { color: var(--fg-primary); }
  .opt-prob   { color: var(--accent-success); }

  .memo-recs {
    display: flex;
    flex-wrap: wrap;
    gap: 0.4rem;
    padding-top: 0.75rem;
    border-top: 1px solid var(--border-soft);
  }
  .memo-foot {
    margin-top: 1rem;
    padding-top: 0.75rem;
    border-top: 1px solid var(--border-soft);
  }
  .memo-cost {
    margin: 0;
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    letter-spacing: 0.1em;
    color: var(--fg-secondary);
    text-align: center;
  }

  /* ── DRAWER (phase 2+) ─────────────────────────────────────────────── */
  .drawer {
    position: fixed;
    top: 0;
    right: 0;
    bottom: 0;
    width: min(28rem, 92vw);
    /* top_20 #2: explicit min-width + left padding via .dec-card */
    min-width: 18rem;
    background: var(--bg-card);
    border-left: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    z-index: 700;
    animation: slideInRight 260ms var(--ease);
    box-shadow: -8px 0 24px rgba(0, 0, 0, 0.35);
  }
  .drawer-head {
    display: flex;
    align-items: baseline;
    gap: 0.75rem;
    padding: 1rem 1.25rem;
    border-bottom: 1px solid var(--border);
  }
  .drawer-head h2 {
    font-family: var(--font-display);
    color: var(--accent-primary);
    margin: 0;
    font-size: 1.15rem;
  }
  .counter {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.15em;
  }
  .drawer-x {
    margin-left: auto;
    background: transparent;
    border: none;
    color: var(--fg-secondary);
    font-size: 1.25rem;
    cursor: pointer;
    padding: 0 0.25rem;
  }
  .drawer-x:hover { color: var(--fg-primary); }

  .drawer-list {
    flex: 1;
    overflow-y: auto;
    padding: 0.75rem;
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .dec-card {
    background: var(--bg-elevated);
    border: 1px solid var(--border);
    border-left-width: 3px;
    border-radius: var(--card-radius);
    padding: 0.85rem 1rem 0.85rem 1rem;
    /* top_20 #2: ensure left padding never gets eaten */
    text-align: left;
    cursor: pointer;
    transition: border-color var(--t-fast) var(--ease);
    color: inherit;
  }
  .dec-card.active {
    border-color: var(--accent-primary);
  }
  .dec-card.urgency-high    { border-left-color: var(--accent-warning); }
  .dec-card.urgency-crisis  { border-left-color: var(--accent-danger);
                              box-shadow: inset 0 0 0 1px rgba(168, 69, 58, 0.25); }
  .dec-card.urgency-low     { border-left-color: var(--fg-secondary); }
  .dec-card.urgency-standard{ border-left-color: var(--accent-primary); }

  .dec-head {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 0.35rem;
  }
  .dec-title {
    font-family: var(--font-body);
    font-size: 0.95rem;
    color: var(--fg-primary);
    font-weight: 500;
  }
  .dec-ap {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--accent-primary);
    letter-spacing: 0.15em;
  }
  .dec-desc {
    font-size: var(--fs-sm);
    color: var(--fg-secondary);
    margin: 0 0 0.6rem 0;
    line-height: 1.4;
  }
  .dec-opts {
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
  }
  .dec-opt {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    gap: 0.5rem;
    padding: 0.45rem 0.6rem;
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 2px;
    font-family: var(--font-body);
    text-align: left;
    cursor: pointer;
  }
  .dec-opt:hover:not(:disabled),
  .dec-opt.selected {
    border-color: var(--accent-primary);
    background: rgba(196, 163, 90, 0.06);
  }
  .dec-opt-title {
    color: var(--fg-primary);
    font-size: 0.85rem;
  }
  .dec-opt-meta {
    display: inline-flex;
    gap: 0.6rem;
    font-family: var(--font-data);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
  }

  .dec-recs {
    margin-top: 0.5rem;
    display: flex;
    flex-wrap: wrap;
    gap: 0.35rem;
  }

  .drawer-foot {
    border-top: 1px solid var(--border);
    padding: 0.5rem 1rem;
  }
  .hint {
    margin: 0;
    font-family: var(--font-chrome);
    font-size: 0.65rem;
    letter-spacing: 0.1em;
    color: var(--fg-muted);
    text-align: center;
  }

  .drawer-tab {
    position: fixed;
    top: 5rem;
    right: 0;
    background: var(--bg-elevated);
    border: 1px solid var(--accent-primary);
    border-right: none;
    border-radius: 4px 0 0 4px;
    padding: 0.6rem 0.75rem;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 0.25rem;
    z-index: 700;
    font-family: var(--font-chrome);
    cursor: pointer;
  }
  .tab-count {
    font-family: var(--font-data);
    font-size: 1.05rem;
    color: var(--accent-primary);
  }
  .tab-label {
    font-size: 0.55rem;
    letter-spacing: 0.2em;
    color: var(--fg-secondary);
  }

  /* ── Shared rec pills ──────────────────────────────────────────────── */
  .rec-pill {
    display: inline-flex;
    align-items: center;
    gap: 0.3rem;
    padding: 0.2rem 0.55rem;
    border-radius: 999px;
    background: var(--bg-card);
    border: 1px solid var(--border);
    font-family: var(--font-chrome);
    font-size: 0.65rem;
    letter-spacing: 0.1em;
    color: var(--fg-secondary);
    cursor: help;
  }
  .rec-icon { font-size: 0.7rem; }
  .rec-support { border-color: var(--accent-success); color: var(--accent-success); }
  .rec-oppose  { border-color: var(--accent-danger);  color: var(--accent-danger); }
  .rec-neutral { border-color: var(--fg-secondary);   color: var(--fg-secondary); }
</style>
