#pragma once

// BankState.h — Bank struct and related helpers.
// Division, DivisionType, HeadHonesty → Division.h
// ActionPointBudget → ActionPoints.h
// BankConfig, bankConfigForPosition, divisionsForPosition → BankConfig.h

#include "Division.h"
#include "ActionPoints.h"
#include "BankConfig.h"
#include "Types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Bank — the central game entity (DYNAMIC DIVISIONS)
// ════════════════════════════════════════════════════════════════════

struct Bank {
    std::string id = "bank_01";
    std::string name = "First National Trust";

    double capital;
    double totalAssets;
    double totalDeposits;
    double reserves;
    double totalRevenue = 0.0;
    double totalCosts = 0.0;
    double netIncome = 0.0;
    double reputation = 65.0;
    double marketShare;
    int branches;
    int employees;

    // ── SFC balance sheet components (Phase A) ──────────────────
    double loans = 0.0;              // sum of lending division budgets (asset)
    double securities = 0.0;         // sum of trading/investment positions (asset)
    double interbankBorrowing = 0.0; // plug variable (liability) — funding gap
    double depositRate = 0.0;        // rate paid to depositors (set by player or auto)
    double fundingCost = 0.0;        // blended cost of funds this quarter
    double loanLossReserve = 0.0;    // accumulated provisions
    double netInterestIncome = 0.0;  // NII: loan revenue - funding cost
    double feeIncome = 0.0;          // advisory/management fee revenue
    double tradingPnL = 0.0;         // trading desk P&L

    // ── Sprint 1: Banking Policy (player-set) ───────────────────
    double loanSpread = 0.02;        // bps over market rate (default 200bps)
    double creditStandards = 0.5;    // 0.0=loose → 1.0=tight (default moderate)
    double quarterlyProvisions = 0.0; // loan loss provisions this quarter

    // Lagged default rate buffer: stores creditStandards from 4-8 quarters ago
    static constexpr int PD_LAG_QUARTERS = 6;
    std::vector<double> standardsHistory;

    // Sprint 2: Duration Risk
    double unrealizedLosses = 0.0;  // mark-to-market (negative = losses)

    // Sprint 3: Wholesale vs Retail funding
    double retailDeposits = 0.0;
    double wholesaleDeposits = 0.0;

    // ── Dynamic division system ─────────────────────────────────
    std::vector<Division> divisions;

    Division* getDivision(const std::string& divId) {
        for (auto& d : divisions) if (d.id == divId) return &d;
        return nullptr;
    }

    Division* getDivisionByType(DivisionType type) {
        for (auto& d : divisions) if (d.type == type) return &d;
        return nullptr;
    }

    const Division* getDivisionByType(DivisionType type) const {
        for (const auto& d : divisions) if (d.type == type) return &d;
        return nullptr;
    }

    bool hasDivision(DivisionType type) const {
        for (const auto& d : divisions) if (d.type == type) return true;
        return false;
    }

    void addDivision(Division div) {
        divisions.push_back(std::move(div));
        recalcBalanceSheet();
    }

    // ── Organizational complexity ───────────────────────────────
    double visibilityBonus = 0.0; // CEO or special ability bonus

    double visibilityPct() const {
        return std::clamp(100.0 * std::exp(-totalAssets / 2e7) + visibilityBonus, 1.0, 100.0);
    }

    double controlPct() const {
        return 100.0 * std::exp(-static_cast<double>(employees) / 5000.0);
    }

    double totalHiddenRisk() const {
        double total = 0;
        for (const auto& d : divisions) total += d.hiddenRisk;
        return total;
    }

    double totalReportedRisk() const {
        double total = 0;
        for (const auto& d : divisions) total += d.reportedRisk;
        return total;
    }

    double capitalRatio() const {
        return (totalAssets > 0) ? capital / totalAssets : 1.0;
    }

    double leverageRatio() const {
        return (capital > 0) ? totalAssets / capital : 0.0;
    }

    double averageMorale() const {
        if (divisions.empty()) return 75.0;
        double sum = 0;
        for (const auto& d : divisions) sum += d.morale;
        return sum / divisions.size();
    }

    // ── SFC helpers ───────────────────────────────────────────────
    // Target leverage for balance sheet derivation (1945 bank = ~10x)
    static constexpr double kDefaultLeverage = 10.0;

    // Sum equity budgets by asset class (these are capital allocations)
    double sumLoanEquity() const {
        double sum = 0;
        for (const auto& d : divisions)
            if (d.assetClass() == Division::AssetClass::Loans) sum += d.budget;
        return sum;
    }

    double sumSecuritiesEquity() const {
        double sum = 0;
        for (const auto& d : divisions)
            if (d.assetClass() == Division::AssetClass::Securities) sum += d.budget;
        return sum;
    }

    // Leveraged asset quantities (what appears on the balance sheet)
    double sumDivisionLoans() const { return sumLoanEquity() * kDefaultLeverage; }
    double sumDivisionSecurities() const { return sumSecuritiesEquity() * kDefaultLeverage; }

    double depositToAssetRatio() const {
        return (totalAssets > 0) ? totalDeposits / totalAssets : 0.5;
    }

    // Blended funding rate: weighted average of deposit and interbank cost
    double blendedFundingRate() const {
        double totalFunding = totalDeposits + interbankBorrowing;
        if (totalFunding <= 0) return depositRate;
        double interbankRate = depositRate + 0.02; // interbank costs 200bps more
        return (totalDeposits * depositRate + interbankBorrowing * interbankRate) / totalFunding;
    }

    // Loan yield: market rate + bank's spread
    double loanYield(double fedFundsRate, double marketCreditSpread) const {
        return fedFundsRate + marketCreditSpread + loanSpread;
    }

    // Record current credit standards into the lag buffer
    void recordCreditStandards() {
        standardsHistory.push_back(creditStandards);
        if ((int)standardsHistory.size() > PD_LAG_QUARTERS * 2)
            standardsHistory.erase(standardsHistory.begin());
    }

    // Get the credit standards from PD_LAG_QUARTERS ago (for delayed defaults)
    double laggedCreditStandards() const {
        if ((int)standardsHistory.size() >= PD_LAG_QUARTERS)
            return standardsHistory[standardsHistory.size() - PD_LAG_QUARTERS];
        return 0.5; // default moderate if not enough history
    }

    // Probability of Default based on lagged credit standards + macro conditions
    double probabilityOfDefault(double gdpGrowth, double creditSpread) const {
        double laggedStandards = laggedCreditStandards();
        // Base PD: 0.5% for tight standards, up to 3% for loose
        double basePD = 0.005 * (1.0 + 2.0 * (1.0 - laggedStandards));
        // Macro adjustment: recession increases PD, expansion decreases
        double macroMult = 1.0 - 2.0 * gdpGrowth + 5.0 * creditSpread;
        macroMult = std::clamp(macroMult, 0.5, 3.0);
        return std::clamp(basePD * macroMult, 0.001, 0.10);
    }

    // Recalculate SFC balance sheet from current divisions. Call after
    // division list changes (starting position override, new division added).
    void recalcBalanceSheet() {
        loans = sumDivisionLoans();
        securities = sumDivisionSecurities();
        double explicitAssets = loans + securities + reserves;
        double minAssets = capital * kDefaultLeverage;
        totalAssets = std::max(explicitAssets, minAssets);
        totalDeposits = std::max(0.0, totalAssets - capital - interbankBorrowing);
    }

    // ── Factory ─────────────────────────────────────────────────
    static Bank create(const BankConfig& cfg) {
        Bank b;
        b.capital = cfg.startingCapital;
        b.reserves = cfg.startingCapital * 0.1;
        b.reputation = cfg.startingReputation;
        b.marketShare = cfg.startingMarketShare;
        b.branches = cfg.startingBranches;
        b.employees = cfg.startingEmployees;

        // Default: 2 starting divisions (1945 bank)
        // Budget = equity allocation (capital-proportional, drives revenue formula)
        Division cl;
        cl.id = "div_commercial";
        cl.name = "Commercial Lending";
        cl.type = DivisionType::CommercialLending;
        cl.employees = cfg.startingEmployees * 2 / 3;
        cl.budget = cfg.startingCapital * 0.6;
        b.divisions.push_back(cl);

        Division ml;
        ml.id = "div_mortgage";
        ml.name = "Mortgage Lending";
        ml.type = DivisionType::MortgageLending;
        ml.employees = cfg.startingEmployees / 3;
        ml.budget = cfg.startingCapital * 0.4;
        b.divisions.push_back(ml);

        // Initialize SFC balance sheet
        // Leverage multiplier converts equity allocations to asset-side quantities
        b.recalcBalanceSheet();
        b.interbankBorrowing = 0.0;
        b.depositRate = 0.015;

        return b;
    }

    // ── End-of-quarter processing (SFC balance sheet identity) ──
    void endQuarter() {
        // 1. Aggregate revenue and costs from divisions
        totalRevenue = 0;
        totalCosts = 0;
        netInterestIncome = 0;
        feeIncome = 0;
        tradingPnL = 0;
        for (auto& div : divisions) {
            div.netIncome = div.revenue - div.costs;
            totalRevenue += div.revenue;
            totalCosts += div.costs;
            switch (div.assetClass()) {
                case Division::AssetClass::Loans:
                    netInterestIncome += div.revenue;
                    break;
                case Division::AssetClass::Securities:
                    tradingPnL += div.revenue;
                    break;
                case Division::AssetClass::FeeBased:
                    feeIncome += div.revenue;
                    break;
            }
        }

        // 2. Compute funding cost (for display and interbank stress pricing)
        // NIM-based lending revenue already deducts funding rate from yield,
        // so fundingCost is NOT subtracted from netIncome to avoid double-counting.
        // It IS used to price interbank stress and for the annual report.
        fundingCost = totalDeposits * depositRate / 4.0
            + interbankBorrowing * std::max(depositRate + 0.02, 0.01) / 4.0;

        // 3. Net income = revenue - operating costs (NIM already net of funding)
        netIncome = totalRevenue - totalCosts;
        capital += netIncome;

        // 4. Update asset components from division budgets
        loans = sumDivisionLoans();
        securities = sumDivisionSecurities();

        // 5. Enforce balance sheet identity: A = L + E
        //    Assets = loans + securities + reserves + other (cash/fee-assets)
        //    Liabilities = deposits + interbank
        //    Equity = capital
        //    When explicit asset components < liabilities + equity, the residual
        //    is "other assets" (cash, fee-based AUM on balance sheet, etc.)
        double explicitAssets = loans + securities + reserves;
        double minAssets = totalDeposits + capital; // identity floor
        totalAssets = std::max(explicitAssets, minAssets);
        interbankBorrowing = std::max(0.0, totalAssets - totalDeposits - capital);

        // 6. Deposits grow modestly with lending (money multiplier effect)
        //    Each quarter, new loans create ~5% of their value as deposits
        double depositGrowth = loans * 0.005;
        totalDeposits += depositGrowth;

        // 7. Reserves target: ~1% of deposits (era-appropriate; higher pre-1960)
        double reserveTarget = totalDeposits * 0.01;
        reserves += 0.2 * (reserveTarget - reserves); // slow convergence

        // 8. Re-enforce identity after deposit/reserve changes
        explicitAssets = loans + securities + reserves;
        totalAssets = std::max(explicitAssets, totalDeposits + capital);
        interbankBorrowing = std::max(0.0, totalAssets - totalDeposits - capital);

        // 9. Final identity enforcement: A = L + E exactly
        totalAssets = totalDeposits + interbankBorrowing + capital;

        // 9. Hidden risk processing (unchanged)
        for (auto& div : divisions) {
            div.accumulateHiddenRisk();
            div.reportedRisk = div.actualRisk * (1.0 - 0.3 * div.autonomyLevel);
        }
    }
};

// God view: full truth including hidden risk totals and SFC balance sheet
inline void to_json(nlohmann::json& j, const Bank& b) {
    j = nlohmann::json{
        {"id", b.id}, {"name", b.name},
        {"capital", b.capital}, {"totalAssets", b.totalAssets},
        {"totalDeposits", b.totalDeposits}, {"reserves", b.reserves},
        {"totalRevenue", b.totalRevenue}, {"totalCosts", b.totalCosts},
        {"netIncome", b.netIncome},
        {"reputation", b.reputation}, {"marketShare", b.marketShare},
        {"branches", b.branches}, {"employees", b.employees},
        {"visibilityPct", b.visibilityPct()},
        {"controlPct", b.controlPct()},
        {"capitalRatio", b.capitalRatio()},
        {"leverageRatio", b.leverageRatio()},
        {"totalHiddenRisk", b.totalHiddenRisk()},
        {"totalReportedRisk", b.totalReportedRisk()},
        {"loans", b.loans},
        {"securities", b.securities},
        {"interbankBorrowing", b.interbankBorrowing},
        {"depositRate", b.depositRate},
        {"fundingCost", b.fundingCost},
        {"loanLossReserve", b.loanLossReserve},
        {"netInterestIncome", b.netInterestIncome},
        {"feeIncome", b.feeIncome},
        {"tradingPnL", b.tradingPnL},
        {"depositToAssetRatio", b.depositToAssetRatio()},
        {"loanSpread", b.loanSpread},
        {"creditStandards", b.creditStandards},
        {"quarterlyProvisions", b.quarterlyProvisions},
        {"divisions", b.divisions}
    };
}

// Phase 3.2 save/load support: restore Bank state from JSON. Restores
// serializable fields and divisions; computed fields (capitalRatio etc.)
// derive from capital/totalAssets after restoration.
inline void loadBankFromJson(Bank& b, const nlohmann::json& j) {
    b.id            = j.value("id", b.id);
    b.name          = j.value("name", b.name);
    b.capital       = j.value("capital", b.capital);
    b.totalAssets   = j.value("totalAssets", b.totalAssets);
    b.totalDeposits = j.value("totalDeposits", b.totalDeposits);
    b.reserves      = j.value("reserves", b.reserves);
    b.totalRevenue  = j.value("totalRevenue", 0.0);
    b.totalCosts    = j.value("totalCosts", 0.0);
    b.netIncome     = j.value("netIncome", 0.0);
    b.reputation    = j.value("reputation", b.reputation);
    b.marketShare   = j.value("marketShare", b.marketShare);
    b.branches      = j.value("branches", b.branches);
    b.employees     = j.value("employees", b.employees);
    b.loans                = j.value("loans", 0.0);
    b.securities           = j.value("securities", 0.0);
    b.interbankBorrowing   = j.value("interbankBorrowing", 0.0);
    b.depositRate          = j.value("depositRate", 0.015);
    b.fundingCost          = j.value("fundingCost", 0.0);
    b.loanLossReserve      = j.value("loanLossReserve", 0.0);
    b.netInterestIncome    = j.value("netInterestIncome", 0.0);
    b.feeIncome            = j.value("feeIncome", 0.0);
    b.tradingPnL           = j.value("tradingPnL", 0.0);
    b.loanSpread           = j.value("loanSpread", 0.02);
    b.creditStandards      = j.value("creditStandards", 0.5);
    b.quarterlyProvisions  = j.value("quarterlyProvisions", 0.0);
    if (j.contains("standardsHistory") && j["standardsHistory"].is_array()) {
        b.standardsHistory.clear();
        for (const auto& v : j["standardsHistory"])
            b.standardsHistory.push_back(v.get<double>());
    }

    if (j.contains("divisions") && j["divisions"].is_array()) {
        b.divisions.clear();
        for (const auto& dj : j["divisions"]) {
            Division d;
            loadDivisionFromJson(d, dj);
            b.divisions.push_back(std::move(d));
        }
    }
}

// Player view: filtered divisions, no hidden risk totals
inline nlohmann::json bankToPlayerJson(const Bank& b) {
    double vis = b.visibilityPct();
    nlohmann::json divs = nlohmann::json::array();
    for (const auto& d : b.divisions) {
        divs.push_back(divisionToPlayerJson(d, vis));
    }
    return nlohmann::json{
        {"id", b.id}, {"name", b.name},
        {"capital", b.capital}, {"totalAssets", b.totalAssets},
        {"totalDeposits", b.totalDeposits}, {"reserves", b.reserves},
        {"totalRevenue", b.totalRevenue}, {"totalCosts", b.totalCosts},
        {"netIncome", b.netIncome},
        {"reputation", b.reputation}, {"marketShare", b.marketShare},
        {"branches", b.branches}, {"employees", b.employees},
        {"visibilityPct", vis},
        {"controlPct", b.controlPct()},
        {"capitalRatio", b.capitalRatio()},
        {"leverageRatio", b.leverageRatio()},
        {"loans", b.loans},
        {"securities", b.securities},
        {"interbankBorrowing", b.interbankBorrowing},
        {"depositRate", b.depositRate},
        {"fundingCost", b.fundingCost},
        {"netInterestIncome", b.netInterestIncome},
        {"feeIncome", b.feeIncome},
        {"tradingPnL", b.tradingPnL},
        {"depositToAssetRatio", b.depositToAssetRatio()},
        {"loanSpread", b.loanSpread},
        {"creditStandards", b.creditStandards},
        {"quarterlyProvisions", b.quarterlyProvisions},
        {"divisions", divs}
    };
}

} // namespace stvg
