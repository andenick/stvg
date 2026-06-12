# P8 KB-Mining Review Sheet — 1945-1972 Content Candidates

**Date:** 2026-06-12
**Plan item:** STAR_02 §P8, first slice — thicken the game's thinnest eras (Post-War 1945-1959, Expansion 1960-1972).
**Status:** STAGING ONLY. Nothing here is wired into the engine, frontend, or live data. Owner reviews, then a follow-up task merges approved candidates into `data/events/historical_events.json`, `data/deals/post_war_deals.json`, and the P4 character registry.

## What's in this folder

| Artifact | Count | File |
|---|---|---|
| Event candidates | 32 | `events_1945_1972_candidates.json` |
| Character candidates | 12 | `characters_1945_1972.json` |
| Deal-archetype candidates | 13 | `deal_archetypes_1945_1972.json` |
| This review sheet | — | `REVIEW_SHEET.md` |

## Coverage context (why these eras needed thickening)

The live `historical_events.json` has **111 events**, but **only 17** fall in 1945-1972 vs **32 in 2020-2025**. The 17 existing era events were *not* duplicated — these 32 candidates deepen the period with distinct angles (e.g. the existing single "1966 Credit Crunch" gains a housing-collapse companion; "Penn Central" gains the commercial-paper-run + Reg-Q-suspension mechanics; the Treasury-Fed Accord gains a bond-repricing event). Merging all 32 would bring 1945-1972 to ~49 events, comfortably ahead of the post-2020 cluster.

## Sources actually used vs unavailable/thin

**Used (KB, VERIFIED/COMPLETE, real body content confirmed):**
- `1951_Allan_Meltzer_History_Federal_Reserve` (Vol II Bk 1, 1951-1969) — **richest source.** Penn Central, Reg Q ceilings, 1956 BHC Act loophole, bills-only, 1957-58 recession, promissory-note dodge. 13 events.
- `1970_Meltzer_Allan_history_Federal_Reserve` (Vol II Bk 2, 1970-1986) — **best for Bretton Woods/gold/Camp David** and the 1966-crunch retrospective. 5 events.
- `1989_William_Greider_Secrets_Temple_How` — Fed culture, Martin "punch bowl," 3-6-3 banking, Eurodollar "51st state," Burns 1972 pump-priming. 5 events.

**Thin / not reaching the era (flagged, not fabricated):**
- `2021_Jonathan_Levy_Ages_American_Capitalism` — extraction **stops ~1895**; its postwar chapters (14-17: New World Hegemon, Postwar Hinge, Consumerism, Ordeal of a Golden Age) are NOT in the KB body files. Zero usable 1945-72 hits despite a keyword sweep. **Recommend re-extracting Levy Ch.14-17 from source** to seed the real-economy texture.
- `1978_Kindleberger_Manias_Panics` — **0 body files** in KB (confirmed thin per the plan note).
- House of Morgan (`XXXX_houseofmorganame_cher`), Chernow Warburgs, Morgan Guaranty Trust — all **PREPARED, not extracted**; no body content. Would need an HDARP pass.

**Filled from `model_knowledge` (honest provenance; well-known facts, 9 events / several deals & characters):** postwar reconversion squeeze, war-bond unwind, negotiable-CD 1961 origin, consumer-finance boom, correspondent banking, BankAmericard mass-drop, interbank/Master Charge 1966, Franklin-overreacher foreshadow, pension-trust growth. These are accurate banking history but were not in the extracted KB bodies.

### Episodes I could NOT source from the KB — flagged `needs_web_research` (NOT fabricated as KB)
- **Negotiable CD origin** specifically attributed to First National City Bank / Citibank in 1961 — KB only documents First National Bank of *Boston* promissory notes (1964). Event `evt_kb_negotiable_cd_1961` is marked `model_knowledge`.
- **Named bank CEOs** Walter Wriston, George Moore (Citibank), A.P. Giannini (BofA), David Rockefeller (Chase) — none named in the two narrative extractions. Character entries carry `needs_web_research` flags in their source notes.
- **BankAmericard / Master Charge** launch detail — not in KB (Levy postwar chapters missing).
- **Conglomerate merger wave** real-economy detail — KB extraction missing (Levy only has the 1890s Great Merger Movement). Note: a live event `evt_conglomerate_wave` already exists, so this is lower priority.

## Era-mechanical sanity (constraints honored)

Per `Division.h` era gates, candidates restrict `affects`/`requiredDivision` to divisions plausibly live 1945-1972: **CommercialLending, MortgageLending, TrustAndCustody, CreditCards (1958+), InternationalBanking**. I deliberately **avoided TradingDesk and InvestmentBanking** benefits pre-~1973 (their `baseRevenueRate(year)` multipliers and the "1970s-80s" unlock band make them anachronistic). Note: several *existing live* events (treasury_accord 1951, vietnam_inflation 1965, conglomerate_wave 1965, kennedy_tax_cut 1962) already list TradingDesk/InvestmentBanking in `benefits` — those affects are simply inert if the player hasn't unlocked the division, so it's not a hard bug, but the new candidates stay era-clean. **Owner decision:** keep candidates era-clean (recommended) or allow forward-referencing divisions like the existing data does. All money is at $1M-scale (deals $70K-$240K, matching live post_war_deals).

---

## The 10 I'd merge first (and why)

Ranked: KB-sourced + high draft_quality + fills a real mechanical/narrative gap + era-clean.

| # | id | title | year | source | quality | Why first |
|---|---|---|---|---|---|---|
| 1 | `evt_kb_penn_central_cp_run` | The Penn Central Paper Run | 1970-71 | Meltzer 1951 | 5 | Vivid, fully KB-sourced crisis (7th-largest corp, $40B CP market); the existing `evt_penn_central` is thin — this adds the contagion+rescue-line drama. |
| 2 | `evt_kb_camp_david_nixon_shock` | Camp David: The Gold Window Slams | 1971-73 | Meltzer 1970 | 5 | The era's defining rupture, richly sourced (Aug 15 1971 NEP); turns the FX/International division live. Pairs with existing `evt_gold_window` without duplicating it. |
| 3 | `evt_kb_credit_crunch_housing_collapse` | The 1966 Crunch Starves Housing | 1966-67 | Meltzer 1970 | 5 | Hard number (housing starts -21%), KB-sourced; gives MortgageLending a real shock the existing crunch event doesn't model. |
| 4 | `evt_kb_regq_disintermediation` | Regulation Q Bites: Disintermediation | 1959-66 | Meltzer 1951 | 5 | The central recurring mechanic of the era (deposits flee ceilings); KB-sourced; teaches the Reg-Q dynamic the plan calls out. |
| 5 | `evt_kb_3_6_3_banker` | The 3-6-3 Banker | 1950-62 | Greider | 5 | Establishes the era's *vibe* (safest business in America, ~14,000 banks); a low-risk baseline event that makes later volatility legible. |
| 6 | `evt_kb_martin_punch_bowl` | Martin Takes Away the Punch Bowl | 1955-65 | Greider+Meltzer | 5 | Introduces the era's defining character (Martin) as a recurring market force; doctrine is KB-sourced. |
| 7 | `evt_kb_gold_drain_1960s` | The Gold Drain | 1965-71 | Meltzer 1970 | 5 | KB-sourced with the Table 5.1 gold-vs-liabilities numbers; the slow-burn setup that makes Camp David land. |
| 8 | `evt_kb_accord_bond_repricing` | The Pegging Ends — Bonds Reprice | 1951-53 | Meltzer 1951 | 5 | KB-sourced; gives the existing `evt_treasury_accord` a concrete balance-sheet consequence (duration risk on gov bonds). |
| 9 | `evt_kb_regq_suspension_large_cd` | Reg Q Lifted on the Big CDs | 1970-72 | Meltzer 1951 | 5 | KB-sourced; the policy escape valve during Penn Central — a rare *opportunity* born of crisis, good gameplay contrast. |
| 10 | `evt_kb_recession_1958` | The Sharp Recession of 1958 | 1957-59 | Meltzer 1951 | 4 | Fills a genuine gap (no live 1957-58 event); sharpest postwar downturn to that point, KB-sourced, gives the otherwise-placid 1950s a teeth-baring moment. |

**Honorable mentions (merge second wave):** `evt_kb_eurodollar_wriston`, `evt_kb_one_bank_holding_surge`, `evt_kb_bhc_act_1956`, `evt_kb_negotiable_cd_1961` (model_knowledge but high gameplay value), `evt_kb_bankamericard_launch`.

## Full candidate index — Events

| id | title | year(s) | category | one-liner | source | quality |
|---|---|---|---|---|---|---|
| evt_kb_reconversion_credit_squeeze | The Reconversion Credit Squeeze | 1945-47 | opportunity | Fund the chaos of factories switching from shells to Buicks | model_knowledge | 4 |
| evt_kb_savings_bond_unwind | The Great War-Bond Unwind | 1945-50 | opportunity | Households cash war bonds; reposition portfolios for fees | model_knowledge | 3 |
| evt_kb_fed_supports_treasury_refunding | Even Keel: Financing the Treasury | 1945-51 | market | The Fed still pegs bond prices — for now | Meltzer 1951 | 4 |
| evt_kb_regulation_w_installment | Regulation W Tightens the Installment Plan | 1950-52 | regulatory | Korean-War curbs on buying-on-time throttle consumer credit | Meltzer 1951 | 3 |
| evt_kb_regulation_x_realestate | Regulation X Reins In the Builders | 1950-52 | regulatory | Real-estate credit controls squeeze mortgages | Meltzer 1951 | 3 |
| evt_kb_accord_bond_repricing | The Pegging Ends — Bonds Reprice | 1951-53 | market | Safe gov bonds suddenly behave like risky ones | Meltzer 1951 | 5 |
| evt_kb_recession_1953 | The 1953 Pause | 1953-54 | market | Mild post-armistice slowdown; trim the riskiest credits | Meltzer 1951 | 3 |
| evt_kb_bills_only_doctrine | Bills Only at the Desk | 1953-58 | market | Martin lets the long end float; read the curve | Meltzer 1951 | 4 |
| evt_kb_bhc_act_1956 | The Bank Holding Company Act | 1956-62 | regulatory | The one-bank-holding-company loophole opens | Meltzer 1951 | 4 |
| evt_kb_recession_1958 | The Sharp Recession of 1958 | 1957-59 | crisis | Deepest postwar slump to date; provision honestly | Meltzer 1951 | 4 |
| evt_kb_negotiable_cd_1961 | The Negotiable CD Arrives | 1961-66 | opportunity | Bid for corporate cash; liability management is born | model_knowledge | 4 |
| evt_kb_regq_disintermediation | Regulation Q Bites: Disintermediation | 1959-66 | regulatory | Savers flee ceilings for T-bills and thrifts | Meltzer 1951 | 5 |
| evt_kb_martin_punch_bowl | Martin Takes Away the Punch Bowl | 1955-65 | market | The Fed tightens into the boom over the President | Greider+Meltzer | 5 |
| evt_kb_consumer_finance_boom | America Buys on Time | 1953-62 | opportunity | Installment credit explodes with the suburban household | model_knowledge | 3 |
| evt_kb_3_6_3_banker | The 3-6-3 Banker | 1950-62 | market | The safest business in America rewards doing nothing clever | Greider | 5 |
| evt_kb_correspondent_banking | The Correspondent Network | 1948-65 | opportunity | Become the city partner to hundreds of country banks | model_knowledge | 3 |
| evt_kb_eurodollar_wriston | The London Dollar Desk | 1960-70 | opportunity | Fund offshore, beyond Reg Q's reach | Greider | 4 |
| evt_kb_bankamericard_launch | The Plastic Gamble | 1958-68 | opportunity | Mass card drops: ugly early losses, then a goldmine | model_knowledge | 4 |
| evt_kb_interbank_card_1966 | The Interbank Card Alliance | 1966-72 | opportunity | Join the rival network or stay a local curiosity | model_knowledge | 4 |
| evt_kb_municipal_finance_boom | Financing the Suburbs' Schools | 1955-65 | opportunity | Tax-exempt muni paper your richest clients love | Meltzer 1951 | 3 |
| evt_kb_credit_crunch_housing_collapse | The 1966 Crunch Starves Housing | 1966-67 | crisis | Reg Q starves the thrifts; housing starts collapse -21% | Meltzer 1970 | 5 |
| evt_kb_promissory_note_dodge | The Promissory-Note Dodge | 1964-69 | opportunity | Run ahead of the rulebook on market-rate funding | Meltzer 1951 | 4 |
| evt_kb_guns_and_butter_inflation | Guns, Butter, and Creeping Inflation | 1965-69 | market | War + Great Society erode every fixed-rate loan | Greider | 4 |
| evt_kb_one_bank_holding_surge | The One-Bank Holding Company Land Grab | 1968-71 | opportunity | Diversify beyond the spread before the law closes the door | Meltzer 1970 | 4 |
| evt_kb_penn_central_cp_run | The Penn Central Paper Run | 1970-71 | crisis | A railroad colossus can't roll its paper; the run spreads | Meltzer 1951 | 5 |
| evt_kb_regq_suspension_large_cd | Reg Q Lifted on the Big CDs | 1970-72 | regulatory | The ceiling drops on jumbo CDs to stop the panic | Meltzer 1951 | 5 |
| evt_kb_gold_drain_1960s | The Gold Drain | 1965-71 | market | More dollars abroad than gold at home; the peg strains | Meltzer 1970 | 5 |
| evt_kb_camp_david_nixon_shock | Camp David: The Gold Window Slams | 1971-73 | crisis | Nixon ends gold convertibility; currencies will float | Meltzer 1970 | 5 |
| evt_kb_eurodollar_recycling_1971 | Floating Currencies, Flooding Dollars | 1971-73 | opportunity | Intermediate the post-float dollar flood from London | Meltzer 1970 | 4 |
| evt_kb_franklin_warning | The Overreacher's Ascent | 1969-72 | market | A rival's breakneck Eurodollar growth — chase it or fear it | model_knowledge | 3 |
| evt_kb_fed_independence_pressure | The White House Leans on the Fed | 1970-72 | market | Election-year easy money: a gift today, kindling tomorrow | Greider | 4 |
| evt_kb_pension_trust_growth | The Pension Money Pours In | 1955-70 | opportunity | Win the swelling corporate-pension mandates | model_knowledge | 3 |

## Full candidate index — Characters

| name | role | era | archetype | source | needs_web_research? |
|---|---|---|---|---|---|
| William McChesney Martin | Fed Chairman 1951-70 | 1951-70 | patrician | Greider+Meltzer | no |
| Marriner Eccles | Former Fed Chairman | 1945-51 | operator | Greider+Meltzer | no |
| Allan Sproul | NY Fed President | 1945-56 | patrician | Meltzer 1951 | no |
| Arthur Burns | Fed Chairman from 1970 | 1970-72 | operator | Greider+Meltzer | no |
| John Connally | Treasury Secretary 1971 | 1971-72 | rainmaker | Meltzer 1970 | no |
| Paul Volcker | Treasury Under-Sec from 1969 | 1969-72 | quant | Greider+Meltzer | no |
| Robert Roosa | Treasury Under-Sec early 1960s | 1961-65 | quant | Meltzer 1970 | no |
| A. P. Giannini | Founder, Bank of America (d.1949) | 1945-49 | rainmaker | model_knowledge | **yes** |
| George S. Moore | Pres/Chair, First National City | 1959-70 | dealmaker | model_knowledge | **yes** |
| Walter Wriston | Rising exec, First National City | 1960-72 | gunslinger | model_knowledge | **yes** |
| David Rockefeller | Pres, Chase Manhattan | 1955-72 | patrician | model_knowledge | **yes** |
| Charles E. Mitchell (ghost) | 1920s cautionary memory | 1945-72 | gunslinger | model_knowledge | partial |

## Full candidate index — Deal archetypes

| title | division | era | risk | $ | source |
|---|---|---|---|---|---|
| Negotiable CD Auction | CommercialLending | 1961-72 | 4 | 140K | Meltzer 1951 |
| Pension Trust Mandate | TrustAndCustody | 1955-72 | 2 | 90K | model_knowledge |
| Country-Bank Correspondent Tie | CommercialLending | 1945-68 | 2 | 110K | Greider |
| Municipal School-Bond Underwriting | CommercialLending | 1955-66 | 3 | 160K | Meltzer 1970 |
| Installment-Finance Book | CommercialLending | 1953-72 | 4 | 150K | model_knowledge |
| Defense-Electronics Term Loan | CommercialLending | 1950-72 | 3 | 200K | model_knowledge |
| One-Bank Holding Company Play | CommercialLending | 1968-71 | 5 | 230K | Meltzer 1970 |
| Master Charge Consortium Buy-In | CreditCards | 1966-72 | 6 | 185K | model_knowledge |
| London Eurodollar Syndication | InternationalBanking | 1960-72 | 6 | 175K | Greider |
| Penn-Central Rescue Line | CommercialLending | 1970-71 | 8 | 240K | Meltzer 1951 |
| Foreign-Exchange Position Book | InternationalBanking | 1969-72 | 7 | 170K | Meltzer 1970 |
| Trust-Department Estate Mandate | TrustAndCustody | 1945-72 | 1 | 70K | model_knowledge |
| Garden-Apartment Construction Loan | MortgageLending | 1955-72 | 4 | 195K | model_knowledge |

## Merge mechanics (for the follow-up task, not done here)

1. **Events:** strip `provenance` + `draft_quality` from each approved candidate; append to `data/events/historical_events.json`. The `evt_kb_` prefix can be renamed to `evt_`. Note the existing file has a stray mojibake `â€”` (em-dash) in `evt_credit_card_revolution` — candidates here use clean punctuation.
2. **Deals:** strip `provenance` + `draft_quality`; append to `data/deals/post_war_deals.json` (plain array).
3. **Characters:** no live character-registry file exists yet (P4 not built). Hold `characters_1945_1972.json` until the P4 registry lands; it already maps each figure to an `archetypeFamily` in `data/archetypes/archetypes.json` for voice/portrait reuse.
4. After any event merge, the P3.4 market-coupling work will consume the `risk` + `affects` fields these candidates already populate.
