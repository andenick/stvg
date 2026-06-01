/*
 * Lightweight toast queue. CSS-only animations in ToastContainer.
 * No external deps; auto-expires.
 */

export type ToastKind = 'info' | 'success' | 'warning' | 'error';

export interface Toast {
  id: number;
  kind: ToastKind;
  message: string;
  ttlMs: number;
}

class ToastStore {
  items = $state<Toast[]>([]);
  private nextId = 1;

  push(message: string, kind: ToastKind = 'info', ttlMs = 4000) {
    const id = this.nextId++;
    this.items.push({ id, kind, message, ttlMs });
    if (ttlMs > 0) {
      setTimeout(() => this.dismiss(id), ttlMs);
    }
    return id;
  }

  info(msg: string, ttlMs?: number)    { return this.push(msg, 'info', ttlMs); }
  success(msg: string, ttlMs?: number) { return this.push(msg, 'success', ttlMs); }
  warn(msg: string, ttlMs?: number)    { return this.push(msg, 'warning', ttlMs); }
  error(msg: string, ttlMs = 7000)     { return this.push(msg, 'error', ttlMs); }

  dismiss(id: number) {
    const idx = this.items.findIndex((t) => t.id === id);
    if (idx !== -1) this.items.splice(idx, 1);
  }

  clear() { this.items = []; }
}

export const toasts = new ToastStore();
