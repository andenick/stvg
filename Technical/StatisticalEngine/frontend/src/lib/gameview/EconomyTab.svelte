<script lang="ts">
  /*
   * Economy tab (P2) — the default day-trading view.
   *
   *   • Hero chart (≥38vh): either the bank's own evolution (CapitalChart) or a
   *     promoted market. Time-scale toggle 1Y/5Y/10Y/20Y/MAX slices the ring
   *     buffers client-side.
   *   • Thumbnail strip: the 6 markets (PREFERRED_MARKET_ORDER) + a "Bank"
   *     thumbnail; click promotes one into the hero spot.
   *   • Market small-multiples grid (the familiar 6-up) with click-to-expand.
   *   • NewsTicker along the bottom.
   *   • Collapsible right rail: Competitor / Leaderboard / Political / Regulatory
   *     (each self-gates on data + year, so phase-1 simply shows nothing).
   *
   * MarketExpanded modal is hosted here (Economy owns market detail). DealBoard
   * lives as a persistent LEFT rail in TabShell, not here.
   */

  import CapitalChart from '../charts/CapitalChart.svelte';
  import MarketChart from '../charts/MarketChart.svelte';
  import MacroChart from '../charts/MacroChart.svelte';
  import MarketExpanded from '../charts/MarketExpanded.svelte';
  import MacroExpanded from '../charts/MacroExpanded.svelte';
  import NewsTicker from './NewsTicker.svelte';
  import TradeControls from './TradeControls.svelte';
  import { fmtMoney } from '../util/money';
  import CompetitorPanel from '../sidebar/CompetitorPanel.svelte';
  import LeaderboardPanel from '../sidebar/LeaderboardPanel.svelte';
  import PoliticalPanel from '../sidebar/PoliticalPanel.svelte';
  import RegulatoryPanel from '../sidebar/RegulatoryPanel.svelte';
  import { marketLabel, PREFERRED_MARKET_ORDER } from '../charts/markets-meta';
  import {
    macroLabel, macroHeroId, heroMacroId, formatMacroValue,
    MACRO_STRIP_IDS, MACRO_CHIP_IDS, type MacroId,
  } from '../charts/macro-meta';
  import { sim } from '../stores/simulation.svelte';
  import { ui, TIMESCALES, TIMESCALE_DAYS } from '../stores/ui.svelte';
  import { formatBigDollar, formatMarketPrice } from '../charts/chart-utils';

  // Markets present, ordered by the canonical list then any extras the server emits.
  let marketIds = $derived.by(() => {
    const have = Object.keys(sim.markets);
    const inOrder = PREFERRED_MARKET_ORDER.filter((id) => have.includes(id));
    const extra = have.filter((id) => !PREFERRED_MARKET_ORDER.includes(id));
    return [...inOrder, ...extra].slice(0, 6);
  });

  // Macro strip = the always-on series (GDP, Unemployment, Fed Funds), shown
  // once the engine has streamed at least one point for them.
  let macroStripIds = $derived(
    MACRO_STRIP_IDS.filter((id) => (sim.macro[id]?.history.length ?? 0) > 0),
  );
  // The chip-row series for the expanded view: chips present-but-unpromoted
  // (Inflation, 10Y, GDP growth) plus the coming-soon Economy P&L placeholder.
  const macroChipIds = MACRO_CHIP_IDS as MacroId[];

  let expandedMarket = $state<string | null>(null);
  let expandedMacro = $state<MacroId | null>(null);

  let heroDays = $derived(TIMESCALE_DAYS[ui.heroTimescale]);
  let heroMarket = $derived(ui.selectedHeroMarket);
  // Hero can hold a market id, a namespaced macro id (`macro:GDP`), or null (Bank).
  let heroMacro = $derived(heroMacroId(heroMarket));
  let heroIsMarket = $derived(heroMarket != null && heroMacro == null);
  let heroMarketLabel = $derived(heroIsMarket ? marketLabel(heroMarket!) : '');
  let heroMacroLabel = $derived(heroMacro ? macroLabel(heroMacro) : '');

  // Thumbnail Δ direction for red/green tinting.
  function dir(id: string): 'up' | 'down' | 'flat' {
    const r = sim.markets[id]?.lastReturn ?? 0;
    return r > 0 ? 'up' : r < 0 ? 'down' : 'flat';
  }
  let capDir = $derived(sim.capitalDelta > 0 ? 'up' : sim.capitalDelta < 0 ? 'down' : 'flat');

  // Promote a macro series into the hero spot (namespaced id) + telemetry via ui.
  function promoteMacro(id: MacroId) { ui.setHeroMarket(macroHeroId(id)); }
</script>

<div class="economy">
  <main class="econ-main">
    <!-- Hero chart + controls -->
    <section class="hero" aria-label="Hero chart">
      <div class="hero-controls">
        <div class="hero-title">
          {#if heroMacro}
            {heroMacroLabel}
          {:else if heroIsMarket}
            {heroMarketLabel}
          {:else}
            Bank Evolution
          {/if}
        </div>
        <div class="scale-toggle" role="group" aria-label="Time scale">
          {#each TIMESCALES as ts}
            <button class="scale" class:active={ui.heroTimescale === ts} onclick={() => ui.setTimescale(ts)}>{ts}</button>
          {/each}
        </div>
      </div>

      <div class="hero-chart">
        {#if heroMacro}
          <MacroChart macroId={heroMacro} label={heroMacroLabel} hero maxDays={heroDays} />
        {:else if heroIsMarket}
          <MarketChart marketId={heroMarket!} label={heroMarketLabel} hero maxDays={heroDays} />
        {:else}
          <CapitalChart maxDays={heroDays} />
        {/if}
      </div>

      <!-- P7: BUY/SELL on the hero chart when a MARKET is promoted (not a macro
           series, not the Bank capital chart). Visually secondary to the Loan Book. -->
      {#if heroIsMarket}
        <TradeControls marketId={heroMarket!} />
      {/if}

      <!-- Thumbnail strip: click to promote into the hero spot -->
      <div class="thumbs" role="group" aria-label="Promote a series">
        <button
          class="thumb thumb-{capDir}"
          class:active={heroMarket === null}
          onclick={() => ui.setHeroMarket(null)}
          title="Show the bank's own evolution"
        >
          <span class="thumb-tk">BANK</span>
          <span class="thumb-px num">{formatBigDollar(sim.bankCapital)}</span>
        </button>
        {#each marketIds as id (id)}
          {@const s = sim.markets[id]}
          <button
            class="thumb thumb-{dir(id)}"
            class:active={heroMarket === id}
            onclick={() => ui.setHeroMarket(id)}
            title="Promote {marketLabel(id)}"
          >
            <span class="thumb-tk">{marketLabel(id)}</span>
            <span class="thumb-px num">{formatMarketPrice(s?.lastPrice ?? 0)}</span>
            <span class="thumb-ret num" class:up={dir(id) === 'up'} class:down={dir(id) === 'down'}>
              {(s?.lastReturn ?? 0) >= 0 ? '+' : ''}{((s?.lastReturn ?? 0) * 100).toFixed(2)}%
            </span>
            {#if sim.positionFor(id)}
              {@const pos = sim.positionFor(id)!}
              <span class="thumb-pos num" class:up={(pos.pnl ?? 0) > 0} class:down={(pos.pnl ?? 0) < 0}
                    title="You hold {fmtMoney(pos.value ?? 0)} · P&L {(pos.pnl ?? 0) >= 0 ? '+' : '−'}{fmtMoney(Math.abs(pos.pnl ?? 0))}">
                ● {fmtMoney(pos.value ?? 0)}
              </span>
            {/if}
          </button>
        {/each}
      </div>
    </section>

    <!-- The living economy (P3): macro strip — GDP, Unemployment, Fed Funds always;
         credit spread market-adjacent; all promotable to the hero spot. -->
    {#if macroStripIds.length}
      <section class="macro" aria-label="The economy">
        <div class="macro-head">
          <span class="macro-title">THE ECONOMY</span>
          <span class="macro-sub">click a series to read what it means · promote to the big chart</span>
        </div>
        <div class="macro-strip">
          {#each macroStripIds as id (id)}
            {@const md = sim.macroDir(id)}
            {@const heroSel = heroMacro === id}
            <div class="macro-cell macro-{md}" class:hero-sel={heroSel}>
              <MacroChart macroId={id} compact onExpand={(m) => expandedMacro = m} />
              <button class="promote" title="Promote {macroLabel(id)} to the big chart"
                      class:on={heroSel} onclick={() => promoteMacro(id)}>★</button>
            </div>
          {/each}
        </div>
      </section>
    {/if}

    <!-- The familiar 6-up small-multiples grid -->
    <section class="market-grid" aria-label="Markets">
      {#each marketIds as id (id)}
        <MarketChart marketId={id} label={marketLabel(id)} compact onExpand={(m) => expandedMarket = m} />
      {/each}
    </section>

    <!-- Credit spread: a market-adjacent chip (the market's fear of default). -->
    {#if (sim.macro['SPREAD']?.history.length ?? 0) > 0}
      {@const sd = sim.macroDir('SPREAD')}
      <div class="market-chips" aria-label="Credit market">
        <button
          class="market-chip chip-{sd}"
          class:active={heroMacro === 'SPREAD'}
          onclick={() => expandedMacro = 'SPREAD'}
          title="Credit spread — the market's fear of default"
        >
          <span class="chip-tk">CREDIT SPREAD</span>
          <span class="chip-val num">{formatMacroValue('SPREAD', sim.macro['SPREAD']?.last ?? 0)}</span>
          <span class="chip-glyph" aria-hidden="true">{sd === 'up' ? '▲' : sd === 'down' ? '▼' : '·'}</span>
        </button>
      </div>
    {/if}

    <NewsTicker />
  </main>

  <!-- Collapsible right rail (panels self-gate by data + year) -->
  <aside class="right-rail" class:collapsed={ui.rightRailCollapsed} aria-label="Market intelligence">
    <button class="rail-toggle" onclick={() => ui.toggleRightRail()} title={ui.rightRailCollapsed ? 'Show panels' : 'Hide panels'}>
      {ui.rightRailCollapsed ? '‹' : '›'}
    </button>
    {#if !ui.rightRailCollapsed}
      <div class="rail-body">
        <LeaderboardPanel />
        <CompetitorPanel />
        <PoliticalPanel />
        <RegulatoryPanel />
      </div>
    {/if}
  </aside>
</div>

{#if expandedMarket}
  <MarketExpanded
    marketId={expandedMarket}
    ids={marketIds}
    onClose={() => expandedMarket = null}
    onSelect={(m) => expandedMarket = m}
  />
{/if}

{#if expandedMacro}
  <MacroExpanded
    macroId={expandedMacro}
    ids={macroStripIds}
    extraIds={macroChipIds}
    onClose={() => expandedMacro = null}
    onSelect={(m) => expandedMacro = m}
  />
{/if}

<style>
  .economy {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    gap: 1rem;
    padding: 1rem 1.25rem;
    align-items: start;
  }
  .econ-main { min-width: 0; display: flex; flex-direction: column; gap: 1.1rem; }

  .hero { display: flex; flex-direction: column; gap: 0.6rem; }
  .hero-controls { display: flex; align-items: center; justify-content: space-between; gap: 1rem; flex-wrap: wrap; }
  .hero-title {
    font-family: var(--font-display);
    color: var(--accent-primary);
    font-size: 1.1rem;
    letter-spacing: 0.04em;
  }
  .scale-toggle { display: inline-flex; gap: 0.2rem; }
  .scale {
    padding: 0.25rem 0.55rem;
    font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.08em;
    color: var(--fg-secondary); background: transparent; border: 1px solid var(--border);
  }
  .scale.active { color: var(--accent-primary); border-color: var(--accent-primary); background: rgba(196, 163, 90, 0.08); }
  .scale:hover:not(.active) { color: var(--fg-primary); border-color: var(--fg-secondary); }

  .hero-chart { min-height: 38vh; display: flex; }
  .hero-chart :global(.capital-chart-wrap) { flex: 1; min-height: 38vh; }
  .hero-chart :global(.market-chart) { flex: 1; }

  .thumbs {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(8rem, 1fr));
    gap: 0.5rem;
  }
  .thumb {
    display: flex; flex-direction: column; align-items: flex-start; gap: 0.1rem;
    padding: 0.4rem 0.55rem;
    background: var(--bg-card); border: 1px solid var(--border); border-left: 3px solid var(--border);
    border-radius: 2px; cursor: pointer; text-align: left; min-width: 0;
  }
  .thumb:hover { border-color: var(--accent-primary); }
  .thumb.active { border-color: var(--accent-primary); background: rgba(196, 163, 90, 0.06); }
  .thumb-up   { border-left-color: var(--accent-success); }
  .thumb-down { border-left-color: var(--accent-danger); }
  .thumb-flat { border-left-color: var(--fg-muted); }
  .thumb-tk { font-family: var(--font-chrome); font-size: 0.58rem; letter-spacing: 0.16em; text-transform: uppercase; color: var(--fg-secondary); }
  .thumb-px { font-family: var(--font-data); font-size: 0.82rem; color: var(--fg-primary); font-variant-numeric: tabular-nums; }
  .thumb-ret { font-family: var(--font-data); font-size: 0.66rem; }
  .thumb-ret.up { color: var(--accent-success); }
  .thumb-ret.down { color: var(--accent-danger); }
  /* P7 position pill — shown on thumbnails where the CEO holds a position */
  .thumb-pos { font-family: var(--font-data); font-size: 0.6rem; color: var(--accent-primary); font-variant-numeric: tabular-nums; }
  .thumb-pos.up { color: var(--accent-success); }
  .thumb-pos.down { color: var(--accent-danger); }

  .market-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    grid-auto-rows: 11rem;
    gap: 0.75rem;
  }

  /* ── The living economy (macro) ──────────────────────────────────────── */
  .macro { display: flex; flex-direction: column; gap: 0.55rem; }
  .macro-head { display: flex; align-items: baseline; gap: 0.75rem; flex-wrap: wrap; }
  .macro-title {
    font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.24em;
    color: var(--accent-primary); text-transform: uppercase;
  }
  .macro-sub { font-family: var(--font-body); font-size: 0.72rem; color: var(--fg-muted); }
  .macro-strip {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(13rem, 1fr));
    grid-auto-rows: 10rem;
    gap: 0.75rem;
  }
  .macro-cell { position: relative; display: flex; min-width: 0; }
  .macro-cell :global(.macro-chart) { flex: 1; border-left: 3px solid var(--border); }
  .macro-up   :global(.macro-chart) { border-left-color: var(--accent-success); }
  .macro-down :global(.macro-chart) { border-left-color: var(--accent-danger); }
  .macro-flat :global(.macro-chart) { border-left-color: var(--fg-muted); }
  .macro-cell.hero-sel :global(.macro-chart) { border-color: var(--accent-primary); }
  .promote {
    position: absolute; top: 0.35rem; right: 0.4rem;
    width: 1.5rem; height: 1.5rem; line-height: 1;
    background: var(--bg-elevated); border: 1px solid var(--border); border-radius: 50%;
    color: var(--fg-muted); cursor: pointer; font-size: 0.8rem; z-index: 2;
  }
  .promote:hover { color: var(--accent-primary); border-color: var(--accent-primary); }
  .promote.on { color: var(--accent-primary); border-color: var(--accent-primary); }

  /* Credit-spread market-adjacent chip */
  .market-chips { display: flex; gap: 0.5rem; flex-wrap: wrap; }
  .market-chip {
    display: inline-flex; align-items: center; gap: 0.5rem;
    padding: 0.35rem 0.7rem;
    background: var(--bg-card); border: 1px solid var(--border);
    border-left: 3px solid var(--border); border-radius: 2px; cursor: pointer;
  }
  .market-chip:hover { border-color: var(--accent-primary); }
  .market-chip.active { border-color: var(--accent-primary); background: rgba(196, 163, 90, 0.06); }
  .market-chip.chip-up   { border-left-color: var(--accent-success); }
  .market-chip.chip-down { border-left-color: var(--accent-danger); }
  .chip-tk { font-family: var(--font-chrome); font-size: 0.58rem; letter-spacing: 0.16em; text-transform: uppercase; color: var(--fg-secondary); }
  .chip-val { font-family: var(--font-data); font-size: 0.82rem; color: var(--fg-primary); font-variant-numeric: tabular-nums; }
  .chip-glyph { font-size: 0.7rem; color: var(--fg-muted); }
  .chip-up .chip-glyph   { color: var(--accent-success); }
  .chip-down .chip-glyph { color: var(--accent-danger); }

  .right-rail { position: relative; display: flex; align-items: flex-start; }
  .right-rail.collapsed { width: 1.5rem; }
  .rail-toggle {
    width: 1.3rem; height: 2.2rem;
    background: var(--bg-card); border: 1px solid var(--border); border-radius: 2px;
    color: var(--accent-primary); cursor: pointer; font-size: 1rem; line-height: 1;
    flex: 0 0 auto;
  }
  .rail-body {
    display: flex; flex-direction: column; gap: 0.75rem;
    width: 16rem;
    margin-left: 0.4rem;
  }

  @media (max-width: 1100px) {
    .economy { grid-template-columns: 1fr; }
    .right-rail { order: 2; }
    .rail-body { width: 100%; }
    .market-grid { grid-template-columns: repeat(2, 1fr); }
  }
  @media (max-width: 640px) {
    .market-grid { grid-template-columns: 1fr; }
  }
</style>
