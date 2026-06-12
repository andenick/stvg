/*
 * WebSocket client — connects to Crow `/ws` route, subscribes to a game,
 * dispatches typed messages into the simulation store, reconnects with
 * exponential backoff on drop.
 *
 * Server envelope (from WebSocketMessage::to_json()): { type, payload, timestamp }
 * Known types observed in engine code:
 *   - market_tick           (payload = marketTickPayload)
 *   - quarter_boundary      (decision_phase | crisis_response | quarter_complete)
 *   - game_over             ({reason, isVictory, title, narrative, state})
 *   - simulation_state      ({paused, speed})
 *   - turn/phase/changed    (legacy advance route)
 *
 * Client -> server:
 *   - {type:"subscribe", gameId}
 *   - {type:"simulation_control", gameId, action:"play"|"pause"|"set_speed", speed?}
 */

import { sim } from '../stores/simulation.svelte';
import { toasts } from '../stores/toasts.svelte';
import { ui } from '../stores/ui.svelte';
import { telemetry } from '../telemetry';
import type {
  MarketTickMsg, QuarterBoundaryMsg, GameOverMsg, PlayerView, PendingDecision,
} from '../types/server';

/**
 * "Major" non-crisis decision test (STAR_02 Addendum A.1 — W5.1). When the
 * decisionPause setting is 'pause-major', a boundary carrying any MAJOR decision
 * freezes the auto-continue scheduler until the player acts. Major = costs real
 * agency or is irreversible:
 *   • AP cost ≥ 3  (the engine's heaviest tier — acquisitions, crisis-grade,
 *                   climate/AI counter-moves all cost 3),
 *   • an M&A / acquisition (deal_approval type or an "acquire/merge" title/id),
 *   • a poach-style team buyout surfaced as a decision.
 * Routine memos (AP 0-2, ordinary operations) keep lapsing. Returns the
 * triggering decision's id + reason for telemetry, or null when none qualifies.
 *
 * Note: standalone poach OFFERS travel outside the pendingDecisions stream (the
 * player accepts them via /poach), so the ≥30%-capital poach rule has no
 * boundary to gate here; it's covered when a poach is surfaced as a decision.
 */
const MAJOR_AP_THRESHOLD = 3;
function majorDecision(decisions: PendingDecision[] | undefined): { id: string; reason: string } | null {
  for (const d of decisions ?? []) {
    if ((d.actionPointCost ?? 0) >= MAJOR_AP_THRESHOLD) {
      return { id: d.id, reason: `ap_cost_${d.actionPointCost}` };
    }
    const type = String(d.type ?? '').toLowerCase();
    const idl = String(d.id ?? '').toLowerCase();
    const title = String(d.title ?? '').toLowerCase();
    if (type === 'deal_approval' && (idl.includes('acquire') || title.includes('acqui') || title.includes('merg'))) {
      return { id: d.id, reason: 'm_and_a' };
    }
    if (idl.includes('poach') || title.includes('poach')) {
      return { id: d.id, reason: 'poach' };
    }
  }
  return null;
}

interface ServerEnvelope {
  type: string;
  payload: unknown;
  timestamp?: string;
}

type Listener = (env: ServerEnvelope) => void;

const MAX_BACKOFF_MS = 15_000;

class WSClient {
  private ws: WebSocket | null = null;
  private gameId: string | null = null;
  private backoffMs = 500;
  private intentionalClose = false;
  private listeners = new Set<Listener>();

  /*
   * Auto-continue ("watch a simulation happen") flow.
   *
   * The engine pauses at every QuarterStart decision boundary and waits for a
   * fresh `play`. Left alone the world freezes between quarters. We keep it
   * flowing: after each boundary we dwell briefly (so the decision / deal cards
   * pop up and are readable) then resume — UNLESS the player explicitly paused,
   * or it's a crisis the player must answer. Dwell shrinks as speed rises.
   */
  autoContinue = true;
  private userPaused = false;
  private autoTimer: ReturnType<typeof setTimeout> | null = null;

  private clearAuto() {
    if (this.autoTimer) { clearTimeout(this.autoTimer); this.autoTimer = null; }
  }

  // Dwell tuning (named consts — the playtest loop tunes these).
  private static readonly DWELL_DECISIONS_MS = 2600;  // pending decision/deal to read
  private static readonly DWELL_ROUTINE_MS = 850;     // routine quarter boundary
  private static readonly DWELL_MIN_MS = 300;         // floor even at 8x
  // P7 pacing: if a fresh loan card arrived within this window of the boundary,
  // grant the player a beat to read it before auto-continuing — capped so the
  // sim never stalls.
  private static readonly FRESH_DEAL_WINDOW_MS = 2000;
  private static readonly FRESH_DEAL_BONUS_MS = 1400;

  private scheduleAutoContinue(decisions?: PendingDecision[]) {
    this.clearAuto();
    if (!this.autoContinue || this.userPaused) return;
    const hasDecisions = (decisions?.length ?? 0) > 0;
    // W5.1: in 'pause-major' mode a MAJOR non-crisis decision freezes the world
    // until acted on — we simply DON'T arm the resume timer. The player resumes
    // via the decision UI (which calls wsClient.play()) once they've dealt with
    // it. Routine decisions still lapse on the timer below.
    if (hasDecisions && ui.decisionPause === 'pause-major') {
      const major = majorDecision(decisions);
      if (major) {
        telemetry.log('decision_pause_engaged', { decisionId: major.id, reason: major.reason });
        return;
      }
    }
    const speed = Math.max(1, sim.speed || 1);
    const base = hasDecisions ? WSClient.DWELL_DECISIONS_MS : WSClient.DWELL_ROUTINE_MS;
    let dwell = Math.max(WSClient.DWELL_MIN_MS, Math.round(base / Math.sqrt(speed)));
    // P7: a deal that arrived in the last 2s gets a modest, speed-scaled bonus so
    // an unread fresh loan card isn't blown past. Capped; never stalls the sim.
    const sinceDeal = Date.now() - (sim.lastDealArrivalAt || 0);
    if (sinceDeal >= 0 && sinceDeal < WSClient.FRESH_DEAL_WINDOW_MS) {
      dwell += Math.round(WSClient.FRESH_DEAL_BONUS_MS / Math.sqrt(speed));
    }
    this.autoTimer = setTimeout(() => {
      this.autoTimer = null;
      if (this.userPaused) return;
      sim.paused = false;
      this.controlAction('play');
    }, dwell);
  }

  /** Compute the ws:// URL relative to current document. */
  private url(): string {
    const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${proto}//${window.location.host}/ws`;
  }

  connect(gameId: string) {
    this.gameId = gameId;
    this.intentionalClose = false;
    this.openSocket();
  }

  disconnect() {
    this.intentionalClose = true;
    this.clearAuto();
    this.ws?.close();
    this.ws = null;
    sim.connected = false;
  }

  private openSocket() {
    if (this.ws && (this.ws.readyState === WebSocket.OPEN ||
                    this.ws.readyState === WebSocket.CONNECTING)) return;

    const ws = new WebSocket(this.url());
    this.ws = ws;
    sim.reconnecting = !sim.connected;

    ws.addEventListener('open', () => {
      sim.connected = true;
      sim.reconnecting = false;
      this.backoffMs = 500;
      if (this.gameId) this.send({ type: 'subscribe', gameId: this.gameId });
      // 0.8 cold-start fix: a fresh game sits paused at Day 0 with flat 0.0000
      // charts because nothing ever issues the first `play`. The quarter-
      // boundary auto-continue only kicks in AFTER the first quarter. So, unless
      // the player explicitly paused, auto-start the sim at the current speed
      // right after we subscribe — the world starts moving within a second of
      // Begin. (A reconnect into an already-running game is harmless: the
      // server is the source of truth for paused state and will correct us.)
      if (this.autoContinue && !this.userPaused) {
        sim.paused = false;
        this.controlAction('play');
      }
    });

    ws.addEventListener('close', () => {
      sim.connected = false;
      if (!this.intentionalClose) this.scheduleReconnect();
    });

    ws.addEventListener('error', () => {
      // close handler will fire; reconnect there
    });

    ws.addEventListener('message', (ev) => {
      let env: ServerEnvelope;
      try { env = JSON.parse(ev.data) as ServerEnvelope; }
      catch { return; }
      this.dispatch(env);
    });
  }

  private scheduleReconnect() {
    if (!sim.reconnecting) toasts.warn('Connection lost — reconnecting…');
    sim.reconnecting = true;
    const wait = this.backoffMs;
    this.backoffMs = Math.min(this.backoffMs * 2, MAX_BACKOFF_MS);
    setTimeout(() => this.openSocket(), wait);
  }

  send(msg: object) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(msg));
    }
  }

  // High-level control wrappers. play()/pause() reflect explicit player intent,
  // which gates the auto-continue scheduler.
  play()  { this.userPaused = false; this.controlAction('play'); }
  pause() { this.userPaused = true; this.clearAuto(); this.controlAction('pause'); }
  setSpeed(speed: 1 | 2 | 4 | 8) {
    this.controlAction('set_speed', speed);
  }

  private controlAction(action: 'play' | 'pause' | 'set_speed', speed?: number) {
    if (!this.gameId) return;
    this.send({ type: 'simulation_control', gameId: this.gameId, action, speed });
  }

  on(fn: Listener) {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  private dispatch(env: ServerEnvelope) {
    switch (env.type) {
      case 'market_tick': {
        const p = env.payload as MarketTickMsg;
        sim.applyMarketTick(p);
        break;
      }
      case 'quarter_boundary': {
        const p = env.payload as QuarterBoundaryMsg;
        if (p.state) sim.applyPlayerView(p.state);
        sim.paused = true;
        if (p.phase === 'decision_phase' && p.pendingDecisions?.length) {
          toasts.info(`${p.pendingDecisions.length} decision${p.pendingDecisions.length > 1 ? 's' : ''} on the desk`);
        }
        // A real crisis needs a human answer — never auto-skip it. Everything
        // else keeps the world flowing so the player can watch it unfold.
        if (p.phase !== 'crisis_response') {
          this.scheduleAutoContinue(p.pendingDecisions);
        }
        break;
      }
      case 'game_over': {
        const p = env.payload as GameOverMsg;
        if (p.state) sim.applyPlayerView(p.state);
        sim.gameEnd = { reason: p.reason, isVictory: p.isVictory, title: p.title, narrative: p.narrative };
        sim.paused = true;
        this.userPaused = true;
        this.clearAuto();
        break;
      }
      case 'simulation_state': {
        const p = env.payload as { paused: boolean; speed: 1 | 2 | 4 | 8 };
        sim.paused = !!p.paused;
        if (p.speed) sim.speed = p.speed;
        break;
      }
      case 'state': {
        sim.applyPlayerView(env.payload as PlayerView);
        break;
      }
      // turn/phase/changed and other legacy events: ignored in real-time mode
      default: break;
    }
    for (const fn of this.listeners) fn(env);
  }
}

export const wsClient = new WSClient();
