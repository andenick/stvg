/*
 * Feed helpers. The engine emits advisor / quarterly-brief `messages` as
 * OBJECTS ({characterName, content, role, sentiment, tone}), but a few code
 * paths still pass plain strings. These coercions keep any consumer safe —
 * fixes the `m?.trim is not a function` crash that froze the live game view.
 */

import type { FeedMessage } from '../types/server';

/** Extract display text from a string or CharacterMessage-like object. */
export function messageText(m: unknown): string {
  if (m == null) return '';
  if (typeof m === 'string') return m.trim();
  if (typeof m === 'object') {
    const o = m as Record<string, unknown>;
    const v = o.content ?? o.text ?? o.description ?? o.message ?? '';
    return typeof v === 'string' ? v.trim() : String(v ?? '').trim();
  }
  return String(m).trim();
}

/** Speaker label ("Dr. Pat Martinez · Chief Risk Officer") if present. */
export function messageSpeaker(m: unknown): string {
  if (!m || typeof m !== 'object') return '';
  const o = m as Record<string, unknown>;
  const name = typeof o.characterName === 'string' ? o.characterName : '';
  const role = typeof o.role === 'string' ? o.role : '';
  if (name && role) return `${name} · ${role}`;
  return name || role || '';
}

/** Tone of a message ('worried' | 'confident' | ...) if present. */
export function messageTone(m: unknown): string {
  if (!m || typeof m !== 'object') return '';
  const o = m as Record<string, unknown>;
  return typeof o.tone === 'string' ? o.tone : '';
}

export type { FeedMessage };
