# START PLAYING — The Banker (STVG)

**One command:** open PowerShell in this folder and run

```powershell
.\play.ps1
```

That kills any stuck processes, starts the game server, and opens the game in your
browser. Close the PowerShell window (or Ctrl+C) when you're done — it stops the server.

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
every trade and hire) to `Technical\StatisticalEngine\telemetry\`. Either run
`.\analyze.ps1` to generate session reports, or just tell Claude **"analyze my
sessions"** — and say how it *felt* (too fast/slow? money too easy? charts readable?
characters annoying or fun?). The data plus your feel is how the next iteration gets
tuned (pace, profit curve, volatility, character cadence are all dials now).

## Known rough edges (already on the list)

- Character portraits are procedural placeholders (DiceBear) — real caricature art comes
  later via local sprite generation on the 5090.
- "My Bank" tab's trading-floor visual is a placeholder strip (tiny-tower view planned).
- Mined 1945-72 historical events are staged awaiting your review
  (`Technical/Content/kb_mining/REVIEW_SHEET.md`) — the early decades get much richer
  once you approve them.
- A deep-game balance pass (leverage death-spiral around 2021 for max-leverage
  strategies) is on the backlog.
