#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "stvg/simulation/DealPortfolio.h"
#include "stvg/math/RandomEngine.h"
#include <fstream>
#include <string>
#include <vector>

// ════════════════════════════════════════════════════════════════════
// Deal Template JSON Loading Tests
// ════════════════════════════════════════════════════════════════════

static const std::vector<std::string> DEAL_PATHS = {
    "data/deals/post_war_deals.json",
    "data/deals/deregulation_deals.json",
    "data/deals/modern_deals.json"
};

static bool tryLoadDeals(const std::string& filename, nlohmann::json& out) {
    for (const auto& prefix : {"", "../", "../../"}) {
        std::ifstream f(std::string(prefix) + filename);
        if (f.is_open()) {
            out = nlohmann::json::parse(f);
            return true;
        }
    }
    return false;
}

TEST(DealTemplates, AllDealFilesExist) {
    int found = 0;
    for (const auto& path : DEAL_PATHS) {
        nlohmann::json j;
        if (tryLoadDeals(path, j)) found++;
    }
    if (found == 0) GTEST_SKIP() << "No deal files found";
    EXPECT_EQ(found, 3) << "All 3 deal template files should exist";
}

TEST(DealTemplates, PostWarDealsValid) {
    nlohmann::json deals;
    if (!tryLoadDeals("data/deals/post_war_deals.json", deals)) GTEST_SKIP();

    EXPECT_GE(deals.size(), 3u) << "Post-war deals should have at least 3 templates";
    for (const auto& d : deals) {
        EXPECT_TRUE(d.contains("title")) << "Deal missing 'title'";
        EXPECT_TRUE(d.contains("requiredDivision")) << "Deal missing 'requiredDivision'";
        EXPECT_TRUE(d.contains("eraRange")) << "Deal missing 'eraRange'";
        EXPECT_TRUE(d.contains("risk")) << "Deal missing 'risk'";
        EXPECT_TRUE(d.contains("investmentRequired")) << "Deal missing 'investmentRequired'";

        // Era range should be within post-war period
        int earliest = d["eraRange"][0].get<int>();
        int latest = d["eraRange"][1].get<int>();
        EXPECT_GE(earliest, 1945) << d["title"].get<std::string>() << " era too early";
        EXPECT_LE(latest, 1980) << d["title"].get<std::string>() << " era too late for post-war";
    }
}

TEST(DealTemplates, DeregulationDealsValid) {
    nlohmann::json deals;
    if (!tryLoadDeals("data/deals/deregulation_deals.json", deals)) GTEST_SKIP();

    EXPECT_GE(deals.size(), 4u) << "Deregulation deals should have at least 4 templates";
    for (const auto& d : deals) {
        EXPECT_TRUE(d.contains("title"));
        EXPECT_TRUE(d.contains("risk"));
        int earliest = d["eraRange"][0].get<int>();
        EXPECT_GE(earliest, 1980) << d["title"].get<std::string>() << " starts before deregulation era";
    }
}

TEST(DealTemplates, ModernDealsValid) {
    nlohmann::json deals;
    if (!tryLoadDeals("data/deals/modern_deals.json", deals)) GTEST_SKIP();

    EXPECT_GE(deals.size(), 4u) << "Modern deals should have at least 4 templates";
    for (const auto& d : deals) {
        EXPECT_TRUE(d.contains("title"));
        int earliest = d["eraRange"][0].get<int>();
        EXPECT_GE(earliest, 2010) << d["title"].get<std::string>() << " starts before modern era";
    }
}

TEST(DealTemplates, DealPortfolioLoadsTemplates) {
    stvg::math::RandomEngine rng(42);
    stvg::simulation::DealPortfolio portfolio(rng);
    portfolio.loadTemplates();
    // If templates found (depends on working directory), verify count
    if (portfolio.templateCount() > 0) {
        EXPECT_GE(portfolio.templateCount(), 10)
            << "Should load at least 10 templates across 3 files";
    }
    // Even if not found (wrong cwd), this shouldn't crash
}

TEST(DealTemplates, RiskLevelsInRange) {
    for (const auto& path : DEAL_PATHS) {
        nlohmann::json deals;
        if (!tryLoadDeals(path, deals)) continue;
        for (const auto& d : deals) {
            int risk = d["risk"]["riskLevel"].get<int>();
            EXPECT_GE(risk, 1) << d["title"].get<std::string>() << " risk too low";
            EXPECT_LE(risk, 10) << d["title"].get<std::string>() << " risk too high";
            double prob = d["risk"]["successProbability"].get<double>();
            EXPECT_GE(prob, 0.0) << d["title"].get<std::string>();
            EXPECT_LE(prob, 1.0) << d["title"].get<std::string>();
        }
    }
}
