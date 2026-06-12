#pragma once

#include "EmployeeCandidate.h"
#include <nlohmann/json.hpp>
#include <array>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// ArchetypeRegistry (STAR_02 P5 / §4.2)
//
// Loads data/archetypes/archetypes.json at startup (mirrors the ceos.json
// load pattern in CEOProfile). Provides the canonical hire-archetype data:
//   • FAMILIES (8): statWeights / integrityBase / salaryMult / eraSpawnWeights
//     — migrated VERBATIM from CharacterGenerator.h so swapping the engine
//     literals for these values is behavior-preserving (a pure refactor).
//   • macroBetas + sigma + cultureShift + crowdingClass + sideBenefits — NEW,
//     consumed by P5 (archetype × macro → P&L), PathEngine::applyHireEffect,
//     and P6.
//   • SPECIALIZATIONS (33): division/era-flavored variants rolled at
//     candidate generation for display by later phases.
//
// One loaded-once static registry (like CEOProfile::loadedProfiles()). If the
// JSON cannot be found, the registry is left empty and callers MUST fall back
// to their hardcoded behavior (CharacterGenerator does exactly this), so the
// engine never depends on the file being present.
// ════════════════════════════════════════════════════════════════════

struct ArchetypeFamily {
    std::string id;
    std::string displayName;
    std::string blurb;
    std::array<double, 10> statWeights{};   // order = EmployeeStats stat order
    int integrityBase = 60;
    double salaryMult = 1.0;
    // era spawn weights, keyed by the JSON era buckets
    double spawnPre1960 = 0.0;   // "<1960"
    double spawn1960 = 0.0;      // "1960-1979"
    double spawn1980 = 0.0;      // "1980-1999"
    double spawn2000 = 0.0;      // ">=2000"
    // P5: macro sensitivity vector + idiosyncratic vol
    std::map<std::string, double> macroBetas;  // gdp/rate/credit/equity/vix
    double sigma = 0.04;
    double credibilityTendency = 0.5;
    std::string crowdingClass;
    std::map<std::string, double> sideBenefits;
    std::map<std::string, double> cultureShift; // PathEngine axes
    std::string voiceFlavor;
    std::string portraitDescriptor;
    std::string specialty;
};

struct ArchetypeSpecialization {
    std::string id;
    std::string family;          // family id this inherits from
    std::string displayName;
    std::vector<std::string> divisionAffinity; // DivisionType json names
    int eraFrom = 1945;
    int eraTo = 2040;
    double baseSalaryMult = 1.0; // multiplier ON TOP of family salaryMult
    std::array<double, 2> salaryRangeUSD{{40000.0, 130000.0}};
    std::array<double, 10> statWeightDeltas{}; // added to family weights, renormalized
    // optional overrides (inherit family when absent)
    int integrityBase = -1;          // -1 = inherit
    double credibilityTendency = -1; // -1 = inherit
    std::map<std::string, double> macroBetas; // empty = inherit
    double sigma = -1.0;             // <0 = inherit
    std::string crowdingClass;       // empty = inherit
    std::map<std::string, double> sideBenefits;
    std::string voiceFlavor;
    std::string portraitDescriptor;
};

class ArchetypeRegistry {
public:
    // The canonical macro-axis order used by the P5 dot product.
    static const std::array<std::string, 5>& macroAxes() {
        static const std::array<std::string, 5> axes{
            {"gdp", "rate", "credit", "equity", "vix"}};
        return axes;
    }

    static const ArchetypeRegistry& instance() {
        static ArchetypeRegistry reg;
        static bool loaded = false;
        if (!loaded) {
            loaded = true;
            reg.load();
        }
        return reg;
    }

    bool loaded() const { return !families_.empty(); }

    const std::vector<ArchetypeFamily>& families() const { return families_; }
    const std::vector<ArchetypeSpecialization>& specializations() const { return specializations_; }

    const ArchetypeFamily* family(const std::string& id) const {
        for (const auto& f : families_) if (f.id == id) return &f;
        return nullptr;
    }

    // Family lookup keyed by the engine Archetype enum (json-serialized name).
    const ArchetypeFamily* family(simulation::Archetype a) const {
        nlohmann::json j = a;            // enum -> "patrician" etc.
        return family(j.get<std::string>());
    }

    // Era spawn weight for a family at a given game year (mirrors the JSON
    // bucket boundaries used by CharacterGenerator::pickArchetype).
    static double spawnWeight(const ArchetypeFamily& f, int year) {
        if (year < 1960) return f.spawnPre1960;
        if (year < 1980) return f.spawn1960;
        if (year < 2000) return f.spawn1980;
        return f.spawn2000;
    }

private:
    std::vector<ArchetypeFamily> families_;
    std::vector<ArchetypeSpecialization> specializations_;

    static std::array<double, 10> readArr10(const nlohmann::json& j, const char* key) {
        std::array<double, 10> out{};
        if (j.contains(key) && j[key].is_array()) {
            const auto& a = j[key];
            for (size_t i = 0; i < 10 && i < a.size(); ++i) out[i] = a[i].get<double>();
        }
        return out;
    }

    static std::map<std::string, double> readMap(const nlohmann::json& j, const char* key) {
        std::map<std::string, double> out;
        if (j.contains(key) && j[key].is_object()) {
            for (auto it = j[key].begin(); it != j[key].end(); ++it)
                out[it.key()] = it.value().get<double>();
        }
        return out;
    }

    void load() {
        for (const auto& path : {
                "data/archetypes/archetypes.json",
                "../data/archetypes/archetypes.json",
                "static/../data/archetypes/archetypes.json"}) {
            std::ifstream f(path);
            if (!f.is_open()) continue;
            try {
                auto j = nlohmann::json::parse(f);
                if (j.contains("families") && j["families"].is_array()) {
                    for (const auto& e : j["families"]) {
                        ArchetypeFamily fam;
                        fam.id = e.value("id", "");
                        fam.displayName = e.value("displayName", "");
                        fam.blurb = e.value("blurb", "");
                        fam.statWeights = readArr10(e, "statWeights");
                        fam.integrityBase = e.value("integrityBase", 60);
                        fam.salaryMult = e.value("salaryMult", 1.0);
                        if (e.contains("eraSpawnWeights")) {
                            const auto& w = e["eraSpawnWeights"];
                            fam.spawnPre1960 = w.value("<1960", 0.0);
                            fam.spawn1960 = w.value("1960-1979", 0.0);
                            fam.spawn1980 = w.value("1980-1999", 0.0);
                            fam.spawn2000 = w.value(">=2000", 0.0);
                        }
                        fam.macroBetas = readMap(e, "macroBetas");
                        fam.sigma = e.value("sigma", 0.04);
                        fam.credibilityTendency = e.value("credibilityTendency", 0.5);
                        fam.crowdingClass = e.value("crowdingClass", "");
                        fam.sideBenefits = readMap(e, "sideBenefits");
                        fam.cultureShift = readMap(e, "cultureShift");
                        fam.voiceFlavor = e.value("voiceFlavor", "");
                        fam.portraitDescriptor = e.value("portraitDescriptor", "");
                        fam.specialty = e.value("specialty", "");
                        families_.push_back(std::move(fam));
                    }
                }
                if (j.contains("specializations") && j["specializations"].is_array()) {
                    for (const auto& e : j["specializations"]) {
                        ArchetypeSpecialization sp;
                        sp.id = e.value("id", "");
                        sp.family = e.value("family", "");
                        sp.displayName = e.value("displayName", "");
                        if (e.contains("divisionAffinity") && e["divisionAffinity"].is_array())
                            for (const auto& d : e["divisionAffinity"])
                                sp.divisionAffinity.push_back(d.get<std::string>());
                        if (e.contains("eraRange")) {
                            sp.eraFrom = e["eraRange"].value("from", 1945);
                            sp.eraTo = e["eraRange"].value("to", 2040);
                        }
                        sp.baseSalaryMult = e.value("baseSalaryMult", 1.0);
                        if (e.contains("salaryRangeUSD") && e["salaryRangeUSD"].is_array()
                            && e["salaryRangeUSD"].size() >= 2) {
                            sp.salaryRangeUSD[0] = e["salaryRangeUSD"][0].get<double>();
                            sp.salaryRangeUSD[1] = e["salaryRangeUSD"][1].get<double>();
                        }
                        sp.statWeightDeltas = readArr10(e, "statWeightDeltas");
                        sp.integrityBase = e.value("integrityBase", -1);
                        sp.credibilityTendency = e.value("credibilityTendency", -1.0);
                        sp.macroBetas = readMap(e, "macroBetas");
                        sp.sigma = e.value("sigma", -1.0);
                        sp.crowdingClass = e.value("crowdingClass", "");
                        sp.sideBenefits = readMap(e, "sideBenefits");
                        sp.voiceFlavor = e.value("voiceFlavor", "");
                        sp.portraitDescriptor = e.value("portraitDescriptor", "");
                        specializations_.push_back(std::move(sp));
                    }
                }
                spdlog::info("Loaded {} archetype families + {} specializations from {}",
                    families_.size(), specializations_.size(), path);
            } catch (const std::exception& e) {
                spdlog::error("Failed to parse archetypes.json: {}", e.what());
            }
            break;
        }
        if (families_.empty())
            spdlog::warn("ArchetypeRegistry: no archetypes.json found — callers fall back to hardcoded tables");
    }
};

} // namespace stvg
