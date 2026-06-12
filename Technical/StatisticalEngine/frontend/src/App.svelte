<script lang="ts">
  import TitleScreen from './lib/screens/TitleScreen.svelte';
  import GameView from './lib/screens/GameView.svelte';
  import GameOverScreen from './lib/screens/GameOverScreen.svelte';
  import ToastContainer from './lib/overlays/ToastContainer.svelte';
  import CrisisModal from './lib/overlays/CrisisModal.svelte';
  import AnnualReportModal from './lib/overlays/AnnualReportModal.svelte';
  import EraTransition from './lib/overlays/EraTransition.svelte';
  import KeyboardHelp from './lib/overlays/KeyboardHelp.svelte';
  import CharacterCard from './lib/overlays/CharacterCard.svelte';
  import { ui } from './lib/stores/ui.svelte';
  import { sim } from './lib/stores/simulation.svelte';
  import { toasts } from './lib/stores/toasts.svelte';
  import { telemetry } from './lib/telemetry';
  import {
    cardFromMessage, cardForCrisis, cardForEra, cardForAnnualReport, cardForCapitalSwing,
    cardForDealOutcome,
  } from './lib/characters/triggers';
  import { characters } from './lib/stores/characters.svelte';
  import { assemblePitch } from './lib/voice/credibility';
  import { familyColor } from './lib/characters/archetypes-data';
  import { api } from './lib/api/rest';
  import { wsClient } from './lib/ws/websocket';
  import type { CharacterMessage } from './lib/types/server';

  /*
   * Debug hook (P4 playtest evidence): exposes the pitch assembler + the card
   * queue on `window` so the puppeteer probe can render an honest-vs-crony pitch
   * pair for the SAME deal class deterministically, and force a portrait card on
   * screen for a screenshot. Read-only helpers — harmless in production.
   */
  if (typeof window !== 'undefined') {
    (window as unknown as Record<string, unknown>).__stvgDebug = {
      pitch: (family: string, cred: number, firm: string, line: string, successPct: number) =>
        assemblePitch(family as never, cred, { firm, line, successPct }, `dbg_${family}`, 'dbg_deal').text,
      showCard: (npcId: string, speaker: string, role: string, family: string | null, text: string) =>
        characters.enqueue({ npcId, speaker, role, family, color: familyColor(family), text, emotion: 'neutral', trigger: 'message' }),
      // Playtest helper: re-pull the player view + apply it (so a probe that
      // drained decisions/crises over REST can refresh the stale client memo).
      refresh: async () => {
        if (!sim.gameId) return;
        try {
          const v = await api.getState(sim.gameId);
          sim.applyPlayerView(v);
        } catch { /* best-effort */ }
      },
      // Playtest helper (P7 evidence): pause the runner so no new decisions
      // spawn, then drain every pending decision/crisis until the desk is empty
      // and refresh — so the memo overlay closes and tab content is unobstructed.
      drainAndPause: async () => {
        if (!sim.gameId) return 0;
        const gid = sim.gameId;
        wsClient.pause();
        for (let i = 0; i < 60; i++) {
          let st: { activeCrises?: { id: string; resolved?: boolean }[];
                    pendingDecisions?: { id: string; options?: { id: string }[] }[] } | null = null;
          try { st = await api.getState(gid); } catch { st = null; }
          if (!st) break;
          for (const c of st.activeCrises ?? []) {
            if (!c.resolved) { try { await api.respondCrisis(gid, c.id, 'measured'); } catch { /* */ } }
          }
          for (const d of st.pendingDecisions ?? []) {
            // Prefer the LAST option (typically "Pass" — 100% odds, no AP cost)
            // so the submit succeeds even when action points are exhausted; the
            // first option ("Take Action") often needs AP we no longer have.
            if (d.options?.length) {
              const opt = d.options[d.options.length - 1];
              try { await api.submitDecision(gid, d.id, opt.id); } catch { /* */ }
            }
          }
          const remaining = (st.pendingDecisions?.length ?? 0) + (st.activeCrises?.length ?? 0);
          if (remaining === 0) break;
        }
        try { const v = await api.getState(gid); sim.applyPlayerView(v); } catch { /* */ }
        return sim.pendingDecisions.length;
      },
    };
  }

  // Player telemetry — record how the game is played (local JSONL on the server).
  // Inject the sim-context provider FIRST so every event (incl. session_start)
  // is auto-stamped with {year, quarter, day, capital, paused, speed} (P1). The
  // provider is a function, not a store import, so telemetry.ts stays free of a
  // circular import back into the stores.
  telemetry.setContext(() => ({
    year: sim.year,
    quarter: sim.quarter,
    day: sim.day,
    capital: sim.bankCapital,
    paused: sim.paused,
    speed: sim.speed,
  }));
  $effect(() => { telemetry.init(); });

  // Auto-route to game-over when the engine emits a gameEnd payload.
  $effect(() => {
    if (sim.gameEnd && ui.screen === 'game') {
      ui.screen = 'game-over';
    }
  });

  // World-news pop-ups: surface fresh engine events as toast "wire" cards while
  // playing. Seed on first run so the opening backlog doesn't spam.
  let newsSeen = new Set<string>();
  let newsPrimed = false;
  $effect(() => {
    if (ui.screen !== 'game') return;
    const evs = sim.events ?? [];
    if (!newsPrimed) { for (const e of evs) if (e?.id) newsSeen.add(e.id); newsPrimed = true; return; }
    for (const e of evs) {
      const id = e?.id;
      if (!id || newsSeen.has(id)) continue;
      newsSeen.add(id);
      toasts.push(`📰 ${e.title}`, 'info', 6500);
      telemetry.log('news', { id, title: e.title });
    }
  });

  /*
   * Character pop-up cards (P4 §4.1). Wire the Hades-cadence triggers off the
   * state the client already receives. Each $effect tracks the minimal previous
   * value needed to detect a transition, so a card fires once per real event
   * (the queue's dedupKey + rate-limit are the final guard). Cards NEVER block
   * input (CharacterCard layer is pointer-events:none).
   */

  // (a) Character MESSAGES — quarterly briefs / advisor remarks with a named
  // speaker. Surface the freshest unseen one as a bust. Seed on first run.
  let msgSeen = new Set<string>();
  let msgPrimed = false;
  $effect(() => {
    if (ui.screen !== 'game') return;
    const msgs = sim.messages ?? [];
    const keyOf = (m: CharacterMessage) => `${m.characterName ?? ''}|${(m.content ?? '').slice(0, 50)}`;
    if (!msgPrimed) {
      for (const m of msgs) if (m && typeof m === 'object') msgSeen.add(keyOf(m as CharacterMessage));
      msgPrimed = true;
      return;
    }
    for (const m of msgs) {
      if (!m || typeof m !== 'object') continue;
      const cm = m as CharacterMessage;
      const k = keyOf(cm);
      if (msgSeen.has(k)) continue;
      msgSeen.add(k);
      cardFromMessage(cm);   // queue dedups + rate-limits
    }
  });

  // (b) CRISIS start / end — fire a preempting card on the active-crisis set
  // changing. A new id → start; an id that vanished → end.
  let prevCrisisIds = new Set<string>();
  $effect(() => {
    if (ui.screen !== 'game') return;
    const live = new Set((sim.activeCrises ?? []).filter((c) => !c.resolved).map((c) => c.id));
    for (const c of sim.activeCrises ?? []) {
      if (!c.resolved && !prevCrisisIds.has(c.id)) cardForCrisis('start', c.title, sim.year);
    }
    for (const id of prevCrisisIds) {
      if (!live.has(id)) {
        const arc = (sim.activeCrises ?? []).find((c) => c.id === id);
        cardForCrisis('end', arc?.title ?? 'The crisis', sim.year);
      }
    }
    prevCrisisIds = live;
  });

  // (c) ERA transition — voiced by the sitting president when the era name moves.
  let prevEraName = '';
  $effect(() => {
    if (ui.screen !== 'game') return;
    const name = sim.era?.name ?? '';
    if (name && name !== prevEraName) {
      if (prevEraName) cardForEra(name, sim.year);   // skip the very first assignment
      prevEraName = name;
    }
  });

  // (d) ANNUAL REPORT headline moment — once per new report year.
  let prevReportYear = 0;
  $effect(() => {
    if (ui.screen !== 'game') return;
    const r = sim.annualReport;
    if (r && r.year !== prevReportYear) {
      prevReportYear = r.year;
      cardForAnnualReport(r.year, r.headlines?.[0]);
    }
  });

  // (e2) LOAN OUTCOMES (P7) — the "loany" feedback loop. When a deal resolves the
  // engine appends a DealOutcome; bark the sourcing banker (crow or eat crow) and
  // log deal_resolved. Seed on first run so a reloaded backlog doesn't bark.
  let outcomeSeen = new Set<string>();
  let outcomePrimed = false;
  $effect(() => {
    if (ui.screen !== 'game') return;
    const outcomes = sim.dealOutcomes ?? [];
    if (!outcomePrimed) {
      for (const o of outcomes) if (o?.dealId) outcomeSeen.add(o.dealId);
      outcomePrimed = true;
      return;
    }
    for (const o of outcomes) {
      if (!o?.dealId || outcomeSeen.has(o.dealId)) continue;
      outcomeSeen.add(o.dealId);
      cardForDealOutcome(o, sim.year);   // queue dedups + rate-limits
      telemetry.log('deal_resolved', {
        id: o.dealId, outcome: o.outcome, pnl: o.realizedPnl, quartersHeld: o.quartersHeld,
      });
    }
  });

  // (f) BIG CAPITAL SWING (>±10% in a quarter). Sample capital at each quarter
  // boundary and compare to the prior quarter's close.
  let lastQuarterKey = '';
  let lastQuarterCapital = 0;
  $effect(() => {
    if (ui.screen !== 'game') return;
    const key = `${sim.year}Q${sim.quarter}`;
    if (key === lastQuarterKey) return;
    const cap = sim.bankCapital;
    if (lastQuarterCapital > 0 && lastQuarterKey) {
      const pct = ((cap - lastQuarterCapital) / lastQuarterCapital) * 100;
      if (Math.abs(pct) >= 10) cardForCapitalSwing(pct, sim.year);
    }
    lastQuarterKey = key;
    lastQuarterCapital = cap;
  });

  /*
   * Era theming — hoist [data-era] to <html> so every fixed/overlay element
   * (DecisionDrawer, SettingsOverlay, modals, toasts, NewsTicker event log)
   * inherits the era CSS variables. The title screen overrides this with its
   * own warm 1950s palette via [data-era="post-war"] on its root.
   *
   * Transition: the 1-second background ease declared in app.css on
   * html/body/#app gives the "shifting world" feel between eras.
   */
  $effect(() => {
    const era = ui.screen === 'title' ? 'post-war' : ui.eraSlug;
    document.documentElement.setAttribute('data-era', era);
  });
</script>

{#if ui.screen === 'title'}
  <TitleScreen />
{:else if ui.screen === 'game'}
  <GameView />
{:else if ui.screen === 'game-over'}
  <GameOverScreen />
{/if}

{#if ui.screen === 'game'}
  <CrisisModal />
  <AnnualReportModal />
  <EraTransition />
{/if}

{#if ui.screen === 'game'}
  <CharacterCard />
{/if}

<KeyboardHelp />
<ToastContainer />
