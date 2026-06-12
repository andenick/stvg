<script lang="ts">
  /*
   * Live financial statements — a real-time moving balance sheet + income
   * statement. Balance-sheet figures stream every tick (sim.bank* fields from
   * market_tick); the income statement refreshes each quarter (sim.bank).
   *
   * Answers the player's asks: "equity value", "loans vs assets", "cash ->
   * investments as deals are taken", and "I can't tell I'm over-invested"
   * (the EXPOSURE bar = leverage vs a safe band).
   */

  import { sim } from '../stores/simulation.svelte';
  import { formatBigDollar } from '../charts/chart-utils';
  import { telemetry } from '../telemetry';
  import { dwell } from '../actions/dwell';

  // P1 hover wiring: throttled in telemetry.hover(). Key `bs:<line>` for balance
  // -sheet line items, `exposure` for the leverage/EXPOSURE bar.
  function hoverLine(key: string) {
    telemetry.hover(`bs:${key}`);
  }

  // ── Balance sheet (live) ────────────────────────────────────────────────
  let reserves = $derived(sim.bankReserves);
  let loans = $derived(sim.bankLoans);
  let securities = $derived(sim.bankSecurities);
  let assets = $derived(sim.bankAssets);
  let otherAssets = $derived(Math.max(0, assets - reserves - loans - securities));
  let deposits = $derived(sim.bankDeposits);
  let interbank = $derived(sim.bankInterbank);
  let equity = $derived(sim.bankCapital);
  let liabPlusEquity = $derived(deposits + interbank + equity);

  // Income statement (per-quarter snapshot from the full PlayerView).
  function bankNum(key: string): number {
    const v = (sim.bank as Record<string, unknown> | null)?.[key];
    return typeof v === 'number' ? v : 0;
  }
  let revenue = $derived(bankNum('totalRevenue'));
  let costs = $derived(bankNum('totalCosts'));
  let nii = $derived(bankNum('netInterestIncome'));
  let fees = $derived(bankNum('feeIncome'));
  let trading = $derived(bankNum('tradingPnL'));
  let netIncome = $derived(bankNum('netIncome'));

  // Exposure: leverage (assets / equity). Safe < 8×, caution 8–12×, danger > 12×.
  let leverage = $derived(equity > 0 ? assets / equity : 0);
  let exposureBand = $derived(leverage > 12 ? 'danger' : leverage > 8 ? 'caution' : 'safe');
  let exposurePct = $derived(Math.min(100, (leverage / 16) * 100));

  function pctOf(part: number, whole: number): string {
    if (whole <= 0) return '0%';
    return `${Math.round((part / whole) * 100)}%`;
  }
</script>

<section class="financials" aria-label="Financial statements" use:dwell={'panel:FinancialsPanel'}>
  <header class="fin-head">
    <span class="fin-title">Financials</span>
    <span class="fin-sub">live</span>
  </header>

  <!-- Exposure -->
  <div class="exposure exp-{exposureBand}" role="presentation" onmouseenter={() => hoverLine('exposure')}>
    <div class="exp-row">
      <span class="exp-label">EXPOSURE · {leverage.toFixed(1)}× leverage</span>
      <span class="exp-band">{exposureBand.toUpperCase()}</span>
    </div>
    <div class="exp-bar"><span class="exp-fill" style="width:{exposurePct}%"></span></div>
  </div>

  <!-- Balance sheet -->
  <div class="bs">
    <div class="bs-col">
      <div class="bs-colhead">Assets <span class="num">{formatBigDollar(assets)}</span></div>
      <div class="bs-line" role="presentation" onmouseenter={() => hoverLine('reserves')}><span class="bs-k">Cash &amp; reserves</span><span class="num bs-v">{formatBigDollar(reserves)}</span></div>
      <div class="bs-line" role="presentation" onmouseenter={() => hoverLine('loans')}><span class="bs-k">Loans <em>{pctOf(loans, assets)}</em></span><span class="num bs-v loan">{formatBigDollar(loans)}</span></div>
      <div class="bs-line" role="presentation" onmouseenter={() => hoverLine('securities')}><span class="bs-k">Securities <em>{pctOf(securities, assets)}</em></span><span class="num bs-v sec">{formatBigDollar(securities)}</span></div>
      {#if otherAssets > 0}
        <div class="bs-line" role="presentation" onmouseenter={() => hoverLine('other')}><span class="bs-k">Other</span><span class="num bs-v">{formatBigDollar(otherAssets)}</span></div>
      {/if}
    </div>
    <div class="bs-col">
      <div class="bs-colhead">Liabilities + Equity <span class="num">{formatBigDollar(liabPlusEquity)}</span></div>
      <div class="bs-line" role="presentation" onmouseenter={() => hoverLine('deposits')}><span class="bs-k">Deposits</span><span class="num bs-v">{formatBigDollar(deposits)}</span></div>
      {#if interbank > 0}
        <div class="bs-line" role="presentation" onmouseenter={() => hoverLine('interbank')}><span class="bs-k">Interbank</span><span class="num bs-v">{formatBigDollar(interbank)}</span></div>
      {/if}
      <div class="bs-line equity" role="presentation" onmouseenter={() => hoverLine('equity')}><span class="bs-k">Equity</span><span class="num bs-v">{formatBigDollar(equity)}</span></div>
    </div>
  </div>

  <!-- Income statement -->
  <div class="is">
    <div class="is-head">Income — {sim.year} Q{sim.quarter}</div>
    <div class="is-line"><span>Revenue</span><span class="num">{formatBigDollar(revenue)}</span></div>
    {#if nii !== 0}<div class="is-line sub"><span>· Net interest</span><span class="num">{formatBigDollar(nii)}</span></div>{/if}
    {#if fees !== 0}<div class="is-line sub"><span>· Fees</span><span class="num">{formatBigDollar(fees)}</span></div>{/if}
    {#if trading !== 0}<div class="is-line sub"><span>· Trading P&amp;L</span><span class="num" class:down={trading < 0}>{formatBigDollar(trading)}</span></div>{/if}
    <div class="is-line"><span>Costs</span><span class="num down">({formatBigDollar(costs)})</span></div>
    <div class="is-line total" class:up={netIncome > 0} class:down={netIncome < 0}>
      <span>Net income</span><span class="num">{formatBigDollar(netIncome)}</span>
    </div>
  </div>
</section>

<style>
  .financials {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 0.9rem 1rem 1rem;
    display: flex;
    flex-direction: column;
    gap: 0.85rem;
  }
  .fin-head { display: flex; align-items: baseline; justify-content: space-between;
    border-bottom: 1px solid var(--border-soft); padding-bottom: 0.5rem; }
  .fin-title { font-family: var(--font-display); color: var(--accent-primary); font-size: 1.15rem; }
  .fin-sub { font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.18em;
    color: var(--accent-success); text-transform: uppercase; }

  .exposure { display: flex; flex-direction: column; gap: 0.3rem; }
  .exp-row { display: flex; justify-content: space-between; align-items: baseline; }
  .exp-label { font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.12em; color: var(--fg-secondary); }
  .exp-band { font-family: var(--font-chrome); font-size: 0.6rem; letter-spacing: 0.18em; }
  .exp-bar { height: 6px; background: var(--bg-base); border-radius: 3px; overflow: hidden; }
  .exp-fill { display: block; height: 100%; }
  .exp-safe .exp-fill    { background: var(--accent-success); }
  .exp-safe .exp-band    { color: var(--accent-success); }
  .exp-caution .exp-fill { background: var(--accent-warning); }
  .exp-caution .exp-band { color: var(--accent-warning); }
  .exp-danger .exp-fill  { background: var(--accent-danger); }
  .exp-danger .exp-band  { color: var(--accent-danger); }

  .bs { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; }
  .bs-col { display: flex; flex-direction: column; gap: 0.25rem; }
  .bs-colhead {
    display: flex; justify-content: space-between; align-items: baseline;
    font-family: var(--font-chrome); font-size: 0.62rem; letter-spacing: 0.16em;
    text-transform: uppercase; color: var(--fg-secondary);
    border-bottom: 1px solid var(--border-soft); padding-bottom: 0.25rem; margin-bottom: 0.15rem;
  }
  .bs-colhead .num { color: var(--fg-primary); font-size: 0.8rem; }
  .bs-line { display: flex; justify-content: space-between; align-items: baseline; gap: 0.5rem; }
  .bs-k { font-size: 0.8rem; color: var(--fg-secondary); }
  .bs-k em { font-style: normal; color: var(--fg-muted); font-size: 0.7rem; margin-left: 0.2rem; }
  .bs-v { font-family: var(--font-data); font-size: 0.82rem; color: var(--fg-primary); }
  .bs-v.loan { color: #4d7836; }
  .bs-v.sec  { color: #3f6ea3; }
  .bs-line.equity .bs-k, .bs-line.equity .bs-v { color: var(--accent-primary); font-weight: 600; }

  .is { display: flex; flex-direction: column; gap: 0.2rem;
    border-top: 1px solid var(--border-soft); padding-top: 0.6rem; }
  .is-head { font-family: var(--font-chrome); font-size: 0.62rem; letter-spacing: 0.16em;
    text-transform: uppercase; color: var(--fg-secondary); margin-bottom: 0.2rem; }
  .is-line { display: flex; justify-content: space-between; font-size: 0.82rem; color: var(--fg-primary); }
  .is-line .num { font-family: var(--font-data); }
  .is-line.sub { color: var(--fg-secondary); font-size: 0.74rem; }
  .is-line.total { border-top: 1px solid var(--border-soft); margin-top: 0.25rem; padding-top: 0.3rem; font-weight: 600; }
  .is-line .num.down, .is-line.down .num { color: var(--accent-danger); }
  .is-line.up .num { color: var(--accent-success); }
</style>
