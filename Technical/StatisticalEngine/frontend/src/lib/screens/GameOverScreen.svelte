<script lang="ts">
  /*
   * Cinematic game-over treatment per WP11.
   *
   *   Victory  -> gold gradient background, slow gold particles rising,
   *               bank name fade-in, legacy stats stagger in one by one.
   *   Defeat   -> dark red vignette, screen desaturate, somber timing.
   *   Both     -> Play Again appears after 3 seconds.
   *   Both     -> Timeline of path.majorDecisions across 95 years.
   *   Both     -> Cause-of-end explanatory copy for every reason the engine
   *               emits (7 death + 6 endgame paths + survivor).
   */

  import { sim } from '../stores/simulation.svelte';
  import { ui } from '../stores/ui.svelte';
  import { wsClient } from '../ws/websocket';
  import { onMount } from 'svelte';
  import { fmtMoney } from '../util/money';
  import { telemetry } from '../telemetry';
  import type { GameEndReason } from '../types/server';

  let playAgainReady = $state(false);
  let timer: ReturnType<typeof setTimeout> | undefined;

  onMount(() => {
    // Lifecycle: game_end (P1). Mount is the single point the run is over.
    telemetry.log('game_end', {
      reason: sim.gameEnd?.reason ?? 'unknown',
      isVictory: sim.gameEnd?.isVictory ?? false,
      finalCapital: sim.bankCapital,
      year: sim.year,
    });
    telemetry.flush(true);
    timer = setTimeout(() => { playAgainReady = true; }, 3000);
    return () => clearTimeout(timer);
  });

  function playAgain() {
    wsClient.disconnect();
    sim.reset();
    ui.screen = 'title';
  }

  let isVictory = $derived(sim.gameEnd?.isVictory ?? false);

  function causeCopy(reason: GameEndReason | string | undefined): string {
    switch (reason) {
      case 'Bankruptcy':
        return 'Capital exhausted. Creditors moved in. The bank was wound down.';
      case 'BoardFired':
        return 'The board lost confidence. You were dismissed and the bank passed to a new CEO.';
      case 'RegulatorySeizure':
        return 'Regulators seized control. Capital ratios fell below mandatory minimums for too long.';
      case 'LiquidityCrisis':
        return 'A run on deposits. The bank could not meet obligations and was resolved by the regulator.';
      case 'FraudScandal':
        return 'A scandal broke. Trust collapsed, key staff departed, and the institution did not survive the fallout.';
      case 'AISingularity':
        return 'An AI bank captured the market. Traditional banking was outcompeted on every dimension.';
      case 'ClimateCatastrophe':
        return 'Climate tipping points crossed. Collateral evaporated, insurance failed, the financial system restructured.';
      case 'Reconstruction':
        return 'You rebuilt banking as patient capital — slower returns, deeper roots, a different kind of victory.';
      case 'ShadowEmperor':
        return 'You became the shadow architecture. Banking law applied to others; you operated above it.';
      case 'AIPartnership':
        return 'You partnered with the AI rather than fought it. A hybrid institution emerged, neither pure machine nor pure human.';
      case 'VoluntaryAbolition':
        return 'You chose to wind down the old form of banking. The successor system you helped midwife may yet be remembered as your legacy.';
      case 'Catastrophe':
        return 'Banking, as you knew it, did not survive 2040. Few institutions did.';
      case 'Fortress':
        return 'You closed the gates. Modest growth, modest losses, modest legacy. The bank still stands.';
      case 'SurvivorWin':
        return 'Ninety-five years through every storm. You did not become the largest, but you became the oldest.';
      default:
        return sim.gameEnd?.narrative ?? 'The game has ended.';
    }
  }

  const fmt = (v: number | undefined): string => fmtMoney(v);
</script>

<main class="game-over" class:victory={isVictory} class:defeat={!isVictory}>
  {#if isVictory}
    <!-- Gold particles for the victory treatment -->
    <div class="particles" aria-hidden="true">
      {#each Array(24) as _, i}
        <span class="particle" style="left: {(i * 4.3 + (i % 3) * 7) % 100}%; animation-delay: {i * 0.15}s;"></span>
      {/each}
    </div>
  {/if}

  <div class="content">
    <p class="kind" class:gold={isVictory}>{isVictory ? '◆ LEGACY' : '☠ END'}</p>
    <h1 class="title">{sim.gameEnd?.title ?? (isVictory ? 'A Quiet Victory' : 'Game Over')}</h1>
    <p class="cause">{causeCopy(sim.gameEnd?.reason)}</p>

    <div class="stats">
      <div class="stat"><span class="label">Years Survived</span><span class="num val">{Math.max(0, sim.year - 1945)}</span></div>
      <div class="stat"><span class="label">Final Capital</span><span class="num val">{fmt(sim.bankCapital)}</span></div>
      <div class="stat"><span class="label">Bank Name</span><span class="val name">{sim.bank?.name ?? '—'}</span></div>
      <div class="stat"><span class="label">Era at End</span><span class="val name">{sim.era?.name ?? '—'}</span></div>
      {#if sim.progression}
        <div class="stat"><span class="label">Final Score</span><span class="num val">{sim.progression.totalScore.toFixed(0)}</span></div>
      {/if}
      {#if sim.path?.archetype}
        <div class="stat"><span class="label">Archetype</span><span class="val name">{sim.path.archetype}</span></div>
      {/if}
    </div>

    {#if sim.path?.majorDecisions && sim.path.majorDecisions.length > 0}
      <section class="timeline" aria-label="Major decisions">
        <h2>Decisions That Mattered</h2>
        <ol>
          {#each sim.path.majorDecisions as d, i (i)}
            <li>
              <span class="year num">{d.year} Q{d.quarter}</span>
              <span class="event">{d.label}</span>
            </li>
          {/each}
        </ol>
      </section>
    {/if}

    {#if sim.gameEnd?.narrative && sim.gameEnd.narrative !== causeCopy(sim.gameEnd?.reason)}
      <p class="narrative">{sim.gameEnd.narrative}</p>
    {/if}

    {#if playAgainReady}
      <button class="play-again" onclick={playAgain}>Play Again</button>
    {:else}
      <span class="waiting" aria-hidden="true"></span>
    {/if}
  </div>
</main>

<style>
  .game-over {
    position: relative;
    min-height: 100vh;
    padding: 4rem 2rem 3rem 2rem;
    display: grid;
    place-items: center;
    text-align: center;
    background: var(--bg-base);
    color: var(--fg-primary);
    font-family: var(--font-body);
    overflow: hidden;
    animation: fadeIn 1200ms var(--ease);
  }
  .game-over.victory {
    background:
      radial-gradient(ellipse at center, rgba(196, 163, 90, 0.18) 0%, transparent 60%),
      var(--bg-base);
  }
  .game-over.defeat {
    background:
      radial-gradient(ellipse at center, rgba(168, 69, 58, 0.18) 0%, transparent 60%),
      var(--bg-base);
    filter: saturate(0.7);
  }

  .content {
    max-width: 40rem;
    width: 100%;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 1.5rem;
    z-index: 2;
  }

  .kind {
    font-family: var(--font-chrome);
    color: var(--accent-danger);
    letter-spacing: 0.5em;
    font-size: 0.75rem;
    margin: 0;
  }
  .kind.gold { color: var(--accent-primary); }

  .title {
    font-family: var(--font-display);
    font-size: 3.5rem;
    color: var(--fg-primary);
    margin: 0;
    letter-spacing: 0.02em;
    line-height: 1.05;
    animation: fadeIn 1800ms var(--ease);
  }
  .victory .title {
    color: var(--accent-primary);
    text-shadow: 0 0 28px rgba(196, 163, 90, 0.45);
  }
  .defeat .title { color: var(--accent-danger); }

  .cause {
    font-size: 1.1rem;
    color: var(--fg-secondary);
    line-height: 1.55;
    max-width: 32rem;
    margin: 0;
    animation: fadeIn 1800ms var(--ease) 0.6s both;
  }

  .stats {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(10rem, 1fr));
    gap: 1rem 1.5rem;
    padding: 1.25rem 1.5rem;
    border-top: 1px solid var(--border);
    border-bottom: 1px solid var(--border);
    width: 100%;
    animation: fadeIn 1400ms var(--ease) 1.2s both;
  }
  .stat { display: flex; flex-direction: column; gap: 0.2rem; }
  .label {
    font-family: var(--font-chrome);
    font-size: 0.65rem;
    letter-spacing: 0.22em;
    color: var(--fg-secondary);
    text-transform: uppercase;
  }
  .val {
    font-family: var(--font-data);
    font-size: 1.1rem;
    color: var(--fg-primary);
    font-variant-numeric: tabular-nums;
  }
  .val.name { font-family: var(--font-body); font-size: 1rem; }

  .timeline {
    width: 100%;
    text-align: left;
    animation: fadeIn 1400ms var(--ease) 1.8s both;
  }
  .timeline h2 {
    font-family: var(--font-display);
    color: var(--accent-primary);
    font-size: 1.15rem;
    margin: 0 0 0.75rem 0;
    text-align: center;
    letter-spacing: 0.05em;
  }
  .timeline ol {
    list-style: none;
    padding: 0;
    margin: 0;
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
  }
  .timeline li {
    display: grid;
    grid-template-columns: 4rem 1fr;
    gap: 0.75rem;
    padding: 0.4rem 0.5rem;
    border-left: 2px solid var(--accent-primary);
    background: var(--bg-card);
  }
  .year { color: var(--accent-primary); font-size: var(--fs-sm); }
  .event { color: var(--fg-primary); font-size: var(--fs-sm); }

  .narrative {
    font-size: var(--fs-sm);
    color: var(--fg-secondary);
    font-style: italic;
    line-height: 1.5;
    max-width: 32rem;
    margin: 0;
  }

  .play-again {
    padding: 0.85rem 2rem;
    font-family: var(--font-chrome);
    font-size: 0.8rem;
    letter-spacing: 0.3em;
    text-transform: uppercase;
    border: 2px solid var(--accent-primary);
    color: var(--accent-primary);
    background: transparent;
    border-radius: 0;
    animation: fadeIn 700ms var(--ease);
  }
  .play-again:hover { background: rgba(196, 163, 90, 0.1); }

  .waiting {
    display: inline-block;
    width: 12rem;
    height: 1px;
    background: var(--border);
  }

  /* ── Gold particle field (victory only) ───────────────────────────── */
  .particles {
    position: absolute;
    inset: 0;
    pointer-events: none;
    overflow: hidden;
  }
  .particle {
    position: absolute;
    bottom: -10px;
    width: 3px;
    height: 3px;
    background: var(--accent-primary);
    border-radius: 50%;
    opacity: 0;
    animation: rise 9s linear infinite;
    box-shadow: 0 0 6px var(--accent-primary);
  }
  @keyframes rise {
    0%   { transform: translateY(0)     scale(1);   opacity: 0; }
    10%  { opacity: 0.85; }
    100% { transform: translateY(-100vh) scale(0.6); opacity: 0; }
  }
</style>
