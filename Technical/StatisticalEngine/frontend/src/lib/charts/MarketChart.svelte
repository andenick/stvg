<script lang="ts">
  /*
   * Per-market line chart. Streams from sim.markets[marketId].history.
   * Phase 1 mode = compact (`compact={true}`) — small multiples, no legend chrome.
   * Phase 3+ mode = expanded — full crosshair, larger fonts.
   *
   * Click-to-expand: calls onExpand() so the parent can swap views.
   */

  import { createChart, LineSeries, type IChartApi, type ISeriesApi, LineType } from 'lightweight-charts';
  import { sim } from '../stores/simulation.svelte';
  import {
    baseChartOptions,
    eraChartColors,
    formatMarketPrice,
    totalDayToTime,
  } from './chart-utils';

  interface Props {
    marketId: string;
    label?: string;
    compact?: boolean;
    onExpand?: (id: string) => void;
  }

  let { marketId, label, compact = false, onExpand }: Props = $props();

  let containerEl: HTMLDivElement | undefined = $state();
  let chart: IChartApi | null = null;
  let series: ISeriesApi<'Line'> | null = null;
  let lastSeenTime = $state<number | null>(null);

  let series_data = $derived(sim.markets[marketId]);
  let lastPrice = $derived(series_data?.lastPrice ?? 0);
  let lastReturn = $derived(series_data?.lastReturn ?? 0);

  function mount() {
    if (!containerEl) return;
    const colors = eraChartColors(sim.era?.name);
    chart = createChart(containerEl, {
      ...baseChartOptions(colors),
      localization: {
        priceFormatter: (p: number) => formatMarketPrice(p),
      },
      timeScale: {
        ...baseChartOptions(colors).timeScale,
        // compact charts get fewer ticks
        rightOffset: 2,
      },
    });
    series = chart.addSeries(LineSeries, {
      color: colors.lineColor,
      lineWidth: 2,
      lineType: LineType.Simple,
      priceLineVisible: false,
      lastValueVisible: !compact,
    });
    hydrate();
  }

  function hydrate() {
    if (!series) return;
    const s = sim.markets[marketId];
    if (!s) return;
    const data = s.history.map((p) => ({
      time: totalDayToTime(p.time),
      value: p.value,
    }));
    if (data.length) {
      series.setData(data);
      chart?.timeScale().fitContent();
      lastSeenTime = data[data.length - 1].time as unknown as number;
    }
  }

  $effect(() => {
    const s = sim.markets[marketId];
    if (!series || !s || s.history.length === 0) return;
    const latest = s.history[s.history.length - 1];
    if (lastSeenTime === latest.time) return;
    if (lastSeenTime === null) {
      hydrate();
    } else {
      series.update({ time: totalDayToTime(latest.time), value: latest.value });
    }
    lastSeenTime = latest.time;
  });

  // Re-theme on era change
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

  function handleClick() {
    if (onExpand) onExpand(marketId);
  }
</script>

{#snippet head()}
  <div class="head">
    <span class="ticker">{label ?? marketId}</span>
    <span class="price num">{formatMarketPrice(lastPrice)}</span>
    <span
      class="ret num"
      class:up={lastReturn > 0}
      class:down={lastReturn < 0}
      aria-label="daily return"
    >
      {lastReturn >= 0 ? '+' : ''}{(lastReturn * 100).toFixed(2)}%
    </span>
  </div>
{/snippet}

{#if onExpand}
  <button
    type="button"
    class="market-chart clickable"
    class:compact
    onclick={handleClick}
  >
    {@render head()}
    <div bind:this={containerEl} class="chart"></div>
  </button>
{:else}
  <div class="market-chart" class:compact>
    {@render head()}
    <div bind:this={containerEl} class="chart"></div>
  </div>
{/if}

<style>
  .market-chart {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    display: flex;
    flex-direction: column;
    padding: 0.45rem 0.6rem 0.5rem 0.6rem;
    min-height: 8rem;
    height: 100%;
    color: inherit;
    font: inherit;
    text-align: left;
  }
  button.market-chart { cursor: pointer; }
  button.market-chart:hover { border-color: var(--accent-primary); }

  .head {
    display: flex;
    align-items: baseline;
    gap: 0.5rem;
    padding-bottom: 0.3rem;
    font-family: var(--font-chrome);
  }
  .ticker {
    font-size: var(--fs-xs);
    letter-spacing: 0.22em;
    color: var(--fg-secondary);
    text-transform: uppercase;
    flex: 1;
  }
  .price {
    font-family: var(--font-data);
    color: var(--fg-primary);
    font-size: 0.85rem;
    font-variant-numeric: tabular-nums;
  }
  .ret {
    font-family: var(--font-data);
    font-size: 0.72rem;
  }
  .ret.up   { color: var(--accent-success); }
  .ret.down { color: var(--accent-danger); }

  .chart {
    flex: 1;
    min-height: 5.5rem;
  }
  .compact .chart {
    min-height: 4.5rem;
  }
</style>
