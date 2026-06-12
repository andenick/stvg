<script lang="ts">
  /*
   * Regulatory frameworks status — active rule set badges. Visible from
   * 1988 when Basel I lands; Glass-Steagall etc are noted whenever active.
   */

  import { sim } from '../stores/simulation.svelte';
  import { dwell, panelView } from '../actions/dwell';

  interface Rule { key: string; label: string; activeIf: boolean | undefined; tone: 'ok' | 'warn' | 'crit' }

  let rules = $derived<Rule[]>([
    { key: 'glassSteagall', label: 'Glass-Steagall', activeIf: sim.regulatory?.glassSteagallActive, tone: 'warn' },
    { key: 'regQ',          label: 'Regulation Q',   activeIf: sim.regulatory?.regulationQ,        tone: 'warn' },
    { key: 'baselI',        label: 'Basel I',        activeIf: sim.regulatory?.baselI,             tone: 'ok' },
    { key: 'baselII',       label: 'Basel II',       activeIf: sim.regulatory?.baselII,            tone: 'ok' },
    { key: 'baselIII',      label: 'Basel III',      activeIf: sim.regulatory?.baselIII,           tone: 'ok' },
    { key: 'doddFrank',     label: 'Dodd-Frank',     activeIf: sim.regulatory?.doddFrank,          tone: 'warn' },
    { key: 'volckerRule',   label: 'Volcker Rule',   activeIf: sim.regulatory?.volckerRule,        tone: 'crit' },
  ]);

  let activeRules = $derived(rules.filter((r) => r.activeIf));
</script>

{#if sim.regulatory && sim.year >= 1988}
  <section class="panel" aria-label="Regulatory" use:dwell={'panel:RegulatoryPanel'} use:panelView={'RegulatoryPanel'}>
    <header>
      <h3>Regulatory</h3>
      {#if sim.regulatory.capitalRequirement != null}
        <span class="cap">Min Cap {(sim.regulatory.capitalRequirement * 100).toFixed(1)}%</span>
      {/if}
    </header>
    {#if activeRules.length === 0}
      <p class="empty">No major frameworks active.</p>
    {:else}
      <div class="badges">
        {#each activeRules as r (r.key)}
          <span class="badge tone-{r.tone}">{r.label}</span>
        {/each}
      </div>
    {/if}
  </section>
{/if}

<style>
  .panel {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 0.85rem 1rem;
  }
  header { display: flex; align-items: baseline; gap: 0.5rem; margin-bottom: 0.5rem; }
  header h3 {
    font-family: var(--font-display);
    color: var(--accent-primary);
    margin: 0;
    font-size: 1rem;
    flex: 1;
  }
  .cap {
    font-family: var(--font-data);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
  }
  .badges { display: flex; flex-wrap: wrap; gap: 0.3rem; }
  .badge {
    font-family: var(--font-chrome);
    font-size: 0.62rem;
    letter-spacing: 0.12em;
    padding: 0.2rem 0.5rem;
    border-radius: 2px;
    border: 1px solid var(--border);
    text-transform: uppercase;
  }
  .badge.tone-ok    { border-color: var(--accent-success); color: var(--accent-success); }
  .badge.tone-warn  { border-color: var(--accent-warning); color: var(--accent-warning); }
  .badge.tone-crit  { border-color: var(--accent-danger);  color: var(--accent-danger); }
  .empty { color: var(--fg-secondary); margin: 0; font-size: var(--fs-sm); }
</style>
