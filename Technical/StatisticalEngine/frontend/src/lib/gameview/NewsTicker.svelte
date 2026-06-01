<script lang="ts">
  /*
   * Horizontal scrolling news strip at the bottom of GameView.
   * Source: sim.simulationDigest + sim.events + sim.messages.
   *
   * Phase 1: simple text only (1950s feel — no chrome).
   * Phase 2+: categorized icons (event vs digest vs narrative).
   *
   * Click opens the full event log overlay (ui.fullEventLogOpen).
   * Scroll speed scales with sim.speed.
   *
   * Fills VISUAL_INSPECTION gap #5 (missing ticker).
   */

  import { sim } from '../stores/simulation.svelte';
  import { ui } from '../stores/ui.svelte';

  type Kind = 'event' | 'digest' | 'narrative';
  interface Item {
    id: string;
    text: string;
    kind: Kind;
    severity?: number;
  }

  // Build a flat feed. Dedupe by text, cap to 24 so the marquee stays smooth.
  let feed = $derived.by<Item[]>(() => {
    const out: Item[] = [];
    const seen = new Set<string>();
    function push(text: string, kind: Kind, key: string, severity?: number) {
      const t = text?.trim();
      if (!t || seen.has(t)) return;
      seen.add(t);
      out.push({ id: `${kind}:${key}`, text: t, kind, severity });
    }
    for (let i = sim.events.length - 1; i >= 0; i--) {
      const ev = sim.events[i];
      push(ev.title ?? ev.description ?? '', 'event', ev.id ?? `e${i}`, ev.severity);
    }
    for (let i = sim.simulationDigest.length - 1; i >= 0; i--) {
      const d = sim.simulationDigest[i];
      push(d.description, 'digest', `d${d.day}-${i}`);
    }
    for (let i = sim.messages.length - 1; i >= 0; i--) {
      push(sim.messages[i], 'narrative', `m${i}`);
    }
    return out.slice(0, 24);
  });

  // Marquee duration scales with item count, inversely with sim speed.
  let durationS = $derived(Math.max(20, feed.length * (10 / Math.max(1, sim.speed))));

  function iconFor(kind: Kind): string {
    if (kind === 'event')     return '◆';   // ◆
    if (kind === 'digest')    return '·';   // ·
    return '›';                              // ›
  }

  function openLog() {
    ui.fullEventLogOpen = true;
  }
</script>

{#if feed.length > 0}
  <button
    type="button"
    class="news-ticker"
    onclick={openLog}
    aria-label="Show full event log"
    title="Click to open full event log"
  >
    <span class="brand">NEWS</span>
    <span class="track" style="animation-duration: {durationS}s">
      <span class="loop">
        {#each feed as item (item.id)}
          <span class="item k-{item.kind}" class:sev-high={(item.severity ?? 0) > 6}>
            {#if ui.phase >= 2}<span class="icon" aria-hidden="true">{iconFor(item.kind)}</span>{/if}
            <span class="text">{item.text}</span>
          </span>
        {/each}
      </span>
      <!-- duplicate the loop so the marquee can scroll seamlessly -->
      <span class="loop" aria-hidden="true">
        {#each feed as item (item.id + '-dup')}
          <span class="item k-{item.kind}" class:sev-high={(item.severity ?? 0) > 6}>
            {#if ui.phase >= 2}<span class="icon" aria-hidden="true">{iconFor(item.kind)}</span>{/if}
            <span class="text">{item.text}</span>
          </span>
        {/each}
      </span>
    </span>
  </button>
{/if}

{#if ui.fullEventLogOpen}
  <div
    class="overlay-back"
    role="button"
    tabindex="0"
    aria-label="Close event log"
    onclick={() => ui.fullEventLogOpen = false}
    onkeydown={(e) => { if (e.key === 'Escape' || e.key === 'Enter') ui.fullEventLogOpen = false; }}
  ></div>
  <div class="log-panel" role="dialog" aria-labelledby="log-title">
    <header class="log-head">
      <h2 id="log-title">Event Log</h2>
      <button class="log-x" aria-label="Close" onclick={() => ui.fullEventLogOpen = false}>&times;</button>
    </header>
    <div class="log-body">
      {#if sim.events.length}
        <section>
          <h3>Events</h3>
          <ul class="log-list">
            {#each sim.events as ev}
              <li>
                <span class="log-kind">{ev.type ?? 'event'}</span>
                <span class="log-title">{ev.title}</span>
                {#if ev.description}<span class="log-desc">{ev.description}</span>{/if}
              </li>
            {/each}
          </ul>
        </section>
      {/if}
      {#if sim.simulationDigest.length}
        <section>
          <h3>Simulation digest</h3>
          <ul class="log-list">
            {#each sim.simulationDigest as d}
              <li>
                <span class="log-day">Day {d.day}</span>
                <span class="log-desc">{d.description}</span>
              </li>
            {/each}
          </ul>
        </section>
      {/if}
      {#if sim.messages.length}
        <section>
          <h3>Messages</h3>
          <ul class="log-list">
            {#each sim.messages as m, i (i)}
              <li><span class="log-desc">{m}</span></li>
            {/each}
          </ul>
        </section>
      {/if}
      {#if sim.events.length === 0 && sim.simulationDigest.length === 0 && sim.messages.length === 0}
        <p class="empty">No events recorded yet.</p>
      {/if}
    </div>
  </div>
{/if}

<style>
  .news-ticker {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 0.4rem 0.6rem;
    overflow: hidden;
    width: 100%;
    cursor: pointer;
    font-family: var(--font-body);
    color: inherit;
    text-align: left;
  }
  .news-ticker:hover { border-color: var(--accent-primary); }

  .brand {
    flex: 0 0 auto;
    font-family: var(--font-chrome);
    font-size: 0.65rem;
    letter-spacing: 0.3em;
    color: var(--accent-primary);
    padding: 0.15rem 0.5rem;
    border: 1px solid var(--accent-primary);
    border-radius: 2px;
  }

  .track {
    display: inline-flex;
    flex: 1;
    min-width: 0;
    overflow: hidden;
    /* Marquee: keyframes shift two side-by-side loops left by 50%. */
    animation: tickerScroll linear infinite;
    animation-duration: 60s; /* overridden inline */
  }
  .loop {
    display: inline-flex;
    flex: 0 0 auto;
    white-space: nowrap;
    padding-right: 2rem;
    gap: 1.5rem;
  }

  @keyframes tickerScroll {
    from { transform: translateX(0); }
    to   { transform: translateX(-50%); }
  }
  /* Pause on hover so the player can actually read a headline */
  .news-ticker:hover .track,
  .news-ticker:focus-visible .track {
    animation-play-state: paused;
  }

  .item {
    display: inline-flex;
    align-items: baseline;
    gap: 0.4rem;
  }
  .icon {
    color: var(--fg-muted);
    font-size: 0.7rem;
  }
  .item.k-event .icon    { color: var(--accent-primary); }
  .item.k-digest .icon   { color: var(--fg-secondary); }
  .item.k-narrative .icon{ color: var(--accent-secondary); }
  .item.sev-high {
    color: var(--accent-danger);
  }
  .text {
    font-size: 0.85rem;
    color: var(--fg-primary);
  }
  .item.k-digest .text { color: var(--fg-secondary); }

  /* ── Full event log overlay ────────────────────────────────────────── */
  .overlay-back {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.65);
    z-index: 880;
    border: none;
    padding: 0;
  }
  .log-panel {
    position: fixed;
    top: 0;
    right: 0;
    bottom: 0;
    width: min(32rem, 92vw);
    background: var(--bg-card);
    border-left: 1px solid var(--border);
    z-index: 890;
    display: flex;
    flex-direction: column;
    animation: slideInRight 240ms var(--ease);
    box-shadow: -8px 0 24px rgba(0, 0, 0, 0.35);
  }
  .log-head {
    display: flex;
    align-items: center;
    padding: 1rem 1.25rem;
    border-bottom: 1px solid var(--border);
  }
  .log-head h2 {
    font-family: var(--font-display);
    color: var(--accent-primary);
    margin: 0;
    font-size: 1.15rem;
    flex: 1;
  }
  .log-x {
    background: transparent;
    border: none;
    color: var(--fg-secondary);
    font-size: 1.4rem;
    cursor: pointer;
  }
  .log-x:hover { color: var(--fg-primary); }

  .log-body {
    flex: 1;
    overflow-y: auto;
    padding: 1rem 1.25rem;
  }
  .log-body section { margin-bottom: 1.25rem; }
  .log-body h3 {
    font-family: var(--font-chrome);
    color: var(--fg-secondary);
    letter-spacing: 0.2em;
    font-size: var(--fs-xs);
    margin: 0 0 0.5rem 0;
    text-transform: uppercase;
  }
  .log-list {
    list-style: none;
    padding: 0;
    margin: 0;
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
  }
  .log-list li {
    padding: 0.4rem 0.5rem;
    border-left: 2px solid var(--border);
    display: flex;
    flex-direction: column;
    gap: 0.15rem;
  }
  .log-kind, .log-day {
    font-family: var(--font-chrome);
    font-size: 0.6rem;
    letter-spacing: 0.18em;
    color: var(--fg-secondary);
    text-transform: uppercase;
  }
  .log-title { color: var(--fg-primary); font-size: var(--fs-sm); }
  .log-desc { color: var(--fg-secondary); font-size: var(--fs-sm); line-height: 1.4; }
  .empty { color: var(--fg-secondary); }
</style>
