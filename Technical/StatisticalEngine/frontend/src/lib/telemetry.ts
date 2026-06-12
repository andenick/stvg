/*
 * Player telemetry — records how the player interacts so we can learn pace,
 * read-times, hovers, and decisions. Events batch in memory and POST to the
 * server (`/api/telemetry` → local JSONL). Best-effort: never blocks gameplay.
 *
 * Local-only for now; structured so a remote shipper can be added at the big
 * release. A stable per-browser sessionId persists in localStorage.
 *
 * Telemetry v2 (P1): every logged event is auto-stamped with the current sim
 * context — {year, quarter, day, capital, paused, speed} — read lazily through
 * a context-provider function injected once from App.svelte (telemetry.
 * setContext). The provider indirection avoids a circular import between this
 * module and the sim/ui stores. From one session JSONL an agent can reconstruct
 * what the player read and for how long, what they ignored, engagement, pace,
 * and the money curve.
 */

import { api } from './api/rest';

/** Auto-context stamped onto every event (P1). */
export interface TelemetryContext {
  year: number;
  quarter: number;
  day: number;
  capital: number;
  paused: boolean;
  speed: number;
}

type ContextProvider = () => TelemetryContext;

interface TelemetryEvent {
  t: number;          // ms since session start
  type: string;
  data?: Record<string, unknown>;
}

const SESSION_KEY = 'stvg.telemetrySession';

function makeSessionId(): string {
  // Avoid Math.random/Date hard-bans elsewhere; here in the browser they're fine.
  try {
    const existing = localStorage.getItem(SESSION_KEY);
    if (existing) return existing;
  } catch { /* ignore */ }
  const id = 's' + Date.now().toString(36) + Math.floor(Math.random() * 1e6).toString(36);
  try { localStorage.setItem(SESSION_KEY, id); } catch { /* ignore */ }
  return id;
}

class Telemetry {
  private sessionId = makeSessionId();
  private start = Date.now();
  private queue: TelemetryEvent[] = [];
  private flushTimer: ReturnType<typeof setInterval> | null = null;
  private hoverThrottle = new Map<string, number>();
  private contextProvider: ContextProvider | null = null;
  enabled = true;

  /**
   * Inject the sim-context provider (P1). Called once from App.svelte. Kept as
   * a function (not a direct store import) so telemetry.ts has no import edge
   * back into the stores — the dwell action and every component import telemetry
   * freely without a circular-dependency hazard.
   */
  setContext(fn: ContextProvider) {
    this.contextProvider = fn;
  }

  private context(): Partial<TelemetryContext> {
    if (!this.contextProvider) return {};
    try { return this.contextProvider(); } catch { return {}; }
  }

  init() {
    if (this.flushTimer) return;
    this.flushTimer = setInterval(() => this.flush(), 5000);
    // Flush on tab hide / unload so we don't lose the tail.
    if (typeof window !== 'undefined') {
      window.addEventListener('pagehide', () => this.flush(true));
      window.addEventListener('visibilitychange', () => {
        if (document.visibilityState === 'hidden') this.flush(true);
      });
    }
    // Self-describing session header (P1.2): emitted client-side since the
    // telemetry endpoint just appends batches verbatim. Lets an analyzer read a
    // file standalone without a server-written meta line.
    this.log('session_start', {
      ua: typeof navigator !== 'undefined' ? navigator.userAgent : '',
      sessionId: this.sessionId,
      startedAt: new Date().toISOString(),
    });
  }

  log(type: string, data?: Record<string, unknown>) {
    if (!this.enabled) return;
    // Auto-stamp sim context onto every event (P1). The explicit `data` wins on
    // key collisions, so a caller may override e.g. `capital` if it needs to.
    const stamped = { ...this.context(), ...(data ?? {}) };
    this.queue.push({ t: Date.now() - this.start, type, data: stamped });
    if (this.queue.length >= 40) this.flush();
  }

  /** Throttled hover logger — coalesces rapid hovers over the same key. */
  hover(key: string, data?: Record<string, unknown>) {
    const now = Date.now();
    const last = this.hoverThrottle.get(key) ?? 0;
    if (now - last < 800) return;
    this.hoverThrottle.set(key, now);
    this.log('hover', { key, ...data });
  }

  flush(force = false) {
    if (this.queue.length === 0) return;
    if (!this.enabled && !force) return;
    const batch = this.queue;
    this.queue = [];
    void api.logTelemetry(this.sessionId, batch);
  }
}

export const telemetry = new Telemetry();
