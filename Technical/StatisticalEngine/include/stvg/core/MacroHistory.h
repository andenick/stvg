#pragma once

#include "Types.h"
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <vector>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// MacroHistory (STAR_02 P3.2)
//
// A per-quarter snapshot ring of the macro state plus each market's close.
// A full 95-year game is 380 quarters, so a plain growable vector is fine —
// no eviction needed at game scale. Snapshots are appended at quarter end
// (see QuarterlyTurnManager::onQuarterEnd) and serialized into the save blob
// so chart hydration survives save/load. Missing field on load → empty.
// ════════════════════════════════════════════════════════════════════

struct MacroSnapshot {
    int year = 0;
    int quarter = 0;
    EconomicIndicators econ;                 // full macro indicator copy
    std::map<std::string, double> closes;    // marketId → close price this quarter
};

inline void to_json(nlohmann::json& j, const MacroSnapshot& s) {
    j = nlohmann::json{
        {"year", s.year},
        {"quarter", s.quarter},
        {"econ", s.econ},
        {"closes", s.closes}
    };
}

inline void from_json(const nlohmann::json& j, MacroSnapshot& s) {
    s.year = j.value("year", 0);
    s.quarter = j.value("quarter", 0);
    if (j.contains("econ")) s.econ = j["econ"].get<EconomicIndicators>();
    s.closes.clear();
    if (j.contains("closes") && j["closes"].is_object()) {
        for (auto it = j["closes"].begin(); it != j["closes"].end(); ++it)
            s.closes[it.key()] = it.value().get<double>();
    }
}

class MacroHistory {
public:
    // Append a quarter-end snapshot. Cheap; full-game length is tiny.
    void record(int year, int quarter, const EconomicIndicators& econ,
                const std::vector<Market>& markets) {
        MacroSnapshot snap;
        snap.year = year;
        snap.quarter = quarter;
        snap.econ = econ;
        for (const auto& m : markets) snap.closes[m.id] = m.currentPrice;
        snapshots_.push_back(std::move(snap));
    }

    const std::vector<MacroSnapshot>& snapshots() const { return snapshots_; }
    size_t size() const { return snapshots_.size(); }
    bool empty() const { return snapshots_.empty(); }
    void clear() { snapshots_.clear(); }

    // ── Save/Load ───────────────────────────────────────────────────
    nlohmann::json toSaveJson() const {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& s : snapshots_) arr.push_back(s);
        return arr;
    }

    // Backward-compat: a missing/non-array field loads as empty history.
    void loadSaveJson(const nlohmann::json& j) {
        snapshots_.clear();
        if (!j.is_array()) return;
        for (const auto& item : j) snapshots_.push_back(item.get<MacroSnapshot>());
    }

    // REST shape: { quarters: [ {year, quarter, econ, closes}, ... ] }
    nlohmann::json toApiJson() const {
        return nlohmann::json{{"quarters", toSaveJson()}};
    }

private:
    std::vector<MacroSnapshot> snapshots_;
};

} // namespace stvg
