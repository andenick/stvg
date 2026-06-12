<script lang="ts">
  /*
   * Combined bank-evolution chart — three live series on one graph:
   *   • Equity (capital)  — the bank's net worth
   *   • Loans             — the loan book
   *   • Total Assets      — the whole balance sheet
   * All stream from market_tick (see simulation store ring buffers). This is the
   * "watch the bank grow in real time" centerpiece the dashboard is built around.
   */

  import { createChart, LineSeries, type IChartApi, type ISeriesApi, LineType } from 'lightweight-charts';
  import { sim } from '../stores/simulation.svelte';
  import {
    baseChartOptions,
    eraChartColors,
    formatBigDollar,
    totalDayToTime,
  } from './chart-utils';
  import type { CapitalPoint } from '../stores/simulation.svelte';

  interface Props {
    /**
     * Time-scale window in trading days (P2.3). When set, the chart only renders
     * the last `maxDays` of each series — the hero chart's 1Y/5Y/… toggle. MAX
     * passes Infinity (the default) so the whole buffer shows.
     */
    maxDays?: number;
  }
  let { maxDays = Infinity }: Props = $props();

  let containerEl: HTMLDivElement | undefined = $state();
  let chart: IChartApi | null = null;

  // Series colors are stable across eras so the legend stays legible.
  const EQUITY = '#876a2c';   // deep gold
  const LOANS = '#4d7836';    // ledger green
  const ASSETS = '#3f6ea3';   // steel blue

  let equitySeries: ISeriesApi<'Line'> | null = null;
  let loanSeries: ISeriesApi<'Line'> | null = null;
  let assetSeries: ISeriesApi<'Line'> | null = null;

  function mount() {
    if (!containerEl) return;
    const colors = eraChartColors(sim.era?.name);
    chart = createChart(containerEl, {
      ...baseChartOptions(colors),
      localization: { priceFormatter: formatBigDollar },
    });
    assetSeries = chart.addSeries(LineSeries, {
      color: ASSETS, lineWidth: 2, lineType: LineType.Curved,
      priceLineVisible: false, lastValueVisible: true,
    });
    loanSeries = chart.addSeries(LineSeries, {
      color: LOANS, lineWidth: 2, lineType: LineType.Curved,
      priceLineVisible: false, lastValueVisible: false,
    });
    equitySeries = chart.addSeries(LineSeries, {
      color: EQUITY, lineWidth: 2, lineType: LineType.Curved,
      priceLineVisible: false, lastValueVisible: true,
    });
    hydrate();
  }

  // Latest totalDay across the kept series — the right edge of the window.
  function latestDay(): number {
    const h = sim.capitalHistory;
    return h.length ? h[h.length - 1].time : 0;
  }

  function toData(hist: CapitalPoint[]) {
    const cutoff = maxDays === Infinity ? -Infinity : latestDay() - maxDays;
    return hist
      .filter((p) => p.time >= cutoff)
      .map((p) => ({ time: totalDayToTime(p.time), value: p.value }));
  }

  function hydrate() {
    if (!chart) return;
    const a = toData(sim.assetHistory);
    const l = toData(sim.loanHistory);
    const e = toData(sim.capitalHistory);
    if (a.length) assetSeries?.setData(a);
    if (l.length) loanSeries?.setData(l);
    if (e.length) equitySeries?.setData(e);
    if (e.length || a.length) chart.timeScale().fitContent();
  }

  // Append the latest point of each series on every tick (O(1) updates).
  let lastSeenTime = $state<number | null>(null);

  // Re-hydrate (full setData with the new window) whenever the time-scale
  // changes. Tracking maxDays here re-runs the effect on toggle.
  $effect(() => {
    void maxDays;
    if (!chart) return;
    hydrate();
    lastSeenTime = sim.capitalHistory.length
      ? sim.capitalHistory[sim.capitalHistory.length - 1].time
      : null;
  });
  $effect(() => {
    const hist = sim.capitalHistory;
    if (!chart || hist.length === 0) return;
    const latest = hist[hist.length - 1];
    if (lastSeenTime === latest.time) return;
    if (lastSeenTime === null) {
      hydrate();
    } else {
      const t = totalDayToTime(latest.time);
      equitySeries?.update({ time: t, value: latest.value });
      const l = sim.loanHistory[sim.loanHistory.length - 1];
      const a = sim.assetHistory[sim.assetHistory.length - 1];
      if (l) loanSeries?.update({ time: totalDayToTime(l.time), value: l.value });
      if (a) assetSeries?.update({ time: totalDayToTime(a.time), value: a.value });
    }
    lastSeenTime = latest.time;
  });

  // Re-theme on era change (series colors stay fixed for legend legibility).
  $effect(() => {
    if (!chart) return;
    const colors = eraChartColors(sim.era?.name);
    chart.applyOptions({
      layout: { background: { color: colors.background }, textColor: colors.text },
      grid: { vertLines: { color: colors.grid }, horzLines: { color: colors.grid } },
      timeScale: { borderColor: colors.border },
      rightPriceScale: { borderColor: colors.border },
    });
  });

  $effect(() => {
    mount();
    return () => {
      chart?.remove();
      chart = null;
      equitySeries = loanSeries = assetSeries = null;
    };
  });
</script>

<div class="capital-chart-wrap">
  <div class="legend">
    <span class="series-chip"><span class="swatch" style="background:{EQUITY}"></span>EQUITY <span class="num v">{formatBigDollar(sim.bankCapital)}</span></span>
    <span class="series-chip"><span class="swatch" style="background:{LOANS}"></span>LOANS <span class="num v">{formatBigDollar(sim.bankLoans)}</span></span>
    <span class="series-chip"><span class="swatch" style="background:{ASSETS}"></span>ASSETS <span class="num v">{formatBigDollar(sim.bankAssets)}</span></span>
    {#if sim.capitalDelta !== 0}
      <span class="num delta" class:up={sim.capitalDelta > 0} class:down={sim.capitalDelta < 0}>
        {sim.capitalDelta > 0 ? '+' : ''}{formatBigDollar(sim.capitalDelta)}
      </span>
    {/if}
  </div>
  <div bind:this={containerEl} class="chart"></div>
</div>

<style>
  .capital-chart-wrap {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 0.6rem 0.75rem 0.75rem 0.75rem;
    display: flex;
    flex-direction: column;
    height: 100%;
    min-height: 20rem;
  }
  .legend {
    display: flex;
    align-items: baseline;
    flex-wrap: wrap;
    gap: 1rem;
    padding-bottom: 0.5rem;
  }
  .series-chip {
    display: inline-flex;
    align-items: baseline;
    gap: 0.4rem;
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    letter-spacing: 0.18em;
    color: var(--fg-secondary);
  }
  .swatch {
    width: 0.7rem;
    height: 0.32rem;
    border-radius: 1px;
    align-self: center;
  }
  .series-chip .v {
    font-family: var(--font-data);
    font-size: 0.95rem;
    color: var(--fg-primary);
    font-variant-numeric: tabular-nums;
    letter-spacing: 0;
  }
  .delta {
    font-family: var(--font-data);
    font-size: 0.78rem;
    padding: 0.1rem 0.35rem;
    border-radius: 2px;
    margin-left: auto;
  }
  .delta.up   { color: var(--accent-success); background: rgba(77, 120, 54, 0.12); }
  .delta.down { color: var(--accent-danger);  background: rgba(157, 56, 44, 0.12); }
  .chart {
    flex: 1;
    min-height: 15rem;
  }
</style>
