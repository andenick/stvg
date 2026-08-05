# START PLAYING — The Banker (STVG)

**Build it once, then one command to play.** The web frontend is generated output and
is not committed, so it has to be built before the server has anything to serve:

```powershell
# 1. Build the frontend (Node 20+) and the engine (once, or after pulling changes)
cd Technical\StatisticalEngine\frontend; npm ci; npm run build; cd ..
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build --config Debug --parallel 4
cd ..\..

# 2. Play
.\play.ps1
```

`play.ps1` stops any stuck processes, starts the game server, and opens the game in your
browser. Close the PowerShell window (or Ctrl+C) when you're done — it stops the server.

No API keys, accounts or network services are required; everything runs locally.

## Your first 10 minutes (1945, First National Trust, $1M)

- **The sim starts itself.** Watch the charts move. `Space` pauses, `[` / `]` change
  speed, keys `1-4` switch tabs (Economy · Hire · My Bank · Financials).
- **Loans are the early game.** Prospects trickle into the **Loan Book** rail on the
  left — click one, read the banker's pitch, Invest or Pass. Outcomes come back later
  ("paid off" / "defaulted") and your book stats accumulate. The pitch language matters:
  terse and specific tends to be honest; long-winded flattery with urgency tends not
  to be. Learn who to trust.
- **The HIRE button** (top bar) glows when you can afford someone. Candidates trickle in
  with personalities — gunslingers, credit hawks, relationship men. Who you hire changes
  what your divisions earn, how volatile they are, and what the world offers you next.
- **Click any chart** to make it the hero; switch time scales (1Y → MAX). GDP,
  unemployment, rates, and the markets all move with the engine — headlines on the
  ticker carry ▲/▼ that match what's really happening. Reading the tape is a skill.
- **Try a trade.** Promote a market (e.g. S&P 500) to the hero chart and BUY a little —
  that's your personal account, separate from the bank's books.
- **Characters will pop up** (bottom-right) — advisors, your risk officer, eventually
  presidents. Click once to finish their line, click again to dismiss. They never block
  the game.
- **Decisions:** routine memos expire if you ignore them (the world moves on). Big ones
  — acquisitions, crises — pause the game until you act. You can flip this in Settings.

## When you're done playing

Everything you did was recorded locally (every click, what you read and for how long,
every trade and hire) to `Technical\StatisticalEngine\telemetry\`. Run `.\analyze.ps1`
to turn those sessions into readable reports. Nothing is uploaded anywhere.

## Known rough edges (already on the list)

- Character portraits are procedural placeholders (DiceBear) — real caricature art comes
  later via local sprite generation.
- "My Bank" tab's trading-floor visual is a placeholder strip (tiny-tower view planned).
- Candidate 1945-72 historical events are staged in `Technical/Content/kb_mining/`
  and not yet merged into play — the early decades get much richer
  once you approve them.
- A deep-game balance pass (leverage death-spiral around 2021 for max-leverage
  strategies) is on the backlog.
