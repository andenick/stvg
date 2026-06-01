#include <gtest/gtest.h>
#include "stvg/core/PersonalityProfile.h"
#include "stvg/simulation/CrisisEngine.h"
#include "stvg/simulation/PoliticalEngine.h"
#include "stvg/simulation/CharacterEngine.h"
#include "stvg/math/RandomEngine.h"

using namespace stvg;
using namespace stvg::simulation;

// ════════════════════════════════════════════════════════════════════
// Personality Gameplay Integration Tests — verifies that the
// PersonalityProfile axes wired in Step 2 actually produce
// observable, differentiated outcomes across game systems.
// ════════════════════════════════════════════════════════════════════

// ── PoliticalEngine: politicalSavvy boost ──────────────────────────

TEST(PersonalityGameplay, WebbOutLobbiesBornOverTrials) {
    // Webb (politicalSavvy 0.9) should win significantly more lobbying
    // attempts than Born (politicalSavvy 0.3) given identical setup.
    auto webb = PersonalityProfile::political();   // savvy 0.9
    auto born = PersonalityProfile::born();        // savvy 0.3

    int webbWins = 0, bornWins = 0;
    const int trials = 200;
    for (int seed = 0; seed < trials; ++seed) {
        // Each trial uses an independent engine + seed
        math::RandomEngine rngW(seed);
        math::RandomEngine rngB(seed);
        PoliticalEngine pW(rngW), pB(rngB);
        // Endow both with capital so capitalMod isn't the bottleneck
        pW.earnCapital(80.0);
        pB.earnCapital(80.0);
        if (pW.lobbyRegulation("test", 1e6, webb.politicalSavvy)) webbWins++;
        if (pB.lobbyRegulation("test", 1e6, born.politicalSavvy)) bornWins++;
    }
    // Webb should win at least 25% more often
    EXPECT_GT(webbWins, bornWins * 1.25)
        << "Webb wins=" << webbWins << " Born wins=" << bornWins;
}

TEST(PersonalityGameplay, PoliticalSavvyDefaultMatchesPreStep2) {
    // Without any CEO (default 0.5), the lobby probability should match
    // a personalityMod of 0.5 + 0.5*0.9 = 0.95 — close to legacy behavior.
    // This test pins the default so we don't accidentally regress.
    math::RandomEngine rng(42);
    PoliticalEngine pe(rng);
    pe.earnCapital(50.0);
    int wins = 0;
    for (int i = 0; i < 200; ++i) {
        math::RandomEngine r(i);
        PoliticalEngine fresh(r);
        fresh.earnCapital(50.0);
        if (fresh.lobbyRegulation("test", 1e6)) wins++;  // default 0.5
    }
    // With personalityMod 0.95, default success rate should be ~30-50%
    EXPECT_GT(wins, 30);
    EXPECT_LT(wins, 150);
}

// ── CrisisEngine: stressResponse modulates resolution ─────────────

TEST(PersonalityGameplay, HighStressResponseAggressiveCutsHarder) {
    // A CEO with high stressResponse (Fuld 1.0) should reduce crisis severity
    // more on an aggressive response than a low-stress CEO (Volcker 0.2).
    math::RandomEngine rng(1);
    CrisisEngine ce(rng);
    auto& crises = ce.activeCrises();

    CrisisArc fuldCrisis;
    fuldCrisis.id = "fuld_test";
    fuldCrisis.severity = 8;
    crises.push_back(fuldCrisis);
    ce.resolveWithResponse("fuld_test", "aggressive", 1.0);  // Fuld stress
    int fuldRemaining = crises.front().severity;
    crises.clear();

    CrisisArc volckerCrisis;
    volckerCrisis.id = "volcker_test";
    volckerCrisis.severity = 8;
    crises.push_back(volckerCrisis);
    ce.resolveWithResponse("volcker_test", "aggressive", 0.2);  // Volcker stress
    int volckerRemaining = crises.front().severity;

    // Fuld's aggressive cut: round(4 * (0.5+1.0)) = 6 → severity 2
    // Volcker's aggressive cut: round(4 * (0.5+0.2)) = 3 → severity 5
    EXPECT_LT(fuldRemaining, volckerRemaining);
}

TEST(PersonalityGameplay, LowStressResponseMeasuredCutsHarder) {
    // Inverse: Volcker (stress 0.2) should be MORE effective with measured response
    // than Fuld (stress 1.0) — calm under pressure.
    math::RandomEngine rng(1);
    CrisisEngine ce(rng);
    auto& crises = ce.activeCrises();

    CrisisArc volckerCrisis;
    volckerCrisis.id = "volcker_test";
    volckerCrisis.severity = 8;
    crises.push_back(volckerCrisis);
    ce.resolveWithResponse("volcker_test", "measured", 0.2);
    int volckerRemaining = crises.front().severity;
    crises.clear();

    CrisisArc fuldCrisis;
    fuldCrisis.id = "fuld_test";
    fuldCrisis.severity = 8;
    crises.push_back(fuldCrisis);
    ce.resolveWithResponse("fuld_test", "measured", 1.0);
    int fuldRemaining = crises.front().severity;

    // Volcker measured cut: round(2 * (1.5-0.2)) = 3 → severity 5
    // Fuld measured cut: round(2 * (1.5-1.0)) = 1 → severity 7
    EXPECT_LT(volckerRemaining, fuldRemaining);
}

// ── CharacterEngine: leadership + integrity drift ─────────────────

TEST(PersonalityGameplay, HighLeadershipBuildsLoyaltyOverTime) {
    math::RandomEngine rng(7);
    CharacterEngine ceHigh, ceLow;

    // Run 20 quarters
    for (int q = 0; q < 20; ++q) {
        ceHigh.updateQuarterly(false, rng, /*leadership=*/0.95, /*integrity=*/0.5);
        ceLow.updateQuarterly(false, rng, /*leadership=*/0.05, /*integrity=*/0.5);
    }
    double hiAvg = 0, loAvg = 0;
    for (const auto& c : ceHigh.coreTeam()) hiAvg += c.loyalty;
    for (const auto& c : ceLow.coreTeam()) loAvg += c.loyalty;
    hiAvg /= ceHigh.coreTeam().size();
    loAvg /= ceLow.coreTeam().size();
    EXPECT_GT(hiAvg, loAvg + 5.0)
        << "high leadership avg loyalty=" << hiAvg
        << " vs low=" << loAvg;
}

TEST(PersonalityGameplay, HighIntegrityRaisesTrustworthiness) {
    math::RandomEngine rng(11);
    CharacterEngine ceHonest, ceCorrupt;

    // 30 quarters of honest vs. corrupt leadership
    for (int q = 0; q < 30; ++q) {
        ceHonest.updateQuarterly(false, rng, 0.5, /*integrity=*/0.95);
        ceCorrupt.updateQuarterly(false, rng, 0.5, /*integrity=*/0.05);
    }
    double honest = 0, corrupt = 0;
    for (const auto& c : ceHonest.coreTeam()) honest += c.trustworthiness;
    for (const auto& c : ceCorrupt.coreTeam()) corrupt += c.trustworthiness;
    honest /= ceHonest.coreTeam().size();
    corrupt /= ceCorrupt.coreTeam().size();
    EXPECT_GT(honest, corrupt + 10.0);
}

// ── Default-personality fallback regression check ─────────────────

TEST(PersonalityGameplay, DefaultPersonalityIsNeutral) {
    // PersonalityProfile{} = all 0.5 / 0.7 defaults. Calling a wired
    // method with the default should NOT crash and should produce
    // a near-baseline outcome (sanity check, not a strict equality).
    math::RandomEngine rng(99);
    CrisisEngine ce(rng);
    auto& crises = ce.activeCrises();
    CrisisArc c;
    c.id = "default_test";
    c.severity = 8;
    crises.push_back(c);
    ce.resolveWithResponse("default_test", "aggressive", 0.5);  // default
    // Default aggressive cut: round(4 * (0.5+0.5)) = 4 → severity 4
    EXPECT_EQ(crises.front().severity, 4);
}
