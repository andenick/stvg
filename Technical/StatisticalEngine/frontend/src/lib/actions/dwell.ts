/*
 * `dwell` — a Svelte action that measures how long a readable surface was
 * actually on screen and visible, then logs a `dwell {key, ms, ...data}` event
 * when the element unmounts / closes (P1 read-time framework).
 *
 * Usage:
 *   <div use:dwell={'decision_memo'}>…</div>
 *   <article use:dwell={{ key: `decision:${d.id}`, data: { title: d.title } }}>…</article>
 *
 * Semantics:
 *   - Visibility is tracked with an IntersectionObserver so time spent while the
 *     panel is scrolled off-screen or covered (intersectionRatio drops to ~0)
 *     does NOT count. We accumulate only the intervals where the element is at
 *     least partially visible.
 *   - Document visibility (tab hidden) also pauses the accumulator.
 *   - On destroy we emit `dwell` only when the accumulated visible time is
 *     ≥ MIN_MS (300ms) — a glance that never settled is noise, not a read.
 *   - The `key` is what groups dwell records by surface in the analyzer
 *     (e.g. `decision:<id>`, `deal:<id>`, `market:<id>`, `panel:PathPanel`).
 *
 * The accumulator is robust to repeated open/close of the same logical surface
 * because each mount creates a fresh action instance with its own timer.
 */

import { telemetry } from '../telemetry';

const MIN_MS = 300;

export interface DwellOptions {
  key: string;
  data?: Record<string, unknown>;
}

type DwellParam = string | DwellOptions;

function normalize(param: DwellParam): DwellOptions {
  return typeof param === 'string' ? { key: param } : param;
}

export function dwell(node: HTMLElement, param: DwellParam) {
  let opts = normalize(param);

  let accumulatedMs = 0;
  let visibleSince: number | null = null;   // performance.now() when visibility began
  let elementVisible = false;               // intersection state
  let docVisible = typeof document === 'undefined' || document.visibilityState === 'visible';

  function now(): number {
    return typeof performance !== 'undefined' ? performance.now() : Date.now();
  }

  /** Recompute whether we should be counting, opening/closing the interval. */
  function sync() {
    const counting = elementVisible && docVisible;
    if (counting && visibleSince === null) {
      visibleSince = now();
    } else if (!counting && visibleSince !== null) {
      accumulatedMs += now() - visibleSince;
      visibleSince = null;
    }
  }

  // IntersectionObserver: any sliver of the element on screen counts as visible.
  let io: IntersectionObserver | null = null;
  if (typeof IntersectionObserver !== 'undefined') {
    io = new IntersectionObserver((entries) => {
      for (const e of entries) {
        elementVisible = e.isIntersecting && e.intersectionRatio > 0;
      }
      sync();
    }, { threshold: [0, 0.01] });
    io.observe(node);
  } else {
    // No IO support — assume visible for the element's lifetime.
    elementVisible = true;
    sync();
  }

  function onDocVisibility() {
    docVisible = document.visibilityState === 'visible';
    sync();
  }
  if (typeof document !== 'undefined') {
    document.addEventListener('visibilitychange', onDocVisibility);
  }

  function totalMs(): number {
    let total = accumulatedMs;
    if (visibleSince !== null) total += now() - visibleSince;
    return total;
  }

  function emit() {
    // Close any open interval into the total.
    sync();
    const ms = Math.round(totalMs());
    if (ms >= MIN_MS) {
      telemetry.log('dwell', { key: opts.key, ms, ...(opts.data ?? {}) });
    }
  }

  return {
    update(next: DwellParam) {
      const n = normalize(next);
      // If the key changed, the surface logically became a different thing —
      // flush the old one and restart the accumulator under the new key.
      if (n.key !== opts.key) {
        emit();
        accumulatedMs = 0;
        visibleSince = null;
        opts = n;
        sync();
      } else {
        opts = n;
      }
    },
    destroy() {
      emit();
      io?.disconnect();
      if (typeof document !== 'undefined') {
        document.removeEventListener('visibilitychange', onDocVisibility);
      }
    },
  };
}

/*
 * `panelView` — fire a one-shot `panel_view {panel}` the moment a sidebar panel
 * element is mounted into the DOM (i.e. its year-gate `{#if}` became true). The
 * action body runs exactly at mount, which is the precise "panel became visible"
 * signal we want — unlike a component-level onMount, which fires when the parent
 * layout mounts regardless of the gate. Pair with `use:dwell` for read-time.
 */
export function panelView(_node: HTMLElement, panel: string) {
  telemetry.log('panel_view', { panel });
  return {};
}
