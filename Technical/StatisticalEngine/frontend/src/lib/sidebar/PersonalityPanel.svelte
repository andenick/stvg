<script lang="ts">
  /*
   * 11-axis CEO personality radar (visual_polish #1: server emits these axes
   * but old React radar hardcoded 0.5). SVG radar so we don't pull a chart
   * library for one diagram.
   */

  import { sim } from '../stores/simulation.svelte';

  const AXES = [
    { key: 'riskTolerance',     label: 'Risk' },
    { key: 'growthAmbition',    label: 'Growth' },
    { key: 'complianceFocus',   label: 'Compliance' },
    { key: 'politicalSavvy',    label: 'Politics' },
    { key: 'analyticalAbility', label: 'Analysis' },
    { key: 'persuasion',        label: 'Persuasion' },
    { key: 'leadership',        label: 'Leadership' },
    { key: 'adaptability',      label: 'Adapt' },
    { key: 'integrity',         label: 'Integrity' },
    { key: 'stressResponse',    label: 'Stress' },
    { key: 'longTermFocus',     label: 'Long-term' },
  ] as const;

  const SIZE = 180;
  const CENTER = SIZE / 2;
  const RADIUS = (SIZE / 2) - 30;

  function axisAngle(i: number): number {
    return -Math.PI / 2 + (i * 2 * Math.PI) / AXES.length;
  }

  function point(i: number, magnitude: number): [number, number] {
    const a = axisAngle(i);
    return [
      CENTER + Math.cos(a) * RADIUS * magnitude,
      CENTER + Math.sin(a) * RADIUS * magnitude,
    ];
  }

  let polygonPoints = $derived(() => {
    if (!sim.ceoPersonality) return '';
    const p = (sim.ceoPersonality as unknown) as Record<string, number>;
    return AXES.map((a, i) => {
      const v = Math.min(1, Math.max(0, p[a.key] ?? 0.5));
      const [x, y] = point(i, v);
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    }).join(' ');
  });

  let gridRings = $derived([0.25, 0.5, 0.75, 1.0]);
</script>

{#if sim.ceoPersonality}
  <section class="panel" aria-label="CEO Personality">
    <header>
      <h3>Personality</h3>
      {#if sim.ceoName}<span class="ceo-name">{sim.ceoName}</span>{/if}
    </header>
    <svg viewBox="0 0 {SIZE} {SIZE}" class="radar" aria-hidden="true">
      <!-- grid rings -->
      {#each gridRings as r}
        <polygon
          class="grid-ring"
          points={AXES.map((_, i) => {
            const [x, y] = point(i, r);
            return `${x.toFixed(1)},${y.toFixed(1)}`;
          }).join(' ')}
        />
      {/each}
      <!-- axis spokes -->
      {#each AXES as _, i}
        {@const [ex, ey] = point(i, 1)}
        <line class="spoke" x1={CENTER} y1={CENTER} x2={ex} y2={ey} />
      {/each}
      <!-- data polygon -->
      <polygon class="data" points={polygonPoints()} />
      <!-- axis labels -->
      {#each AXES as axis, i}
        {@const [lx, ly] = point(i, 1.15)}
        <text
          x={lx}
          y={ly}
          class="axis-label"
          text-anchor={lx < CENTER - 1 ? 'end' : lx > CENTER + 1 ? 'start' : 'middle'}
          dominant-baseline="middle"
        >{axis.label}</text>
      {/each}
    </svg>
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
  .ceo-name {
    font-family: var(--font-chrome);
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    letter-spacing: 0.1em;
  }
  .radar { width: 100%; height: auto; display: block; }
  .grid-ring {
    fill: none;
    stroke: var(--border);
    stroke-width: 0.5;
  }
  .spoke {
    stroke: var(--border-soft);
    stroke-width: 0.5;
  }
  .data {
    fill: var(--accent-primary);
    fill-opacity: 0.25;
    stroke: var(--accent-primary);
    stroke-width: 1.25;
    stroke-linejoin: round;
  }
  .axis-label {
    font-family: var(--font-chrome);
    font-size: 8px;
    fill: var(--fg-secondary);
    letter-spacing: 0.05em;
  }
</style>
