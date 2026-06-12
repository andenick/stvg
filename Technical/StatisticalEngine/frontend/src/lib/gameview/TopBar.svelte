<script lang="ts">
  /*
   * Persistent top bar (P2) — the single chrome strip that replaces
   * MiniHeader + GameHeader + SimControls. Always visible across all four tabs.
   *
   * Left → right:
   *   identity (bank name + era badge w/ tooltip)
   *   date readout (year Q / day) with a running pulse
   *   capital + Δ (green/red flash on change, fmtMoney)
   *   play/pause + speed selector (preserves sim_toggle / sim_speed telemetry)
   *   era + regime pill
   *   tab strip (Economy | Hire | My Bank | Financials) — keyboard 1-4
   *   HIRE button — colored/pulsing when capital comfortably covers the cheapest
   *     hiring candidate's salary (P2.2). Click switches to the Hire tab.
   *   Loan Book deal badge (ui.dealBadge) near the Economy end of the bar.
   *
   * Keyboard (extends the old SimControls handler; KeyboardHelp mirrors this):
   *   Space        play / pause
   *   1 2 3 4      switch tab (Economy / Hire / My Bank / Financials)
   *   [ ]          slower / faster simulation speed
   *   ?            toggle keyboard help
   */

  import { sim } from '../stores/simulation.svelte';
  import { ui, TABS, TAB_LABEL, HIRE_AFFORDABILITY_MULT, type Tab } from '../stores/ui.svelte';
  import { wsClient } from '../ws/websocket';
  import { telemetry } from '../telemetry';
  import { fmtMoney } from '../util/money';

  const SPEEDS = [1, 2, 4, 8] as const;
  type Speed = (typeof SPEEDS)[number];

  function toggle() {
    if (!sim.gameId) return;
    if (sim.paused) wsClient.play();
    else wsClient.pause();
    sim.paused = !sim.paused;
    telemetry.log('sim_toggle', { paused: sim.paused, year: sim.year, quarter: sim.quarter });
  }

  function setSpeed(s: Speed) {
    if (!sim.gameId) return;
    wsClient.setSpeed(s);
    sim.speed = s;
    telemetry.log('sim_speed', { speed: s });
  }

  function stepSpeed(dir: -1 | 1) {
    const i = SPEEDS.indexOf(sim.speed);
    const next = SPEEDS[Math.min(SPEEDS.length - 1, Math.max(0, i + dir))];
    if (next !== sim.speed) setSpeed(next);
  }

  // Engine refuses to advance while decisions/crises are pending → disable play.
  let gated = $derived(sim.pendingDecisions.length > 0 || sim.activeCrises.length > 0);

  let regimeLabel = $derived(
    sim.regime === 'normal'   ? 'Calm' :
    sim.regime === 'volatile' ? 'Volatile' :
    sim.regime === 'stressed' ? 'Stressed' :
    sim.regime === 'crisis'   ? 'Crisis' :
    String(sim.regime ?? 'Calm')
  );

  let eraTooltip = $derived(
    sim.era
      ? `${sim.era.name} · ${sim.era.startYear}–${sim.era.endYear}${sim.era.description ? ` · ${sim.era.description}` : ''}`
      : ''
  );

  function fmtDelta(v: number): string {
    const sign = v >= 0 ? '+' : '−';
    return `${sign}${fmtMoney(Math.abs(v))}`;
  }

  // P7: surface the Personal Book line once the CEO has actually traded (open
  // positions or a non-zero marked P&L). Keeps it secondary to bank capital.
  let showBook = $derived(
    (sim.tradeBook?.positions.length ?? 0) > 0 || sim.personalBookPnl !== 0
  );

  // Capital flash: re-trigger the green/red flash whenever capital moves.
  let flashKey = $state(0);
  let flashDir = $state<'up' | 'down' | 'none'>('none');
  let lastCap = sim.bankCapital;
  $effect(() => {
    const c = sim.bankCapital;
    if (c === lastCap) return;
    flashDir = c > lastCap ? 'up' : c < lastCap ? 'down' : 'none';
    flashKey++;
    lastCap = c;
  });

  // ── HIRE affordability pulse (P2.2) ───────────────────────────────────────
  // Cheapest candidate's first-year salary. annualSalary is honest dollars
  // (P0.9); the heuristic is "can I comfortably cover ~2 years of it?".
  let cheapestSalary = $derived.by(() => {
    const pool = sim.hiringPool ?? [];
    if (!pool.length) return Infinity;
    return Math.min(...pool.map((c) => c.annualSalary || Infinity));
  });
  let hireAffordable = $derived(
    cheapestSalary !== Infinity &&
    sim.bankCapital >= cheapestSalary * HIRE_AFFORDABILITY_MULT
  );

  // Log `hire_affordable_pulse_seen` once per pulse activation (rising edge).
  let pulseWasOn = false;
  $effect(() => {
    const on = hireAffordable;
    if (on && !pulseWasOn) {
      telemetry.log('hire_affordable_pulse_seen', { cheapestSalary, capital: sim.bankCapital });
    }
    pulseWasOn = on;
  });

  function goHire() { ui.setTab('hire'); }

  // ── Reputation tag (P6 ReputationLens) ────────────────────────────────────
  // The world recognizes what you've become ("gunslinger shop" / "fortress
  // bank" / …). Shown subtly next to the era badge; a transition logs
  // reputation_tag_changed {from,to} once per real change.
  let prevRepTag: string | null = null;
  $effect(() => {
    const t = sim.reputationTag;
    if (t && t !== prevRepTag) {
      if (prevRepTag) telemetry.log('reputation_tag_changed', { from: prevRepTag, to: t });
      prevRepTag = t;
    }
  });

  // ── Keyboard (extends old SimControls handler) ────────────────────────────
  const TAB_KEY: Record<string, Tab> = { '1': 'economy', '2': 'hire', '3': 'bank', '4': 'financials' };

  function onKey(e: KeyboardEvent) {
    const t = e.target as HTMLElement;
    if (t && (t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.isContentEditable)) return;

    if (e.code === 'Space') { e.preventDefault(); toggle(); }
    else if (TAB_KEY[e.key]) { e.preventDefault(); ui.setTab(TAB_KEY[e.key]); }
    else if (e.key === '[') stepSpeed(-1);
    else if (e.key === ']') stepSpeed(1);
    else if (e.key === '?' && !ui.keyboardHelpOpen) ui.keyboardHelpOpen = true;
  }

  $effect(() => {
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  });
</script>

<header class="topbar" data-phase={ui.phase}>
  <!-- Identity -->
  <div class="seg identity">
    <span class="bank-name">{sim.bank?.name ?? 'Bank'}</span>
    <span class="era-badge" title={eraTooltip}>{sim.era?.name ?? 'Post-War Stability'}</span>
    {#if sim.reputationTag}
      <span class="rep-tag" title="How the Street sees you (shifts with your hires & risk culture)">{sim.reputationTag}</span>
    {/if}
  </div>

  <!-- Date -->
  <div class="seg date" aria-live="polite">
    <span class="pulse" class:on={!sim.paused && !gated} aria-hidden="true"></span>
    <span class="date-text num">{sim.year} Q{sim.quarter}</span>
    <span class="day-text num">Day {sim.day}</span>
  </div>

  <!-- Capital + Δ -->
  <div class="seg cap">
    <span class="cap-label">CAPITAL</span>
    {#key flashKey}
      <span class="cap-val num" class:flash-up={flashDir === 'up'} class:flash-down={flashDir === 'down'}>
        {fmtMoney(sim.bankCapital)}
      </span>
    {/key}
    {#if sim.capitalDelta !== 0}
      <span class="cap-delta num" class:positive={sim.capitalDelta > 0} class:negative={sim.capitalDelta < 0}>
        {fmtDelta(sim.capitalDelta)}
      </span>
    {/if}
  </div>

  <!-- Personal Book (P7): the CEO's own light trading account, secondary to the
       bank capital. Shown only once it carries positions or a non-zero P&L. -->
  {#if showBook}
    <div class="seg book" title="Your personal trading account (separate from the bank)">
      <span class="book-label">BOOK</span>
      <span class="book-val num">{fmtMoney(sim.personalBookValue)}</span>
      {#if sim.personalBookPnl !== 0}
        <span class="book-pnl num" class:positive={sim.personalBookPnl > 0} class:negative={sim.personalBookPnl < 0}>
          {fmtDelta(sim.personalBookPnl)}
        </span>
      {/if}
    </div>
  {/if}

  <!-- Transport -->
  <div class="seg transport" role="toolbar" aria-label="Simulation controls">
    <button
      class="play"
      class:running={!sim.paused && !gated}
      aria-label={sim.paused ? 'Play' : 'Pause'}
      aria-pressed={!sim.paused}
      onclick={toggle}
      disabled={gated || !sim.gameId}
      title={gated ? 'Resolve pending decisions / crises first' : (sim.paused ? 'Play (Space)' : 'Pause (Space)')}
    >
      {#if sim.paused}<span aria-hidden="true">&#9654;</span>{:else}<span aria-hidden="true">&#9208;</span>{/if}
    </button>
    <div class="speeds" role="group" aria-label="Simulation speed">
      {#each SPEEDS as s}
        <button class="speed" class:active={sim.speed === s} onclick={() => setSpeed(s)} disabled={!sim.gameId} title="Speed {s}x">{s}&times;</button>
      {/each}
    </div>
  </div>

  <!-- Regime pill -->
  <div class="seg regime-wrap" aria-label="Market regime">
    <span class="regime regime-{sim.regime}">
      <span class="regime-dot" aria-hidden="true"></span>{regimeLabel}
    </span>
  </div>

  <!-- Tab strip -->
  <nav class="seg tabs" aria-label="Sections">
    {#each TABS as tab, i (tab)}
      <button
        class="tab"
        class:active={ui.activeTab === tab}
        onclick={() => ui.setTab(tab)}
        aria-current={ui.activeTab === tab ? 'page' : undefined}
        title="{TAB_LABEL[tab]} ({i + 1})"
      >
        <span class="tab-key" aria-hidden="true">{i + 1}</span>{TAB_LABEL[tab]}
        {#if tab === 'economy' && ui.dealBadge > 0}
          <span class="deal-badge" title="New loan-book prospects">+{ui.dealBadge}</span>
        {/if}
      </button>
    {/each}
  </nav>

  <!-- HIRE button -->
  <button
    class="hire-btn"
    class:pulse-on={hireAffordable}
    onclick={goHire}
    title={hireAffordable ? 'You can afford to hire — bring in talent' : 'Open the hiring desk'}
  >
    HIRE
  </button>
</header>

<style>
  .topbar {
    display: flex;
    align-items: center;
    gap: 1rem;
    padding: 0.45rem 1rem;
    background: var(--bg-card);
    border-bottom: 1px solid var(--border);
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    letter-spacing: 0.1em;
    flex-wrap: wrap;
    position: sticky;
    top: 0;
    z-index: 500;
  }
  .seg { display: inline-flex; align-items: center; gap: 0.5rem; }

  .identity { gap: 0.6rem; }
  .bank-name {
    font-family: var(--font-display);
    font-size: 1.05rem;
    letter-spacing: 0.04em;
    color: var(--fg-primary);
  }
  .era-badge {
    color: var(--accent-primary);
    border: 1px solid var(--border);
    padding: 0.12rem 0.5rem;
    border-radius: 2px;
    cursor: help;
    font-size: 0.62rem;
    text-transform: uppercase;
  }
  .rep-tag {
    color: var(--accent-secondary);
    border: 1px solid var(--accent-secondary);
    background: color-mix(in srgb, var(--accent-secondary) 10%, transparent);
    padding: 0.1rem 0.45rem;
    border-radius: 999px;
    cursor: help;
    font-size: 0.58rem;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    white-space: nowrap;
  }

  .pulse { width: 0.5rem; height: 0.5rem; border-radius: 50%; background: var(--fg-muted); }
  .pulse.on { background: var(--accent-success); animation: pulse 1.4s ease-in-out infinite; }
  .date-text { color: var(--fg-primary); font-size: var(--fs-sm); letter-spacing: 0.08em; }
  .day-text { color: var(--fg-secondary); }

  .cap { gap: 0.45rem; }
  .cap-label { color: var(--fg-secondary); text-transform: uppercase; }
  .cap-val {
    font-family: var(--font-data);
    font-size: 1rem;
    color: var(--fg-primary);
    font-variant-numeric: tabular-nums;
    letter-spacing: 0;
  }
  .cap-val.flash-up { animation: flashUp 0.6s var(--ease); }
  .cap-val.flash-down { animation: flashDown 0.6s var(--ease); }
  @keyframes flashUp { 0% { color: var(--accent-success); } 100% { color: var(--fg-primary); } }
  @keyframes flashDown { 0% { color: var(--accent-danger); } 100% { color: var(--fg-primary); } }
  .cap-delta { font-size: 0.7rem; padding: 0.08rem 0.35rem; border-radius: 2px; }
  .cap-delta.positive { color: var(--accent-success); background: rgba(107, 142, 78, 0.12); }
  .cap-delta.negative { color: var(--accent-danger); background: rgba(168, 69, 58, 0.12); }

  /* P7 Personal Book line — smaller than capital (secondary to the bank). */
  .book { gap: 0.35rem; }
  .book-label { color: var(--fg-muted); text-transform: uppercase; font-size: 0.62rem; letter-spacing: 0.12em; }
  .book-val { font-family: var(--font-data); font-size: 0.82rem; color: var(--fg-secondary); font-variant-numeric: tabular-nums; }
  .book-pnl { font-size: 0.66rem; padding: 0.05rem 0.3rem; border-radius: 2px; }
  .book-pnl.positive { color: var(--accent-success); background: rgba(107, 142, 78, 0.12); }
  .book-pnl.negative { color: var(--accent-danger); background: rgba(168, 69, 58, 0.12); }

  .play {
    width: 2rem; height: 2rem; border-radius: 50%;
    border: 2px solid var(--accent-primary); color: var(--accent-primary);
    font-size: 0.9rem; padding: 0;
    display: inline-flex; align-items: center; justify-content: center;
    transition: all var(--t-fast) var(--ease);
  }
  .play:hover:not(:disabled) { background: rgba(196, 163, 90, 0.12); }
  .play.running { animation: simPulse 1.6s ease-in-out infinite; }
  .speeds { display: inline-flex; gap: 0.2rem; }
  .speed {
    padding: 0.28rem 0.5rem; font-size: var(--fs-xs); letter-spacing: 0.08em;
    color: var(--fg-secondary); background: transparent; border: 1px solid var(--border);
  }
  .speed.active { color: var(--accent-primary); border-color: var(--accent-primary); background: rgba(196, 163, 90, 0.08); }
  .speed:hover:not(:disabled):not(.active) { color: var(--fg-primary); border-color: var(--fg-secondary); }

  .regime {
    display: inline-flex; align-items: center; gap: 0.35rem;
    border: 1px solid var(--border); border-radius: 999px; padding: 0.12rem 0.5rem;
    color: var(--fg-secondary); text-transform: uppercase; letter-spacing: 0.14em; font-size: 0.62rem;
  }
  .regime-dot { width: 0.45rem; height: 0.45rem; border-radius: 50%; background: var(--accent-success); }
  .regime-volatile .regime-dot { background: var(--accent-warning); }
  .regime-stressed .regime-dot { background: var(--accent-danger); }
  .regime-crisis   .regime-dot { background: var(--accent-danger); animation: pulse 1s ease-in-out infinite; }

  .tabs { margin-left: auto; gap: 0.25rem; }
  .tab {
    position: relative;
    display: inline-flex; align-items: center; gap: 0.4rem;
    padding: 0.4rem 0.7rem;
    background: transparent; border: 1px solid var(--border); border-bottom: none;
    border-radius: 4px 4px 0 0;
    color: var(--fg-secondary);
    font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.1em;
    text-transform: uppercase;
    cursor: pointer;
    transition: color var(--t-fast) var(--ease), background var(--t-fast) var(--ease);
  }
  .tab:hover { color: var(--fg-primary); }
  .tab.active {
    color: var(--accent-primary);
    background: var(--bg-base);
    border-color: var(--accent-primary);
  }
  .tab-key {
    font-size: 0.55rem; color: var(--fg-muted);
    border: 1px solid var(--border); border-radius: 2px; padding: 0 0.22rem; line-height: 1.3;
  }
  .tab.active .tab-key { color: var(--accent-primary); border-color: var(--accent-primary); }
  .deal-badge {
    background: var(--accent-primary); color: var(--bg-base);
    font-size: 0.55rem; padding: 0.02rem 0.28rem; border-radius: 999px; font-weight: 600; letter-spacing: 0;
  }

  .hire-btn {
    padding: 0.42rem 0.95rem;
    background: transparent; color: var(--accent-primary);
    border: 1px solid var(--accent-primary); border-radius: 3px;
    font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.18em; font-weight: 600;
    cursor: pointer;
    transition: all var(--t-fast) var(--ease);
  }
  .hire-btn:hover { background: rgba(196, 163, 90, 0.12); }
  .hire-btn.pulse-on {
    background: var(--accent-primary);
    color: var(--bg-base);
    animation: hirePulse 1.5s ease-in-out infinite;
  }
  @keyframes hirePulse {
    0%, 100% { box-shadow: 0 0 0 0 rgba(196, 163, 90, 0.5); }
    50% { box-shadow: 0 0 0 6px rgba(196, 163, 90, 0); }
  }

  @media (max-width: 1100px) {
    .tabs { margin-left: 0; }
  }
</style>
