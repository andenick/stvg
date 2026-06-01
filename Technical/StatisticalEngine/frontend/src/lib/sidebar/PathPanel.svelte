<script lang="ts">
  /*
   * PathPanel — reads sim.path (5 axes + archetype). Fixes visual_polish #1
   * (server emits these axes but old React panel hardcoded 0.5).
   *
   * Visible from year 1955 — when path divergence becomes meaningful.
   */

  import { sim } from '../stores/simulation.svelte';

  const AXES = [
    { key: 'riskCulture',         label: 'Risk Culture',         leftLabel: 'Conservative', rightLabel: 'Gunslinger' },
    { key: 'internationalFocus',  label: 'International Focus',  leftLabel: 'Domestic',     rightLabel: 'Global' },
    { key: 'techInvestment',      label: 'Tech Investment',      leftLabel: 'Traditional',  rightLabel: 'Innovator' },
    { key: 'politicalCapital',    label: 'Political Capital',    leftLabel: 'Outsider',     rightLabel: 'Insider' },
    { key: 'innovationBias',      label: 'Innovation Bias',      leftLabel: 'Follower',     rightLabel: 'Pioneer' },
  ] as const;

  function val(key: string): number {
    if (!sim.path) return 0.5;
    return Math.min(1, Math.max(0, ((sim.path as unknown) as Record<string, number>)[key] ?? 0.5));
  }
</script>

{#if sim.path && sim.year >= 1955}
  <section class="panel" aria-label="Path">
    <header>
      <h3>Path</h3>
      {#if sim.path.archetype}
        <span class="archetype">{sim.path.archetype}</span>
      {/if}
    </header>
    <div class="axes">
      {#each AXES as axis (axis.key)}
        {@const v = val(axis.key)}
        <div class="axis">
          <span class="axis-label">{axis.label}</span>
          <div class="bar" aria-hidden="true">
            <span class="bar-center"></span>
            <span class="bar-fill" style="left: {Math.min(v, 0.5) * 100}%; width: {Math.abs(v - 0.5) * 100}%"></span>
            <span class="bar-marker" style="left: {v * 100}%"></span>
          </div>
          <div class="poles">
            <span>{axis.leftLabel}</span>
            <span>{axis.rightLabel}</span>
          </div>
        </div>
      {/each}
    </div>
    {#if sim.path.majorDecisions && sim.path.majorDecisions.length > 0}
      <p class="decisions">{sim.path.majorDecisions.length} major decision(s) recorded</p>
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
    letter-spacing: 0.04em;
    flex: 1;
  }
  .archetype {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--accent-secondary);
    letter-spacing: 0.15em;
    text-transform: uppercase;
    padding: 0.15rem 0.4rem;
    border: 1px solid var(--border);
    border-radius: 2px;
  }
  .axes { display: flex; flex-direction: column; gap: 0.65rem; }
  .axis { display: flex; flex-direction: column; gap: 0.2rem; }
  .axis-label {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.1em;
  }
  .bar {
    position: relative;
    height: 4px;
    background: var(--bg-base);
    border-radius: 1px;
  }
  .bar-center {
    position: absolute;
    left: 50%; top: 0; bottom: 0;
    width: 1px;
    background: var(--border);
  }
  .bar-fill {
    position: absolute;
    top: 0; bottom: 0;
    background: var(--accent-primary);
    opacity: 0.35;
  }
  .bar-marker {
    position: absolute;
    top: -2px; bottom: -2px;
    width: 2px;
    transform: translateX(-1px);
    background: var(--accent-primary);
  }
  .poles {
    display: flex;
    justify-content: space-between;
    font-family: var(--font-chrome);
    font-size: 0.6rem;
    color: var(--fg-muted);
    letter-spacing: 0.05em;
  }
  .decisions {
    margin: 0.6rem 0 0 0;
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.08em;
    border-top: 1px solid var(--border-soft);
    padding-top: 0.5rem;
  }
</style>
