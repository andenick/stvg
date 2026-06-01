# STVG North Star

> **This document is immutable.** It captures the founding vision of STVG and must not be modified.
> All future development decisions reference this document. If the vision evolves,
> create a separate VISION_AMENDMENTS.md — never edit this file.
>
> Written: 2026-04-02

---

## 1. Game Identity

STVG is a banking CEO simulation spanning **1945 to ~2040**. The player begins with a simple commercial bank — something resembling a Morgan Guaranty Trust — and navigates 95 years of American financial history. The game is **historically grounded with procedural surprise**: even players who know the history of US financial markets and banking should be sometimes surprised. Not surprised by the GFC itself, but by which of *their* positions blow up because of it. The randomness is in the player's exposure, not in whether history happens.

The game is meant to be **fun first**. Early game should have satisfying "number go up" feedback from growing a simple business. Later game should feel increasingly tense, complex, and consequential. The progression from simple to complex is the **central design principle** — it is what makes the game both accessible and deep.

It is also meant to be **historically interesting and mostly accurate**. The regulatory barriers, market dynamics, and institutional evolution of American banking from the post-war era to the present are not just setting — they are the game mechanics themselves.

---

## 2. Core Loop

Management happens in **real time within each game year**. As ticks flow during the year, the player makes decisions: hire people, approve loans, open new business lines, respond to events, manage risk. The tick rate is roughly constant throughout the game, but the **density of decisions** increases over time as the bank grows and more business lines become active.

At **year-end**, the player receives an **annual report** containing:
- Revenue, costs, and profit for each business line
- Total capital growth and balance sheet evolution
- Risk metrics and regulatory compliance status
- Market performance and economic indicators
- Competitor standings and market share
- Key events of the year and their impact

The game spans approximately **95 game-years** (1945-2040). Early years have fewer levers and pass with less action — a comfortable pace for learning. Later years have more divisions, more instruments, more risk, and more reward — each year feels packed with decisions.

---

## 3. Progressive Complexity

This is the **most important design principle**. The player starts with a simple business and the game introduces new mechanics gradually, gated by historical era. The player never faces all the complexity at once.

### Era 1: Post-War Stability (1945-1959)
- **Starting bank:** A boring, stable commercial bank. Think Morgan Guaranty Trust.
- **Available businesses:** Commercial lending, mortgage lending (FHA/VA loans, GI Bill boom)
- **Levers:** Interest rates on deposits, loan approval standards, branch expansion
- **Feel:** Steady, predictable. "3-6-3 banking" — borrow at 3%, lend at 6%, golf at 3pm.
- **Growth:** Slow but reliable. Number goes up satisfyingly.
- **Risks:** Overexpansion (e.g., expanding to Korea before the Korean War), concentration risk
- **Landmines exist** but this isn't a huge risk-taking operation

### Era 2: Expansion & Innovation (1960-1972)
- **Unlocks:** Trust & custody, credit cards (1958 BankAmericard), international banking
- **New levers:** Eurodollar market, correspondent banking, consumer credit
- **Feel:** The world is opening up. New opportunities appear but require new skills.
- **Risks:** International exposure, currency risk (under Bretton Woods fixed rates)

### Era 3: Volatility & Deregulation Begins (1973-1986)
- **Unlocks:** Trading desk, asset management
- **Trigger events:** Bretton Woods collapse (1971), oil embargo (1973), Volcker shock (1979-82)
- **New levers:** Proprietary trading, interest rate management, hedging
- **Feel:** The calm is over. Volatility arrives. Players who built conservatively are rewarded; risk-takers face real consequences for the first time.
- **Player choice:** Start hiring "gunslinger" types for the trading desk, or stay conservative?

### Era 4: Big Bang & Excess (1987-1999)
- **Unlocks:** Investment banking, private equity, restructuring, securitization
- **Trigger events:** Black Monday (1987), S&L crisis, LTCM (1998), Glass-Steagall repeal (1999)
- **Feel:** Exhilarating. Deregulation opens enormous new profit opportunities. The game gets significantly more complex and more profitable.
- **Player choice:** This is where path dependence matters most. Players who built trading capabilities can now merge them with banking. Those who stayed conservative face a choice: adapt or be left behind.
- **The Glass-Steagall wall comes down** — players can now become universal banks

### Era 5: Shadow Banking & Hubris (2000-2007)
- **Unlocks:** Derivatives desk, wealth management, venture capital
- **Feel:** Maximum complexity. CDOs, CDS, structured products. Enormous profits available. The game is at its most complex and most dangerous.
- **Risks:** Systemic risk is building. Players who load up on mortgage exposure are sitting on a bomb.
- **Design challenge:** Make the player feel excited by the profits while sensing the danger

### Era 6: Crisis & Reform (2008-2019)
- **No new divisions** — but everything changes
- **Trigger events:** GFC (the big existential event), Dodd-Frank, European debt crisis
- **Feel:** Chaos, then constraint. Compliance costs increase dramatically. The game feels different.
- **Players can lobby** to influence regulation, meet politicians, shape policy
- **Path dependence:** Players who avoided subprime exposure thrive; others fight for survival

### Era 7: Modern & Endgame (2020-2040+)
- **Unlocks:** Fintech, crypto custody
- **Trigger events:** COVID crash, meme stocks, AI emergence, climate tipping points
- **Feel:** The endgame. New threats (AI CEO, climate) compete for attention.
- **Multiple endings** based on accumulated decisions across all eras

---

## 4. Regulatory Eras as Gameplay Gates

Historical regulatory barriers are **crucial to gameplay**. They are not flavor text — they are hard mechanical constraints that define what the player can and cannot do.

- **Glass-Steagall (1933-1999):** A HARD WALL separating commercial banking from securities underwriting. Players CANNOT open investment banking, securitization, or derivatives businesses until this wall comes down. It erodes partially in the 1980s-90s (Section 20 subsidiaries) and is fully repealed in 1999 (Gramm-Leach-Bliley).

- **Regulation Q (1933-2011):** Caps interest rates on deposits. In early eras, this constrains deposit-gathering competition. Its removal creates new competitive dynamics.

- **Bretton Woods (1944-1971):** Fixed exchange rates. International banking exists but forex trading does not. When Nixon closes the gold window in 1971, forex markets open — a new revenue source and a new risk.

- **Basel I (1988), II (2004), III (2010):** Progressively higher capital requirements. Force balance sheet restructuring as rules evolve. Players must maintain capital ratios or face regulatory seizure.

- **Dodd-Frank (2010):** Creates massive compliance burden. Volcker Rule restricts proprietary trading. Too-big-to-fail designation carries costs. Players can lobby to weaken these rules.

- **AI/Climate Regulation (2030s):** New regulatory frameworks around AI in finance and climate risk disclosure. These shape the endgame.

Players can **lobby and meet politicians** to influence regulatory outcomes. This costs political capital and creates its own path dependence — a bank known as a heavy lobbyist faces reputational risk but gains regulatory advantage.

---

## 5. Path Dependence

Early choices **open some doors and close others**. This is not optional flavor — it fundamentally shapes each playthrough.

### How It Works

Every major decision shifts the bank's **institutional DNA**:
- **Risk culture:** Hiring "gunslingers" and risk-takers shifts culture toward aggressive. This enables trading firm conversion but makes conservative strategies less effective.
- **Geographic footprint:** Entering international markets in the 1960s creates expertise but locks in regulatory exposure. Staying domestic is safe but limits growth.
- **Technology investment:** Early investment in computing (1970s) and later in AI (2020s) creates compounding advantages but diverts capital from traditional businesses.
- **Political relationships:** Lobbying builds political capital but creates dependency and reputational risk.

### Concrete Examples

- Players who hire risk-taker types and build trading capabilities CAN choose to **convert into a trading firm** (like a Morgan Stanley or Lehman Brothers). But they CANNOT also be a conservative commercial bank — the culture doesn't support both.
- Players who build strong international operations in the 1960s have a head start when deregulation opens global markets in the 1980s-90s. Players who stayed domestic need to catch up.
- A bank that pioneered securitization in the 1990s has enormous CDO profits in 2000-2007 — but is maximally exposed when the GFC hits.
- Decisions create **15-30% performance differentiation** vs. alternative paths and persist for **10+ years** before reversal is practical.

### Unlockable Starting Positions

After completing the game or achieving great success, players can unlock alternative starting positions:
- **Default:** Commercial bank (1945) — the canonical experience
- **Trading firm start:** Begin as a Lehman Brothers-type firm (unlocked after first completion)
- **Investment bank start:** Begin as a Morgan Stanley-type firm (unlocked after high score)

Different starting positions mean different early-game strategies, different path dependencies, and different experiences with the same historical events.

---

## 6. Market Simulation

The market simulation must generate **realistic multi-asset time series** that are procedurally generated but historically informed. The goal is markets that *feel* right for each era.

### Requirements

- **Multiple simultaneous correlated time series:** Stocks, bonds, interest rates, forex, commodities. More asset classes unlock as eras progress.
- **Era-specific calibration:** 1945 markets (calm, low volatility, pegged rates) feel fundamentally different from 2008 markets (high volatility, correlated crashes, complex derivatives).
- **Correlations that change over time:** Asset correlations should drift with market conditions. In normal times, stocks and bonds have low correlation. In crises, correlations converge toward 1.0 — **diversification fails when you need it most.**
- **Scripted crisis magnitudes:** Major historical events (Black Monday, GFC) fire at approximately historical dates (with ±10% timing variance) with approximately correct magnitudes. The player knows the crisis is coming — but not exactly when, and not which of their positions it hits hardest.
- **Procedural crises from player behavior:** If the player takes on excessive leverage, builds concentrated positions, or ignores hidden risk, the engine should generate **unscripted crises** that punish these choices. History provides the backdrop; the player's own decisions provide the landmines.
- **Surprise through exposure, not through events:** Players who know history still get surprised because the *combination* of their positions and the historical event creates unexpected outcomes. A player heavily exposed to Korean commercial real estate gets destroyed by the Asian Financial Crisis in ways they didn't anticipate, even if they knew the crisis was coming.

---

## 7. Persistent AI Competitors

**4-6 named rival banks** persist throughout the entire game, evolving alongside the player. They are inspired by Civilization's AI leaders — persistent, personality-driven opponents who feel like real rivals.

### Design

Each competitor has:
- **Visible personality:** Aggressive, conservative, innovative, political. The player should be able to predict roughly how each rival will react to events.
- **Own path dependence:** Competitors make their own strategic choices that constrain their futures. An aggressive competitor that loaded up on CDOs will struggle in 2008. A conservative one that avoided them will thrive.
- **Annual decisions:** Each year, competitors expand, hire, lobby, invest, or retrench — visible to the player.
- **Direct competition:** Bidding on the same deals, competing for the same hires, fighting for market share in the same businesses.
- **Vulnerability:** Competitors can **fail, get acquired, or merge**. These create dramatic gameplay moments. A rival that collapses in 2008 might be available for acquisition — at a steep price with hidden risks.
- **Emergence timing:** Not all competitors start in 1945. Some emerge later (a trading firm in the 1980s, a fintech challenger in the 2010s). This keeps the competitive landscape evolving.
- **Leaderboard:** A visible ranking by total assets, reputation, profitability, and market share. The player always knows where they stand.

### Personality Archetypes (intentionally suboptimal, like real institutions)

- **The Traditionalist:** Conservative, slow to change, strong in commercial banking. Thrives in stable eras, struggles during disruption.
- **The Gunslinger:** Aggressive risk-taker, early adopter of trading and derivatives. Enormous profits in booms, existential risk in busts.
- **The Innovator:** First to adopt new technologies and instruments. Often ahead of the curve but sometimes too early.
- **The Politician:** Heavy lobbying, regulatory capture. Profits from favorable regulation but vulnerable to political shifts.
- **The Survivor:** Adapts to whatever the current era requires. Not the best at anything but rarely fails.

---

## 8. AI CEO Endgame (2030-2040)

The AI CEO is an **extension of the competitor system**, not a separate mechanic. One of the rival banks (or a new entrant) invests heavily in artificial intelligence and eventually develops an AI that begins operating as a superhuman bank CEO.

### How It Works

- **Emergence:** The AI CEO appears when cumulative global AI investment (by all banks including the player) crosses a threshold. This happens somewhere between 2030 and 2040, depending on investment levels. The more the player invests in AI, the sooner it appears — but it helps everyone, including competitors.
- **Escalation:** It starts as a normal competitor with slightly better efficiency. Over time, its operational efficiency, risk assessment, and deal-making become superhuman. It begins capturing market share at an accelerating rate.
- **Mechanism:** The AI bank hijacks capital accumulation and profit extraction mechanisms to fund further AI development, creating a positive feedback loop driving toward singularity.
- **Player options:**
  - Invest in AI themselves to keep pace (expensive, accelerates the timeline)
  - Lobby for AI regulation (slows the AI but costs political capital)
  - Pursue alternative strategies (sustainable banking, relationship banking that AI can't replicate)
  - Attempt acquisition of the AI bank (extremely expensive, high risk)

### Climate Change as Competing Final Boss

Climate change operates as a **parallel existential threat** that competes with the AI singularity for attention and resources:
- Physical risk: Extreme weather damages assets, destroys collateral, disrupts operations
- Transition risk: Carbon regulations strand fossil fuel assets
- The AI bank is more exposed to climate risk (concentrated in coastal tech hubs, energy-intensive AI operations)
- Players who built sustainable, climate-resilient portfolios have an advantage in the final act
- **The tension:** Do you invest in AI (accelerating singularity but capturing short-term profits) or in climate resilience (slower returns but better positioned for the endgame)?

### Multiple Endgame Scenarios

The game can end in several ways after 2040:
- **Player surpasses AI:** Through regulation, competition, or acquisition, the player maintains dominance
- **AI achieves singularity:** The AI bank becomes so efficient it captures the entire market — game over for traditional banking
- **Climate restructuring:** Climate catastrophe forces global financial system restructuring — new rules, new winners
- **Sustainable legacy:** Player builds a banking institution that survives everything — the "quiet victory"
- **Board fires player:** Poor performance across too many years

---

## 9. Historical Landmines & Surprises

These are the major scripted events that *will* happen (with timing variance). The surprise isn't that they happen — it's how the player's specific portfolio and decisions interact with them.

| Year | Event | Player Risk |
|------|-------|-------------|
| 1950-53 | Korean War | Overexpansion to Asia, commodity exposure |
| 1966 | Credit crunch | Overextended lending, thin reserves |
| 1971 | Nixon shock / Gold window closes | International exposure, gold positions |
| 1973 | Oil embargo | Energy lending, international operations |
| 1979-82 | Volcker shock | Mortgage portfolios, rate-sensitive positions |
| 1982 | Latin American debt crisis | International lending exposure |
| 1987 | Black Monday | Trading desk positions, leveraged exposure |
| 1989-91 | S&L crisis | Real estate lending, thrift exposure |
| 1994 | Bond market massacre | Fixed income positions, leveraged bond bets |
| 1997-98 | Asian crisis / LTCM | International positions, correlated bets |
| 2000-02 | Dot-com crash | Tech lending, VC exposure, IPO pipeline |
| 2001 | 9/11 | Insurance, real estate, general market shock |
| 2007-09 | Global Financial Crisis | THE BIG ONE. Mortgage exposure, CDO positions, counterparty risk |
| 2010-12 | European debt crisis | European exposure, sovereign risk |
| 2015 | China market crash | International exposure, commodity lending |
| 2020 | COVID crash | Commercial real estate, travel/hospitality lending |
| 2022 | Crypto winter / Rate shock | Crypto custody, rate-sensitive positions |
| 2023 | SVB-style bank run | Concentrated depositors, underwater bonds |
| 2030s | AI disruption | Traditional banking business model |
| 2035+ | Climate tipping points | Physical asset exposure, stranded assets |

Each event is **scripted to happen** but the **player's specific exposure** determines the impact. Two players hitting the same GFC will have wildly different experiences based on their decade of preceding decisions.

---

## 10. Engine Requirements

The engine must be:

- **Adaptable:** Variables and parameters that can change the flow and feel of the game. Not hardcoded behavior.
- **Versatile:** Support many different types of business, instrument, and regulation without rewriting core systems.
- **Fungible:** Systems that can be adapted or changed as the game design evolves. Modularity over monoliths.
- **Functional:** Clean, working code that does what it claims. Tests for everything.
- **Changeable:** Easy to modify without breaking other systems. Clear interfaces between components.

Specifically:
- Market generation for multiple simultaneous correlated time series
- Era-specific calibration that can be loaded from data files (not hardcoded)
- Regulatory framework that can be configured per era without code changes
- Path dependence system that tracks decades of cumulative player decisions
- AI competitor decision-making that produces varied, personality-consistent behavior
- Support for future expansion: more events, more mechanics, more paths, more endings

The engine should be designed so that **feeding it a mountain of banking history data** (which will be supplied later) automatically enriches the game without requiring structural changes.

---

## 11. The Feel

The game should make the player **feel like a banking CEO across American financial history.**

- **1945-1960:** Calm, satisfying growth. Like tending a garden. Number goes up.
- **1960-1972:** Curiosity and opportunity. New markets opening. Mild anxiety.
- **1973-1986:** Volatility and adrenaline. The first real scares. "How did I lose money?"
- **1987-1999:** Exhilaration and excess. Making more money than ever. Feeling invincible.
- **2000-2007:** Hubris and unease. Enormous profits but something feels wrong.
- **2008-2009:** Terror. "Am I going to survive this?"
- **2010-2020:** Recovery and constraint. Rebuilding under new rules. Frustration at regulation.
- **2020-2030:** Disruption and uncertainty. The world is changing faster than you can adapt.
- **2030-2040:** Existential stakes. AI and climate are not just business risks — they're civilizational challenges.

The **emotional arc** of the game mirrors the emotional arc of real American banking history. That's what makes it work.

---

*This document was written on April 2, 2026 and must not be modified.*
*All future development of STVG references this vision.*
