<script lang="ts">
  /*
   * Financials tab (P2) — the live statements (promoted out of phase-1-only so
   * the balance-sheet stream is readable in every era), the year-end Annual
   * Report sidebar, and a funding-mix card.
   *
   * Funding mix ("what a silly bank CEO looks at"): deposits vs wholesale
   * (interbank) vs equity — all stream every tick (sim.bankDeposits /
   * bankInterbank / bankCapital). Loan yield vs funding rate are derived from the
   * per-quarter income statement when the engine provides the components
   * (netInterestIncome over loans; fundingCost over interest-bearing
   * liabilities); they're omitted gracefully when those fields aren't present
   * rather than faked.
   */

  import FinancialsPanel from './FinancialsPanel.svelte';
  import AnnualReportPanel from '../sidebar/AnnualReportPanel.svelte';
  import { sim } from '../stores/simulation.svelte';
  import { formatBigDollar } from '../charts/chart-utils';
  import { fmtMoney } from '../util/money';
  import { dwell } from '../actions/dwell';

  // ── P7 Loan Book card ──────────────────────────────────────────────────────
  let lbStats = $derived(sim.loanBookStats);
  let lbResolved = $derived(lbStats ? lbStats.paidOff + lbStats.defaulted + lbStats.windfalls : 0);
  // Last 5 outcomes, most recent first (the feed is most-recent-last).
  let recentOutcomes = $derived((sim.dealOutcomes ?? []).slice(-5).reverse());
  function outcomeWord(o: string): string {
    return o === 'windfall' ? 'Windfall' : o === 'paid_off' ? 'Paid off' : 'Defaulted';
  }

  // ── Funding mix (live stream) ─────────────────────────────────────────────
  let deposits = $derived(sim.bankDeposits);
  let wholesale = $derived(sim.bankInterbank);
  let equity = $derived(sim.bankCapital);
  let fundingTotal = $derived(Math.max(1, deposits + wholesale + equity));
  function pct(part: number): number { return (part / fundingTotal) * 100; }

  // ── Rates (derived from the quarterly income statement when available) ─────
  function bankNum(key: string): number | null {
    const v = (sim.bank as Record<string, unknown> | null)?.[key];
    return typeof v === 'number' ? v : null;
  }
  // Annualized loan yield ≈ 4 × quarterly net-interest income / loan book.
  let loanYield = $derived.by(() => {
    const nii = bankNum('netInterestIncome');
    const loans = sim.bankLoans;
    if (nii == null || loans <= 0) return null;
    return (nii * 4) / loans;
  });
  // Annualized funding rate ≈ 4 × quarterly funding cost / interest-bearing liabilities.
  let fundingRate = $derived.by(() => {
    const fc = bankNum('fundingCost') ?? bankNum('interestExpense');
    const liab = deposits + wholesale;
    if (fc == null || liab <= 0) return null;
    return (fc * 4) / liab;
  });
  let spread = $derived(loanYield != null && fundingRate != null ? loanYield - fundingRate : null);
  function rate(v: number | null): string { return v == null ? '—' : `${(v * 100).toFixed(2)}%`; }
</script>

<div class="fin-tab">
  <main class="fin-main">
    <FinancialsPanel />
  </main>

  <aside class="fin-side">
    <!-- P7 Loan Book card: the cumulative scoreboard + last 5 outcomes. -->
    {#if lbStats && (lbStats.accepted > 0 || lbResolved > 0)}
      <section class="loanbook" aria-label="Loan book" use:dwell={'loanbook'}>
        <header class="lb-head">
          <span class="lb-title">Loan Book</span>
          <span class="lb-sub">cumulative</span>
        </header>
        <div class="lb-stats">
          <div class="lb-stat"><span class="lb-k">Accepted</span><span class="lb-v num">{lbStats.accepted}</span></div>
          <div class="lb-stat"><span class="lb-k">Paid off</span><span class="lb-v num good">{lbStats.paidOff}</span></div>
          <div class="lb-stat"><span class="lb-k">Windfalls</span><span class="lb-v num good">{lbStats.windfalls}</span></div>
          <div class="lb-stat"><span class="lb-k">Defaulted</span><span class="lb-v num bad">{lbStats.defaulted}</span></div>
        </div>
        <div class="lb-pnl">
          <span class="lb-k">Realized P&amp;L</span>
          <span class="lb-pnl-v num" class:good={lbStats.realizedPnl > 0} class:bad={lbStats.realizedPnl < 0}>
            {lbStats.realizedPnl >= 0 ? '+' : '−'}{fmtMoney(Math.abs(lbStats.realizedPnl))}
          </span>
        </div>
        {#if recentOutcomes.length}
          <ul class="lb-list">
            {#each recentOutcomes as o (o.dealId)}
              <li class="lb-row outc-{o.outcome}">
                <span class="lb-firm">{o.clientName || o.title}</span>
                <span class="lb-outcome">{outcomeWord(o.outcome)}</span>
                <span class="lb-amt num" class:good={o.realizedPnl > 0} class:bad={o.realizedPnl < 0}>
                  {o.realizedPnl >= 0 ? '+' : '−'}{fmtMoney(Math.abs(o.realizedPnl))}
                </span>
              </li>
            {/each}
          </ul>
        {:else}
          <p class="lb-empty">No loans have resolved yet. Accepted deals mature over their duration.</p>
        {/if}
      </section>
    {/if}

    <!-- Funding mix card -->
    <section class="funding" aria-label="Funding mix">
      <header class="funding-head">
        <span class="funding-title">Funding Mix</span>
        <span class="funding-sub">live</span>
      </header>

      <div class="mix-bar" aria-hidden="true">
        <span class="mix-seg seg-dep" style="width: {pct(deposits)}%" title="Deposits"></span>
        <span class="mix-seg seg-whl" style="width: {pct(wholesale)}%" title="Wholesale / interbank"></span>
        <span class="mix-seg seg-eq" style="width: {pct(equity)}%" title="Equity"></span>
      </div>

      <ul class="mix-legend">
        <li><span class="dot seg-dep"></span>Deposits<span class="num v">{formatBigDollar(deposits)}</span><span class="num p">{pct(deposits).toFixed(0)}%</span></li>
        <li><span class="dot seg-whl"></span>Wholesale<span class="num v">{formatBigDollar(wholesale)}</span><span class="num p">{pct(wholesale).toFixed(0)}%</span></li>
        <li><span class="dot seg-eq"></span>Equity<span class="num v">{formatBigDollar(equity)}</span><span class="num p">{pct(equity).toFixed(0)}%</span></li>
      </ul>

      <div class="rates">
        <div class="rate-row"><span class="rk">Loan yield</span><span class="num rv earn">{rate(loanYield)}</span></div>
        <div class="rate-row"><span class="rk">Funding rate</span><span class="num rv pay">{rate(fundingRate)}</span></div>
        <div class="rate-row spread"><span class="rk">Net spread</span><span class="num rv" class:up={(spread ?? 0) > 0} class:down={(spread ?? 0) < 0}>{rate(spread)}</span></div>
      </div>
    </section>

    <AnnualReportPanel />
  </aside>
</div>

<style>
  .fin-tab {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 18rem;
    gap: 1.25rem;
    padding: 1.25rem 1.5rem;
    align-items: start;
  }
  .fin-main, .fin-side { min-width: 0; display: flex; flex-direction: column; gap: 1rem; }

  .funding {
    background: var(--bg-card); border: 1px solid var(--border); border-radius: var(--card-radius);
    padding: 0.9rem 1rem 1rem; display: flex; flex-direction: column; gap: 0.7rem;
  }
  .funding-head { display: flex; align-items: baseline; justify-content: space-between;
    border-bottom: 1px solid var(--border-soft); padding-bottom: 0.5rem; }
  .funding-title { font-family: var(--font-display); color: var(--accent-primary); font-size: 1.15rem; }
  .funding-sub { font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.18em; color: var(--accent-success); text-transform: uppercase; }

  .mix-bar { display: flex; height: 14px; border-radius: 3px; overflow: hidden; background: var(--bg-base); }
  .mix-seg { display: block; height: 100%; }
  .seg-dep { background: #4d7836; }
  .seg-whl { background: #c1763a; }
  .seg-eq  { background: var(--accent-primary); }

  .mix-legend { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: 0.3rem; }
  .mix-legend li { display: grid; grid-template-columns: auto 1fr auto auto; align-items: center; gap: 0.5rem; font-size: 0.8rem; color: var(--fg-secondary); }
  .dot { width: 0.7rem; height: 0.4rem; border-radius: 1px; }
  .mix-legend .v { font-family: var(--font-data); color: var(--fg-primary); font-variant-numeric: tabular-nums; }
  .mix-legend .p { font-family: var(--font-data); color: var(--fg-muted); font-size: 0.72rem; }

  .rates { border-top: 1px solid var(--border-soft); padding-top: 0.55rem; display: flex; flex-direction: column; gap: 0.25rem; }
  .rate-row { display: flex; align-items: baseline; justify-content: space-between; font-size: 0.82rem; }
  .rk { color: var(--fg-secondary); }
  .rv { font-family: var(--font-data); color: var(--fg-primary); }
  .rv.earn { color: var(--accent-success); }
  .rv.pay { color: var(--accent-danger); }
  .rate-row.spread { border-top: 1px solid var(--border-soft); margin-top: 0.2rem; padding-top: 0.35rem; font-weight: 600; }
  .rate-row.spread .rv.up { color: var(--accent-success); }
  .rate-row.spread .rv.down { color: var(--accent-danger); }

  /* P7 Loan Book card */
  .loanbook {
    background: var(--bg-card); border: 1px solid var(--border); border-radius: var(--card-radius);
    padding: 0.9rem 1rem 1rem; display: flex; flex-direction: column; gap: 0.6rem;
  }
  .lb-head { display: flex; align-items: baseline; justify-content: space-between;
    border-bottom: 1px solid var(--border-soft); padding-bottom: 0.5rem; }
  .lb-title { font-family: var(--font-display); color: var(--accent-primary); font-size: 1.15rem; }
  .lb-sub { font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.18em; color: var(--fg-secondary); text-transform: uppercase; }
  .lb-stats { display: grid; grid-template-columns: 1fr 1fr; gap: 0.4rem 0.75rem; }
  .lb-stat { display: flex; align-items: baseline; justify-content: space-between; }
  .lb-k { font-size: 0.78rem; color: var(--fg-secondary); }
  .lb-v { font-family: var(--font-data); color: var(--fg-primary); font-variant-numeric: tabular-nums; }
  .lb-v.good { color: var(--accent-success); }
  .lb-v.bad { color: var(--accent-danger); }
  .lb-pnl { display: flex; align-items: baseline; justify-content: space-between;
    border-top: 1px solid var(--border-soft); padding-top: 0.5rem; }
  .lb-pnl-v { font-family: var(--font-data); font-size: 1rem; color: var(--fg-primary); font-variant-numeric: tabular-nums; }
  .lb-pnl-v.good { color: var(--accent-success); }
  .lb-pnl-v.bad { color: var(--accent-danger); }
  .lb-list { list-style: none; margin: 0; padding: 0.3rem 0 0; display: flex; flex-direction: column; gap: 0.3rem;
    border-top: 1px solid var(--border-soft); }
  .lb-row { display: grid; grid-template-columns: 1fr auto auto; align-items: baseline; gap: 0.5rem; font-size: 0.78rem; }
  .lb-firm { color: var(--fg-primary); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .lb-outcome { font-family: var(--font-chrome); font-size: 0.56rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--fg-muted); }
  .lb-row.outc-defaulted .lb-outcome { color: var(--accent-danger); }
  .lb-row.outc-windfall .lb-outcome { color: var(--accent-success); }
  .lb-amt { font-family: var(--font-data); font-variant-numeric: tabular-nums; }
  .lb-amt.good { color: var(--accent-success); }
  .lb-amt.bad { color: var(--accent-danger); }
  .lb-empty { font-size: 0.8rem; color: var(--fg-secondary); margin: 0; line-height: 1.4; }

  @media (max-width: 900px) {
    .fin-tab { grid-template-columns: 1fr; }
    .fin-side { order: 2; }
  }
</style>
