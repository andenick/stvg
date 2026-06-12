#include <gtest/gtest.h>
#include "stvg/core/Types.h"
#include "stvg/core/BankState.h"
#include "stvg/core/QuarterlyTurnManager.h"

using namespace stvg;

TEST(StartingPositions, CommercialBankHasDefaultDivisions) {
    auto cfg = bankConfigForPosition(StartingPosition::CommercialBank);
    EXPECT_NEAR(cfg.startingCapital, 1e6, 1e3);
    auto divs = divisionsForPosition(StartingPosition::CommercialBank);
    EXPECT_EQ(divs.size(), 2u);
    EXPECT_EQ(divs[0], DivisionType::CommercialLending);
    EXPECT_EQ(divs[1], DivisionType::MortgageLending);
}

TEST(StartingPositions, TradingFirmHasTradingDivisions) {
    auto cfg = bankConfigForPosition(StartingPosition::TradingFirm);
    EXPECT_NEAR(cfg.startingCapital, 5e5, 1e2);
    EXPECT_EQ(cfg.startingReputation, 40.0);
    auto divs = divisionsForPosition(StartingPosition::TradingFirm);
    EXPECT_EQ(divs.size(), 2u);
    EXPECT_EQ(divs[0], DivisionType::TradingDesk);
    EXPECT_EQ(divs[1], DivisionType::AssetManagement);
}

TEST(StartingPositions, InvestmentBankHasIBDivisions) {
    auto cfg = bankConfigForPosition(StartingPosition::InvestmentBank);
    EXPECT_NEAR(cfg.startingCapital, 2e6, 1e3);
    auto divs = divisionsForPosition(StartingPosition::InvestmentBank);
    EXPECT_EQ(divs.size(), 3u);
    EXPECT_EQ(divs[0], DivisionType::InvestmentBanking);
}

TEST(StartingPositions, CommunityBankIsSmall) {
    auto cfg = bankConfigForPosition(StartingPosition::CommunityBank);
    EXPECT_NEAR(cfg.startingCapital, 5e4, 1e1);
    EXPECT_EQ(cfg.startingReputation, 70.0);
    auto divs = divisionsForPosition(StartingPosition::CommunityBank);
    EXPECT_EQ(divs.size(), 1u);
    EXPECT_EQ(divs[0], DivisionType::CommercialLending);
}

TEST(StartingPositions, UniversalBankIsLarge) {
    auto cfg = bankConfigForPosition(StartingPosition::UniversalBank);
    EXPECT_NEAR(cfg.startingCapital, 2e7, 1e3);
    auto divs = divisionsForPosition(StartingPosition::UniversalBank);
    EXPECT_GE(divs.size(), 5u);
}

TEST(StartingPositions, TradingFirmCanBeCreated) {
    SimulationConfig config;
    config.seed = 42;
    config.startYear = 1980;
    auto bankCfg = bankConfigForPosition(StartingPosition::TradingFirm);
    // Should not crash when creating a trading firm even though TradingDesk
    // is not normally available until Era 3 (1973+)
    QuarterlyTurnManager mgr(config, bankCfg, "", StartingPosition::TradingFirm);
    auto state = mgr.toPlayerJson();
    EXPECT_GT(state["bank"]["divisions"].size(), 0u)
        << "Trading firm should have divisions";
}

TEST(StartingPositions, CommunityBankCanBeCreated) {
    SimulationConfig config;
    config.seed = 42;
    config.startYear = 1945;
    auto bankCfg = bankConfigForPosition(StartingPosition::CommunityBank);
    // Should not crash when creating a community bank
    QuarterlyTurnManager mgr(config, bankCfg, "", StartingPosition::CommunityBank);
    auto state = mgr.toPlayerJson();
    EXPECT_GT(state["bank"]["capital"].get<double>(), 0)
        << "Community bank should have positive capital";
    EXPECT_EQ(state["bank"]["divisions"].size(), 1u)
        << "Community bank should have 1 division (CommercialLending)";
}

TEST(StartingPositions, EnumSerializesToJson) {
    nlohmann::json j = StartingPosition::TradingFirm;
    EXPECT_EQ(j.get<std::string>(), "trading_firm");

    nlohmann::json j2 = StartingPosition::InvestmentBank;
    EXPECT_EQ(j2.get<std::string>(), "investment_bank");
}

TEST(StartingPositions, MetadataReturnsAllFivePositions) {
    auto metadata = allStartingPositionMetadata();
    EXPECT_EQ(metadata.size(), 5u);

    EXPECT_EQ(metadata[0]["id"], "commercial_bank");
    EXPECT_EQ(metadata[0]["name"], "Commercial Bank");
    EXPECT_TRUE(metadata[0]["isDefault"].get<bool>());
    EXPECT_EQ(metadata[0]["divisions"].size(), 2u);

    EXPECT_EQ(metadata[1]["id"], "trading_firm");
    EXPECT_NEAR(metadata[1]["capital"].get<double>(), 5e5, 1e2);

    EXPECT_EQ(metadata[4]["id"], "universal_bank");
    EXPECT_GE(metadata[4]["divisions"].size(), 5u);
}

TEST(StartingPositions, MetadataContainsRequiredFields) {
    auto metadata = allStartingPositionMetadata();
    for (const auto& pos : metadata) {
        EXPECT_TRUE(pos.contains("id"));
        EXPECT_TRUE(pos.contains("name"));
        EXPECT_TRUE(pos.contains("description"));
        EXPECT_TRUE(pos.contains("capital"));
        EXPECT_TRUE(pos.contains("employees"));
        EXPECT_TRUE(pos.contains("divisions"));
        EXPECT_TRUE(pos.contains("isDefault"));
    }
}
