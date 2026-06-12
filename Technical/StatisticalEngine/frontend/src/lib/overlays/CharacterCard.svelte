<script lang="ts">
  /*
   * CharacterCard (STAR_02 §4.1) — the Persona-bustup × Hades-cadence pop-up.
   *
   *   • Bust portrait anchored BOTTOM-RIGHT at z-index 870 (between the memo at
   *     850 and the log/market at 890 — fits the documented overlay ladder).
   *   • Nameplate color-coded by archetype family.
   *   • Typewriter at ~45 chars/sec.
   *   • Two-stage dismiss: click 1 = complete the text instantly; click 2 = dismiss.
   *   • 200ms slide-in; auto-dismiss after 12s if left unread (still logs dwell).
   *   • NEVER blocks gameplay input: the overlay layer is pointer-events:none; only
   *     the card element itself takes clicks.
   *
   * Telemetry: portrait_shown {npcId, trigger} on mount; portrait_dismissed
   * {npcId, ms, read} when it goes away (read = the player finished/clicked it).
   */

  import { characters } from '../stores/characters.svelte';
  import { portraitFor } from '../portraits';
  import { telemetry } from '../telemetry';

  const CPS = 45;                 // typewriter chars/sec
  const TICK_MS = Math.round(1000 / CPS);

  let card = $derived(characters.active);

  // Per-card lifecycle state. Reset whenever the active card id changes.
  let shownAt = $state(0);
  let typed = $state('');
  let complete = $state(false);
  let read = $state(false);       // did the player engage (click to complete)?
  let typeTimer: ReturnType<typeof setInterval> | null = null;
  let autoTimer: ReturnType<typeof setTimeout> | null = null;
  let currentId = $state<number | null>(null);

  function clearTimers() {
    if (typeTimer) { clearInterval(typeTimer); typeTimer = null; }
    if (autoTimer) { clearTimeout(autoTimer); autoTimer = null; }
  }

  function startCard(c: NonNullable<typeof card>) {
    clearTimers();
    currentId = c.id;
    shownAt = Date.now();
    typed = '';
    complete = false;
    read = false;
    telemetry.log('portrait_shown', { npcId: c.npcId, trigger: c.trigger });

    let i = 0;
    typeTimer = setInterval(() => {
      i += 1;
      typed = c.text.slice(0, i);
      if (i >= c.text.length) {
        typed = c.text;
        complete = true;
        if (typeTimer) { clearInterval(typeTimer); typeTimer = null; }
      }
    }, TICK_MS);

    // Auto-dismiss after the unread window (whether or not typing finished).
    autoTimer = setTimeout(() => dismiss(false), characters.autoDismissMs);
  }

  // React to a NEW active card (id change) — start its typewriter + timers.
  $effect(() => {
    const c = card;
    if (c && c.id !== currentId) {
      startCard(c);
    } else if (!c && currentId !== null) {
      currentId = null;
      clearTimers();
    }
  });

  function dismiss(wasRead: boolean) {
    clearTimers();
    const ms = shownAt ? Date.now() - shownAt : 0;
    // The store logs portrait_dismissed with ms+read so the dwell is DOM-accurate.
    characters.finishActive(wasRead || read, ms);
  }

  function onCardClick() {
    if (!complete) {
      // Stage 1: complete the text instantly.
      const c = card;
      if (c) { typed = c.text; complete = true; }
      read = true;
      if (typeTimer) { clearInterval(typeTimer); typeTimer = null; }
    } else {
      // Stage 2: dismiss (counts as read).
      dismiss(true);
    }
  }

  $effect(() => () => clearTimers());

  let portrait = $derived(card ? portraitFor(card.npcId, card.emotion) : '');
</script>

<!-- Layer is pointer-events:none so it NEVER blocks the sim; only the bust takes clicks. -->
<div class="char-layer" aria-hidden={!card}>
  {#if card}
    {#key card.id}
      <button
        class="char-card"
        style="--plate: {card.color}"
        onclick={onCardClick}
        title={complete ? 'Click to dismiss' : 'Click to skip typing'}
      >
        <div class="bust">
          <img src={portrait} alt={card.speaker} draggable="false" />
        </div>
        <div class="bubble">
          <div class="plate">
            <span class="name">{card.speaker}</span>
            {#if card.role}<span class="role">{card.role}</span>{/if}
          </div>
          <p class="line">{typed}<span class="caret" class:done={complete}></span></p>
        </div>
      </button>
    {/key}
  {/if}
</div>

<style>
  .char-layer {
    position: fixed;
    inset: 0;
    z-index: 870;          /* between memo (850) and log/market (890) */
    pointer-events: none;  /* NEVER blocks gameplay input */
  }
  .char-card {
    pointer-events: auto;  /* …only the card is clickable */
    position: absolute;
    bottom: 1.5rem;
    right: 1.25rem;
    display: flex;
    align-items: center;   /* center the bubble on the bust so wrapping text
                              grows symmetrically and never clips the bottom */
    gap: 0.6rem;
    max-width: min(30rem, 92vw);
    background: transparent;
    border: none;
    padding: 0;
    cursor: pointer;
    text-align: left;
    animation: cardIn 200ms var(--ease, ease-out);
  }
  @keyframes cardIn {
    from { opacity: 0; transform: translate(14px, 14px); }
    to   { opacity: 1; transform: translate(0, 0); }
  }

  .bust {
    flex: 0 0 auto;
    width: 6.75rem;
    height: 6.75rem;
    border-radius: 50%;
    overflow: hidden;
    border: 3px solid var(--plate, var(--accent-primary));
    background: var(--bg-elevated, #1b1b1b);
    box-shadow: 0 8px 28px rgba(0, 0, 0, 0.45);
  }
  .bust img { width: 100%; height: 100%; object-fit: cover; display: block; }

  .bubble {
    flex: 1 1 auto;
    min-width: 14rem;
    background: var(--bg-card, #14110d);
    border: 1px solid var(--border, #3a3326);
    border-left: 3px solid var(--plate, var(--accent-primary));
    border-radius: var(--card-radius, 4px);
    padding: 0.55rem 0.75rem 0.65rem;
    box-shadow: 0 6px 24px rgba(0, 0, 0, 0.4);
  }
  .plate {
    display: flex;
    align-items: baseline;
    gap: 0.5rem;
    flex-wrap: wrap;
    border-bottom: 1px solid var(--border-soft, rgba(255,255,255,0.08));
    padding-bottom: 0.3rem;
    margin-bottom: 0.35rem;
  }
  .name {
    font-family: var(--font-display, serif);
    color: var(--plate, var(--accent-primary));
    font-size: 0.95rem;
    letter-spacing: 0.02em;
  }
  .role {
    font-family: var(--font-chrome, monospace);
    font-size: 0.56rem;
    letter-spacing: 0.14em;
    text-transform: uppercase;
    color: var(--fg-secondary, #9a9486);
  }
  .line {
    margin: 0;
    font-family: var(--font-body, sans-serif);
    font-size: 0.86rem;
    line-height: 1.45;
    color: var(--fg-primary, #ece6da);
    font-style: italic;
  }
  .caret {
    display: inline-block;
    width: 0.5ch;
    margin-left: 1px;
    border-bottom: 2px solid var(--plate, var(--accent-primary));
    animation: blink 0.9s step-end infinite;
  }
  .caret.done { display: none; }
  @keyframes blink { 50% { opacity: 0; } }

  @media (max-width: 640px) {
    .bust { width: 5.5rem; height: 5.5rem; }
    .char-card { max-width: 94vw; }
  }
</style>
