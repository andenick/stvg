#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <vector>

// ════════════════════════════════════════════════════════════════════
// Scenario JSON Loading Tests
// ════════════════════════════════════════════════════════════════════

static const std::vector<std::string> SCENARIO_PATHS = {
    "data/scenarios/tutorial.json",
    "data/scenarios/crisis_mode.json",
    "data/scenarios/full_game.json",
    "data/scenarios/speedrun.json",
    "data/scenarios/ai_endgame.json"
};

static bool tryLoadScenario(const std::string& filename, nlohmann::json& out) {
    for (const auto& prefix : {"", "../", "../../"}) {
        std::ifstream f(std::string(prefix) + filename);
        if (f.is_open()) {
            out = nlohmann::json::parse(f);
            return true;
        }
    }
    return false;
}

TEST(Scenarios, AllScenariosExist) {
    int found = 0;
    for (const auto& path : SCENARIO_PATHS) {
        nlohmann::json j;
        if (tryLoadScenario(path, j)) found++;
    }
    if (found == 0) GTEST_SKIP() << "No scenario files found (not in build directory)";
    EXPECT_EQ(found, 5) << "All 5 scenario files should exist";
}

TEST(Scenarios, AllScenariosHaveRequiredFields) {
    for (const auto& path : SCENARIO_PATHS) {
        nlohmann::json j;
        if (!tryLoadScenario(path, j)) continue;

        EXPECT_TRUE(j.contains("id")) << path << " missing 'id'";
        EXPECT_TRUE(j.contains("name")) << path << " missing 'name'";
        EXPECT_TRUE(j.contains("description")) << path << " missing 'description'";
        EXPECT_TRUE(j.contains("config")) << path << " missing 'config'";
        EXPECT_TRUE(j["config"].contains("startYear")) << path << " missing 'config.startYear'";
        EXPECT_TRUE(j["config"].contains("quartersPerGame")) << path << " missing 'config.quartersPerGame'";
        EXPECT_TRUE(j.contains("difficulty")) << path << " missing 'difficulty'";
    }
}

TEST(Scenarios, TutorialStartsIn1950) {
    nlohmann::json j;
    if (!tryLoadScenario("data/scenarios/tutorial.json", j)) GTEST_SKIP();
    EXPECT_EQ(j["config"]["startYear"].get<int>(), 1950);
    EXPECT_EQ(j["config"]["quartersPerGame"].get<int>(), 40);
    EXPECT_EQ(j["difficulty"].get<std::string>(), "Easy");
}

TEST(Scenarios, FullGameIs380Quarters) {
    nlohmann::json j;
    if (!tryLoadScenario("data/scenarios/full_game.json", j)) GTEST_SKIP();
    EXPECT_EQ(j["config"]["startYear"].get<int>(), 1945);
    EXPECT_EQ(j["config"]["quartersPerGame"].get<int>(), 380);
}
