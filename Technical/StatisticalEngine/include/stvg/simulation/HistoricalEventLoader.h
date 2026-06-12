#pragma once

#include "../core/Types.h"
#include "../math/RandomEngine.h"
#include "RiskReturnProfile.h"
#include "EventEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <spdlog/spdlog.h>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Historical Event — loaded from JSON, filtered by era, converted
// to GameEvent with randomized risk/return parameters.
// ════════════════════════════════════════════════════════════════════

struct HistoricalEvent {
    std::string id;
    std::string title;
    std::string description;
    std::string category;
    int eraEarliest = 1945;
    int eraLatest = 2030;
    RiskReturnProfile risk;
    std::vector<std::string> benefits;  // DivisionType names that benefit
    std::vector<std::string> suffers;   // DivisionType names that suffer
    double weight = 1.0;
    std::string historicalNote;
};

// ════════════════════════════════════════════════════════════════════
// Loader — reads historical events from JSON, filters by game year
// ════════════════════════════════════════════════════════════════════

class HistoricalEventLoader {
public:
    // Load events from JSON file
    bool loadFromFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) {
            spdlog::warn("Could not open historical events file: {}", path);
            return false;
        }

        try {
            auto j = nlohmann::json::parse(f);
            for (const auto& item : j) {
                HistoricalEvent evt;
                evt.id = item.value("id", "");
                evt.title = item.value("title", "");
                evt.description = item.value("description", "");
                evt.category = item.value("category", "market");
                evt.weight = item.value("weight", 1.0);
                evt.historicalNote = item.value("historicalNote", "");

                if (item.contains("era")) {
                    evt.eraEarliest = item["era"].value("earliest", 1945);
                    evt.eraLatest = item["era"].value("latest", 2030);
                }

                if (item.contains("risk")) {
                    evt.risk = item["risk"].get<RiskReturnProfile>();
                }

                if (item.contains("affects")) {
                    if (item["affects"].contains("benefits")) {
                        for (const auto& b : item["affects"]["benefits"])
                            evt.benefits.push_back(b.get<std::string>());
                    }
                    if (item["affects"].contains("suffers")) {
                        for (const auto& s : item["affects"]["suffers"])
                            evt.suffers.push_back(s.get<std::string>());
                    }
                }

                allEvents_.push_back(std::move(evt));
            }

            spdlog::info("Loaded {} historical events from {}", allEvents_.size(), path);
            return true;
        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse historical events: {}", e.what());
            return false;
        }
    }

    // Get events available for a given game year
    std::vector<const HistoricalEvent*> getEventsForYear(int gameYear) const {
        std::vector<const HistoricalEvent*> available;
        for (const auto& evt : allEvents_) {
            if (gameYear >= evt.eraEarliest && gameYear <= evt.eraLatest) {
                available.push_back(&evt);
            }
        }
        return available;
    }

    // Convert a historical event to a GameEvent for the event pool
    static GameEvent toGameEvent(const HistoricalEvent& hist, math::RandomEngine& rng) {
        GameEvent ge;
        ge.id = hist.id;
        ge.title = hist.title;
        ge.description = hist.description;

        // Map category string to enum
        if (hist.category == "market") ge.category = EventCategory::Market;
        else if (hist.category == "opportunity") ge.category = EventCategory::Opportunity;
        else if (hist.category == "regulatory") ge.category = EventCategory::Regulatory;
        else if (hist.category == "crisis") ge.category = EventCategory::Crisis;
        else if (hist.category == "personnel") ge.category = EventCategory::Personnel;
        else if (hist.category == "competitive") ge.category = EventCategory::Competitive;
        else if (hist.category == "strategic") ge.category = EventCategory::Strategic;
        else if (hist.category == "operational") ge.category = EventCategory::Operational;
        else ge.category = EventCategory::Market;

        // Randomize weight slightly for variety
        ge.baseWeight = hist.weight * (0.8 + rng.uniform() * 0.4);
        ge.isHistorical = true;
        ge.historicalNote = hist.historicalNote;

        return ge;
    }

    // Map a historical-event category string to the EventCategory enum so the
    // sign-bias table can be applied. Mirrors toGameEvent's mapping.
    static EventCategory categoryEnum(const std::string& c) {
        if (c == "market") return EventCategory::Market;
        if (c == "opportunity") return EventCategory::Opportunity;
        if (c == "regulatory") return EventCategory::Regulatory;
        if (c == "crisis") return EventCategory::Crisis;
        if (c == "personnel") return EventCategory::Personnel;
        if (c == "competitive") return EventCategory::Competitive;
        if (c == "strategic") return EventCategory::Strategic;
        if (c == "operational") return EventCategory::Operational;
        return EventCategory::Market;
    }

    // Draw N historical events for this quarter (era-filtered, weighted).
    // P3.4 Stage A: the optional EventBias up/down-weights crisis-vs-boom
    // flavored events to match the engine's macro direction. Weight-only —
    // the rng_ draw sequence (one uniform per drawn event, same order) is
    // unchanged, preserving the day-by-day vs batch lockstep contract.
    std::vector<GameEvent> drawHistoricalEvents(int gameYear, int count,
                                                  math::RandomEngine& rng,
                                                  const EventBias& bias = {}) const {
        auto available = getEventsForYear(gameYear);
        if (available.empty()) return {};

        // Precompute biased weights once (no rng involved here).
        std::vector<double> biasedWeights(available.size());
        for (size_t j = 0; j < available.size(); ++j) {
            double w = available[j]->weight
                     * bias.weightFor(categoryEnum(available[j]->category));
            biasedWeights[j] = std::max(w, 0.0001);
        }

        // Weighted random selection
        std::vector<GameEvent> drawn;
        std::vector<bool> used(available.size(), false);

        for (int i = 0; i < count && i < (int)available.size(); ++i) {
            double totalWeight = 0;
            for (size_t j = 0; j < available.size(); ++j) {
                if (!used[j]) totalWeight += biasedWeights[j];
            }
            if (totalWeight <= 0) break;

            double r = rng.uniform() * totalWeight;
            double cum = 0;
            for (size_t j = 0; j < available.size(); ++j) {
                if (used[j]) continue;
                cum += biasedWeights[j];
                if (cum >= r) {
                    drawn.push_back(toGameEvent(*available[j], rng));
                    used[j] = true;
                    break;
                }
            }
        }

        return drawn;
    }

    size_t totalEvents() const { return allEvents_.size(); }

    const std::vector<HistoricalEvent>& allEvents() const { return allEvents_; }

private:
    std::vector<HistoricalEvent> allEvents_;
};

} // namespace stvg::simulation
