#pragma once

#include "Types.h"
#include "Division.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace stvg {

struct BankConfig {
    double startingCapital = 10e9;
    int startingEmployees = 50;
    int startingBranches = 1;
    double startingMarketShare = 0.001;
    double startingReputation = 50.0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BankConfig,
    startingCapital, startingEmployees, startingBranches,
    startingMarketShare, startingReputation)

// Factory for alternative starting positions
inline BankConfig bankConfigForPosition(StartingPosition pos) {
    BankConfig cfg;
    switch (pos) {
        case StartingPosition::CommercialBank:
            break; // Use defaults
        case StartingPosition::TradingFirm:
            cfg.startingCapital = 5e9;
            cfg.startingEmployees = 30;
            cfg.startingReputation = 40.0;
            cfg.startingMarketShare = 0.0005;
            break;
        case StartingPosition::InvestmentBank:
            cfg.startingCapital = 20e9;
            cfg.startingEmployees = 80;
            cfg.startingReputation = 55.0;
            cfg.startingMarketShare = 0.002;
            break;
        case StartingPosition::CommunityBank:
            cfg.startingCapital = 500e6;
            cfg.startingEmployees = 15;
            cfg.startingBranches = 3;
            cfg.startingReputation = 70.0;
            cfg.startingMarketShare = 0.00005;
            break;
        case StartingPosition::UniversalBank:
            cfg.startingCapital = 200e9;
            cfg.startingEmployees = 200;
            cfg.startingBranches = 50;
            cfg.startingReputation = 60.0;
            cfg.startingMarketShare = 0.05;
            break;
    }
    return cfg;
}

// Initial divisions for each starting position
inline std::vector<DivisionType> divisionsForPosition(StartingPosition pos) {
    switch (pos) {
        case StartingPosition::CommercialBank:
            return {DivisionType::CommercialLending, DivisionType::MortgageLending};
        case StartingPosition::TradingFirm:
            return {DivisionType::TradingDesk, DivisionType::AssetManagement};
        case StartingPosition::InvestmentBank:
            return {DivisionType::InvestmentBanking, DivisionType::Restructuring, DivisionType::Securitization};
        case StartingPosition::CommunityBank:
            return {DivisionType::CommercialLending};
        case StartingPosition::UniversalBank:
            return {DivisionType::CommercialLending, DivisionType::MortgageLending,
                    DivisionType::TrustAndCustody, DivisionType::CreditCards,
                    DivisionType::InternationalBanking, DivisionType::TradingDesk,
                    DivisionType::AssetManagement};
    }
    return {DivisionType::CommercialLending, DivisionType::MortgageLending};
}

// Human-readable metadata for the starting position selection screen
inline nlohmann::json allStartingPositionMetadata() {
    nlohmann::json positions = nlohmann::json::array();

    struct PosInfo {
        StartingPosition pos;
        std::string name;
        std::string description;
    };

    std::vector<PosInfo> infos = {
        {StartingPosition::CommercialBank, "Commercial Bank",
         "A traditional 1945 commercial bank with lending and mortgages. The default starting point."},
        {StartingPosition::TradingFirm, "Trading Firm",
         "A Lehman-style trading house focused on markets and asset management. Higher risk, lower reputation."},
        {StartingPosition::InvestmentBank, "Investment Bank",
         "A Morgan Stanley-style firm with IB advisory, restructuring, and securitization. Prestigious but complex."},
        {StartingPosition::CommunityBank, "Community Bank",
         "A small local bank with $500M capital and high community reputation. Grow from humble beginnings."},
        {StartingPosition::UniversalBank, "Universal Bank",
         "A post-Glass-Steagall giant with $200B capital and all era-available divisions. Maximum complexity."},
    };

    for (const auto& info : infos) {
        auto cfg = bankConfigForPosition(info.pos);
        auto divTypes = divisionsForPosition(info.pos);
        nlohmann::json divNames = nlohmann::json::array();
        for (auto dt : divTypes) {
            nlohmann::json dj = dt;
            divNames.push_back(dj.get<std::string>());
        }
        positions.push_back({
            {"id", nlohmann::json(info.pos).get<std::string>()},
            {"name", info.name},
            {"description", info.description},
            {"capital", cfg.startingCapital},
            {"employees", cfg.startingEmployees},
            {"branches", cfg.startingBranches},
            {"reputation", cfg.startingReputation},
            {"marketShare", cfg.startingMarketShare},
            {"divisions", divNames},
            {"isDefault", info.pos == StartingPosition::CommercialBank}
        });
    }
    return positions;
}

} // namespace stvg
