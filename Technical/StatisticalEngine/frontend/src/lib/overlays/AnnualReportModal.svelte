<script lang="ts">
  /*
   * Auto-shown at year-end (Q4) when sim.annualReport.year changes.
   * Tracks the last-shown year in localStorage so refresh or save/load
   * doesn't re-pop the same report.
   *
   * Pulls full server-generated content (visual_polish #7):
   *   - year headline + era name
   *   - revenue/costs/net-income table
   *   - capital growth percentage (prominent)
   *   - headlines bullets
   *   - narrative paragraph
   *   - key events list
   */

  import { sim } from '../stores/simulation.svelte';
  import { ui } from '../stores/ui.svelte';
  import { fmtMoney } from '../util/money';
  import { telemetry } from '../telemetry';
  import { dwell } from '../actions/dwell';

  let open = $state(false);
  let lastShownYear = $state<number | null>(loadLastShown());

  // 0.6: at 1x the full modal hijacks the screen, which is fine when you're
  // watching slowly; at 2x+ the report is on-screen most of the wall clock, so
  // render a compact corner card instead (click to expand to the full modal).
  // Mode is configurable (ui.annualReportMode): 'modal' | 'card' | 'auto'.
  // 'auto' (default) is speed-dependent. expanded=true forces the full modal
  // even in card mode (the user clicked to expand).
  let expanded = $state(false);
  let effectiveMode = $derived(
    ui.annualReportMode === 'modal' ? 'modal'
    : ui.annualReportMode === 'card' ? 'card'
    : ((sim.speed ?? 1) >= 2 ? 'card' : 'modal')   // 'auto'
  );
  // Show the full modal when mode resolves to modal, OR the user expanded a card.
  let showFull = $derived(open && (effectiveMode === 'modal' || expanded));
  let showCard = $derived(open && effectiveMode === 'card' && !expanded);

  function loadLastShown(): number | null {
    try {
      const raw = localStorage.getItem('stvg.annualReport.lastShownYear');
      return raw ? parseInt(raw, 10) : null;
    } catch { return null; }
  }
  function persistLastShown(y: number) {
    try { localStorage.setItem('stvg.annualReport.lastShownYear', String(y)); } catch { /* ignore */ }
  }

  let openedYear = $state<number | null>(null);
  $effect(() => {
    const r = sim.annualReport;
    if (!r || !r.year) return;
    if (lastShownYear === r.year) return;
    open = true;
    // Lifecycle: annual_report_open (P1) — fire ONCE per year, on the open
    // transition. The effect re-runs while the modal stays open (deps like
    // effectiveMode/speed change); openedYear guards against a re-log storm.
    if (openedYear !== r.year) {
      openedYear = r.year;
      telemetry.log('annual_report_open', { year: r.year, mode: effectiveMode });
    }
  });

  function dismiss() {
    if (open && sim.annualReport?.year) {
      telemetry.log('annual_report_close', { year: sim.annualReport.year, mode: effectiveMode });
    }
    open = false;
    expanded = false;
    if (sim.annualReport?.year) {
      lastShownYear = sim.annualReport.year;
      persistLastShown(sim.annualReport.year);
    }
  }

  function expand() { expanded = true; }

  function onKey(e: KeyboardEvent) {
    // Only steal Enter/Esc/Space when the full modal is up. In card mode the
    // sim keeps running and Space must still control play/pause.
    if (showFull && (e.key === 'Enter' || e.key === 'Escape' || e.key === ' ')) {
      e.preventDefault();
      dismiss();
    }
  }

  $effect(() => {
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });

  const fmt = (v: number | undefined): string => fmtMoney(v);
</script>

{#if showCard && sim.annualReport}
  {@const r = sim.annualReport}
  <!-- Compact corner card (0.6): non-blocking, sim keeps running. Click to
       expand to the full modal, or × to dismiss the year. -->
  <div class="card" role="dialog" aria-label="Annual report — {r.year}"
       use:dwell={{ key: `annual_report_card:${r.year}`, data: { year: r.year } }}>
    <button class="card-body" onclick={expand} title="Expand full annual report">
      <div class="card-head">
        <span class="card-badge">ANNUAL REPORT</span>
        <span class="card-year num">{r.year}</span>
      </div>
      <div class="card-growth" class:up={r.capitalGrowthPct > 0} class:down={r.capitalGrowthPct < 0}>
        <span class="num">{r.capitalGrowthPct > 0 ? '+' : ''}{r.capitalGrowthPct.toFixed(1)}%</span>
        <span class="card-ni num" class:up={r.netIncome > 0} class:down={r.netIncome < 0}>{fmt(r.netIncome)}</span>
      </div>
      <span class="card-hint">Click to expand</span>
    </button>
    <button class="card-x" aria-label="Dismiss annual report" onclick={dismiss}>×</button>
  </div>
{/if}

{#if showFull && sim.annualReport}
  {@const r = sim.annualReport}
  <div
    class="back"
    role="button"
    tabindex="0"
    aria-label="Close annual report"
    onclick={dismiss}
    onkeydown={(e) => { if (e.key === 'Enter' || e.key === ' ') dismiss(); }}
  ></div>
  <div class="modal" role="dialog" aria-labelledby="ann-title"
       use:dwell={{ key: `annual_report:${r.year}`, data: { year: r.year } }}>
    <header class="head">
      <p class="badge">ANNUAL REPORT</p>
      <h1 id="ann-title">Year of {r.year}</h1>
      <p class="era">{r.eraName}</p>
    </header>

    <table class="pl">
      <tbody>
        <tr><th>Total Revenue</th><td class="num">{fmt(r.totalRevenue)}</td></tr>
        <tr><th>Total Costs</th><td class="num">{fmt(r.totalCosts)}</td></tr>
        <tr class="ni">
          <th>Net Income</th>
          <td class="num" class:up={r.netIncome > 0} class:down={r.netIncome < 0}>{fmt(r.netIncome)}</td>
        </tr>
      </tbody>
    </table>

    <div class="growth-block" class:up={r.capitalGrowthPct > 0} class:down={r.capitalGrowthPct < 0}>
      <span class="growth-label">Capital Growth</span>
      <span class="num growth-pct">{r.capitalGrowthPct > 0 ? '+' : ''}{r.capitalGrowthPct.toFixed(1)}%</span>
      <span class="growth-sub num">{fmt(r.capitalStart)} &rarr; {fmt(r.capitalEnd)}</span>
    </div>

    <div class="ratios">
      <div class="ratio"><span class="r-label">Capital Ratio</span><span class="num r-val">{(r.capitalRatio * 100).toFixed(1)}%</span></div>
      <div class="ratio"><span class="r-label">Leverage</span><span class="num r-val">{r.leverageRatio.toFixed(1)}&times;</span></div>
      <div class="ratio"><span class="r-label">Avg Qtr Score</span><span class="num r-val">{r.avgQuarterScore.toFixed(1)}</span></div>
    </div>

    {#if r.headlines && r.headlines.length}
      <section class="headlines">
        <h2>Headlines</h2>
        <ul>{#each r.headlines as h}<li>{h}</li>{/each}</ul>
      </section>
    {/if}

    {#if r.keyEvents && r.keyEvents.length}
      <section class="events">
        <h2>Key Events</h2>
        <ul>{#each r.keyEvents as e}<li>{e}</li>{/each}</ul>
      </section>
    {/if}

    {#if r.narrative}
      <p class="narrative">{r.narrative}</p>
    {/if}

    <footer>
      <button class="continue" onclick={dismiss}>Continue</button>
    </footer>
  </div>
{/if}

<style>
  /* ── Compact corner card (0.6) ───────────────────────────────────────── */
  .card {
    position: fixed;
    bottom: 1.5rem;
    right: 1.5rem;
    z-index: 880;            /* below toasts (1000), above most panels */
    width: 14rem;
    background: var(--bg-card);
    border: 1px solid var(--accent-primary);
    border-radius: var(--card-radius);
    box-shadow: 0 10px 36px rgba(0, 0, 0, 0.45);
    display: flex;
    align-items: stretch;
    animation: slideInRight 240ms var(--ease);
  }
  .card-body {
    flex: 1;
    text-align: left;
    background: transparent;
    border: none;
    padding: 0.7rem 0.85rem;
    cursor: pointer;
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
    color: inherit;
  }
  .card-body:hover { background: rgba(135, 106, 44, 0.08); }
  .card-head { display: flex; align-items: baseline; justify-content: space-between; }
  .card-badge { font-family: var(--font-chrome); font-size: 0.55rem; letter-spacing: 0.28em; color: var(--accent-primary); }
  .card-year { font-family: var(--font-display); font-size: 1.1rem; color: var(--fg-primary); }
  .card-growth { display: flex; align-items: baseline; justify-content: space-between; font-family: var(--font-data); font-variant-numeric: tabular-nums; }
  .card-growth > .num { font-size: 1.35rem; color: var(--fg-primary); }
  .card-growth.up > .num { color: var(--accent-success); }
  .card-growth.down > .num { color: var(--accent-danger); }
  .card-ni { font-size: 0.8rem; color: var(--fg-secondary); }
  .card-ni.up { color: var(--accent-success); }
  .card-ni.down { color: var(--accent-danger); }
  .card-hint { font-family: var(--font-chrome); font-size: 0.55rem; letter-spacing: 0.12em; color: var(--fg-muted); text-transform: uppercase; }
  .card-x {
    background: transparent; border: none; border-left: 1px solid var(--border-soft);
    color: var(--fg-secondary); font-size: 1.1rem; cursor: pointer; padding: 0 0.55rem;
  }
  .card-x:hover { color: var(--fg-primary); }

  .back {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.7);
    z-index: 910;
    border: none;
    padding: 0;
  }
  .modal {
    position: fixed;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: min(38rem, 92vw);
    max-height: 92vh;
    overflow-y: auto;
    background: var(--bg-card);
    border: 1px solid var(--accent-primary);
    border-radius: var(--card-radius);
    z-index: 930;
    padding: 2rem 2.25rem;
    font-family: var(--font-body);
    animation: fadeIn 280ms var(--ease);
    box-shadow: 0 16px 60px rgba(0, 0, 0, 0.55);
  }
  .head {
    text-align: center;
    border-bottom: 1px solid var(--border-soft);
    padding-bottom: 1rem;
    margin-bottom: 1.25rem;
  }
  .badge {
    font-family: var(--font-chrome);
    color: var(--accent-primary);
    letter-spacing: 0.5em;
    font-size: 0.65rem;
    margin: 0 0 0.5rem 0;
  }
  .head h1 {
    font-family: var(--font-display);
    font-size: 2.2rem;
    color: var(--fg-primary);
    margin: 0;
    letter-spacing: 0.02em;
  }
  .era {
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    letter-spacing: 0.25em;
    color: var(--fg-secondary);
    margin: 0.4rem 0 0 0;
  }

  .pl {
    width: 100%;
    border-collapse: collapse;
    margin-bottom: 1rem;
  }
  .pl th {
    text-align: left;
    padding: 0.4rem 0;
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    letter-spacing: 0.18em;
    color: var(--fg-secondary);
    text-transform: uppercase;
  }
  .pl td {
    text-align: right;
    padding: 0.4rem 0;
    font-family: var(--font-data);
    font-variant-numeric: tabular-nums;
    color: var(--fg-primary);
  }
  .pl .ni th, .pl .ni td {
    border-top: 1px solid var(--border-soft);
    padding-top: 0.6rem;
    font-size: 1rem;
  }
  .pl td.up { color: var(--accent-success); }
  .pl td.down { color: var(--accent-danger); }

  .growth-block {
    display: grid;
    grid-template-columns: 1fr auto;
    grid-template-rows: auto auto;
    align-items: baseline;
    padding: 1rem 1.25rem;
    margin-bottom: 1rem;
    background: var(--bg-elevated);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    column-gap: 1rem;
  }
  .growth-block.up   { border-color: var(--accent-success); }
  .growth-block.down { border-color: var(--accent-danger); }
  .growth-label {
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    letter-spacing: 0.25em;
    color: var(--fg-secondary);
    text-transform: uppercase;
    grid-column: 1; grid-row: 1;
  }
  .growth-pct {
    font-family: var(--font-display);
    font-size: 2rem;
    grid-column: 2; grid-row: 1 / span 2;
    align-self: center;
  }
  .growth-block.up   .growth-pct { color: var(--accent-success); }
  .growth-block.down .growth-pct { color: var(--accent-danger); }
  .growth-sub {
    grid-column: 1; grid-row: 2;
    color: var(--fg-secondary);
    font-size: var(--fs-sm);
  }

  .ratios {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 0.5rem;
    margin-bottom: 1rem;
  }
  .ratio {
    display: flex;
    flex-direction: column;
    padding: 0.5rem 0.75rem;
    background: var(--bg-elevated);
    border: 1px solid var(--border-soft);
    border-radius: 2px;
  }
  .r-label {
    font-family: var(--font-chrome);
    font-size: 0.6rem;
    letter-spacing: 0.18em;
    color: var(--fg-secondary);
    text-transform: uppercase;
  }
  .r-val {
    font-family: var(--font-data);
    font-size: 0.95rem;
    color: var(--fg-primary);
    font-variant-numeric: tabular-nums;
  }

  .headlines h2, .events h2 {
    font-family: var(--font-chrome);
    font-size: 0.7rem;
    letter-spacing: 0.22em;
    color: var(--fg-secondary);
    margin: 1rem 0 0.5rem 0;
    text-transform: uppercase;
  }
  .headlines ul, .events ul {
    list-style: none;
    padding: 0;
    margin: 0;
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
  }
  .headlines li, .events li {
    padding-left: 1rem;
    position: relative;
    color: var(--fg-primary);
    font-size: var(--fs-sm);
    line-height: 1.4;
  }
  .headlines li::before, .events li::before {
    content: '•';
    color: var(--accent-primary);
    position: absolute;
    left: 0;
  }

  .narrative {
    margin: 1rem 0 0 0;
    padding: 0.75rem 1rem;
    border-left: 2px solid var(--accent-primary);
    background: var(--bg-elevated);
    color: var(--fg-secondary);
    font-style: italic;
    line-height: 1.5;
    font-size: var(--fs-sm);
  }

  footer {
    margin-top: 1.5rem;
    text-align: center;
  }
  .continue {
    padding: 0.65rem 2rem;
    font-family: var(--font-chrome);
    font-size: 0.8rem;
    letter-spacing: 0.3em;
    text-transform: uppercase;
    border: 1px solid var(--accent-primary);
    color: var(--accent-primary);
    background: transparent;
    border-radius: 0;
  }
  .continue:hover { background: rgba(196, 163, 90, 0.1); }
</style>
