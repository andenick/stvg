<script lang="ts">
  /*
   * Bank-capital line chart, live-updated from market_tick.
   *
   * Bugs fixed:
   *   - top_20 #1: priceFormatter -> $10.5B / $500M / $1.2T (no 10467384616)
   *   - top_20 #5: X-axis shows "1947 Q2", not raw totalDay integers
   *   - top_20 #13: watermark hidden
   *   - top_20 #14: lineType=2 (curved) softens the step-function
   *   - top_20 #9: era-keyed color map matches canonical EraEngine names
   */

  import { createChart, LineSeries, type IChartApi, type ISeriesApi, LineType } from 'lightweight-charts';
  import { sim } from '../stores/simulation.svelte';
  import {
    baseChartOptions,
    eraChartColors,
    formatBigDollar,
    totalDayToTime,
  } from './chart-utils';

  let containerEl: HTMLDivElement | undefined = $state();
  let chart: IChartApi | null = null;
  let series: ISeriesApi<'Line'> | null = null;

  function mount() {
    if (!containerEl) return;
    const colors = eraChartColors(sim.era?.name);
    chart = createChart(containerEl, {
      ...baseChartOptions(colors),
      localization: {
        priceFormatter: formatBigDollar,
      },
    });
    series = chart.addSeries(LineSeries, {
      color: colors.lineColor,
      lineWidth: 2,
      lineType: LineType.Curved,   // top_20 #14
      priceLineVisible: false,
      lastValueVisible: true,
    });
    hydrate();
  }

  function hydrate() {
    if (!series) return;
    const data = sim.capitalHistory.map((p) => ({
      time: totalDayToTime(p.time),
      value: p.value,
    }));
    if (data.length) {
      series.setData(data);
      chart?.timeScale().fitContent();
    }
  }

  // React to live ticks: append the latest point. Avoid full setData on every
  // tick — series.update() is O(1) and what lightweight-charts is built for.
  let lastSeenTime = $state<number | null>(null);

  $effect(() => {
    const history = sim.capitalHistory;
    if (!series || history.length === 0) return;
    const latest = history[history.length - 1];
    if (lastSeenTime === latest.time) return;
    if (lastSeenTime === null) {
      hydrate();
    } else {
      series.update({ time: totalDayToTime(latest.time), value: latest.value });
    }
    lastSeenTime = latest.time;
  });

  // Re-theme on era change.
  $effect(() => {
    if (!chart || !series) return;
    const colors = eraChartColors(sim.era?.name);
    chart.applyOptions({
      layout: { background: { color: colors.background }, textColor: colors.text },
      grid: {
        vertLines: { color: colors.grid },
        horzLines: { color: colors.grid },
      },
      timeScale: { borderColor: colors.border },
      rightPriceScale: { borderColor: colors.border },
    });
    series.applyOptions({ color: colors.lineColor });
  });

  $effect(() => {
    mount();
    return () => {
      chart?.remove();
      chart = null;
      series = null;
    };
  });
</script>

<div class="capital-chart-wrap">
  <div class="legend">
    <span class="label">BANK CAPITAL</span>
    <span class="num value">{formatBigDollar(sim.bankCapital)}</span>
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
    min-height: 16rem;
  }
  .legend {
    display: flex;
    align-items: baseline;
    gap: 0.75rem;
    padding-bottom: 0.4rem;
  }
  .label {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    letter-spacing: 0.25em;
    color: var(--fg-secondary);
  }
  .value {
    font-family: var(--font-data);
    font-size: 1.05rem;
    color: var(--fg-primary);
    font-variant-numeric: tabular-nums;
  }
  .delta {
    font-family: var(--font-data);
    font-size: 0.78rem;
    padding: 0.1rem 0.35rem;
    border-radius: 2px;
  }
  .delta.up   { color: var(--accent-success); background: rgba(107, 142, 78, 0.12); }
  .delta.down { color: var(--accent-danger);  background: rgba(168, 69, 58, 0.12); }
  .chart {
    flex: 1;
    min-height: 12rem;
  }
</style>
