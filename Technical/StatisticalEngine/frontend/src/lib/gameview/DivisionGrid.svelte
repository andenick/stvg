<script lang="ts">
  /*
   * Division cards — colored left border per type, net-income mini bar,
   * warning border on hidden risk >30%, hover reveals autonomy/morale.
   *
   * Visual_polish #8 + #1 (wiring revealedRisk + hiddenRisk).
   *
   * Type-color families:
   *   lending (commercial / mortgage / credit_cards): blue
   *   trading (trading / derivatives):                red
   *   IB (investment_banking / restructuring / private_equity / securitization): gold
   *   asset mgmt (asset_management / wealth_management / trust_custody): green
   *   international (international): teal
   *   innovation (venture_capital / fintech / crypto_custody): purple
   */

  import { sim } from '../stores/simulation.svelte';

  type Family = 'lending' | 'trading' | 'ib' | 'asset' | 'intl' | 'innovation' | 'other';

  const TYPE_FAMILY: Record<string, Family> = {
    commercial_lending: 'lending',
    mortgage_lending: 'lending',
    credit_cards: 'lending',
    trading: 'trading',
    derivatives: 'trading',
    investment_banking: 'ib',
    restructuring: 'ib',
    private_equity: 'ib',
    securitization: 'ib',
    asset_management: 'asset',
    wealth_management: 'asset',
    trust_custody: 'asset',
    international: 'intl',
    venture_capital: 'innovation',
    fintech: 'innovation',
    crypto_custody: 'innovation',
  };

  const TYPE_LABEL: Record<string, string> = {
    commercial_lending: 'Commercial Lending',
    mortgage_lending: 'Mortgage Lending',
    credit_cards: 'Credit Cards',
    trading: 'Trading Desk',
    derivatives: 'Derivatives',
    investment_banking: 'Investment Banking',
    restructuring: 'Restructuring',
    private_equity: 'Private Equity',
    securitization: 'Securitization',
    asset_management: 'Asset Management',
    wealth_management: 'Wealth Management',
    trust_custody: 'Trust & Custody',
    international: 'International',
    venture_capital: 'Venture Capital',
    fintech: 'Fintech',
    crypto_custody: 'Crypto Custody',
  };

  function family(type: string): Family {
    return TYPE_FAMILY[type] ?? 'other';
  }
  function label(type: string): string {
    return TYPE_LABEL[type] ?? type.replace(/_/g, ' ');
  }

  function fmt(v: number): string {
    const abs = Math.abs(v);
    const sign = v < 0 ? '-' : '';
    if (abs >= 1e9)  return `${sign}$${(abs / 1e9).toFixed(2)}B`;
    if (abs >= 1e6)  return `${sign}$${(abs / 1e6).toFixed(0)}M`;
    if (abs >= 1e3)  return `${sign}$${(abs / 1e3).toFixed(0)}K`;
    return `${sign}$${abs.toFixed(0)}`;
  }

  // Net-income mini bar: width proportional to |netIncome| / max(|netIncome|) in
  // the current set. Positive bars extend right, negative left from a centerline.
  let maxAbsNI = $derived(
    Math.max(1, ...sim.bank?.divisions.map((d) => Math.abs(d.netIncome ?? 0)) ?? [1])
  );
</script>

{#if sim.bank && sim.bank.divisions.length >= 2}
  <section class="division-grid" aria-label="Divisions">
    <h2 class="title">Divisions</h2>
    <div class="grid">
      {#each sim.bank.divisions as div (div.id)}
        {@const fam = family(div.type)}
        {@const ni = div.netIncome ?? 0}
        {@const hiddenPct = (div.hiddenRisk ?? 0) * 100}
        {@const highRisk = hiddenPct > 30}
        <article
          class="card fam-{fam}"
          class:high-risk={highRisk}
          title="Autonomy {(div.autonomy * 100).toFixed(0)}% · Morale {(div.morale * 100).toFixed(0)}%"
        >
          <header>
            <span class="type-label">{label(div.type)}</span>
            <span class="div-name">{div.name}</span>
          </header>
          <div class="metrics">
            <div class="metric">
              <span class="m-label">REV</span>
              <span class="num m-val">{fmt(div.revenue)}</span>
            </div>
            <div class="metric">
              <span class="m-label">NI</span>
              <span class="num m-val" class:up={ni > 0} class:down={ni < 0}>{fmt(ni)}</span>
            </div>
            <div class="metric">
              <span class="m-label">RISK</span>
              <span class="num m-val" class:warn={highRisk}>{((div.revealedRisk ?? 0) * 100).toFixed(0)}%</span>
            </div>
          </div>

          <div class="ni-bar" aria-hidden="true">
            <span class="centerline"></span>
            <span
              class="ni-fill"
              class:up={ni > 0}
              class:down={ni < 0}
              style="width: {(Math.abs(ni) / maxAbsNI) * 50}%; {ni >= 0 ? 'left: 50%;' : 'right: 50%;'}"
            ></span>
          </div>

          {#if div.commentary}
            <p class="commentary">{div.commentary}</p>
          {/if}
        </article>
      {/each}
    </div>
  </section>
{:else if sim.bank && sim.bank.divisions.length === 1}
  <section class="single-division" aria-label="Division">
    <span class="single-label">{label(sim.bank.divisions[0].type)}</span>
    <span class="single-name">{sim.bank.divisions[0].name}</span>
    <span class="num single-ni">NI {fmt(sim.bank.divisions[0].netIncome ?? 0)}</span>
  </section>
{/if}

<style>
  .division-grid {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: var(--card-radius);
    padding: 1rem 1.25rem;
  }
  .title {
    font-family: var(--font-display);
    color: var(--accent-primary);
    margin: 0 0 0.75rem 0;
    font-size: 1.05rem;
    letter-spacing: 0.04em;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(14rem, 1fr));
    gap: 0.6rem;
  }
  .card {
    background: var(--bg-elevated);
    border: 1px solid var(--border);
    border-left-width: 3px;
    border-radius: 2px;
    padding: 0.6rem 0.75rem;
    transition: border-color var(--t-fast) var(--ease);
    cursor: help;
  }
  .card.high-risk {
    border-color: var(--accent-warning);
    box-shadow: inset 0 0 0 1px rgba(196, 137, 43, 0.18);
  }
  /* type families */
  .fam-lending    { border-left-color: #3b82f6; }
  .fam-trading    { border-left-color: #ef4444; }
  .fam-ib         { border-left-color: #c4a35a; }
  .fam-asset      { border-left-color: #22c55e; }
  .fam-intl       { border-left-color: #2dd4bf; }
  .fam-innovation { border-left-color: #a855f7; }
  .fam-other      { border-left-color: var(--fg-secondary); }

  .card header {
    display: flex;
    flex-direction: column;
    margin-bottom: 0.4rem;
  }
  .type-label {
    font-family: var(--font-chrome);
    font-size: 0.6rem;
    letter-spacing: 0.22em;
    color: var(--fg-secondary);
    text-transform: uppercase;
  }
  .div-name {
    font-family: var(--font-body);
    color: var(--fg-primary);
    font-size: 0.85rem;
  }

  .metrics {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 0.4rem;
    margin-bottom: 0.45rem;
  }
  .metric { display: flex; flex-direction: column; }
  .m-label {
    font-family: var(--font-chrome);
    font-size: 0.55rem;
    letter-spacing: 0.2em;
    color: var(--fg-muted);
  }
  .m-val {
    font-family: var(--font-data);
    font-variant-numeric: tabular-nums;
    font-size: 0.78rem;
    color: var(--fg-primary);
  }
  .m-val.up   { color: var(--accent-success); }
  .m-val.down { color: var(--accent-danger); }
  .m-val.warn { color: var(--accent-warning); }

  .ni-bar {
    position: relative;
    height: 4px;
    background: var(--bg-base);
    border-radius: 1px;
    overflow: hidden;
  }
  .centerline {
    position: absolute;
    left: 50%;
    top: 0;
    bottom: 0;
    width: 1px;
    background: var(--border);
  }
  .ni-fill {
    position: absolute;
    top: 0;
    bottom: 0;
    border-radius: 1px;
  }
  .ni-fill.up   { background: var(--accent-success); }
  .ni-fill.down { background: var(--accent-danger); }

  .commentary {
    font-size: var(--fs-xs);
    color: var(--fg-secondary);
    margin: 0.5rem 0 0 0;
    line-height: 1.35;
    font-style: italic;
  }

  .single-division {
    display: inline-flex;
    align-items: baseline;
    gap: 0.6rem;
    padding: 0.45rem 0.7rem;
    background: var(--bg-elevated);
    border: 1px solid var(--border);
    border-left: 3px solid var(--accent-primary);
    border-radius: 2px;
    font-family: var(--font-body);
  }
  .single-label {
    font-family: var(--font-chrome);
    font-size: 0.6rem;
    letter-spacing: 0.22em;
    color: var(--fg-secondary);
  }
  .single-name { color: var(--fg-primary); }
  .single-ni {
    font-family: var(--font-data);
    color: var(--accent-primary);
    font-size: 0.8rem;
    margin-left: auto;
  }
</style>
