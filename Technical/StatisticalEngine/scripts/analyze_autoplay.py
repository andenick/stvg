"""STVG Autoplay Balance Analysis Script

Reads autoplay CSV results and produces a pass/fail balance report.
Usage: python scripts/analyze_autoplay.py [--data-dir data/autoplay_results]
"""

import csv
import sys
import os
from collections import defaultdict
from pathlib import Path

def load_csv(path):
    with open(path, newline='') as f:
        return list(csv.DictReader(f))

def analyze_strategy_comparison(rows):
    print("\n" + "="*70)
    print("  STRATEGY COMPARISON ANALYSIS")
    print("="*70)

    strategies = []
    for r in rows:
        s = {
            'name': r['strategy'],
            'games': int(r['games']),
            'survived': int(r['survived']),
            'survivalRate': float(r['survivalRate']),
            'avgScore': float(r['avgScore']),
            'stdDev': float(r['stdDev']),
            'avgCapital': float(r['avgCapital']),
            'nanCount': int(r['nanCount']),
            'stuckCount': int(r['stuckCount']),
            'bankruptcies': int(r['bankruptcies']),
        }
        strategies.append(s)

    print(f"\n  {'Strategy':<20} {'Surv%':>6} {'AvgScore':>9} {'StdDev':>8} {'AvgCap':>10} {'NaN':>4}")
    print("  " + "-"*60)
    for s in strategies:
        cap_str = f"${s['avgCapital']/1e9:.1f}B"
        print(f"  {s['name']:<20} {s['survivalRate']*100:>5.1f}% {s['avgScore']:>9.1f} {s['stdDev']:>8.1f} {cap_str:>10} {s['nanCount']:>4}")

    return strategies

def analyze_full_matrix(rows):
    print("\n" + "="*70)
    print("  FULL MATRIX ANALYSIS (Per-Game Detail)")
    print("="*70)

    by_strategy = defaultdict(list)
    for r in rows:
        by_strategy[r['strategy']].append(r)

    print(f"\n  {'Strategy':<20} {'Games':>5} {'Surv%':>6} {'AvgQ':>6} {'MaxQ':>6} {'Eras':>5} {'EndReason (mode)'}")
    print("  " + "-"*75)

    era_counts = defaultdict(int)
    total_games = 0
    total_survived = 0
    era_reached = defaultdict(int)

    for name, games in sorted(by_strategy.items()):
        quarters = [int(g['quartersPlayed']) for g in games]
        survived = sum(1 for g in games if g['survived'] == '1')
        avg_q = sum(quarters) / len(quarters) if quarters else 0
        max_q = max(quarters) if quarters else 0
        eras = [int(g['erasReached']) for g in games]
        avg_eras = sum(eras) / len(eras) if eras else 0

        # Count end reasons
        reasons = defaultdict(int)
        for g in games:
            reasons[g['gameEndReason']] += 1
        mode_reason = max(reasons, key=reasons.get) if reasons else "N/A"

        # Track era progression
        for g in games:
            era_idx = int(g['erasReached'])
            for i in range(1, era_idx + 1):
                era_reached[i] += 1

        total_games += len(games)
        total_survived += survived

        print(f"  {name:<20} {len(games):>5} {survived/len(games)*100:>5.1f}% {avg_q:>6.0f} {max_q:>6} {avg_eras:>5.1f} {mode_reason}")

    print(f"\n  Era Progression (across all {total_games} games):")
    era_names = {1: "Post-War", 2: "Expansion", 3: "Volatility", 4: "Big Bang",
                 5: "Shadow", 6: "Reform", 7: "Modern"}
    for era_num in sorted(era_reached.keys()):
        pct = era_reached[era_num] / total_games * 100
        name = era_names.get(era_num, f"Era {era_num}")
        bar = "#" * int(pct / 2)
        print(f"    Era {era_num} ({name:<12}): {era_reached[era_num]:>4}/{total_games} ({pct:>5.1f}%) {bar}")

    return by_strategy, era_reached, total_games

def run_balance_checks(strategies, era_reached=None, total_games=0):
    print("\n" + "="*70)
    print("  BALANCE CRITERIA - PASS/FAIL")
    print("="*70)

    results = []

    # MUST PASS
    total_nan = sum(s['nanCount'] for s in strategies)
    total_stuck = sum(s['stuckCount'] for s in strategies)
    total_games_sc = sum(s['games'] for s in strategies)

    results.append(("MUST", "0 NaN/Inf", total_nan == 0, f"{total_nan} found"))
    results.append(("MUST", "0 stuck states", total_stuck == 0, f"{total_stuck} found"))

    total_bankrupt = sum(s['bankruptcies'] for s in strategies)
    bankrupt_rate = total_bankrupt / total_games_sc if total_games_sc > 0 else 0
    results.append(("MUST", "Bankruptcy < 20%", bankrupt_rate < 0.20, f"{bankrupt_rate*100:.1f}%"))

    min_surv = min(s['survivalRate'] for s in strategies)
    max_surv = max(s['survivalRate'] for s in strategies)
    results.append(("MUST", "No strategy < 10% survival", min_surv >= 0.10, f"min={min_surv*100:.1f}%"))
    results.append(("MUST", "No strategy > 95% survival", max_surv <= 0.95, f"max={max_surv*100:.1f}%"))

    # Dominance check
    scores = sorted([s['avgScore'] for s in strategies], reverse=True)
    if len(scores) >= 2:
        best_s = [s for s in strategies if s['avgScore'] == scores[0]][0]
        advantage = scores[0] - scores[1]
        dominant = advantage > 2.0 * best_s['stdDev']
        results.append(("MUST", "No dominant strategy (< 2 std dev)", not dominant,
                        f"gap={advantage:.1f}, 2sd={2*best_s['stdDev']:.1f}"))

    # SHOULD PASS (only if full matrix data available)
    if era_reached and total_games > 0:
        era3_pct = era_reached.get(3, 0) / total_games
        era5_pct = era_reached.get(5, 0) / total_games
        era7_pct = era_reached.get(7, 0) / total_games
        results.append(("SHOULD", "50% reach Era 3 (Volatility)", era3_pct >= 0.50, f"{era3_pct*100:.1f}%"))
        results.append(("SHOULD", "20% reach Era 5 (Shadow)", era5_pct >= 0.20, f"{era5_pct*100:.1f}%"))
        results.append(("SHOULD", "5% reach Era 7 (Modern)", era7_pct >= 0.05, f"{era7_pct*100:.1f}%"))

    # Print results
    passed = 0
    failed = 0
    for level, name, ok, detail in results:
        status = "PASS" if ok else "FAIL"
        icon = "+" if ok else "X"
        if ok:
            passed += 1
        else:
            failed += 1
        print(f"  [{icon}] {level:>6} | {name:<40} | {status} ({detail})")

    print(f"\n  TOTAL: {passed} passed, {failed} failed out of {passed+failed} checks")
    return failed == 0

def main():
    data_dir = "data/autoplay_results"
    if len(sys.argv) > 2 and sys.argv[1] == "--data-dir":
        data_dir = sys.argv[2]

    print("STVG Autoplay Balance Analysis")
    print(f"Data directory: {data_dir}")

    # Load strategy comparison (always present)
    sc_path = os.path.join(data_dir, "strategy_comparison.csv")
    if not os.path.exists(sc_path):
        print(f"ERROR: {sc_path} not found. Run autoplay first.")
        sys.exit(1)

    strategies = analyze_strategy_comparison(load_csv(sc_path))

    # Load full matrix if available
    fm_path = os.path.join(data_dir, "full_matrix.csv")
    era_reached = None
    total_games = 0
    if os.path.exists(fm_path):
        by_strategy, era_reached, total_games = analyze_full_matrix(load_csv(fm_path))
    else:
        print(f"\n  (No full_matrix.csv found — skipping era progression analysis)")

    # Run balance checks
    all_pass = run_balance_checks(strategies, era_reached, total_games)

    print("\n" + "="*70)
    if all_pass:
        print("  RESULT: ALL CHECKS PASSED")
    else:
        print("  RESULT: SOME CHECKS FAILED — balance tuning needed")
    print("="*70 + "\n")

    sys.exit(0 if all_pass else 1)

if __name__ == "__main__":
    main()
