<script lang="ts">
  /*
   * Macro line chart (P3) — the "living economy" sibling of MarketChart.
   * Streams from sim.macro[macroId].history (per-day ring buffer, fed by the
   * `econ` block on each market_tick and backfilled from /macro-history).
   *
   * Rate series (Fed Funds, Unemployment, Inflation, spreads) are DECIMALS in
   * the engine (0.05 = 5%); the price axis formats them as percentages. GDP is
   * an index — plain numbers.
   *
   * Same lightweight-charts conventions / era theming as MarketChart. Compact =
   * small-multiple; hero = tall + last-value; onExpand = click-to-expand.
   */

  import { createChart, LineSeries, type IChartApi, type ISeriesApi, LineType } from 'lightweight-charts';
  import { sim } from '../stores/simulation.svelte';
  import { telemetry } from '../telemetry';
  import { baseChartOptions, eraChartColors, totalDayToTime } from './chart-utils';
  import { macroLabel, macroFormat, formatMacroValue, type MacroId } from './macro-meta';

  interface Props {
    macroId: MacroId;
    label?: string;
    compact?: boolean;
    hero?: boolean;
    maxDays?: number;
    onExpand?: (id: MacroId) => void;
  }
  let { macroId, label, compact = false, hero = false, maxDays = Infinity, onExpand }: Props = $props();

  let containerEl: HTMLDivElement | undefined = $state();
  let chart: IChartApi | null = null;
  let series: ISeriesApi<'Line'> | null = null;
  let lastSeenTime = $state<number | null>(null);

  let series_data = $derived(sim.macro[macroId]);
  let last = $derived(series_data?.last ?? 0);
  let fmt = $derived(macroFormat(macroId));

  function priceFormatter(p: number): string {
    return fmt === 'percent' ? `${(p * 100).toFixed(1)}%` : formatMacroValue(macroId, p);
  }

  function mount() {
    if (!containerEl) return;
    const colors = eraChartColors(sim.era?.name);
    chart = createChart(containerEl, {
      ...baseChartOptions(colors),
      localization: { priceFormatter },
      timeScale: { ...baseChartOptions(colors).timeScale, rightOffset: 2 },
    });
    series = chart.addSeries(LineSeries, {
      color: colors.lineColor,
      lineWidth: hero ? 3 : 2,
      lineType: LineType.Simple,
      priceLineVisible: false,
      lastValueVisible: hero || !compact,
    });
    hydrate();
  }

  function hydrate() {
    if (!series) return;
    const s = sim.macro[macroId];
    if (!s || !s.history.length) return;
    const lastDay = s.history[s.history.length - 1].time;
    const cutoff = maxDays === Infinity ? -Infinity : lastDay - maxDays;
    const data = s.history
      .filter((p) => p.time >= cutoff)
      .map((p) => ({ time: totalDayToTime(p.time), value: p.value }));
    if (data.length) {
      series.setData(data);
      chart?.timeScale().fitContent();
      lastSeenTime = s.history[s.history.length - 1].time;
    }
  }

  // Re-hydrate when the time-scale window changes (hero toggle).
  $effect(() => {
    void maxDays;
    if (series) hydrate();
  });

  $effect(() => {
    const s = sim.macro[macroId];
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

  // Re-theme on era change.
  $effect(() => {
    if (!chart || !series) return;
    const colors = eraChartColors(sim.era?.name);
    chart.applyOptions({
      layout: { background: { color: colors.background }, textColor: colors.text },
      grid: { vertLines: { color: colors.grid }, horzLines: { color: colors.grid } },
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
    if (onExpand) onExpand(macroId);
  }
  function handleHover() {
    telemetry.hover(`macro:${macroId}`, { id: macroId });
  }
</script>

{#snippet head()}
  <div class="head">
    <span class="ticker">{label ?? macroLabel(macroId)}</span>
    <span class="value num">{priceFormatter(last)}</span>
  </div>
{/snippet}

{#if onExpand}
  <button
    type="button"
    class="macro-chart clickable"
    class:compact
    onclick={handleClick}
    onmouseenter={handleHover}
  >
    {@render head()}
    <div bind:this={containerEl} class="chart"></div>
  </button>
{:else}
  <div class="macro-chart" class:compact class:hero onmouseenter={handleHover} role="presentation">
    {@render head()}
    <div bind:this={containerEl} class="chart"></div>
  </div>
{/if}

<style>
  .macro-chart {
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
  button.macro-chart { cursor: pointer; }
  button.macro-chart:hover { border-color: var(--accent-primary); }

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
  .value {
    font-family: var(--font-data);
    color: var(--fg-primary);
    font-size: 0.85rem;
    font-variant-numeric: tabular-nums;
  }

  .chart { flex: 1; min-height: 5.5rem; }
  .compact .chart { min-height: 4.5rem; }
  .hero { min-height: 38vh; }
  .hero .chart { min-height: 34vh; }
  .hero .ticker { font-size: var(--fs-sm); }
  .hero .value { font-size: 1.3rem; }
</style>
