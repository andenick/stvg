/*
 * UI store — screen routing, layout phase, contextual-hint dismissal,
 * keyboard-help visibility, settings overlay state. Pure UI; no server data.
 */

import { uiPhase, eraSlug, type EraSlug } from '../types/server';
import { sim } from './simulation.svelte';

export type Screen = 'title' | 'game' | 'game-over';

class UIStore {
  screen = $state<Screen>('title');
  settingsOpen = $state(false);
  decisionsOpen = $state(false);
  keyboardHelpOpen = $state(false);
  saveLoadOpen = $state(false);
  fullEventLogOpen = $state(false);

  /** Phase 1-4 derived from current sim year. */
  phase = $derived<1 | 2 | 3 | 4>(uiPhase(sim.year));

  /** Era CSS slug for [data-era=""]. */
  eraSlug = $derived<EraSlug>(eraSlug(sim.era?.name ?? 'Post-War Stability'));

  dismissedHints = $state<Set<string>>(this.loadDismissed());

  private loadDismissed(): Set<string> {
    try {
      const raw = localStorage.getItem('stvg.dismissedHints');
      if (raw) return new Set(JSON.parse(raw) as string[]);
    } catch { /* ignore */ }
    return new Set();
  }

  dismissHint(id: string) {
    this.dismissedHints.add(id);
    try {
      localStorage.setItem('stvg.dismissedHints', JSON.stringify([...this.dismissedHints]));
    } catch { /* ignore */ }
  }

  isHintDismissed(id: string): boolean {
    return this.dismissedHints.has(id);
  }
}

export const ui = new UIStore();
