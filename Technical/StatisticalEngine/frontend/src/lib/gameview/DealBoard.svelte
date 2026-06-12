<script lang="ts">
  /*
   * Left-rail "Loan Book" — the firms you could lend to right now. Deals trickle
   * in from the engine through the quarter. Click a deal to expand its detail
   * (full pitch, plain-language risk, the sourcing banker's voice) and Invest or
   * Pass. New prospects also fire a toast so they "pop up" as the sim flows.
   */

  import { sim } from '../stores/simulation.svelte';
  import { toasts } from '../stores/toasts.svelte';
  import { ui } from '../stores/ui.svelte';
  import { api, ApiError } from '../api/rest';
  import { telemetry } from '../telemetry';
  import { dwell } from '../actions/dwell';
  import { pitchForDeal } from '../voices';
  import { cardForDealPitch } from '../characters/triggers';
  import { fmtMoney } from '../util/money';
  import type { Deal, DealRisk } from '../types/server';

  const LINE_LABEL: Record<string, string> = {
    commercial_lending: 'Commercial', mortgage_lending: 'Mortgage', credit_cards: 'Cards',
    trading: 'Trading', derivatives: 'Derivatives', investment_banking: 'IB',
    restructuring: 'Restructuring', private_equity: 'PE', securitization: 'Securitization',
    asset_management: 'Asset Mgmt', wealth_management: 'Wealth', trust_custody: 'Trust',
    international: 'International', venture_capital: 'VC', fintech: 'Fintech', crypto_custody: 'Crypto',
  };
  function lineLabel(line?: string): string {
    if (!line) return 'Lending';
    return LINE_LABEL[line] ?? line.replace(/_/g, ' ');
  }
  function amountOf(d: Deal): number { return d.investmentRequired ?? d.size ?? 0; }
  function riskObj(d: Deal): DealRisk | null { return d.risk && typeof d.risk === 'object' ? (d.risk as DealRisk) : null; }
  function successPct(d: Deal): number | null {
    const r = riskObj(d);
    return r?.successProbability != null ? Math.round(r.successProbability * 100) : null;
  }
  function riskLevel(d: Deal): number {
    const r = riskObj(d);
    if (r?.riskLevel != null) return r.riskLevel;
    if (typeof d.risk === 'number') return d.risk;
    return 1;
  }
  function durationQ(d: Deal): number | null {
    const r = riskObj(d);
    return r?.duration != null ? r.duration : null;
  }
  // Expected annual return % from the deal's return range midpoint (P7 terms).
  // Engine streams returnRange as PERCENT pair [min, max]; midpoint is the
  // central expectation. Falls back to null when the range isn't present.
  function expectedReturnPct(d: Deal): number | null {
    const r = riskObj(d);
    const rng = r?.returnRange;
    if (Array.isArray(rng) && rng.length >= 2) return (rng[0] + rng[1]) / 2;
    return null;
  }
  // Plain-language soundness band from the 1-10 risk level (P7 terms).
  function soundnessWord(lvl: number): string {
    if (lvl <= 3) return 'Sound';
    if (lvl <= 6) return 'Speculative';
    return 'Wildcat';
  }
  // Expected interest the book picks up per quarter from this stake — the NIM/
  // book impact line. expected $/qtr ≈ stake × (annual return % / 100) / qtrs.
  // Honest: computed only from fields that already stream; null otherwise.
  function bookImpactPerQtr(d: Deal): number | null {
    const er = expectedReturnPct(d);
    const dur = durationQ(d);
    if (er == null || dur == null || dur <= 0) return null;
    return (amountOf(d) * (er / 100)) / dur;
  }
  const fmt = (v: number): string => fmtMoney(v);
  function riskWord(lvl: number): string {
    if (lvl <= 2) return 'Low risk';
    if (lvl <= 4) return 'Moderate';
    if (lvl <= 6) return 'Spicy';
    if (lvl <= 8) return 'High risk';
    return 'Wild gamble';
  }

  let passed = $state(new Set<string>());
  let selectedId = $state<string | null>(null);
  let submitting = $state(false);

  let deals = $derived((sim.availableDeals ?? []).filter((d) => !passed.has(d.id)));
  let selected = $derived(deals.find((d) => d.id === selectedId) ?? null);

  // P7: the Book strip — accepted count + cumulative realized P&L (green/red).
  let book = $derived(sim.loanBookStats);
  let bookResolved = $derived(book ? book.paidOff + book.defaulted + book.windfalls : 0);

  // Deal arrivals bump the rail badge counter instead of flooding the toast
  // stack (0.5) — one toast per deal buried everything else. Telemetry's
  // deal_arrived log is preserved. Seed on first run so the initial backlog
  // doesn't inflate the badge.
  //
  // P1: also track each deal's first-seen wall-clock so we can compute
  // `deal_expired {id, visibleMs}` when the engine drops a prospect the player
  // never acted on (distinguishes "saw it and let it go" from "never offered").
  let seen = new Set<string>();
  let firstSeenAt = new Map<string, number>();
  let resolvedByPlayer = new Set<string>();   // invested or explicitly passed
  let prevIds = new Set<string>();
  let primed = false;
  $effect(() => {
    const live = sim.availableDeals ?? [];
    const ids = live.map((d) => d.id);
    const liveSet = new Set(ids);

    if (!primed) {
      const now = Date.now();
      for (const id of ids) { seen.add(id); firstSeenAt.set(id, now); }
      prevIds = liveSet;
      primed = true;
      return;
    }

    // Arrivals.
    for (const d of live) {
      if (!seen.has(d.id)) {
        seen.add(d.id);
        firstSeenAt.set(d.id, Date.now());
        ui.bumpDealBadge();
        telemetry.log('deal_arrived', { id: d.id, title: d.title, amount: amountOf(d) });
      }
    }

    // Expiries: present last tick, gone now, and the player never resolved it.
    for (const id of prevIds) {
      if (!liveSet.has(id)) {
        if (!resolvedByPlayer.has(id)) {
          const since = firstSeenAt.get(id);
          const visibleMs = since != null ? Date.now() - since : 0;
          telemetry.log('deal_expired', { id, visibleMs });
        }
        firstSeenAt.delete(id);
        resolvedByPlayer.delete(id);
      }
    }
    prevIds = liveSet;
  });

  // Count the player's own staff in the deal's relevant division. The engine
  // exposes per-division employee COUNTS (no per-employee identity), so we use
  // this only to decide the honest "Your trading desk's view" prefix — we NEVER
  // invent a fake named employee claiming to be hired staff (STAR_02 §4.2).
  function staffInLine(line?: string): number {
    if (!line) return 0;
    const divs = sim.bank?.divisions ?? [];
    const d = divs.find((x) => x.type === line);
    return d ? (d.employees ?? 0) : 0;
  }

  function open(d: Deal) {
    selectedId = selectedId === d.id ? null : d.id;
    // Engaging with the loan book clears the unseen-prospect badge.
    ui.clearDealBadge();
    if (selectedId) {
      telemetry.log('deal_opened', { id: d.id, title: d.title, risk: riskLevel(d) });
      // Deal-pitch portrait bark (P4 §4.1): the sourcing banker pitches in their
      // own credibility-weighted voice. Queue dedups + rate-limits.
      cardForDealPitch(
        { id: d.id, clientName: d.clientName, title: d.title, requiredLine: d.requiredLine },
        sim.year, sim.gameId, successPct(d), riskLevel(d),
      );
    }
  }

  async function invest(d: Deal) {
    if (submitting || !sim.gameId) return;
    submitting = true;
    const amount = amountOf(d);
    resolvedByPlayer.add(d.id);   // suppress a spurious deal_expired when it leaves the list
    telemetry.log('deal_invest_click', { id: d.id, amount, risk: riskLevel(d) });
    try {
      await api.acceptDeal(sim.gameId, d.id, amount);
      const view = await api.getState(sim.gameId);
      sim.applyPlayerView(view);
      toasts.success(`Invested ${fmt(amount)} in ${d.clientName ?? d.title}`);
      selectedId = null;
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      toasts.error(`Deal failed: ${msg}`);
    } finally {
      submitting = false;
    }
  }

  function pass(d: Deal) {
    passed.add(d.id);
    passed = new Set(passed);
    resolvedByPlayer.add(d.id);   // a pass is an explicit decision, not an expiry
    if (selectedId === d.id) selectedId = null;
    telemetry.log('deal_pass', { id: d.id, title: d.title });
  }
</script>

<aside class="deal-board" aria-label="Loan book — prospects">
  <header class="board-head">
    <span class="board-title">Loan Book</span>
    <span class="board-sub">
      Prospects · {deals.length}
      {#if ui.dealBadge > 0}<span class="deal-badge" title="New prospects since you last looked">+{ui.dealBadge}</span>{/if}
    </span>
  </header>

  <!-- P7: Book strip — running scoreboard of the loan book you've built. -->
  {#if book && (book.accepted > 0 || bookResolved > 0)}
    <div class="book-strip" title="Your loan book so far">
      <span class="bs-cell">
        <span class="bs-k">BOOK</span>
        <span class="bs-v num">{book.accepted}</span>
        <span class="bs-sub">accepted</span>
      </span>
      <span class="bs-cell">
        <span class="bs-k">REALIZED</span>
        <span class="bs-v num" class:gain={book.realizedPnl > 0} class:loss={book.realizedPnl < 0}>
          {book.realizedPnl >= 0 ? '+' : '−'}{fmt(Math.abs(book.realizedPnl))}
        </span>
        <span class="bs-sub">{book.paidOff + book.windfalls}W · {book.defaulted}L</span>
      </span>
    </div>
  {/if}

  {#if deals.length}
    <ul class="deal-list">
      {#each deals as d (d.id)}
        {@const pct = successPct(d)}
        {@const lvl = riskLevel(d)}
        {@const isOpen = selectedId === d.id}
        <li class="deal" class:fresh={!seen.has(d.id)} class:open={isOpen}>
          <button class="deal-summary" onclick={() => open(d)} aria-expanded={isOpen}>
            <div class="firm-row">
              <span class="firm">{d.clientName ?? d.title}</span>
              <span class="amt num">{fmt(amountOf(d))}</span>
            </div>
            <div class="deal-title">{d.title}</div>
            <div class="meta">
              <span class="line">{lineLabel(d.requiredLine)}</span>
              {#if pct != null}<span class="prob num">{pct}% ok</span>{/if}
              <span class="risk lvl-{Math.min(5, Math.max(1, lvl))}">{riskWord(lvl)}</span>
            </div>
          </button>

          {#if isOpen}
            {@const p = pitchForDeal(d.id, d.clientName ?? d.title, lineLabel(d.requiredLine), lvl, {
              division: d.requiredLine, year: sim.year, gameId: sim.gameId, successPct: pct,
            })}
            {@const staff = staffInLine(d.requiredLine)}
            {@const er = expectedReturnPct(d)}
            {@const impact = bookImpactPerQtr(d)}
            <div class="detail" use:dwell={{ key: `deal:${d.id}`, data: { title: d.title, risk: lvl } }}>
              {#if d.description}<p class="desc">{d.description}</p>{/if}
              <div class="pitch">
                <span class="pitch-by">
                  {#if staff > 0}Your {lineLabel(d.requiredLine)} desk's view — {/if}{p.banker.name} · {p.banker.archetype}
                </span>
                <p class="pitch-line">“{p.pitch}”</p>
              </div>

              <!-- P7 loan-card depth: terms + the book/NIM impact, computed only
                   from RiskReturnProfile fields that already stream (no fabrication). -->
              <dl class="terms">
                <div class="term"><dt>Amount</dt><dd class="num">{fmt(amountOf(d))}</dd></div>
                {#if er != null}<div class="term"><dt>Expected return</dt><dd class="num">{er >= 0 ? '+' : ''}{er.toFixed(1)}%</dd></div>{/if}
                {#if durationQ(d) != null}<div class="term"><dt>Duration</dt><dd class="num">{durationQ(d)} qtrs</dd></div>{/if}
                <div class="term"><dt>Risk</dt><dd class="sound sound-{soundnessWord(lvl).toLowerCase()}">{soundnessWord(lvl)}</dd></div>
              </dl>
              {#if impact != null}
                <p class="impact">Adds <b class="num">{fmt(impact)}</b>/qtr expected interest to the book.</p>
              {/if}

              <div class="facts">
                <span>Stake <b class="num">{fmt(amountOf(d))}</b></span>
                {#if pct != null}<span>Success <b class="num">{pct}%</b></span>{/if}
                {#if durationQ(d) != null}<span>~{durationQ(d)} qtrs</span>{/if}
              </div>
              <div class="actions">
                <button class="invest" disabled={submitting} onclick={() => invest(d)}>Invest {fmt(amountOf(d))}</button>
                <button class="pass" disabled={submitting} onclick={() => pass(d)}>Pass</button>
              </div>
            </div>
          {/if}
        </li>
      {/each}
    </ul>
  {:else}
    <p class="empty">Loan officers are out sourcing deals. Prospects will appear here as the quarter unfolds.</p>
  {/if}
</aside>

<style>
  .deal-board {
    background: var(--bg-card); border: 1px solid var(--border); border-radius: var(--card-radius);
    padding: 0.9rem 1rem 1rem; display: flex; flex-direction: column; gap: 0.75rem;
    align-self: flex-start; position: sticky; top: 1rem; max-height: calc(100vh - 6rem); overflow-y: auto;
  }
  .board-head { display: flex; align-items: baseline; justify-content: space-between;
    border-bottom: 1px solid var(--border-soft); padding-bottom: 0.5rem; }
  .board-title { font-family: var(--font-display); color: var(--accent-primary); font-size: 1.15rem; letter-spacing: 0.03em; }
  .board-sub { font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.18em; color: var(--fg-secondary); text-transform: uppercase; }
  .deal-badge { display: inline-block; margin-left: 0.4rem; background: var(--accent-primary); color: var(--bg-base);
    font-size: 0.6rem; letter-spacing: 0.04em; padding: 0.05rem 0.3rem; border-radius: 999px; font-weight: 600; }

  /* P7 Book strip — the running loan-book scoreboard */
  .book-strip { display: flex; gap: 0.6rem; background: var(--bg-elevated); border: 1px solid var(--border-soft);
    border-radius: 2px; padding: 0.4rem 0.55rem; }
  .bs-cell { display: flex; flex-direction: column; gap: 0.05rem; flex: 1; min-width: 0; }
  .bs-k { font-family: var(--font-chrome); font-size: 0.54rem; letter-spacing: 0.16em; text-transform: uppercase; color: var(--fg-muted); }
  .bs-v { font-family: var(--font-data); font-size: 0.95rem; color: var(--fg-primary); font-variant-numeric: tabular-nums; }
  .bs-v.gain { color: var(--accent-success); }
  .bs-v.loss { color: var(--accent-danger); }
  .bs-sub { font-family: var(--font-chrome); font-size: 0.52rem; letter-spacing: 0.06em; color: var(--fg-muted); }

  .deal-list { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: 0.55rem; }
  .deal { background: var(--bg-elevated); border: 1px solid var(--border-soft); border-left: 3px solid var(--accent-secondary);
    border-radius: 2px; overflow: hidden; }
  .deal.open { border-left-color: var(--accent-primary); }
  .deal.fresh { border-left-color: var(--accent-primary); animation: dealIn 600ms var(--ease); }
  @keyframes dealIn { from { opacity: 0; transform: translateX(-10px); } to { opacity: 1; transform: translateX(0); } }

  .deal-summary { display: block; width: 100%; text-align: left; background: transparent; border: none;
    padding: 0.55rem 0.65rem; cursor: pointer; color: inherit; font: inherit; }
  .deal-summary:hover { background: rgba(135, 106, 44, 0.06); }
  .firm-row { display: flex; align-items: baseline; justify-content: space-between; gap: 0.5rem; }
  .firm { font-weight: 600; color: var(--fg-primary); font-size: 0.92rem; }
  .amt { font-family: var(--font-data); color: var(--fg-data); font-size: 0.85rem; font-variant-numeric: tabular-nums; }
  .deal-title { font-family: var(--font-chrome); font-size: 0.62rem; letter-spacing: 0.16em; text-transform: uppercase; color: var(--fg-secondary); margin-top: 0.1rem; }
  .meta { display: flex; align-items: center; gap: 0.4rem; flex-wrap: wrap; margin-top: 0.35rem; }
  .line { font-family: var(--font-chrome); font-size: 0.58rem; letter-spacing: 0.12em; text-transform: uppercase;
    color: var(--fg-primary); border: 1px solid var(--border); border-radius: 2px; padding: 0.08rem 0.35rem; }
  .prob { font-size: 0.7rem; color: var(--accent-success); }
  .risk { font-family: var(--font-chrome); font-size: 0.58rem; letter-spacing: 0.1em; padding: 0.08rem 0.35rem; border-radius: 2px; margin-left: auto; }
  .lvl-1, .lvl-2 { color: var(--accent-success); border: 1px solid var(--accent-success); }
  .lvl-3 { color: var(--accent-warning); border: 1px solid var(--accent-warning); }
  .lvl-4, .lvl-5 { color: var(--accent-danger); border: 1px solid var(--accent-danger); }

  .detail { padding: 0 0.65rem 0.7rem; border-top: 1px solid var(--border-soft); }
  .desc { margin: 0.55rem 0 0.5rem; font-size: 0.82rem; line-height: 1.4; color: var(--fg-secondary); }
  .pitch { background: var(--bg-card); border-left: 2px solid var(--accent-secondary); padding: 0.4rem 0.55rem; border-radius: 2px; margin-bottom: 0.5rem; }
  .pitch-by { font-family: var(--font-chrome); font-size: 0.58rem; letter-spacing: 0.12em; text-transform: uppercase; color: var(--accent-primary); }
  .pitch-line { margin: 0.15rem 0 0; font-style: italic; font-size: 0.84rem; line-height: 1.4; color: var(--fg-primary); }
  /* P7 loan-card terms grid + book impact */
  .terms { display: grid; grid-template-columns: 1fr 1fr; gap: 0.3rem 0.75rem; margin: 0 0 0.5rem; }
  .term { display: flex; align-items: baseline; justify-content: space-between; gap: 0.4rem; }
  .term dt { font-family: var(--font-chrome); font-size: 0.56rem; letter-spacing: 0.1em; text-transform: uppercase; color: var(--fg-muted); margin: 0; }
  .term dd { margin: 0; font-size: 0.8rem; color: var(--fg-primary); }
  .term dd.num { font-family: var(--font-data); font-variant-numeric: tabular-nums; }
  .sound { font-family: var(--font-chrome); font-size: 0.6rem; letter-spacing: 0.08em; padding: 0.05rem 0.35rem; border-radius: 2px; }
  .sound-sound { color: var(--accent-success); border: 1px solid var(--accent-success); }
  .sound-speculative { color: var(--accent-warning); border: 1px solid var(--accent-warning); }
  .sound-wildcat { color: var(--accent-danger); border: 1px solid var(--accent-danger); }
  .impact { margin: 0 0 0.55rem; font-size: 0.76rem; color: var(--fg-secondary); }
  .impact b { color: var(--accent-success); font-family: var(--font-data); }

  .facts { display: flex; flex-wrap: wrap; gap: 0.75rem; font-size: 0.74rem; color: var(--fg-secondary); margin-bottom: 0.55rem; }
  .facts b { color: var(--fg-primary); font-family: var(--font-data); }
  .actions { display: flex; gap: 0.5rem; }
  .invest { flex: 1; background: var(--accent-primary); color: var(--bg-base); border: 1px solid var(--accent-primary);
    font-family: var(--font-chrome); font-size: 0.7rem; letter-spacing: 0.08em; padding: 0.45rem; border-radius: 2px; font-weight: 600; }
  .invest:hover:not(:disabled) { background: var(--accent-secondary); border-color: var(--accent-secondary); }
  .pass { background: transparent; color: var(--fg-secondary); border: 1px solid var(--border);
    font-family: var(--font-chrome); font-size: 0.7rem; letter-spacing: 0.08em; padding: 0.45rem 0.85rem; border-radius: 2px; }
  .pass:hover:not(:disabled) { color: var(--fg-primary); border-color: var(--fg-secondary); }

  .empty { font-size: 0.82rem; color: var(--fg-secondary); line-height: 1.4; margin: 0.5rem 0; }
</style>
