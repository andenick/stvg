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
import type { MarketTickMsg, QuarterBoundaryMsg, GameOverMsg, PlayerView } from '../types/server';

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

  // High-level control wrappers
  play()  { this.controlAction('play'); }
  pause() { this.controlAction('pause'); }
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
          toasts.info(`${p.pendingDecisions.length} decision${p.pendingDecisions.length > 1 ? 's' : ''} pending`);
        }
        break;
      }
      case 'game_over': {
        const p = env.payload as GameOverMsg;
        if (p.state) sim.applyPlayerView(p.state);
        sim.gameEnd = { reason: p.reason, isVictory: p.isVictory, title: p.title, narrative: p.narrative };
        sim.paused = true;
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
