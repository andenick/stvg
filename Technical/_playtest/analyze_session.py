#!/usr/bin/env python3
"""
analyze_session.py — turn one STVG telemetry session JSONL into a human- and
agent-readable markdown report (Telemetry v2 / STAR_02 P1).

From a single session file an agent can reconstruct the whole play timeline:
what the player read and for how long, what they ignored, engagement, pace, and
the money curve.

Input: a session JSONL where each line is one event:
    {"t": <ms_since_session_start>, "type": "<event>", "data": { ... }}
Every event is auto-stamped (P1) with {year, quarter, day, capital, paused,
speed} inside `data`, so most cross-sections can be computed from any event.

Usage:
    python analyze_session.py <file.jsonl> [-o report.md]
    python analyze_session.py --selftest          # synthetic-JSONL parser test

Stdlib only. No third-party deps, no network.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from typing import Any


# ─────────────────────────────────────────────────────────────────────────────
# Parsing
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class Event:
    t: int                              # ms since session start
    type: str
    data: dict[str, Any] = field(default_factory=dict)

    # Auto-context accessors (P1 stamp). All optional — older files predate it.
    @property
    def year(self) -> int | None: return _num(self.data.get("year"))
    @property
    def quarter(self) -> int | None: return _num(self.data.get("quarter"))
    @property
    def day(self) -> int | None: return _num(self.data.get("day"))
    @property
    def capital(self) -> float | None: return _fnum(self.data.get("capital"))
    @property
    def paused(self): return self.data.get("paused")
    @property
    def speed(self) -> float | None: return _fnum(self.data.get("speed"))


def _num(v) -> int | None:
    try:
        if v is None:
            return None
        return int(v)
    except (TypeError, ValueError):
        return None


def _fnum(v) -> float | None:
    try:
        if v is None:
            return None
        return float(v)
    except (TypeError, ValueError):
        return None


def parse_events(text: str) -> list[Event]:
    """Parse JSONL text → list[Event]. Tolerant of blank lines and bad lines."""
    out: list[Event] = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
        except json.JSONDecodeError:
            continue
        if not isinstance(obj, dict):
            continue
        t = obj.get("t", 0)
        try:
            t = int(t)
        except (TypeError, ValueError):
            t = 0
        typ = str(obj.get("type", "unknown"))
        data = obj.get("data")
        if not isinstance(data, dict):
            data = {}
        out.append(Event(t=t, type=typ, data=data))
    out.sort(key=lambda e: e.t)
    return out


# ─────────────────────────────────────────────────────────────────────────────
# Formatting helpers
# ─────────────────────────────────────────────────────────────────────────────

def fmt_money(v: float | None) -> str:
    """Mirror the frontend fmtMoney scale ladder (T/B/M/K/plain)."""
    if v is None:
        return "—"
    neg = v < 0
    a = abs(v)
    if a >= 1e12:
        body = f"${a / 1e12:.2f}T"
    elif a >= 1e9:
        body = f"${a / 1e9:.2f}B"
    elif a >= 1e6:
        body = f"${a / 1e6:.2f}M"
    elif a >= 1e3:
        body = f"${a / 1e3:.1f}K"
    else:
        body = f"${a:.0f}"
    return ("-" if neg else "") + body


def fmt_ms(ms: float) -> str:
    if ms < 1000:
        return f"{ms:.0f}ms"
    s = ms / 1000.0
    if s < 60:
        return f"{s:.1f}s"
    m = int(s // 60)
    rem = s - m * 60
    return f"{m}m{rem:04.1f}s"


def fmt_clock(ms: float) -> str:
    """mm:ss of session wall-clock for a timeline."""
    s = int(ms // 1000)
    return f"{s // 60:02d}:{s % 60:02d}"


def yq(ev: Event) -> str:
    y, q = ev.year, ev.quarter
    if y is None:
        return "—"
    return f"{y} Q{q}" if q is not None else str(y)


# ─────────────────────────────────────────────────────────────────────────────
# Analysis
# ─────────────────────────────────────────────────────────────────────────────

# Surfaces whose dwell `key` prefix groups them in the dwell table.
def surface_of(key: str) -> str:
    if ":" in key:
        return key.split(":", 1)[0]
    return key


def analyze(events: list[Event]) -> dict[str, Any]:
    r: dict[str, Any] = {}
    r["n_events"] = len(events)
    r["type_counts"] = Counter(e.type for e in events)

    if not events:
        r["duration_ms"] = 0
        return r

    t0, t1 = events[0].t, events[-1].t
    r["duration_ms"] = t1 - t0
    r["t0"], r["t1"] = t0, t1

    # ── Session header (P1 session_start) ─────────────────────────────────────
    start = next((e for e in events if e.type == "session_start"), None)
    r["session_start"] = start.data if start else {}

    # ── Pace: game-years covered ──────────────────────────────────────────────
    years = [e.year for e in events if e.year is not None]
    if years:
        r["year_min"], r["year_max"] = min(years), max(years)
        r["game_years"] = r["year_max"] - r["year_min"]
    else:
        r["year_min"] = r["year_max"] = r["game_years"] = None
    active_min = max(r["duration_ms"] / 60000.0, 1e-9)
    r["active_minutes"] = r["duration_ms"] / 60000.0
    r["pace_years_per_min"] = (r["game_years"] / active_min) if r["game_years"] else 0.0

    # ── Money curve (capital over time, drawdowns) ────────────────────────────
    curve = [(e.t, e.capital) for e in events if e.capital is not None]
    r["capital_points"] = curve
    if curve:
        caps = [c for _, c in curve]
        r["capital_start"] = curve[0][1]
        r["capital_end"] = curve[-1][1]
        r["capital_max"] = max(caps)
        r["capital_min"] = min(caps)
        # Max drawdown: largest peak→trough drop over the run.
        peak = caps[0]
        max_dd = 0.0
        max_dd_pct = 0.0
        for c in caps:
            peak = max(peak, c)
            if peak > 0:
                dd = peak - c
                if dd > max_dd:
                    max_dd = dd
                    max_dd_pct = dd / peak
        r["max_drawdown"] = max_dd
        r["max_drawdown_pct"] = max_dd_pct
    else:
        r["capital_start"] = r["capital_end"] = None

    # ── Dwell-by-surface table ────────────────────────────────────────────────
    dwell_by_key: dict[str, list[float]] = defaultdict(list)
    for e in events:
        if e.type == "dwell":
            ms = _fnum(e.data.get("ms"))
            key = e.data.get("key")
            if ms is not None and key:
                dwell_by_key[str(key)].append(ms)
    surf_agg: dict[str, dict[str, float]] = defaultdict(
        lambda: {"total": 0.0, "views": 0, "keys": set()}  # type: ignore
    )
    for key, msl in dwell_by_key.items():
        s = surface_of(key)
        surf_agg[s]["total"] += sum(msl)
        surf_agg[s]["views"] += len(msl)
        surf_agg[s]["keys"].add(key)  # type: ignore
    r["dwell_by_surface"] = surf_agg
    r["dwell_by_key"] = dwell_by_key

    # ── Decisions: seen / acted / lapsed ──────────────────────────────────────
    submitted = [e for e in events if e.type == "decision_submit"]
    lapsed = [e for e in events if e.type == "decision_lapsed"]
    seen_ids = set()
    for e in events:
        if e.type == "dwell" and str(e.data.get("key", "")).startswith("decision:"):
            seen_ids.add(str(e.data["key"]).split(":", 1)[1])
    for e in submitted:
        if e.data.get("decisionId"):
            seen_ids.add(str(e.data["decisionId"]))
    for e in lapsed:
        if e.data.get("decisionId"):
            seen_ids.add(str(e.data["decisionId"]))
    r["decisions_seen"] = len(seen_ids)
    r["decisions_acted"] = len(submitted)
    r["decisions_lapsed"] = len(lapsed)
    # Read-time on decisions: from decision: dwell records.
    dec_dwells = [
        _fnum(e.data.get("ms"))
        for e in events
        if e.type == "dwell" and str(e.data.get("key", "")).startswith("decision:")
        and _fnum(e.data.get("ms")) is not None
    ]
    r["decision_read_avg_ms"] = (sum(dec_dwells) / len(dec_dwells)) if dec_dwells else 0.0
    r["lapsed_visible_ms"] = [
        _fnum(e.data.get("visibleMs")) or 0.0 for e in lapsed
    ]

    # ── Deals: viewed / invested / passed / expired ───────────────────────────
    r["deals_arrived"] = sum(1 for e in events if e.type == "deal_arrived")
    r["deals_opened"] = sum(1 for e in events if e.type == "deal_opened")
    r["deals_invested"] = sum(1 for e in events if e.type == "deal_invest_click")
    r["deals_passed"] = sum(1 for e in events if e.type == "deal_pass")
    r["deals_expired"] = sum(1 for e in events if e.type == "deal_expired")

    # ── Hires ─────────────────────────────────────────────────────────────────
    r["hire_detail_opens"] = sum(1 for e in events if e.type == "hire_detail_open")
    r["hires"] = sum(1 for e in events if e.type == "hire_click")

    # ── Markets / panels ──────────────────────────────────────────────────────
    r["markets_opened"] = sum(1 for e in events if e.type == "market_opened")
    r["panel_views"] = Counter(
        str(e.data.get("panel", "?")) for e in events if e.type == "panel_view"
    )

    # ── Top hovered surfaces ──────────────────────────────────────────────────
    hovers = Counter(
        str(e.data.get("key", "?")) for e in events if e.type == "hover"
    )
    r["hover_counts"] = hovers

    # ── Notable-event timeline ────────────────────────────────────────────────
    notable = {
        "game_start", "game_end", "era_transition",
        "crisis_start", "crisis_respond", "crisis_end",
        "annual_report_open", "decision_submit", "decision_lapsed",
        "deal_invest_click", "hire_click",
    }
    r["timeline"] = [e for e in events if e.type in notable]

    # ── Engagement index: interactions per active minute, over time ───────────
    interaction_types = {
        "deal_opened", "deal_invest_click", "deal_pass", "decision_submit",
        "market_opened", "market_chip_switch", "news_open", "hire_detail_open",
        "hire_click", "hover", "panel_view", "sim_toggle", "sim_speed",
        "crisis_respond", "dwell",
    }
    interactions = [e for e in events if e.type in interaction_types]
    r["n_interactions"] = len(interactions)
    r["engagement_overall"] = len(interactions) / active_min
    # Per-minute buckets.
    buckets: dict[int, int] = defaultdict(int)
    for e in interactions:
        buckets[(e.t - t0) // 60000] += 1
    r["engagement_buckets"] = dict(sorted(buckets.items()))

    return r


# ─────────────────────────────────────────────────────────────────────────────
# Reporting
# ─────────────────────────────────────────────────────────────────────────────

def render_report(r: dict[str, Any], source: str) -> str:
    L: list[str] = []
    add = L.append

    add(f"# STVG Session Report")
    add("")
    add(f"**Source:** `{source}`  ")
    hdr = r.get("session_start", {})
    if hdr.get("sessionId"):
        add(f"**Session:** `{hdr['sessionId']}`  ")
    if hdr.get("startedAt"):
        add(f"**Started:** {hdr['startedAt']}  ")
    add(f"**Events:** {r['n_events']}  ")
    add("")

    if r["n_events"] == 0:
        add("_Empty session — no events to analyze._")
        return "\n".join(L) + "\n"

    # ── Overview ──────────────────────────────────────────────────────────────
    add("## Overview")
    add("")
    add(f"- **Session duration:** {fmt_ms(r['duration_ms'])} "
        f"({r['active_minutes']:.1f} active min)")
    if r.get("game_years") is not None:
        add(f"- **Game-years covered:** {r['game_years']} "
            f"({r['year_min']}→{r['year_max']})")
        add(f"- **Pace:** {r['pace_years_per_min']:.2f} game-years / active minute")
    add(f"- **Distinct event types:** {len(r['type_counts'])}")
    add(f"- **Total interactions:** {r['n_interactions']} "
        f"({r['engagement_overall']:.1f} / active min)")
    add("")

    # ── Money curve ───────────────────────────────────────────────────────────
    add("## Money curve")
    add("")
    if r.get("capital_start") is not None:
        add(f"- **Capital:** {fmt_money(r['capital_start'])} → "
            f"{fmt_money(r['capital_end'])}")
        add(f"- **Peak / trough:** {fmt_money(r['capital_max'])} / "
            f"{fmt_money(r['capital_min'])}")
        add(f"- **Max drawdown:** {fmt_money(r['max_drawdown'])} "
            f"({r['max_drawdown_pct'] * 100:.1f}%)")
        # Sparkline of up to ~20 sampled points.
        add("")
        add("```")
        add(_sparkline([c for _, c in r["capital_points"]]))
        add("```")
    else:
        add("_No capital readings in this session._")
    add("")

    # ── Timeline ──────────────────────────────────────────────────────────────
    add("## Timeline of notable events")
    add("")
    if r["timeline"]:
        add("| Clock | Y/Q | Event | Detail |")
        add("|---|---|---|---|")
        for e in r["timeline"]:
            add(f"| {fmt_clock(e.t - r['t0'])} | {yq(e)} | `{e.type}` | "
                f"{_timeline_detail(e)} |")
    else:
        add("_No notable lifecycle events recorded._")
    add("")

    # ── Dwell-by-surface ──────────────────────────────────────────────────────
    add("## Dwell by surface (read-time)")
    add("")
    surf = r["dwell_by_surface"]
    if surf:
        add("| Surface | Total | Avg/view | Views | Distinct |")
        add("|---|---|---|---|---|")
        for s, agg in sorted(surf.items(), key=lambda kv: -kv[1]["total"]):
            views = agg["views"]
            avg = agg["total"] / views if views else 0
            add(f"| `{s}` | {fmt_ms(agg['total'])} | {fmt_ms(avg)} | "
                f"{views} | {len(agg['keys'])} |")
    else:
        add("_No dwell records (nothing was read long enough to count, ≥300ms)._")
    add("")

    # ── Decisions ─────────────────────────────────────────────────────────────
    add("## Decisions")
    add("")
    add(f"- **Seen:** {r['decisions_seen']}  ·  "
        f"**Acted:** {r['decisions_acted']}  ·  "
        f"**Lapsed:** {r['decisions_lapsed']}")
    if r["decision_read_avg_ms"]:
        add(f"- **Avg read-time before deciding:** {fmt_ms(r['decision_read_avg_ms'])}")
    if r["lapsed_visible_ms"]:
        avg_lapse = sum(r["lapsed_visible_ms"]) / len(r["lapsed_visible_ms"])
        add(f"- **Lapsed decisions were on screen avg** {fmt_ms(avg_lapse)} "
            f"before expiring (saw-and-ignored, not never-saw)")
    add("")

    # ── Deals ─────────────────────────────────────────────────────────────────
    add("## Deals")
    add("")
    add(f"- **Arrived:** {r['deals_arrived']}  ·  "
        f"**Opened:** {r['deals_opened']}  ·  "
        f"**Invested:** {r['deals_invested']}  ·  "
        f"**Passed:** {r['deals_passed']}  ·  "
        f"**Expired (unacted):** {r['deals_expired']}")
    add("")

    # ── Hiring & markets ──────────────────────────────────────────────────────
    add("## Hiring, markets & panels")
    add("")
    add(f"- **Hire detail opens:** {r['hire_detail_opens']}  ·  "
        f"**Hires:** {r['hires']}")
    add(f"- **Markets opened:** {r['markets_opened']}")
    if r["panel_views"]:
        pv = ", ".join(f"{p} ×{n}" for p, n in r["panel_views"].most_common())
        add(f"- **Panel views:** {pv}")
    add("")

    # ── Top hovered surfaces ──────────────────────────────────────────────────
    add("## Top hovered surfaces")
    add("")
    if r["hover_counts"]:
        add("| Surface key | Hovers |")
        add("|---|---|")
        for key, n in r["hover_counts"].most_common(15):
            add(f"| `{key}` | {n} |")
    else:
        add("_No hover events recorded._")
    add("")

    # ── Engagement over time ──────────────────────────────────────────────────
    add("## Engagement index (interactions / minute)")
    add("")
    if r["engagement_buckets"]:
        add("| Minute | Interactions |")
        add("|---|---|")
        for minute, n in r["engagement_buckets"].items():
            bar = "█" * min(40, n)
            add(f"| {minute:>2} | {n:>3} {bar} |")
    else:
        add("_No interactions bucketed._")
    add("")

    # ── Event-type inventory ──────────────────────────────────────────────────
    add("## Event-type inventory")
    add("")
    add("| Event type | Count |")
    add("|---|---|")
    for typ, n in r["type_counts"].most_common():
        add(f"| `{typ}` | {n} |")
    add("")

    return "\n".join(L) + "\n"


def _timeline_detail(e: Event) -> str:
    d = e.data
    bits = []
    for k in ("reason", "title", "choice", "from", "to", "year", "mode",
              "optionId", "archetype", "severity", "visibleMs", "amount",
              "finalCapital"):
        if k in d and d[k] not in (None, ""):
            v = d[k]
            if k in ("amount", "finalCapital") and isinstance(v, (int, float)):
                v = fmt_money(float(v))
            bits.append(f"{k}={v}")
    return ", ".join(bits) if bits else ""


def _sparkline(values: list[float]) -> str:
    """ASCII sparkline of a numeric series (downsampled to ~50 cols)."""
    vals = [v for v in values if v is not None]
    if not vals:
        return "(no data)"
    cols = 50
    if len(vals) > cols:
        step = len(vals) / cols
        vals = [vals[int(i * step)] for i in range(cols)]
    lo, hi = min(vals), max(vals)
    bars = "▁▂▃▄▅▆▇█"
    if hi == lo:
        return bars[0] * len(vals)
    out = []
    for v in vals:
        idx = int((v - lo) / (hi - lo) * (len(bars) - 1))
        out.append(bars[idx])
    return "".join(out) + f"   [{fmt_money(lo)} … {fmt_money(hi)}]"


# ─────────────────────────────────────────────────────────────────────────────
# Self-test
# ─────────────────────────────────────────────────────────────────────────────

SELFTEST_JSONL = "\n".join([
    json.dumps({"t": 0, "type": "session_start",
                "data": {"sessionId": "selftest", "startedAt": "2026-06-11T00:00:00Z",
                         "year": 1945, "quarter": 1, "day": 0, "capital": 1_000_000,
                         "paused": True, "speed": 1}}),
    json.dumps({"t": 500, "type": "game_start",
                "data": {"ceoId": "ceo_1", "scenario": "default", "startingPosition": 1,
                         "year": 1945, "quarter": 1, "capital": 1_000_000}}),
    json.dumps({"t": 2000, "type": "sim_toggle",
                "data": {"paused": False, "year": 1945, "quarter": 1, "capital": 1_000_000}}),
    json.dumps({"t": 3000, "type": "deal_arrived",
                "data": {"id": "deal_1", "title": "Regional Bank", "amount": 50000,
                         "year": 1945, "quarter": 1, "capital": 1_010_000}}),
    json.dumps({"t": 4000, "type": "deal_opened",
                "data": {"id": "deal_1", "year": 1945, "quarter": 1, "capital": 1_010_000}}),
    json.dumps({"t": 6500, "type": "dwell",
                "data": {"key": "deal:deal_1", "ms": 2500, "year": 1945,
                         "quarter": 1, "capital": 1_010_000}}),
    json.dumps({"t": 6600, "type": "deal_invest_click",
                "data": {"id": "deal_1", "amount": 50000, "year": 1945,
                         "quarter": 1, "capital": 1_010_000}}),
    json.dumps({"t": 8000, "type": "panel_view",
                "data": {"panel": "PathPanel", "year": 1946, "quarter": 1, "capital": 1_050_000}}),
    json.dumps({"t": 9000, "type": "dwell",
                "data": {"key": "panel:PathPanel", "ms": 1200, "year": 1946,
                         "quarter": 1, "capital": 1_050_000}}),
    json.dumps({"t": 10000, "type": "hover",
                "data": {"key": "market:SP500", "year": 1946, "quarter": 1, "capital": 1_050_000}}),
    json.dumps({"t": 11000, "type": "market_opened",
                "data": {"id": "SP500", "year": 1946, "quarter": 1, "capital": 1_050_000}}),
    json.dumps({"t": 12000, "type": "decision_lapsed",
                "data": {"decisionId": "dec_1", "visibleMs": 4000, "year": 1947,
                         "quarter": 2, "capital": 900_000}}),
    json.dumps({"t": 13000, "type": "decision_submit",
                "data": {"decisionId": "dec_2", "optionId": "opt_a", "title": "Expand",
                         "year": 1947, "quarter": 3, "capital": 950_000}}),
    json.dumps({"t": 14000, "type": "crisis_start",
                "data": {"crisisId": "c1", "title": "Bank Run", "severity": 8,
                         "year": 1948, "quarter": 1, "capital": 800_000}}),
    json.dumps({"t": 15000, "type": "crisis_respond",
                "data": {"crisisId": "c1", "choice": "measured", "year": 1948,
                         "quarter": 1, "capital": 800_000}}),
    json.dumps({"t": 16000, "type": "era_transition",
                "data": {"from": "Post-War", "to": "Go-Go Years", "year": 1960,
                         "quarter": 1, "capital": 2_000_000}}),
    json.dumps({"t": 17000, "type": "game_end",
                "data": {"reason": "SurvivorWin", "finalCapital": 2_500_000,
                         "year": 2040, "quarter": 4, "capital": 2_500_000}}),
])


def selftest() -> int:
    events = parse_events(SELFTEST_JSONL)
    assert len(events) == 17, f"expected 17 events, got {len(events)}"
    # Auto-context stamp readable.
    assert events[0].year == 1945, "session_start year stamp missing"
    assert events[0].capital == 1_000_000, "capital stamp missing"
    # Sorted by t.
    assert all(events[i].t <= events[i + 1].t for i in range(len(events) - 1)), "not sorted"

    r = analyze(events)
    assert r["game_years"] == 2040 - 1945, f"game_years wrong: {r['game_years']}"
    assert r["decisions_acted"] == 1, f"acted wrong: {r['decisions_acted']}"
    assert r["decisions_lapsed"] == 1, f"lapsed wrong: {r['decisions_lapsed']}"
    assert r["deals_invested"] == 1, f"invested wrong: {r['deals_invested']}"
    assert r["deals_arrived"] == 1, f"arrived wrong: {r['deals_arrived']}"
    assert r["markets_opened"] == 1, "market_opened miscount"
    assert "deal" in r["dwell_by_surface"], "deal dwell surface missing"
    assert "panel" in r["dwell_by_surface"], "panel dwell surface missing"
    assert r["dwell_by_surface"]["deal"]["total"] == 2500, "deal dwell total wrong"
    assert r["capital_max"] == 2_500_000, f"capital_max wrong: {r['capital_max']}"
    assert r["capital_min"] == 800_000, f"capital_min wrong: {r['capital_min']}"
    assert r["max_drawdown"] > 0, "expected a drawdown"
    assert r["hover_counts"]["market:SP500"] == 1, "hover not counted"
    assert len(r["timeline"]) >= 8, f"timeline too short: {len(r['timeline'])}"

    # Robustness: blank lines + a malformed line must not crash.
    noisy = SELFTEST_JSONL + "\n\n{not json}\n" + json.dumps({"t": "x", "type": "weird"})
    ev2 = parse_events(noisy)
    assert len(ev2) == 18, f"expected 18 (noisy adds 1 valid), got {len(ev2)}"

    # The report must render without error and contain key sections.
    md = render_report(r, "<selftest>")
    for section in ("# STVG Session Report", "## Money curve", "## Timeline",
                    "## Dwell by surface", "## Decisions", "## Deals",
                    "## Engagement index", "## Event-type inventory"):
        assert section in md, f"report missing section: {section}"

    print("SELFTEST PASS — parser, analyzer, and report all OK "
          f"({len(events)} events, {len(r['type_counts'])} types).")
    return 0


# ─────────────────────────────────────────────────────────────────────────────
# CLI
# ─────────────────────────────────────────────────────────────────────────────

def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="Analyze an STVG telemetry session JSONL.")
    ap.add_argument("file", nargs="?", help="session JSONL file")
    ap.add_argument("-o", "--output", help="write markdown report to this path")
    ap.add_argument("--selftest", action="store_true",
                    help="run the synthetic-JSONL self-test and exit")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()

    if not args.file:
        ap.error("a session JSONL file is required (or use --selftest)")

    try:
        with open(args.file, "r", encoding="utf-8") as f:
            text = f.read()
    except OSError as e:
        print(f"ERROR: cannot read {args.file}: {e}", file=sys.stderr)
        return 2

    events = parse_events(text)
    report = render_report(analyze(events), args.file)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(report)
        # Also print a compact summary to stdout.
        types = Counter(e.type for e in events)
        print(f"Wrote {args.output} — {len(events)} events, "
              f"{len(types)} distinct event types.")
    else:
        sys.stdout.write(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
