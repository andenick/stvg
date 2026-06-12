#include "stvg/game/AnnualReportBuilder.h"
#include <spdlog/spdlog.h>
#include <unordered_set>

namespace stvg::game {

void AnnualReportBuilder::generate(
    AnnualReport& report,
    int year,
    const Bank& bank,
    const PlayerProgression& progression,
    const RegulatoryState& regulatoryStatus,
    const std::string& eraName,
    const AnnualAccumulator& acc)
{
    report.year = year;
    report.totalRevenue = acc.yearTotalRevenue;
    report.totalCosts = acc.yearTotalCosts;
    report.netIncome = acc.yearTotalRevenue - acc.yearTotalCosts;
    report.capitalStart = acc.yearStartCapital;
    report.capitalEnd = bank.capital;
    report.capitalGrowthPct = (acc.yearStartCapital > 0)
        ? (bank.capital - acc.yearStartCapital) / acc.yearStartCapital * 100.0 : 0;
    report.capitalRatio = bank.capitalRatio();
    report.leverageRatio = bank.leverageRatio();
    report.avgQuarterScore = (acc.yearQuarterCount > 0)
        ? acc.yearTotalScore / acc.yearQuarterCount : 0;
    report.progression = progression;
    report.regulatoryStatus = regulatoryStatus;
    report.eraName = eraName;
    // 0.3: the year accumulates each quarter's headlines, so genuinely-identical
    // lines (e.g. a flat-quarter "...$0K Q? profit" before quarter-stamping, or
    // any repeated consequence/character headline) would otherwise appear 3-4×.
    // Dedup while preserving first-seen order.
    report.headlines.clear();
    {
        std::unordered_set<std::string> seen;
        for (const auto& h : acc.yearHeadlines) {
            if (seen.insert(h).second) report.headlines.push_back(h);
        }
    }
    report.quartersPlayed = acc.yearQuarterCount;

    // Financial statement fields
    report.netInterestIncome = bank.netInterestIncome * 4;
    report.feeIncome = bank.feeIncome * 4;
    report.tradingPnL = bank.tradingPnL * 4;
    report.interestIncome = report.netInterestIncome + bank.fundingCost * 4;
    report.interestExpense = bank.fundingCost * 4;
    report.loanLossProvisions = bank.quarterlyProvisions * 4;
    report.fundingCost = bank.fundingCost * 4;
    report.loans = bank.loans;
    report.securities = bank.securities;
    report.deposits = bank.totalDeposits;
    report.retailDeposits = bank.retailDeposits;
    report.wholesaleDeposits = bank.wholesaleDeposits;
    report.interbankBorrowing = bank.interbankBorrowing;
    report.unrealizedLosses = bank.unrealizedLosses;
    double avgAssets = (acc.yearStartCapital * 10 + bank.totalAssets) / 2.0;
    double avgEquity = (acc.yearStartCapital + bank.capital) / 2.0;
    report.nim = (avgAssets > 0)
        ? report.netInterestIncome / avgAssets : 0;
    report.roe = (avgEquity > 0)
        ? report.netIncome / avgEquity : 0;
    report.roa = (avgAssets > 0)
        ? report.netIncome / avgAssets : 0;
    report.efficiencyRatio = (report.totalRevenue > 0)
        ? report.totalCosts / report.totalRevenue : 0;
    report.loanToDepositRatio = (bank.totalDeposits > 0)
        ? bank.loans / bank.totalDeposits : 0;
    report.nplRatio = (bank.loans > 0)
        ? report.loanLossProvisions / bank.loans : 0;

    spdlog::info("=== ANNUAL REPORT {} | Rev: ${:.0f}M | Net: ${:.0f}M | Cap Growth: {:.1f}% | Era: {} ===",
        report.year,
        report.totalRevenue / 1e6,
        report.netIncome / 1e6,
        report.capitalGrowthPct,
        report.eraName);
}

} // namespace stvg::game
