/*
 * Character-card queue (STAR_02 §4.1) — the Persona-bustup × Hades-cadence layer.
 *
 * Modeled on toasts.svelte.ts, but a SINGLE active card at a time with a priority
 * queue and a HARD rate limit: at most one card surfaces per RATE_LIMIT_MS (20s).
 * Crisis-priority cards PREEMPT the active card and bypass the rate limit (the
 * world is on fire — the player should hear it now).
 *
 * The card NEVER blocks gameplay input — that's enforced in CharacterCard.svelte
 * (pointer-events only on the bust itself). This store only manages WHICH bark is
 * showing and WHEN the next may appear.
 *
 * Telemetry (portrait_shown / portrait_dismissed) is emitted by the component on
 * mount/dismiss so the dwell ms and read:bool are accurate to the DOM lifetime.
 */

import { telemetry } from '../telemetry';
import type { Emotion } from '../portraits';

export type CardTrigger =
  | 'message' | 'advisor' | 'crisis_start' | 'crisis_end'
  | 'era' | 'annual_report' | 'capital_swing' | 'deal_pitch' | 'deal_outcome' | 'quarter';

/** Priority: crisis preempts everything; higher number wins ties in the queue. */
const PRIORITY: Record<CardTrigger, number> = {
  crisis_start: 100, crisis_end: 90,
  advisor: 60, deal_outcome: 58, deal_pitch: 55,
  annual_report: 40, era: 45, capital_swing: 50,
  message: 30, quarter: 20,
};

export interface CharacterCard {
  id: number;
  npcId: string;
  speaker: string;     // display name
  role?: string;       // subtitle ("Chief Risk Officer")
  family: string | null;
  color: string;       // nameplate accent
  text: string;
  emotion: Emotion;
  trigger: CardTrigger;
  /** Optional dedup key — a card with a key already queued/shown is dropped. */
  dedupKey?: string;
}

type QueuedCard = Omit<CharacterCard, 'id'>;

const RATE_LIMIT_MS = 20000;       // ≤ 1 card per 20s (crisis bypasses)
const AUTO_DISMISS_MS = 12000;     // unread cards self-dismiss after 12s

class CharacterStore {
  /** The single visible card, or null. */
  active = $state<CharacterCard | null>(null);

  private queue: QueuedCard[] = [];
  private nextId = 1;
  private lastShownAt = 0;
  private seenKeys = new Set<string>();
  private pumpTimer: ReturnType<typeof setTimeout> | null = null;

  /**
   * Enqueue a card. Crisis triggers preempt the active card immediately; others
   * join the priority queue and surface subject to the rate limit. Returns false
   * if dropped (dedup).
   */
  enqueue(card: QueuedCard): boolean {
    if (card.dedupKey && this.seenKeys.has(card.dedupKey)) return false;
    if (card.dedupKey) this.seenKeys.add(card.dedupKey);

    const isCrisis = card.trigger === 'crisis_start' || card.trigger === 'crisis_end';
    if (isCrisis) {
      // Preempt: the current card (if any) is dismissed, crisis shows now.
      if (this.active) this.finishActive(false);
      this.show(card);
      return true;
    }

    // Insert by priority (stable: higher priority toward the front).
    const p = PRIORITY[card.trigger] ?? 0;
    let i = 0;
    while (i < this.queue.length && (PRIORITY[this.queue[i].trigger] ?? 0) >= p) i++;
    this.queue.splice(i, 0, card);
    this.pump();
    return true;
  }

  /** Try to surface the next queued card if the rate limit allows. */
  private pump() {
    if (this.active) return;
    if (this.queue.length === 0) return;
    const now = Date.now();
    const wait = this.lastShownAt + RATE_LIMIT_MS - now;
    if (wait > 0) {
      if (!this.pumpTimer) this.pumpTimer = setTimeout(() => { this.pumpTimer = null; this.pump(); }, wait + 20);
      return;
    }
    const next = this.queue.shift()!;
    this.show(next);
  }

  private show(card: QueuedCard) {
    const full: CharacterCard = { ...card, id: this.nextId++ };
    this.active = full;
    this.lastShownAt = Date.now();
    // portrait_shown is logged by the component onMount (accurate to render).
  }

  /** Called by the component when its dwell completes (dismiss or auto). */
  finishActive(read: boolean, ms?: number) {
    const cur = this.active;
    if (!cur) return;
    this.active = null;
    if (ms != null) {
      telemetry.log('portrait_dismissed', { npcId: cur.npcId, ms, read, trigger: cur.trigger });
    }
    // Allow the next card to surface (subject to rate limit) on the next tick.
    queueMicrotask(() => this.pump());
  }

  /** Convenience for the component to read constants. */
  get autoDismissMs() { return AUTO_DISMISS_MS; }

  clear() {
    this.queue = [];
    this.active = null;
    if (this.pumpTimer) { clearTimeout(this.pumpTimer); this.pumpTimer = null; }
  }
}

export const characters = new CharacterStore();
