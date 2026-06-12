<script lang="ts">
  /*
   * Hire tab (P6) — the hiring economy.
   *
   *   • HiringPanel: candidate cards now show the specialization displayName, a
   *     voiceFlavor hint, salary, and the family nameplate color; the detail view
   *     (use:dwell key candidate:<id>) shows stat highlights, era flavor, and a
   *     credibility-assembled one-line SELF-pitch (honest ones admit weaknesses).
   *   • Arriving candidates animate in (DealBoard trickle pattern) + telemetry
   *     hire_candidate_arrived; expiring candidates log candidate_expired.
   *   • Poach board (P6.3): rival divisions you can buy whole, with distinct
   *     visual weight ("this team made $X over Y years"), a confirm dialog, and
   *     poach_offered / poach_opened / poach_accepted / poach_declined telemetry.
   *   • Roster (P6.2): a per-DIVISION, per-EMPLOYEE staff list (now possible — the
   *     engine exposes staff JSON with specializationId), showing name /
   *     specialization / tenure.
   */

  import { sim } from '../stores/simulation.svelte';
  import { toasts } from '../stores/toasts.svelte';
  import { api, ApiError } from '../api/rest';
  import { telemetry } from '../telemetry';
  import { dwell } from '../actions/dwell';
  import { fmtMoney } from '../util/money';
  import { familyColor, FAMILIES, type FamilyId } from '../characters/archetypes-data';
  import { specializationFor, eraFlavor } from '../characters/specializations';
  import { assemblePitch } from '../voice/credibility';
  import { cardForPoachOffer } from '../characters/triggers';
  import type { HiringCandidate, PoachOffer } from '../types/server';

  const DIV_LABEL: Record<string, string> = {
    commercial_lending: 'Commercial Lending', mortgage_lending: 'Mortgage Lending', credit_cards: 'Credit Cards',
    trading: 'Trading Desk', derivatives: 'Derivatives', investment_banking: 'Investment Banking',
    restructuring: 'Restructuring', private_equity: 'Private Equity', securitization: 'Securitization',
    asset_management: 'Asset Management', wealth_management: 'Wealth Management', trust_custody: 'Trust & Custody',
    international: 'International', venture_capital: 'Venture Capital', fintech: 'Fintech', crypto_custody: 'Crypto Custody',
  };
  function divLabel(type: string): string { return DIV_LABEL[type] ?? type.replace(/_/g, ' '); }
  const fmt = (v: number): string => fmtMoney(v);

  let divisions = $derived(sim.bank?.divisions ?? []);
  let totalStaff = $derived(divisions.reduce((n, d) => n + (d.employees ?? 0), 0));
  let candidates = $derived(sim.hiringPool ?? []);
  let offers = $derived((sim.poachOffers ?? []).filter((o) => o.active !== false));

  // ── Candidate display helpers ─────────────────────────────────────────────
  function famOf(c: HiringCandidate): FamilyId {
    const a = (c.archetype ?? 'operator') as FamilyId;
    return a in FAMILIES ? a : 'operator';
  }
  function specName(c: HiringCandidate): string {
    const s = specializationFor(c.specializationId);
    return s ? s.displayName : FAMILIES[famOf(c)].displayName;
  }
  function voiceHint(c: HiringCandidate): string {
    const s = specializationFor(c.specializationId);
    return s?.voiceFlavor ?? FAMILIES[famOf(c)].voiceFlavor;
  }
  function topStats(c: HiringCandidate): Array<[string, number]> {
    const s = c.stats ?? {};
    return Object.entries(s)
      .filter(([, v]) => typeof v === 'number')
      .sort((a, b) => (b[1] as number) - (a[1] as number))
      .slice(0, 4) as Array<[string, number]>;
  }
  // A deterministic per-candidate hidden credibility (no NPC registry entry for a
  // raw candidate): family tendency ± a stable jitter from the candidate id.
  function candidateCredibility(c: HiringCandidate): number {
    const fam = FAMILIES[famOf(c)];
    let h = 5381;
    const seed = `${sim.gameId ?? 'g'}|${c.id}|cred`;
    for (let i = 0; i < seed.length; i++) h = ((h << 5) + h + seed.charCodeAt(i)) | 0;
    const j = (((h >>> 0) % 100000) / 100000 - 0.5) * 0.36;
    return Math.max(0, Math.min(1, fam.credibilityTendency + j));
  }
  // The candidate pitches THEMSELVES — honest ones (high cred) are terse and
  // admit a weakness; cronies pad and oversell. The repTag slot lets the pitch
  // reference what the bank has become (P6.5).
  function selfPitch(c: HiringCandidate): string {
    const cred = candidateCredibility(c);
    const { text } = assemblePitch(
      famOf(c), cred,
      { firm: 'myself', line: specName(c), successPct: Math.round((c.expectedSharpe ?? 0.5) * 100), repTag: sim.reputationTag },
      c.id, 'self_pitch',
    );
    return text;
  }

  let submitting = $state(false);
  let openCand = $state<string | null>(null);

  // ── Candidate arrival / expiry telemetry + animation (trickle pattern) ─────
  let seenCand = new Set<string>();
  let candFirstSeen = new Map<string, number>();
  let prevCandIds = new Set<string>();
  let candPrimed = false;
  let freshCand = $state(new Set<string>());
  $effect(() => {
    const live = sim.hiringPool ?? [];
    const ids = live.map((c) => c.id);
    const liveSet = new Set(ids);
    if (!candPrimed) {
      const now = Date.now();
      for (const id of ids) { seenCand.add(id); candFirstSeen.set(id, now); }
      prevCandIds = liveSet; candPrimed = true; return;
    }
    for (const c of live) {
      if (!seenCand.has(c.id)) {
        seenCand.add(c.id);
        candFirstSeen.set(c.id, Date.now());
        const next = new Set(freshCand); next.add(c.id); freshCand = next;
        setTimeout(() => { const n = new Set(freshCand); n.delete(c.id); freshCand = n; }, 900);
        telemetry.log('hire_candidate_arrived', { id: c.id, specializationId: c.specializationId ?? null });
      }
    }
    for (const id of prevCandIds) {
      if (!liveSet.has(id)) {
        const since = candFirstSeen.get(id);
        telemetry.log('candidate_expired', { id, visibleMs: since != null ? Date.now() - since : 0 });
        candFirstSeen.delete(id);
      }
    }
    prevCandIds = liveSet;
  });

  // ── Poach offer arrival telemetry + bark ───────────────────────────────────
  let seenOffer = new Set<string>();
  let offerPrimed = false;
  $effect(() => {
    const live = sim.poachOffers ?? [];
    if (!offerPrimed) { for (const o of live) seenOffer.add(o.id); offerPrimed = true; return; }
    for (const o of live) {
      if (!seenOffer.has(o.id)) {
        seenOffer.add(o.id);
        telemetry.log('poach_offered', { id: o.id, rival: o.rivalName, divisionType: o.divisionType, askPrice: o.askPrice });
        // Portrait bark: the rival head pitches their own bench (P6.3).
        cardForPoachOffer(o, sim.gameId);
      }
    }
  });

  function engageCandidate(c: HiringCandidate) {
    telemetry.hover(`candidate:${c.id}`, { id: c.id, archetype: c.archetype });
  }
  function toggleDetail(c: HiringCandidate) {
    openCand = openCand === c.id ? null : c.id;
    if (openCand) telemetry.log('hire_detail_open', { id: c.id, specializationId: c.specializationId ?? null });
  }

  async function hire(c: HiringCandidate) {
    if (submitting || !sim.gameId) return;
    const div = sim.bank?.divisions?.[0];
    if (!div) { toasts.warn('No division to staff yet.'); return; }
    submitting = true;
    telemetry.log('hire_click', { id: c.id, archetype: c.archetype, salary: c.annualSalary });
    try {
      await api.hire(sim.gameId, c.id, div.id);
      const view = await api.getState(sim.gameId);
      sim.applyPlayerView(view);
      toasts.success(`Hired ${c.name} — ${specName(c)}`);
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      toasts.error(`Hire failed: ${msg}`);
    } finally { submitting = false; }
  }

  // ── Poach flow ──────────────────────────────────────────────────────────
  let confirmOffer = $state<PoachOffer | null>(null);
  function openOffer(o: PoachOffer) {
    confirmOffer = o;
    telemetry.log('poach_opened', { id: o.id, rival: o.rivalName, askPrice: o.askPrice });
  }
  function declineOffer(o: PoachOffer) {
    confirmOffer = null;
    telemetry.log('poach_declined', { id: o.id, rival: o.rivalName });
  }
  async function acceptOffer(o: PoachOffer) {
    if (submitting || !sim.gameId) return;
    submitting = true;
    telemetry.log('poach_accepted', { id: o.id, rival: o.rivalName, askPrice: o.askPrice, teamSize: o.teamSize });
    try {
      await api.poach(sim.gameId, o.id);
      const view = await api.getState(sim.gameId);
      sim.applyPlayerView(view);
      toasts.success(`Poached the ${o.divisionName} team from ${o.rivalName}`);
      confirmOffer = null;
    } catch (e) {
      const msg = e instanceof ApiError ? e.message : (e as Error).message;
      toasts.error(`Poach failed: ${msg}`);
    } finally { submitting = false; }
  }
  let affordablePoach = $derived.by(() => {
    const cap = sim.bankCapital;
    return (o: PoachOffer) => cap >= o.askPrice;
  });

  // ── Roster: per-employee staff for a division (engine exposes staff JSON). ──
  interface StaffMember { id: string; name: string; archetype?: string; specializationId?: string; quartersEmployed?: number; level?: number; }
  function staffOf(d: { [key: string]: unknown }): StaffMember[] {
    const s = d['staff'];
    return Array.isArray(s) ? (s as StaffMember[]) : [];
  }
  function tenure(m: StaffMember): string {
    const q = m.quartersEmployed ?? 0;
    if (q <= 0) return 'new';
    const y = Math.floor(q / 4); const r = q % 4;
    return y > 0 ? `${y}y${r ? ` ${r}q` : ''}` : `${q}q`;
  }
  function memberSpec(m: StaffMember): string {
    const s = specializationFor(m.specializationId);
    if (s) return s.displayName;
    const a = (m.archetype ?? '') as FamilyId;
    return a in FAMILIES ? FAMILIES[a].displayName : (m.archetype ?? '—');
  }
</script>

<div class="hire-tab">
  <!-- Hiring desk: candidate cards with specialization + self-pitch -->
  <section class="desk" aria-label="Hiring desk">
    <header class="col-head">
      <span class="col-title">Hiring Desk</span>
      <span class="col-sub">{candidates.length} available</span>
    </header>
    {#if candidates.length}
      <ul class="cand-list">
        {#each candidates as c (c.id)}
          {@const fam = famOf(c)}
          {@const isOpen = openCand === c.id}
          <li
            class="cand"
            class:fresh={freshCand.has(c.id)}
            class:open={isOpen}
            style="--plate: {familyColor(fam)}"
            use:dwell={{ key: `candidate:${c.id}`, data: { archetype: c.archetype, specializationId: c.specializationId } }}
            onmouseenter={() => engageCandidate(c)}
            onfocusin={() => engageCandidate(c)}
          >
            <button class="cand-summary" onclick={() => toggleDetail(c)} aria-expanded={isOpen}>
              <div class="cand-top">
                <span class="cand-name">{c.name}</span>
                <span class="salary num">{fmt(c.annualSalary)}/yr</span>
              </div>
              <div class="cand-spec"><span class="plate-dot"></span>{specName(c)}</div>
              <div class="voice-hint">{voiceHint(c)}</div>
            </button>

            {#if isOpen}
              {@const s = specializationFor(c.specializationId)}
              <div class="detail">
                <div class="stat-grid">
                  {#each topStats(c) as [k, v]}
                    <span class="stat"><b>{k.slice(0, 3).toUpperCase()}</b> {v}</span>
                  {/each}
                </div>
                {#if s}<div class="era">Era · {eraFlavor(s)} · leans {s.statHi.join(' + ')}</div>{/if}
                {#if c.traits?.length}
                  <div class="traits">
                    {#each c.traits.slice(0, 3) as t}
                      <span class="trait" class:bad={!t.positive} title={t.description}>{t.name}</span>
                    {/each}
                  </div>
                {/if}
                <div class="pitch">
                  <span class="pitch-by">In their own words</span>
                  <p class="pitch-line">“{selfPitch(c)}”</p>
                </div>
                <button class="hire-btn" disabled={submitting} onclick={() => hire(c)}>Hire — {fmt(c.annualSalary)}/yr</button>
              </div>
            {/if}
          </li>
        {/each}
      </ul>
    {:else}
      <p class="empty">No candidates on the desk. Talent trickles in as the quarter unfolds.</p>
    {/if}
  </section>

  <!-- Poach board: rival divisions you can buy whole -->
  <section class="poach" aria-label="Poach board">
    <header class="col-head">
      <span class="col-title">Poach Board</span>
      <span class="col-sub">{offers.length} team{offers.length === 1 ? '' : 's'} in play</span>
    </header>
    {#if offers.length}
      <ul class="offer-list">
        {#each offers as o (o.id)}
          {@const fam = (o.archetypeMix?.[0] ?? 'dealmaker') as FamilyId}
          <li class="offer" style="--plate: {familyColor(fam in FAMILIES ? fam : 'dealmaker')}">
            <div class="offer-top">
              <span class="offer-div">{divLabel(o.divisionType)}</span>
              <span class="offer-rival">@ {o.rivalName}</span>
            </div>
            <div class="track">This team made <b class="num">{fmt(o.trackRecord)}</b> over {o.yearsTrackRecord} years</div>
            <div class="offer-meta">
              <span class="team">{o.teamSize} bankers</span>
              <span class="ask num">Ask {fmt(o.askPrice)}</span>
            </div>
            <button class="poach-btn" disabled={submitting} onclick={() => openOffer(o)}>Review raid</button>
          </li>
        {/each}
      </ul>
    {:else}
      <p class="empty">No teams on the block. Rivals' divisions become poachable as they build a track record.</p>
    {/if}
  </section>

  <!-- Roster: per-division, per-employee staff list -->
  <section class="roster" aria-label="Staff roster">
    <header class="col-head">
      <span class="col-title">Staff Roster</span>
      <span class="col-sub">{totalStaff} on payroll · {divisions.length} division{divisions.length === 1 ? '' : 's'}</span>
    </header>
    {#if divisions.length}
      <div class="div-blocks">
        {#each divisions as d (d.id)}
          {@const staff = staffOf(d)}
          <div class="div-block">
            <div class="div-block-head">
              <span class="div-name">{d.name}</span>
              <span class="div-type">{divLabel(d.type)}</span>
              <span class="div-count num">{staff.length || (d.employees ?? 0)}</span>
            </div>
            {#if staff.length}
              <ul class="emp-list">
                {#each staff.slice(0, 8) as m (m.id)}
                  {@const efam = (m.archetype ?? 'operator') as FamilyId}
                  <li class="emp" style="--plate: {familyColor(efam in FAMILIES ? efam : 'operator')}">
                    <span class="emp-dot"></span>
                    <span class="emp-name">{m.name}</span>
                    <span class="emp-spec">{memberSpec(m)}</span>
                    <span class="emp-ten num">{tenure(m)}</span>
                  </li>
                {/each}
                {#if staff.length > 8}<li class="emp-more">+{staff.length - 8} more</li>{/if}
              </ul>
            {:else}
              <p class="empty small">Headcount {d.employees ?? 0} — individual roster fills as you hire.</p>
            {/if}
          </div>
        {/each}
      </div>
    {:else}
      <p class="empty">No divisions staffed yet. Hire your first banker to open a division.</p>
    {/if}
  </section>
</div>

<!-- Poach confirm dialog -->
{#if confirmOffer}
  {@const o = confirmOffer}
  <div
    class="modal-scrim"
    role="button"
    tabindex="0"
    aria-label="Cancel team raid"
    onclick={() => declineOffer(o)}
    onkeydown={(e) => { if (e.key === 'Escape') declineOffer(o); }}
  ></div>
  <div class="poach-modal-wrap">
    <div class="poach-modal" role="dialog" aria-modal="true" aria-label="Confirm team raid">
      <h3>Raid the {divLabel(o.divisionType)} team</h3>
      <p class="modal-track">{o.rivalName}'s team booked <b class="num">{fmt(o.trackRecord)}</b> over {o.yearsTrackRecord} years.</p>
      <ul class="modal-facts">
        <li>Team size <b>{o.teamSize} bankers</b></li>
        <li>Ask price <b class="num">{fmt(o.askPrice)}</b></li>
        <li>Capital after <b class="num">{fmt(sim.bankCapital - o.askPrice)}</b></li>
      </ul>
      <p class="modal-warn">They'll remember this. Expect the rivalry to harden.</p>
      <div class="modal-actions">
        <button class="confirm" disabled={submitting || !affordablePoach(o)} onclick={() => acceptOffer(o)}>
          {affordablePoach(o) ? `Poach for ${fmt(o.askPrice)}` : 'Cannot afford'}
        </button>
        <button class="cancel" disabled={submitting} onclick={() => declineOffer(o)}>Walk away</button>
      </div>
    </div>
  </div>
{/if}

<svelte:window onkeydown={(e) => { if (confirmOffer && e.key === 'Escape') declineOffer(confirmOffer); }} />

<style>
  .hire-tab {
    display: grid;
    grid-template-columns: minmax(20rem, 24rem) minmax(16rem, 22rem) minmax(0, 1fr);
    gap: 1.1rem; padding: 1.1rem 1.4rem; align-items: start;
  }
  .col-head { display: flex; align-items: baseline; justify-content: space-between;
    border-bottom: 1px solid var(--border-soft); padding-bottom: 0.5rem; margin-bottom: 0.7rem; }
  .col-title { font-family: var(--font-display); color: var(--accent-primary); font-size: 1.1rem; }
  .col-sub { font-family: var(--font-chrome); font-size: var(--fs-xs); letter-spacing: 0.14em; color: var(--fg-secondary); text-transform: uppercase; }

  .desk, .poach, .roster {
    background: var(--bg-card); border: 1px solid var(--border); border-radius: var(--card-radius);
    padding: 0.9rem 1rem 1rem; min-width: 0;
  }

  /* Candidate cards */
  .cand-list, .offer-list { list-style: none; margin: 0; padding: 0; display: flex; flex-direction: column; gap: 0.5rem; }
  .cand { background: var(--bg-elevated); border: 1px solid var(--border-soft); border-left: 3px solid var(--plate); border-radius: 2px; overflow: hidden; }
  .cand.open { border-left-width: 4px; }
  .cand.fresh { animation: cardIn 700ms var(--ease); }
  @keyframes cardIn { from { opacity: 0; transform: translateX(-12px); } to { opacity: 1; transform: translateX(0); } }
  .cand-summary { display: block; width: 100%; text-align: left; background: transparent; border: none; padding: 0.55rem 0.65rem; cursor: pointer; color: inherit; font: inherit; }
  .cand-summary:hover { background: rgba(135, 106, 44, 0.06); }
  .cand-top { display: flex; align-items: baseline; justify-content: space-between; gap: 0.5rem; }
  .cand-name { font-weight: 600; color: var(--fg-primary); font-size: 0.9rem; }
  .salary { font-family: var(--font-data); font-size: 0.78rem; color: var(--fg-data); }
  .cand-spec { display: flex; align-items: center; gap: 0.35rem; margin-top: 0.2rem;
    font-family: var(--font-chrome); font-size: 0.62rem; letter-spacing: 0.1em; text-transform: uppercase; color: var(--plate); }
  .plate-dot { width: 0.45rem; height: 0.45rem; border-radius: 50%; background: var(--plate); }
  .voice-hint { margin-top: 0.25rem; font-size: 0.74rem; font-style: italic; color: var(--fg-secondary); line-height: 1.3;
    overflow: hidden; text-overflow: ellipsis; display: -webkit-box; -webkit-line-clamp: 2; line-clamp: 2; -webkit-box-orient: vertical; }

  .detail { padding: 0 0.65rem 0.7rem; border-top: 1px solid var(--border-soft); }
  .stat-grid { display: flex; flex-wrap: wrap; gap: 0.6rem; margin-top: 0.5rem; font-family: var(--font-data); font-size: 0.72rem; color: var(--fg-secondary); }
  .stat b { color: var(--fg-muted); font-weight: 400; }
  .era { margin-top: 0.4rem; font-family: var(--font-chrome); font-size: 0.58rem; letter-spacing: 0.08em; text-transform: uppercase; color: var(--accent-secondary); }
  .traits { display: flex; flex-wrap: wrap; gap: 0.3rem; margin-top: 0.4rem; }
  .trait { font-size: 0.6rem; letter-spacing: 0.04em; padding: 0.06rem 0.3rem; border-radius: 2px; border: 1px solid var(--accent-success); color: var(--accent-success); }
  .trait.bad { border-color: var(--accent-danger); color: var(--accent-danger); }
  .pitch { background: var(--bg-card); border-left: 2px solid var(--plate); padding: 0.4rem 0.55rem; border-radius: 2px; margin: 0.55rem 0; }
  .pitch-by { font-family: var(--font-chrome); font-size: 0.55rem; letter-spacing: 0.12em; text-transform: uppercase; color: var(--accent-primary); }
  .pitch-line { margin: 0.15rem 0 0; font-style: italic; font-size: 0.82rem; line-height: 1.4; color: var(--fg-primary); }
  .hire-btn { width: 100%; background: var(--accent-primary); color: var(--bg-base); border: 1px solid var(--accent-primary);
    font-family: var(--font-chrome); font-size: 0.66rem; letter-spacing: 0.1em; padding: 0.42rem; border-radius: 2px; font-weight: 600; }
  .hire-btn:hover:not(:disabled) { background: var(--accent-secondary); }

  /* Poach cards — heavier visual weight */
  .offer { background: var(--bg-elevated); border: 1px solid var(--plate); border-left: 4px solid var(--plate);
    border-radius: 3px; padding: 0.6rem 0.7rem; box-shadow: inset 0 0 0 1px rgba(0,0,0,0.15); }
  .offer-top { display: flex; align-items: baseline; justify-content: space-between; gap: 0.5rem; }
  .offer-div { font-family: var(--font-display); font-size: 0.98rem; color: var(--fg-primary); }
  .offer-rival { font-family: var(--font-chrome); font-size: 0.62rem; letter-spacing: 0.06em; color: var(--fg-secondary); }
  .track { margin-top: 0.35rem; font-size: 0.85rem; color: var(--fg-primary); line-height: 1.35; }
  .track b { font-family: var(--font-data); color: var(--accent-success); }
  .offer-meta { display: flex; align-items: center; justify-content: space-between; margin-top: 0.45rem; font-size: 0.76rem; color: var(--fg-secondary); }
  .ask { font-family: var(--font-data); color: var(--fg-data); }
  .poach-btn { width: 100%; margin-top: 0.5rem; background: transparent; color: var(--plate); border: 1px solid var(--plate);
    font-family: var(--font-chrome); font-size: 0.66rem; letter-spacing: 0.12em; padding: 0.42rem; border-radius: 2px; font-weight: 600; cursor: pointer; }
  .poach-btn:hover:not(:disabled) { background: color-mix(in srgb, var(--plate) 14%, transparent); }

  /* Roster */
  .div-blocks { display: flex; flex-direction: column; gap: 0.7rem; }
  .div-block { background: var(--bg-elevated); border: 1px solid var(--border-soft); border-radius: 2px; padding: 0.5rem 0.6rem; }
  .div-block-head { display: grid; grid-template-columns: 1fr auto auto; align-items: baseline; gap: 0.6rem; border-bottom: 1px solid var(--border-soft); padding-bottom: 0.35rem; }
  .div-name { color: var(--fg-primary); font-size: 0.88rem; font-weight: 600; }
  .div-type { font-family: var(--font-chrome); font-size: 0.56rem; letter-spacing: 0.1em; text-transform: uppercase; color: var(--accent-secondary); }
  .div-count { font-family: var(--font-data); font-size: 0.78rem; color: var(--fg-secondary); }
  .emp-list { list-style: none; margin: 0.4rem 0 0; padding: 0; display: flex; flex-direction: column; gap: 0.25rem; }
  .emp { display: grid; grid-template-columns: auto 1fr auto auto; align-items: center; gap: 0.5rem; font-size: 0.78rem; }
  .emp-dot { width: 0.45rem; height: 0.45rem; border-radius: 50%; background: var(--plate); }
  .emp-name { color: var(--fg-primary); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .emp-spec { font-family: var(--font-chrome); font-size: 0.55rem; letter-spacing: 0.06em; text-transform: uppercase; color: var(--fg-secondary); white-space: nowrap; }
  .emp-ten { font-family: var(--font-data); font-size: 0.7rem; color: var(--fg-muted); }
  .emp-more { font-size: 0.7rem; color: var(--fg-muted); padding-left: 0.95rem; }

  .empty { font-size: 0.82rem; color: var(--fg-secondary); line-height: 1.4; margin: 0.4rem 0; }
  .empty.small { font-size: 0.74rem; margin: 0.3rem 0 0; }

  /* Poach confirm modal */
  .modal-scrim { position: fixed; inset: 0; background: rgba(0,0,0,0.55); z-index: 850; border: none; cursor: pointer; }
  .poach-modal-wrap { position: fixed; inset: 0; display: grid; place-items: center; z-index: 851; pointer-events: none; }
  .poach-modal { pointer-events: auto; background: var(--bg-card); border: 1px solid var(--accent-primary); border-radius: var(--card-radius);
    padding: 1.2rem 1.4rem; max-width: 26rem; width: 90vw; box-shadow: 0 12px 40px rgba(0,0,0,0.5); }
  .poach-modal h3 { font-family: var(--font-display); color: var(--accent-primary); margin: 0 0 0.5rem; font-size: 1.2rem; }
  .modal-track { font-size: 0.88rem; color: var(--fg-primary); line-height: 1.4; margin: 0 0 0.6rem; }
  .modal-track b { font-family: var(--font-data); color: var(--accent-success); }
  .modal-facts { list-style: none; margin: 0 0 0.6rem; padding: 0; display: flex; flex-direction: column; gap: 0.3rem; font-size: 0.84rem; color: var(--fg-secondary); }
  .modal-facts b { color: var(--fg-primary); }
  .modal-facts .num { font-family: var(--font-data); }
  .modal-warn { font-size: 0.78rem; color: var(--accent-warning); font-style: italic; margin: 0 0 0.8rem; }
  .modal-actions { display: flex; gap: 0.6rem; }
  .confirm { flex: 1; background: var(--accent-primary); color: var(--bg-base); border: 1px solid var(--accent-primary);
    font-family: var(--font-chrome); font-size: 0.7rem; letter-spacing: 0.08em; padding: 0.5rem; border-radius: 2px; font-weight: 600; }
  .confirm:disabled { opacity: 0.5; }
  .cancel { background: transparent; color: var(--fg-secondary); border: 1px solid var(--border);
    font-family: var(--font-chrome); font-size: 0.7rem; letter-spacing: 0.08em; padding: 0.5rem 1rem; border-radius: 2px; }

  @media (max-width: 1200px) {
    .hire-tab { grid-template-columns: 1fr 1fr; }
    .roster { grid-column: 1 / -1; }
  }
  @media (max-width: 720px) {
    .hire-tab { grid-template-columns: 1fr; }
  }
</style>
