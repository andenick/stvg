#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// RPG Employee Types — extracted for use by both CharacterGenerator
// and BankState (Division.staff) without circular dependencies.
// ════════════════════════════════════════════════════════════════════

enum class Archetype {
    Patrician, Gunslinger, Quant, Dealmaker,
    Operator, Rainmaker, Prodigy, Lifer,
};
NLOHMANN_JSON_SERIALIZE_ENUM(Archetype, {
    {Archetype::Patrician, "patrician"}, {Archetype::Gunslinger, "gunslinger"},
    {Archetype::Quant, "quant"}, {Archetype::Dealmaker, "dealmaker"},
    {Archetype::Operator, "operator"}, {Archetype::Rainmaker, "rainmaker"},
    {Archetype::Prodigy, "prodigy"}, {Archetype::Lifer, "lifer"},
})

enum class CareerLevel { Analyst, Associate, VP, Director, MD, CSuite };
NLOHMANN_JSON_SERIALIZE_ENUM(CareerLevel, {
    {CareerLevel::Analyst, "analyst"}, {CareerLevel::Associate, "associate"},
    {CareerLevel::VP, "vp"}, {CareerLevel::Director, "director"},
    {CareerLevel::MD, "managing_director"}, {CareerLevel::CSuite, "c_suite"},
})

struct Trait {
    std::string name;
    std::string description;
    bool positive;
    std::string affectedStat;
    double modifier;
};

inline void to_json(nlohmann::json& j, const Trait& t) {
    j = nlohmann::json{{"name", t.name}, {"description", t.description}, {"positive", t.positive}};
}

struct EmployeeStats {
    int analytical = 50, intuition = 50, judgment = 50;
    int persuasion = 50, networking = 50, leadership = 50;
    int ambition = 50, resilience = 50, stamina = 50, adaptability = 50;

    int& stat(int idx) {
        switch (idx) {
            case 0: return analytical; case 1: return intuition;
            case 2: return judgment; case 3: return persuasion;
            case 4: return networking; case 5: return leadership;
            case 6: return ambition; case 7: return resilience;
            case 8: return stamina; case 9: return adaptability;
        }
        return analytical;
    }

    int total() const {
        return analytical + intuition + judgment + persuasion + networking
             + leadership + ambition + resilience + stamina + adaptability;
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EmployeeStats,
    analytical, intuition, judgment, persuasion, networking,
    leadership, ambition, resilience, stamina, adaptability)

struct EmployeeCandidate {
    std::string id;
    std::string name;
    Archetype archetype;
    CareerLevel level = CareerLevel::Analyst;
    EmployeeStats stats;
    int yearsExperience = 0;
    int age = 22;
    double annualSalary = 50000;
    std::string specialty;
    std::string education;
    std::string backstory;
    std::vector<Trait> traits;
    std::string incapability;

    int hiddenPotential = 50;
    int hiddenIntegrity = 70;
    bool potentialRevealed = false;
    bool integrityRevealed = false;
    int quartersEmployed = 0;

    double expectedSharpe() const {
        return (stats.analytical * 0.3 + stats.intuition * 0.3 + stats.judgment * 0.2
              + stats.adaptability * 0.1 + stats.resilience * 0.1) / 100.0;
    }

    double dealWinRate() const {
        return (stats.persuasion * 0.3 + stats.networking * 0.3 + stats.judgment * 0.2
              + stats.analytical * 0.1 + stats.leadership * 0.1);
    }

    double departureProbability() const {
        double loyalty = 100.0 - stats.ambition * 0.5;
        return std::max(0.0, (100.0 - loyalty) * stats.ambition / 10000.0);
    }

    double fraudRisk() const {
        return std::max(0.0, (100.0 - hiddenIntegrity) * stats.ambition / 10000.0);
    }

    CareerLevel suggestedLevel() const {
        if (yearsExperience >= 25) return CareerLevel::CSuite;
        if (yearsExperience >= 18) return CareerLevel::MD;
        if (yearsExperience >= 12) return CareerLevel::Director;
        if (yearsExperience >= 6) return CareerLevel::VP;
        if (yearsExperience >= 2) return CareerLevel::Associate;
        return CareerLevel::Analyst;
    }
};

inline void to_json(nlohmann::json& j, const EmployeeCandidate& e) {
    j = nlohmann::json{
        {"id", e.id}, {"name", e.name}, {"archetype", e.archetype},
        {"level", e.level}, {"stats", e.stats},
        {"yearsExperience", e.yearsExperience}, {"age", e.age},
        {"annualSalary", e.annualSalary}, {"specialty", e.specialty},
        {"education", e.education}, {"backstory", e.backstory},
        {"traits", e.traits}, {"incapability", e.incapability},
        {"expectedSharpe", e.expectedSharpe()},
        {"dealWinRate", e.dealWinRate()},
        {"quartersEmployed", e.quartersEmployed}
    };
}

} // namespace stvg::simulation
