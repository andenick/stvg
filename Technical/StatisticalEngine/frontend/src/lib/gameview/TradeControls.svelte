<script lang="ts">
  /*
   * STAR_02 P7 — BUY / SELL on the hero chart for a promoted MARKET (not a macro
   * series, not the Bank capital chart). The CEO's light "trade yourself" book.
   *
   * Market orders only, fixed $ notional ticket. Trading is available from 1945
   * (the owner's "could also be tradey" hedge) but visually SECONDARY to the Loan
   * Book — it's a compact control strip, not a desk. Buy commits idle personal
   * cash; Sell closes part of an existing long (no shorts). The bank's capital is
   * never touched (off-balance-sheet personal account — see PersonalBook.h).
   */

  import { sim } from '../stores/simulation.svelte';
  import { api, ApiError } from '../api/rest';
  import { toasts } from '../stores/toasts.svelte';
  import { telemetry } from '../telemetry';
  import { fmtMoney } from '../util/money';
  import { marketLabel } from '../charts/markets-meta';

  let { marketId }: { marketId: string } = $props();

  // A small fixed ticket so the affordance stays "light" — 2% of the personal
  // account's seed, floored, so it's always a sensible bite at the apple.
  const TICKET_FRACTION = 0.25;   // a quarter of available personal cash per click
  let cash = $derived(sim.tradeBook?.cash ?? 0);
  let ticket = $derived(Math.max(1, Math.round(cash * TICKET_FRACTION)));
  let pos = $derived(sim.positionFor(marketId));
  let holding = $derived(pos?.value ?? 0);
  let submitting = $state(false);

  async function place(side: 'buy' | 'sell') {
    if (submitting || !sim.gameId) return;
    // Buy spends from idle cash; sell liquidates a quarter of the holding.
    const amount = side === 'buy' ? ticket : Math.max(1, Math.round(holding * TICKET_FRACTION));
    submitting = true;
    try {
      const r = await api.trade(sim.gameId, marketId, side, amount);
      sim.tradeBook = r.tradeBook;
      sim.personalBookValue = r.tradeBook.value;
      sim.personalBookPnl = r.tradeBook.pnl;
      const after = sim.positionFor(marketId)?.qty ?? 0;
      telemetry.log(side === 'buy' ? 'trade_buy' : 'trade_sell', {
        marketId, amount, positionAfter: after,
      });
      if (side === 'sell') toasts.success(`Sold ${fmtMoney(amount)} of ${marketLabel(marketId)} (P&L ${r.realizedPnl >= 0 ? '+' : '−'}${fmtMoney(Math.abs(r.realizedPnl))})`);
      else toasts.success(`Bought ${fmtMoney(amount)} of ${marketLabel(marketId)}`);
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      telemetry.log('trade_rejected', { marketId, reason: msg });
      toasts.error(`Trade rejected: ${msg}`);
    } finally {
      submitting = false;
    }
  }
</script>

<div class="trade" aria-label="Personal trading controls">
  <span class="trade-label" title="Your personal account — separate from the bank">MY BOOK</span>
  <button class="t-buy" disabled={submitting || cash <= 0} onclick={() => place('buy')} title="Buy {fmtMoney(ticket)} on your personal account">
    BUY {fmtMoney(ticket)}
  </button>
  <button class="t-sell" disabled={submitting || holding <= 0} onclick={() => place('sell')} title="Sell part of your position">
    SELL
  </button>
  {#if pos}
    <span class="t-pos num" class:up={(pos.pnl ?? 0) > 0} class:down={(pos.pnl ?? 0) < 0}>
      {fmtMoney(holding)} held · {(pos.pnl ?? 0) >= 0 ? '+' : '−'}{fmtMoney(Math.abs(pos.pnl ?? 0))}
    </span>
  {:else}
    <span class="t-cash num">{fmtMoney(cash)} cash</span>
  {/if}
</div>

<style>
  .trade {
    display: flex; align-items: center; gap: 0.5rem; flex-wrap: wrap;
    padding: 0.35rem 0.55rem;
    background: var(--bg-card); border: 1px solid var(--border-soft); border-radius: 2px;
  }
  .trade-label { font-family: var(--font-chrome); font-size: 0.56rem; letter-spacing: 0.16em;
    text-transform: uppercase; color: var(--fg-muted); }
  .t-buy, .t-sell {
    font-family: var(--font-chrome); font-size: 0.66rem; letter-spacing: 0.06em;
    padding: 0.3rem 0.6rem; border-radius: 2px; cursor: pointer; font-weight: 600;
  }
  .t-buy { background: var(--accent-success); color: var(--bg-base); border: 1px solid var(--accent-success); }
  .t-buy:hover:not(:disabled) { filter: brightness(1.1); }
  .t-sell { background: transparent; color: var(--accent-danger); border: 1px solid var(--accent-danger); }
  .t-sell:hover:not(:disabled) { background: rgba(180, 60, 60, 0.08); }
  .t-buy:disabled, .t-sell:disabled { opacity: 0.4; cursor: not-allowed; }
  .t-pos, .t-cash { font-family: var(--font-data); font-size: 0.72rem; color: var(--fg-secondary); font-variant-numeric: tabular-nums; }
  .t-pos.up { color: var(--accent-success); }
  .t-pos.down { color: var(--accent-danger); }
</style>
