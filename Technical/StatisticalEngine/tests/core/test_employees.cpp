#include <gtest/gtest.h>
#include <stvg/core/EmployeeCandidate.h>

using namespace stvg::simulation;

// ── Formula Tests ───────────────────────────────────────────────

TEST(EmployeeCandidate, ExpectedSharpeDefaultStats) {
    EmployeeCandidate e;
    // All stats default to 50
    // (50*0.3 + 50*0.3 + 50*0.2 + 50*0.1 + 50*0.1) / 100 = 50/100 = 0.5
    EXPECT_DOUBLE_EQ(e.expectedSharpe(), 0.5);
}

TEST(EmployeeCandidate, ExpectedSharpeWeightedStats) {
    EmployeeCandidate e;
    e.stats.analytical = 100;  // weight 0.3
    e.stats.intuition = 0;     // weight 0.3
    e.stats.judgment = 80;     // weight 0.2
    e.stats.adaptability = 60; // weight 0.1
    e.stats.resilience = 40;   // weight 0.1
    // (100*0.3 + 0*0.3 + 80*0.2 + 60*0.1 + 40*0.1) / 100
    // = (30 + 0 + 16 + 6 + 4) / 100 = 56/100 = 0.56
    EXPECT_DOUBLE_EQ(e.expectedSharpe(), 0.56);
}

TEST(EmployeeCandidate, DealWinRateFormula) {
    EmployeeCandidate e;
    // Default stats (all 50): NOT divided by 100
    // (50*0.3 + 50*0.3 + 50*0.2 + 50*0.1 + 50*0.1) = 50.0
    EXPECT_DOUBLE_EQ(e.dealWinRate(), 50.0);

    // Custom stats
    e.stats.persuasion = 100;  // weight 0.3
    e.stats.networking = 80;   // weight 0.3
    e.stats.judgment = 60;     // weight 0.2
    e.stats.analytical = 40;   // weight 0.1
    e.stats.leadership = 20;   // weight 0.1
    // (100*0.3 + 80*0.3 + 60*0.2 + 40*0.1 + 20*0.1) = 30+24+12+4+2 = 72.0
    EXPECT_DOUBLE_EQ(e.dealWinRate(), 72.0);
}

TEST(EmployeeCandidate, DepartureProbabilityFormula) {
    EmployeeCandidate e;
    // With ambition=80: loyalty = 100 - 80*0.5 = 60
    // departure = max(0, (100-60)*80/10000) = 40*80/10000 = 0.32
    e.stats.ambition = 80;
    EXPECT_NEAR(e.departureProbability(), 0.32, 1e-10);

    // With ambition=0: loyalty = 100, departure = max(0, 0*0/10000) = 0
    e.stats.ambition = 0;
    EXPECT_DOUBLE_EQ(e.departureProbability(), 0.0);

    // With ambition=100: loyalty = 50, departure = (50*100)/10000 = 0.5
    e.stats.ambition = 100;
    EXPECT_NEAR(e.departureProbability(), 0.5, 1e-10);
}

TEST(EmployeeCandidate, FraudRiskFormula) {
    EmployeeCandidate e;
    // With hiddenIntegrity=30, ambition=80:
    // max(0, (100-30)*80/10000) = 70*80/10000 = 0.56
    e.hiddenIntegrity = 30;
    e.stats.ambition = 80;
    EXPECT_NEAR(e.fraudRisk(), 0.56, 1e-10);

    // With hiddenIntegrity=100: (100-100)*anything/10000 = 0
    e.hiddenIntegrity = 100;
    EXPECT_DOUBLE_EQ(e.fraudRisk(), 0.0);

    // With ambition=0: 0 regardless of integrity
    e.hiddenIntegrity = 30;
    e.stats.ambition = 0;
    EXPECT_DOUBLE_EQ(e.fraudRisk(), 0.0);
}

TEST(EmployeeCandidate, SuggestedLevelByExperience) {
    EmployeeCandidate e;

    e.yearsExperience = 0;
    EXPECT_EQ(e.suggestedLevel(), CareerLevel::Analyst);

    e.yearsExperience = 1;
    EXPECT_EQ(e.suggestedLevel(), CareerLevel::Analyst);

    e.yearsExperience = 2;
    EXPECT_EQ(e.suggestedLevel(), CareerLevel::Associate);

    e.yearsExperience = 6;
    EXPECT_EQ(e.suggestedLevel(), CareerLevel::VP);

    e.yearsExperience = 12;
    EXPECT_EQ(e.suggestedLevel(), CareerLevel::Director);

    e.yearsExperience = 18;
    EXPECT_EQ(e.suggestedLevel(), CareerLevel::MD);

    e.yearsExperience = 25;
    EXPECT_EQ(e.suggestedLevel(), CareerLevel::CSuite);

    e.yearsExperience = 40;
    EXPECT_EQ(e.suggestedLevel(), CareerLevel::CSuite);
}

TEST(EmployeeCandidate, JsonSerializationIncludesComputedFields) {
    EmployeeCandidate e;
    e.id = "emp_42";
    e.name = "Test Employee";
    e.archetype = Archetype::Quant;
    e.stats.analytical = 90;

    nlohmann::json j;
    to_json(j, e);

    EXPECT_EQ(j["id"], "emp_42");
    EXPECT_EQ(j["name"], "Test Employee");
    EXPECT_TRUE(j.contains("expectedSharpe"));
    EXPECT_TRUE(j.contains("dealWinRate"));
    EXPECT_DOUBLE_EQ(j["expectedSharpe"].get<double>(), e.expectedSharpe());
    EXPECT_DOUBLE_EQ(j["dealWinRate"].get<double>(), e.dealWinRate());
}
