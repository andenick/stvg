# STAR 02 — The Day-Trading Feel, Characters, and the Living Economy

**Received:** 2026-06-11 (dictated; cleaned of dictation artifacts only)
**Status:** Active. Extends STAR_01 (`../NORTH_STAR.md`); contradicts nothing in it.
**Executing plan:** `../Technical/Plans/STAR_02_OVERHAUL_PLAN.md`
**Note:** The owner referenced a pasted text block (the hire-archetype list). It did not
arrive with the message. The archetype language traces to NORTH_STAR.md §7 ("Gunslinger,
Traditionalist…") and §3 Era 3 ("gunslingers… or stay conservative"), plus "relationship
banking" in §8. If a fuller list exists, it should be appended here as §11.

---

## 1. The core feel: Bloomberg day trading

> "Somehow we need to engender in the user an intuitive understanding of the environment
> that makes it feel like day trading. I wanna be looking at Bloomberg and day trading and
> having a lot of fun with that and just kind of making gut picks and seeing how that
> plays out."

- Red lines and green lines, headlines about unemployment, wars, booms.
- "Oh, we're going up. Oh, it's going down. I'm gonna invest." Gut picks that pay off or
  don't — and the *news should be in sync with the engine's direction* so reading the
  tape is a learnable skill.
- Line graphs need to be **bigger**. Users click between charts while weighing investment
  decisions. Time-scale control (e.g., a 10- or 20-year view) matters.
- Players who know economic history **should be able to do well** — knowledge is an edge —
  but a stochastic quality remains. The game generally follows the direction of history,
  *more volatile* than reality.

**Annotation:** Today the market charts are ~5.5rem sparklines and macro data
(`state.economics`) is discarded client-side. Events never move markets (risk/affects
parsed then dropped). See plan §P2/P3/P5.

## 2. Economy aggregates (the Economy tab)

> "A thoughtful consideration of how many different economic aggregates it would be a lot
> of fun to implement… GDP is obvious. Economy total revenue could be something. Total
> profit could be something, and maybe those come after unemployment, obviously. Inflation
> might be too complicated, but interest rates or some basic interest rate… or it could
> just be a credit market and an equity market."

- Candidate set (owner's order of confidence): **GDP** (obvious) → **unemployment**
  (obvious) → **economy total revenue / total profit** (later additions) → **interest
  rate** (basic, maybe) → **inflation** (maybe too complicated) → or simply **a credit
  market + an equity market**.
- These need design thought because users click between them while making investment
  decisions.

**Annotation:** Engine already simulates gdpGrowth, unemployment, fedFunds, CPI, 10Y,
credit spread, VIX (Taylor rule + Okun's law OU system, era-calibrated from FRED). GDP
*level* and economy-wide revenue/profit need small engine additions. Plan §P3.

## 3. Screens / tabs

The game has distinct views (tabs):

1. **Economy** — the Bloomberg view. Big charts, click between aggregates and markets,
   change time scale, a news button, red/green. Possibly actions: research, "invest
   yourself" / day-trade button (maybe how the early game starts).
2. **Hire** — the hiring menu. Employees/divisions trickle in as cards; click for detail;
   you watch the performance of people you've hired; hire more as money grows. Poaching a
   whole division with a famous track record is expensive ("this team has already made
   $20B"). **The Hire button lives in the top menu and changes color when you can afford
   someone** (like other games do).
3. **My Bank** — (later) the visual bank: a tiny-tower-style horizontal floor slice with
   little sprites trading, yelling, on the phone. Starts as one small room with a couple
   of dots; ends as a skyscraper. Upgrade the building to add capacity. *Tabled for a
   later aesthetic phase — placeholders fine.*
4. **Financials** — the bank CEO's statement view: assets, liabilities, equity, revenue,
   costs, profit, NIM ("the percent you're making… the percent you're paying on your
   deposits"), funding sources. "What would a silly bank CEO have to look at?"

**Annotation:** No tab structure exists today (single scrolling column per phase). Plan §P2.

## 4. Characters: Hades-style pop-ups + credibility through language

> "Maybe they could just be pop-ups like in Hades when we see the gods. They kind of only
> show up at certain times when certain events are proc'd."

- **Cartoon caricature, business-y, toned down from Hades** — "more detailed than Stardew
  Valley, definitely still a cartoon, not super realistic." Caricatures of the great bank
  CEOs and historical figures; possibly **presidents in the same style** (Civil Rights Act
  and political history should be *felt*).
- As your bank grows you naturally meet more famous people of the time.
- **Credibility is communicated through language**: honest analysts vs cronies/salesmen.
  "We need people to seem like they're good analysts versus cronies or people selling
  stuff that is not really worth it" — and explicitly **NOT** "long-winded = smart."
  The player vibe-reads trust; trust pays off or burns you.
- Example scene: "some old-timey looking finance aristocrat tells me the equity market's
  doing really great" — and you learn over time whether to trust him.
- An advisor's pitch should come from **the actual person you hired** (the gunslinger you
  hired pitches you the wildcat oil play).
- A Fable-style (the video game) reputation dynamic: the world *recognizes* what you're
  becoming. Build a conservative fortress bank → NPCs treat you as one, you get
  conservative opportunities. Hire gunslingers → risky deals find you. (Extends
  NORTH_STAR path dependence into the *social/narrative* layer.)

**Annotation:** CharacterRecommendation/CharacterMessage data already flows to the client
with names, sentiment, tone — there's just no portrait layer, and voices.ts assigns
archetype by hashing the deal id. Plan §P4.

## 5. The hiring economy (the gameplay)

> "On average, players are gonna make money, but that should scale very considerably as
> they hire more expensive employees or divisions… Game players invest. Sometimes they
> take risk. If they take too much risk they can lose a lot of money quickly. And if
> they're too leveraged, that might be a problem."

- Hire **gunslingers, conservatives, relationship men** — archetypes with different cost,
  profitability, volatility, and **different responses to different economies**.
- "An intuitive way for users to make money that's kind of foolproof, but also allow for
  the flexibility to lose money if they make really bad decisions — really risky, or
  they're not paying attention to the macro environment."
- Maybe you start by **trading yourself**, then hire; investing should feel more or less
  automatic once delegated — you choose more risky / less risky.
- Over time users **discover new archetypes / investment strategies** with different
  success rates and different results.
- **Crowding/saturation:** hiring too many of the same type creates correlated risk, or
  decreases average per-person profitability; an industry can grow and become saturated.
- **The math ask:** "The connection between those two — how employees or divisions of
  certain attributes have different profitabilities, different volatilities, different
  responses to different economies — that's a mathematical equation I want you to figure
  out and model… the statistical engine creates the distribution by which employees
  perform."

**Annotation:** This is the central engine workstream. Archetypes currently do not enter
the P&L equation at all (deterministic revenue; variance only from trading desk/deals).
Single implementation seam identified at QuarterlyPhases.h:388-395. Plan §P5/P6.

## 6. The statistical engine

- Must be "sophisticated enough that it provides a stunning entree into the dynamics of
  economies in different decades and is in sync with the news being given."
- Follows the direction of history, **more volatile**; volatility is transmitted
  mechanically into the P&L of employees/divisions with different characteristics.
- Era vibes to hit: post-WW2 American supremacy + rising financialization → 1970s
  inflation/high rates/unemployment/political comeuppance → 1980s cowboy capitalism
  (deregulation, leverage, a big banking crisis) → 1990s dot-com / tech change → (the
  rest per NORTH_STAR). Civil Rights Act-type political texture included.

## 7. Narrative & history sources

- Review all in-game stories/narrative events; mine **financial-history knowledge bases**
  (Volcker KB: Chernow, Lewis's Liar's Poker, Greider, FDIC crisis histories, FCIC, etc.;
  Wynne's Minsky cluster) for "clues about events in business history… the history of
  financial markets" to enrich events, characters, and era vibes.

**Annotation:** Volcker KB mapped: ~1,452 docs, 15 high-value narrative sources
identified, clean chunked markdown, mining workflow drafted. In-copyright trade books →
facts/beats/paraphrase, not verbatim. Plan §P8.

## 8. Telemetry & the iteration loop

> "I want hyper-detailed telemetry so that I can play the game and give you data to
> inspect about how I'm playing — how much I'm dwelling on certain features, how much I'm
> reading, how engaged I appear to be."

- The loop: instrument → owner plays → agent reads JSONL + feedback → tune pacing, speed,
  profit curve ("maybe you want people to make money slower… or maybe people wanna make
  more money, so you tune up volatility or profitability of the spec they're building").
- Infer engagement from limited information; speed of gameplay, money-over-time curve,
  dwell on features are first-class signals.

**Annotation:** Pipeline works; coverage is 10 event types, no dwell/read-time, hover
helper dead, markets/panels/lifecycle uninstrumented. Plan §P1 (FIRST, so the owner's
next playtest is fully recorded).

## 9. Art pipeline (now vs later)

- **Now:** placeholders are fine — "cute little placeholders, whatever makes sense for the
  types of characters we're trying to display."
- **Later:** owner's RTX 5090 for local 2D/3D generation; careful prompt control to mint
  ~20 emotion/style variants per character; agent inspects its own output with native
  image reading and iterates. Antigravity credits also available.
- Sprite work informs the vibe — generate a lot of character variants.

## 10. Process directives

- **Stars convention:** keep capturing vision prompts like this one as star documents
  agents refer back to. (This file inaugurates the convention; see `README.md`.)
- Review past sessions' directions vs delivery (done — audit 2026-06-11; recurring
  failure: open decisions silently resolved, claimed-done-but-not-wired).
- "Feel free to change any of the technical scope or infrastructure — I'm not married to
  any front end or back end or style or image-generating tool."
- "Do all the research you need… then we're gonna do a lot of great work."

---

## Addendum A — Owner answers to the plan's open questions (2026-06-11)

1. **Lapse vs hard-pause:** question not understood by owner → implement plan default
   (routine decisions lapse; crisis/major decisions hard-pause) behind `decisionPause`
   config; demonstrate both in the playtest and re-ask with the game in hand.
2. **Tabs** Economy | Hire | My Bank | Financials: **confirmed**.
3. **Aggregate unlock order** (GDP+Unemployment → rates → economy revenue/profit;
   inflation unpromoted): **confirmed, tentatively** — "we might alter that but we can try."
4. **Art band** (Two Point ⇄ Persona caricature) + DiceBear placeholders: **confirmed**.
5. **Presidents in the character registry** (portraits later): **yes**.
6. **Early game:** "might be more loany but it could also be tradey — figure it out."
   → Decision: lending-first early game (loan prospects as the hero surface), with a
   light/optional trading affordance; telemetry decides which grows.
7. **Archetype list:** lives in an old session or project documentation — search docs;
   AND brainstorm a full roster of hire types across all divisions, many of them,
   connected to hiring cost and the attribute system, with varied looks and
   characteristics. → becomes **§11** below when produced.

---

## 11. The hire-archetype roster (produced 2026-06-11 per Addendum A.7)

**Was the original list found?** **No.** An exhaustive case-insensitive search of
`Projects/STVG` (including `Archive/`, which holds only the legacy SDL2 engine and CMake
test artifacts — no design docs) turned up nothing beyond the four vocabularies already
known: NORTH_STAR §7's five *competitor-bank* archetypes (Traditionalist, Gunslinger,
Innovator, Politician, Survivor), NORTH_STAR §8's "relationship banking," and
EmployeeCandidate.h's eight *hire* archetypes. The pasted block confirmed missing here
(§ note, lines 6-9) and in the plan's Open Question 7 was never recovered. So the roster
was **brainstormed fresh** from those roots and the engine's actual systems.

**What was produced.** A two-level taxonomy: the **8 EmployeeCandidate hire archetypes as
FAMILIES** (engine values migrated verbatim) + **33 division-flavored SPECIALIZATIONS**
across the 16 divisions and 7 eras. Cost (`salaryMult`) correlates with expected value
(`Σ|macro-beta|`) and, for aggressive types, with volatility (σ); **relationship types
matter mechanically** via deposit stickiness, dealflow quality, and book stability — not
merely low σ. The owner's named trio maps as: *gunslingers* = the Gunslinger family;
*conservatives* = the Lifer/Patrician/Operator cluster; *relationship men* = a first-class
"Relationship Man" specialization + the Rainmaker/Private-Banker line.

**Artifacts:** data → `Technical/StatisticalEngine/data/archetypes/archetypes.json`;
full design (cost↔value logic, per-family β rationale, crowding classes, era matrix, 5
worked examples) → `Technical/Plans/ARCHETYPE_ROSTER_DESIGN.md`.

### 8-family β / σ summary

| Family | salMult | integ. | β(gdp, rate, credit, equity, vix) | σ | crowding | owner's bucket |
|---|---|---|---|---|---|---|
| **Patrician** | 1.3 | 75 | (.20, +.25, .15, .05, .05) | .06 | relationship_credit | relationship/conservative |
| **Gunslinger** | 1.5 | 40 | (.45, −.30, .25, .60, −.20) | .35 | directional_macro | **gunslinger** |
| **Quant** | 1.4 | 65 | (.10, .05, .20, .15, +.30) | .18 | systematic_quant | (post-1980) |
| **Dealmaker** | 1.6 | 55 | (.40, −.15, .30, .35, .10) | .22 | fee_advisory | aggressive-fee |
| **Operator** | 1.2 | 70 | (.15, .10, .10, .05, .10) | .08 | operations_control | conservative |
| **Rainmaker** | 1.7 | 60 | (.25, .05, .10, .25, .10) | .10 | relationship_credit | relationship |
| **Prodigy** | 0.6 | 60 | (.30, −.05, .15, .30, .05) | .28 | directional_macro | lottery-ticket |
| **Lifer** | 0.8 | 80 | (.10, +.15, .10, .03, +.15) | .05 | relationship_credit | **conservative** |

Sign convention: rate = Δfedfunds (positive β = asset-sensitive, benefits from hikes);
credit = −Δspread (positive = likes tightening); vix = −Δvix (positive = wants calm).
Gunslingers carry the biggest |β| and σ (high cost, high upside, fat left tail);
Lifers/Patricians the smallest (cheap-to-modest, near-deterministic, sticky books).
