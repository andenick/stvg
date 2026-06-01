#pragma once

#include "../core/GameTypes.h"
#include "../core/BankState.h"
#include <string>
#include <vector>

namespace stvg::game {

namespace AnnualReportBuilder {

    struct AnnualAccumulator {
        double yearStartCapital = 0;
        double yearTotalRevenue = 0;
        double yearTotalCosts = 0;
        double yearTotalScore = 0;
        int yearQuarterCount = 0;
        std::vector<std::string> yearHeadlines;
        std::vector<std::string> yearKeyEvents;

        void accumulate(double revenue, double costs, double score,
                        const std::vector<std::string>& headlines) {
            yearTotalRevenue += revenue;
            yearTotalCosts += costs;
            yearTotalScore += score;
            yearQuarterCount++;
            for (const auto& h : headlines) yearHeadlines.push_back(h);
        }

        void reset(double currentCapital) {
            yearStartCapital = currentCapital;
            yearTotalRevenue = 0;
            yearTotalCosts = 0;
            yearTotalScore = 0;
            yearQuarterCount = 0;
            yearHeadlines.clear();
            yearKeyEvents.clear();
        }
    };

    void generate(
        AnnualReport& report,
        int year,
        const Bank& bank,
        const PlayerProgression& progression,
        const RegulatoryState& regulatoryStatus,
        const std::string& eraName,
        const AnnualAccumulator& acc);

} // namespace AnnualReportBuilder

} // namespace stvg::game
