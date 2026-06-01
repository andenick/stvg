#pragma once

#include "Types.h"
#include "BankState.h"
#include "GameConfig.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <spdlog/spdlog.h>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Era Engine — gates divisions, instruments, market parameters,
// and regulatory regimes by historical era. The backbone of the
// 1945-2040 progressive complexity system.
// ════════════════════════════════════════════════════════════════════

struct EraDefinition {
    std::string id;
    std::string name;
    std::string description;
    std::string feel;           // Emotional tone for narrative engine
    int startYear, endYear;

    // Division gating — which business lines are available in this era
    std::vector<DivisionType> availableDivisions;

    // Instrument gating — what can be traded/offered
    std::vector<std::string> availableInstruments;

    // Economy — OU mean-reversion targets for this era
    GameConfig::EconomyParams economyParams;

    // GARCH — volatility regime defaults
    GameConfig::GARCHConfig garchParams;

    // Initial economy state when entering this era
    GameConfig::InitialEconomy initialEconomy;

    // Scripted historical events that MUST fire during this era
    std::vector<std::string> scriptedEventIds;
};

inline void to_json(nlohmann::json& j, const EraDefinition& e) {
    j = nlohmann::json{
        {"id", e.id}, {"name", e.name}, {"description", e.description},
        {"feel", e.feel}, {"startYear", e.startYear}, {"endYear", e.endYear},
        {"availableInstruments", e.availableInstruments},
        {"scriptedEventIds", e.scriptedEventIds}
    };
    // Serialize available divisions as strings
    nlohmann::json divs = nlohmann::json::array();
    for (auto d : e.availableDivisions) {
        nlohmann::json dj = d;
        divs.push_back(dj);
    }
    j["availableDivisions"] = divs;
}

class EraEngine {
public:
    void init() {
        eras_.clear();
        buildEras();
        spdlog::info("EraEngine initialized with {} eras ({}-{})",
            eras_.size(), eras_.front().startYear, eras_.back().endYear);
    }

    // Load calibrated parameters from JSON file (overrides hand-tuned defaults)
    bool loadCalibrationFromFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) {
            // Try alternate paths
            for (const auto& alt : {"../" + path, "../../" + path}) {
                f.open(alt);
                if (f.is_open()) break;
            }
        }
        if (!f.is_open()) {
            spdlog::debug("Calibration file not found: {} (using defaults)", path);
            return false;
        }
        try {
            auto j = nlohmann::json::parse(f);
            int overridden = 0;
            for (auto& era : eras_) {
                if (!j.contains(era.id)) continue;
                const auto& cal = j[era.id];
                auto loadOU = [&](const std::string& key, GameConfig::OUParams& params) {
                    if (!cal.contains(key)) return;
                    const auto& p = cal[key];
                    if (p.contains("mu")) params.mu = p["mu"].get<double>();
                    if (p.contains("theta")) params.theta = p["theta"].get<double>();
                    if (p.contains("sigma")) params.sigma = p["sigma"].get<double>();
                    if (p.contains("floor")) params.floor = p["floor"].get<double>();
                    if (p.contains("ceiling")) params.ceiling = p["ceiling"].get<double>();
                    overridden++;
                };
                loadOU("fedFunds", era.economyParams.fedFunds);
                loadOU("inflation", era.economyParams.inflation);
                loadOU("gdpGrowth", era.economyParams.gdpGrowth);
                loadOU("unemployment", era.economyParams.unemployment);
                loadOU("vix", era.economyParams.vix);
                loadOU("treasury10Y", era.economyParams.treasury10Y);
                loadOU("creditSpread", era.economyParams.creditSpread);

                // Also override initialEconomy if provided
                if (cal.contains("initialEconomy")) {
                    const auto& ie = cal["initialEconomy"];
                    if (ie.contains("fedFundsRate")) era.initialEconomy.fedFundsRate = ie["fedFundsRate"];
                    if (ie.contains("gdpGrowth")) era.initialEconomy.gdpGrowth = ie["gdpGrowth"];
                    if (ie.contains("unemployment")) era.initialEconomy.unemployment = ie["unemployment"];
                    if (ie.contains("cpiInflation")) era.initialEconomy.cpiInflation = ie["cpiInflation"];
                    if (ie.contains("vix")) era.initialEconomy.vix = ie["vix"];
                }
            }
            spdlog::info("Loaded calibration from {}: {} parameters overridden", path, overridden);
            return true;
        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse calibration {}: {}", path, e.what());
            return false;
        }
    }

    const EraDefinition& currentEra() const {
        return eras_[currentEraIndex_];
    }

    int currentEraIndex() const { return currentEraIndex_; }

    const EraDefinition& eraForYear(int year) const {
        for (const auto& era : eras_) {
            if (year >= era.startYear && year <= era.endYear) return era;
        }
        return eras_.back(); // Default to last era if beyond range
    }

    bool shouldTransition(int year) const {
        return year > eras_[currentEraIndex_].endYear
            && currentEraIndex_ + 1 < (int)eras_.size();
    }

    bool transition(int year) {
        if (!shouldTransition(year)) return false;
        int prev = currentEraIndex_;
        currentEraIndex_++;
        spdlog::info("ERA TRANSITION: {} -> {} (year {})",
            eras_[prev].name, eras_[currentEraIndex_].name, year);
        return true;
    }

    bool isDivisionAvailable(DivisionType type, int year) const {
        const auto& era = eraForYear(year);
        return std::find(era.availableDivisions.begin(),
                         era.availableDivisions.end(), type)
               != era.availableDivisions.end();
    }

    bool isInstrumentAvailable(const std::string& instrument, int year) const {
        const auto& era = eraForYear(year);
        return std::find(era.availableInstruments.begin(),
                         era.availableInstruments.end(), instrument)
               != era.availableInstruments.end();
    }

    // All divisions available up to and including the given year
    std::vector<DivisionType> allAvailableDivisions(int year) const {
        std::vector<DivisionType> result;
        for (const auto& era : eras_) {
            if (era.startYear > year) break;
            for (auto d : era.availableDivisions) {
                if (std::find(result.begin(), result.end(), d) == result.end()) {
                    result.push_back(d);
                }
            }
        }
        return result;
    }

    const std::vector<EraDefinition>& allEras() const { return eras_; }
    size_t eraCount() const { return eras_.size(); }

private:
    std::vector<EraDefinition> eras_;
    int currentEraIndex_ = 0;

    void buildEras() {
        // ── Era 1: Post-War Stability (1945-1959) ──────────────
        {
            EraDefinition era;
            era.id = "post_war";
            era.name = "Post-War Stability";
            era.description = "The golden age of boring banking. Deposits in, loans out, "
                "golf at 3pm. Glass-Steagall keeps the walls high and the risks low.";
            era.feel = "Calm, satisfying growth. Like tending a garden.";
            era.startYear = 1945;
            era.endYear = 1959;
            era.availableDivisions = {
                DivisionType::CommercialLending,
                DivisionType::MortgageLending
            };
            era.availableInstruments = {"treasuries", "mortgages", "corporate_bonds"};
            era.initialEconomy = {0.025, 0.02, 0.04, 0.035, 12.0, 0.025, 0.005};
            // OU targets: low rates, high growth, calm markets
            era.economyParams.fedFunds    = {0.02,  0.05, 0.003, 0.0,  0.20};
            era.economyParams.inflation   = {0.02,  0.10, 0.002, -0.05, 0.15};
            era.economyParams.gdpGrowth   = {0.04,  0.08, 0.003, -0.10, 0.10};
            era.economyParams.unemployment= {0.035, 0.03, 0.002, 0.02, 0.25};
            era.economyParams.vix         = {12.0,  0.15, 2.0,   8.0,  80.0};
            era.economyParams.treasury10Y = {0.025, 0.05, 0.002, 0.001, 0.15};
            era.economyParams.creditSpread= {0.005, 0.10, 0.001, 0.003, 0.10};
            era.garchParams.calm     = {0.10, 0.85};
            era.garchParams.normal   = {0.15, 0.80};
            era.garchParams.cautious = {0.20, 0.75};
            era.garchParams.stressed = {0.25, 0.70};
            era.garchParams.crisis   = {0.30, 0.65};
            era.scriptedEventIds = {"gi_bill_boom", "korean_war"};
            eras_.push_back(std::move(era));
        }

        // ── Era 2: Expansion & Innovation (1960-1972) ──────────
        {
            EraDefinition era;
            era.id = "expansion";
            era.name = "Expansion & Innovation";
            era.description = "The world is opening up. Credit cards, eurodollars, and "
                "international banking create new opportunities for the ambitious.";
            era.feel = "Curiosity and opportunity. New markets opening.";
            era.startYear = 1960;
            era.endYear = 1972;
            era.availableDivisions = {
                DivisionType::CommercialLending, DivisionType::MortgageLending,
                DivisionType::TrustAndCustody, DivisionType::CreditCards,
                DivisionType::InternationalBanking
            };
            era.availableInstruments = {"treasuries", "mortgages", "corporate_bonds",
                                        "eurodollars", "consumer_credit"};
            era.initialEconomy = {0.04, 0.025, 0.035, 0.04, 14.0, 0.04, 0.008};
            era.economyParams.fedFunds    = {0.04,  0.05, 0.004, 0.0,  0.20};
            era.economyParams.inflation   = {0.025, 0.10, 0.003, -0.05, 0.15};
            era.economyParams.gdpGrowth   = {0.035, 0.08, 0.004, -0.10, 0.10};
            era.economyParams.unemployment= {0.04,  0.03, 0.002, 0.02, 0.25};
            era.economyParams.vix         = {14.0,  0.15, 2.5,   8.0,  80.0};
            era.economyParams.treasury10Y = {0.04,  0.05, 0.003, 0.001, 0.15};
            era.economyParams.creditSpread= {0.008, 0.10, 0.002, 0.003, 0.10};
            era.garchParams.calm     = {0.12, 0.83};
            era.garchParams.normal   = {0.18, 0.75};
            era.garchParams.cautious = {0.22, 0.72};
            era.garchParams.stressed = {0.27, 0.68};
            era.garchParams.crisis   = {0.32, 0.62};
            era.scriptedEventIds = {"credit_card_revolution", "eurodollar_birth",
                                    "vietnam_costs", "nixon_shock"};
            eras_.push_back(std::move(era));
        }

        // ── Era 3: Volatility & Deregulation (1973-1986) ───────
        {
            EraDefinition era;
            era.id = "volatility";
            era.name = "Volatility & Deregulation";
            era.description = "The calm is over. Oil shocks, Volcker's rate war, and the "
                "S&L crisis transform banking from predictable to perilous.";
            era.feel = "Volatility and adrenaline. The first real scares.";
            era.startYear = 1973;
            era.endYear = 1986;
            era.availableDivisions = {
                DivisionType::CommercialLending, DivisionType::MortgageLending,
                DivisionType::TrustAndCustody, DivisionType::CreditCards,
                DivisionType::InternationalBanking,
                DivisionType::TradingDesk, DivisionType::AssetManagement
            };
            era.availableInstruments = {"treasuries", "mortgages", "corporate_bonds",
                                        "eurodollars", "consumer_credit", "forex",
                                        "commodities", "interest_rate_swaps"};
            era.initialEconomy = {0.08, 0.06, 0.02, 0.06, 22.0, 0.08, 0.015};
            era.economyParams.fedFunds    = {0.08,  0.05, 0.008, 0.0,  0.20};
            era.economyParams.inflation   = {0.06,  0.08, 0.005, -0.05, 0.15};
            era.economyParams.gdpGrowth   = {0.02,  0.08, 0.005, -0.10, 0.10};
            era.economyParams.unemployment= {0.06,  0.03, 0.003, 0.02, 0.25};
            era.economyParams.vix         = {22.0,  0.12, 4.0,   8.0,  80.0};
            era.economyParams.treasury10Y = {0.08,  0.05, 0.005, 0.001, 0.15};
            era.economyParams.creditSpread= {0.015, 0.10, 0.003, 0.003, 0.10};
            era.garchParams.calm     = {0.15, 0.80};
            era.garchParams.normal   = {0.25, 0.70};
            era.garchParams.cautious = {0.28, 0.67};
            era.garchParams.stressed = {0.32, 0.63};
            era.garchParams.crisis   = {0.38, 0.57};
            era.scriptedEventIds = {"oil_embargo", "volcker_shock",
                                    "latin_debt_crisis", "snl_crisis_begins"};
            eras_.push_back(std::move(era));
        }

        // ── Era 4: Big Bang & Excess (1987-1999) ──────────────
        {
            EraDefinition era;
            era.id = "big_bang";
            era.name = "Big Bang & Excess";
            era.description = "Deregulation unleashes a torrent of innovation. Investment banking, "
                "securitization, and derivatives create fortunes — and systemic risk.";
            era.feel = "Exhilaration and excess. Making more money than ever.";
            era.startYear = 1987;
            era.endYear = 1999;
            era.availableDivisions = {
                DivisionType::CommercialLending, DivisionType::MortgageLending,
                DivisionType::TrustAndCustody, DivisionType::CreditCards,
                DivisionType::InternationalBanking,
                DivisionType::TradingDesk, DivisionType::AssetManagement,
                DivisionType::InvestmentBanking, DivisionType::PrivateEquity,
                DivisionType::Restructuring, DivisionType::Securitization
            };
            era.availableInstruments = {"treasuries", "mortgages", "corporate_bonds",
                                        "eurodollars", "consumer_credit", "forex",
                                        "commodities", "interest_rate_swaps",
                                        "mbs", "junk_bonds", "equity_derivatives"};
            era.initialEconomy = {0.05, 0.03, 0.03, 0.05, 20.0, 0.06, 0.012};
            era.economyParams.fedFunds    = {0.05,  0.05, 0.005, 0.0,  0.20};
            era.economyParams.inflation   = {0.03,  0.10, 0.003, -0.05, 0.15};
            era.economyParams.gdpGrowth   = {0.03,  0.08, 0.004, -0.10, 0.10};
            era.economyParams.unemployment= {0.05,  0.03, 0.002, 0.02, 0.25};
            era.economyParams.vix         = {20.0,  0.15, 3.5,   8.0,  80.0};
            era.economyParams.treasury10Y = {0.06,  0.05, 0.004, 0.001, 0.15};
            era.economyParams.creditSpread= {0.012, 0.10, 0.002, 0.003, 0.10};
            era.garchParams.calm     = {0.15, 0.80};
            era.garchParams.normal   = {0.25, 0.70};
            era.garchParams.cautious = {0.25, 0.70};
            era.garchParams.stressed = {0.30, 0.65};
            era.garchParams.crisis   = {0.35, 0.60};
            era.scriptedEventIds = {"black_monday", "snl_crisis_peak",
                                    "ltcm_collapse", "glass_steagall_repeal"};
            eras_.push_back(std::move(era));
        }

        // ── Era 5: Shadow Banking & Hubris (2000-2007) ─────────
        {
            EraDefinition era;
            era.id = "shadow";
            era.name = "Shadow Banking & Hubris";
            era.description = "CDOs, CDS, and structured products generate enormous profits. "
                "The system looks stable. It isn't.";
            era.feel = "Hubris and unease. Enormous profits but something feels wrong.";
            era.startYear = 2000;
            era.endYear = 2007;
            era.availableDivisions = {
                DivisionType::CommercialLending, DivisionType::MortgageLending,
                DivisionType::TrustAndCustody, DivisionType::CreditCards,
                DivisionType::InternationalBanking,
                DivisionType::TradingDesk, DivisionType::AssetManagement,
                DivisionType::InvestmentBanking, DivisionType::PrivateEquity,
                DivisionType::Restructuring, DivisionType::Securitization,
                DivisionType::DerivativesDesk, DivisionType::WealthManagement,
                DivisionType::VentureCapital
            };
            era.availableInstruments = {"treasuries", "mortgages", "corporate_bonds",
                                        "eurodollars", "consumer_credit", "forex",
                                        "commodities", "interest_rate_swaps",
                                        "mbs", "junk_bonds", "equity_derivatives",
                                        "cds", "cdo", "structured_products"};
            era.initialEconomy = {0.03, 0.025, 0.025, 0.045, 18.0, 0.04, 0.008};
            era.economyParams.fedFunds    = {0.03,  0.05, 0.005, 0.0,  0.20};
            era.economyParams.inflation   = {0.025, 0.10, 0.003, -0.05, 0.15};
            era.economyParams.gdpGrowth   = {0.025, 0.08, 0.004, -0.10, 0.10};
            era.economyParams.unemployment= {0.045, 0.03, 0.002, 0.02, 0.25};
            era.economyParams.vix         = {18.0,  0.15, 3.0,   8.0,  80.0};
            era.economyParams.treasury10Y = {0.04,  0.05, 0.003, 0.001, 0.15};
            era.economyParams.creditSpread= {0.008, 0.10, 0.002, 0.003, 0.10};
            era.garchParams.calm     = {0.15, 0.80};
            era.garchParams.normal   = {0.22, 0.73};
            era.garchParams.cautious = {0.25, 0.70};
            era.garchParams.stressed = {0.30, 0.65};
            era.garchParams.crisis   = {0.35, 0.60};
            era.scriptedEventIds = {"dotcom_crash", "enron_scandal",
                                    "subprime_machine", "cdo_explosion"};
            eras_.push_back(std::move(era));
        }

        // ── Era 6: Crisis & Reform (2008-2019) ─────────────────
        {
            EraDefinition era;
            era.id = "reform";
            era.name = "Crisis & Reform";
            era.description = "The financial crisis reshapes everything. Dodd-Frank, stress tests, "
                "and Basel III impose new constraints. Compliance costs soar.";
            era.feel = "Terror, then constraint. Rebuilding under new rules.";
            era.startYear = 2008;
            era.endYear = 2019;
            // Same divisions as Shadow — no new unlocks, but regulations reshape all
            era.availableDivisions = {
                DivisionType::CommercialLending, DivisionType::MortgageLending,
                DivisionType::TrustAndCustody, DivisionType::CreditCards,
                DivisionType::InternationalBanking,
                DivisionType::TradingDesk, DivisionType::AssetManagement,
                DivisionType::InvestmentBanking, DivisionType::PrivateEquity,
                DivisionType::Restructuring, DivisionType::Securitization,
                DivisionType::DerivativesDesk, DivisionType::WealthManagement,
                DivisionType::VentureCapital
            };
            era.availableInstruments = {"treasuries", "mortgages", "corporate_bonds",
                                        "eurodollars", "consumer_credit", "forex",
                                        "commodities", "interest_rate_swaps",
                                        "mbs", "junk_bonds", "equity_derivatives",
                                        "cds", "cdo", "structured_products"};
            era.initialEconomy = {0.005, 0.015, 0.02, 0.06, 22.0, 0.02, 0.015};
            era.economyParams.fedFunds    = {0.005, 0.05, 0.003, 0.0,  0.20};
            era.economyParams.inflation   = {0.015, 0.10, 0.002, -0.05, 0.15};
            era.economyParams.gdpGrowth   = {0.02,  0.08, 0.004, -0.10, 0.10};
            era.economyParams.unemployment= {0.06,  0.03, 0.003, 0.02, 0.25};
            era.economyParams.vix         = {22.0,  0.12, 4.0,   8.0,  80.0};
            era.economyParams.treasury10Y = {0.02,  0.05, 0.003, 0.001, 0.15};
            era.economyParams.creditSpread= {0.015, 0.10, 0.003, 0.003, 0.10};
            era.garchParams.calm     = {0.18, 0.77};
            era.garchParams.normal   = {0.25, 0.70};
            era.garchParams.cautious = {0.28, 0.67};
            era.garchParams.stressed = {0.33, 0.62};
            era.garchParams.crisis   = {0.38, 0.57};
            era.scriptedEventIds = {"gfc_crash", "tarp_bailout", "dodd_frank",
                                    "european_debt_crisis", "flash_crash"};
            eras_.push_back(std::move(era));
        }

        // ── Era 7: Modern & Endgame (2020-2040) ───────────────
        {
            EraDefinition era;
            era.id = "modern";
            era.name = "Modern & Endgame";
            era.description = "COVID, meme stocks, AI disruption, and climate risk define "
                "the final act. New threats demand new strategies.";
            era.feel = "Existential stakes. AI and climate compete for attention.";
            era.startYear = 2020;
            era.endYear = 2040;
            era.availableDivisions = {
                DivisionType::CommercialLending, DivisionType::MortgageLending,
                DivisionType::TrustAndCustody, DivisionType::CreditCards,
                DivisionType::InternationalBanking,
                DivisionType::TradingDesk, DivisionType::AssetManagement,
                DivisionType::InvestmentBanking, DivisionType::PrivateEquity,
                DivisionType::Restructuring, DivisionType::Securitization,
                DivisionType::DerivativesDesk, DivisionType::WealthManagement,
                DivisionType::VentureCapital,
                DivisionType::Fintech, DivisionType::CryptoCustody
            };
            era.availableInstruments = {"treasuries", "mortgages", "corporate_bonds",
                                        "eurodollars", "consumer_credit", "forex",
                                        "commodities", "interest_rate_swaps",
                                        "mbs", "junk_bonds", "equity_derivatives",
                                        "cds", "cdo", "structured_products",
                                        "crypto", "esg_bonds", "digital_assets"};
            era.initialEconomy = {0.04, 0.03, 0.02, 0.04, 20.0, 0.04, 0.012};
            era.economyParams.fedFunds    = {0.04,  0.05, 0.005, 0.0,  0.20};
            era.economyParams.inflation   = {0.03,  0.10, 0.004, -0.05, 0.15};
            era.economyParams.gdpGrowth   = {0.02,  0.08, 0.004, -0.10, 0.10};
            era.economyParams.unemployment= {0.04,  0.03, 0.002, 0.02, 0.25};
            era.economyParams.vix         = {20.0,  0.15, 3.5,   8.0,  80.0};
            era.economyParams.treasury10Y = {0.04,  0.05, 0.004, 0.001, 0.15};
            era.economyParams.creditSpread= {0.012, 0.10, 0.002, 0.003, 0.10};
            era.garchParams.calm     = {0.15, 0.80};
            era.garchParams.normal   = {0.25, 0.70};
            era.garchParams.cautious = {0.25, 0.70};
            era.garchParams.stressed = {0.30, 0.65};
            era.garchParams.crisis   = {0.35, 0.60};
            era.scriptedEventIds = {"covid_crash", "meme_stocks", "svb_bank_run",
                                    "ai_emergence", "climate_tipping"};
            eras_.push_back(std::move(era));
        }
    }
};

} // namespace stvg
