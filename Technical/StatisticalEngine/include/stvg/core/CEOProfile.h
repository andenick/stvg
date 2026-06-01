#pragma once

#include "PersonalityProfile.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <spdlog/spdlog.h>

namespace stvg {

struct CEOProfile {
    std::string id;
    std::string name;
    std::string nickname;
    std::string backstory;
    std::string specialAbilityName;
    std::string specialAbilityDesc;
    int specialAbilityCooldown = 4;

    double startingCapital = 10e9;
    double startingReputation = 50.0;

    double riskMultiplier = 1.0;
    double revenueMultiplier = 1.0;
    double visibilityBonus = 0.0;
    double regulatoryBonus = 0.0;

    PersonalityProfile personality;

    static std::vector<CEOProfile>& loadedProfiles() {
        static std::vector<CEOProfile> profiles;
        static bool loaded = false;
        if (!loaded) {
            loaded = true;
            for (const auto& path : {
                "data/ceos/ceos.json",
                "../data/ceos/ceos.json",
                "static/../data/ceos/ceos.json"
            }) {
                std::ifstream f(path);
                if (f.is_open()) {
                    try {
                        auto j = nlohmann::json::parse(f);
                        for (const auto& entry : j) {
                            CEOProfile p;
                            p.id = entry.value("id", "");
                            p.name = entry.value("name", "");
                            p.nickname = entry.value("nickname", "");
                            p.backstory = entry.value("backstory", "");
                            p.specialAbilityName = entry.value("specialAbilityName", "");
                            p.specialAbilityDesc = entry.value("specialAbilityDesc", "");
                            p.specialAbilityCooldown = entry.value("specialAbilityCooldown", 4);
                            p.startingCapital = entry.value("startingCapital", 10e9);
                            p.startingReputation = entry.value("startingReputation", 50.0);
                            p.riskMultiplier = entry.value("riskMultiplier", 1.0);
                            p.revenueMultiplier = entry.value("revenueMultiplier", 1.0);
                            p.visibilityBonus = entry.value("visibilityBonus", 0.0);
                            p.regulatoryBonus = entry.value("regulatoryBonus", 0.0);
                            if (entry.contains("personality") && entry["personality"].is_array()) {
                                auto& pa = entry["personality"];
                                if (pa.size() >= 11) {
                                    p.personality = {
                                        pa[0], pa[1], pa[2], pa[3], pa[4],
                                        pa[5], pa[6], pa[7], pa[8], pa[9], pa[10]
                                    };
                                }
                            }
                            profiles.push_back(std::move(p));
                        }
                        spdlog::info("Loaded {} CEO profiles from {}", profiles.size(), path);
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to parse CEO profiles: {}", e.what());
                    }
                    break;
                }
            }
            if (profiles.empty()) {
                spdlog::warn("No CEO profiles loaded — using fallback defaults");
                profiles.push_back({"sterling", "Victoria Sterling", "The Gambler",
                    "Default CEO", "Double Down", "Doubles revenue", 4,
                    50e9, 45.0, 1.3, 1.2, 0.0, 0.0, PersonalityProfile::gambler()});
                profiles.push_back({"okonkwo", "Dr. Amara Okonkwo", "The Quant",
                    "Default CEO", "Full Spectrum Analysis", "Reveals hidden risk", 4,
                    75e9, 55.0, 1.0, 1.0, 15.0, 0.0, PersonalityProfile::balanced()});
            }
        }
        return profiles;
    }

    static std::vector<CEOProfile> allProfiles() {
        return loadedProfiles();
    }

    static const CEOProfile* findById(const std::string& ceoId) {
        auto& profiles = loadedProfiles();
        for (const auto& p : profiles) {
            if (p.id == ceoId) return &p;
        }
        return nullptr;
    }
};

inline void to_json(nlohmann::json& j, const CEOProfile& p) {
    j = nlohmann::json{
        {"id", p.id}, {"name", p.name}, {"nickname", p.nickname},
        {"backstory", p.backstory},
        {"specialAbilityName", p.specialAbilityName},
        {"specialAbilityDesc", p.specialAbilityDesc},
        {"specialAbilityCooldown", p.specialAbilityCooldown},
        {"startingCapital", p.startingCapital},
        {"startingReputation", p.startingReputation},
        {"riskMultiplier", p.riskMultiplier},
        {"revenueMultiplier", p.revenueMultiplier},
        {"visibilityBonus", p.visibilityBonus},
        {"regulatoryBonus", p.regulatoryBonus},
        {"personality", p.personality}
    };
}

} // namespace stvg
