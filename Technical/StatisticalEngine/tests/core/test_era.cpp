#include <gtest/gtest.h>
#include <stvg/core/EraEngine.h>
#include <stvg/core/QuarterlyTurnManager.h>

using namespace stvg;

// ── Era Engine Unit Tests ───────────────────────────────────────────

TEST(EraEngine, InitializesWith7Eras) {
    EraEngine engine;
    engine.init();
    EXPECT_EQ(engine.eraCount(), 7);
}

TEST(EraEngine, StartsInPostWarEra) {
    EraEngine engine;
    engine.init();
    EXPECT_EQ(engine.currentEra().id, "post_war");
    EXPECT_EQ(engine.currentEra().startYear, 1945);
}

TEST(EraEngine, EraLookupByYear) {
    EraEngine engine;
    engine.init();

    EXPECT_EQ(engine.eraForYear(1945).id, "post_war");
    EXPECT_EQ(engine.eraForYear(1955).id, "post_war");
    EXPECT_EQ(engine.eraForYear(1965).id, "expansion");
    EXPECT_EQ(engine.eraForYear(1980).id, "volatility");
    EXPECT_EQ(engine.eraForYear(1995).id, "big_bang");
    EXPECT_EQ(engine.eraForYear(2005).id, "shadow");
    EXPECT_EQ(engine.eraForYear(2015).id, "reform");
    EXPECT_EQ(engine.eraForYear(2030).id, "modern");
}

TEST(EraEngine, TransitionDetection) {
    EraEngine engine;
    engine.init();

    EXPECT_FALSE(engine.shouldTransition(1950)); // Still in post-war
    EXPECT_TRUE(engine.shouldTransition(1960));  // Past post-war endYear (1959)

    engine.transition(1960);
    EXPECT_EQ(engine.currentEra().id, "expansion");
    EXPECT_FALSE(engine.shouldTransition(1965)); // Still in expansion
    EXPECT_TRUE(engine.shouldTransition(1973));  // Past expansion endYear (1972)
}

TEST(EraEngine, DivisionGatingPostWar) {
    EraEngine engine;
    engine.init();

    // 1945: Only CommercialLending and MortgageLending available
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::CommercialLending, 1950));
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::MortgageLending, 1950));
    EXPECT_FALSE(engine.isDivisionAvailable(DivisionType::TradingDesk, 1950));
    EXPECT_FALSE(engine.isDivisionAvailable(DivisionType::InvestmentBanking, 1950));
    EXPECT_FALSE(engine.isDivisionAvailable(DivisionType::CryptoCustody, 1950));
}

TEST(EraEngine, DivisionGatingExpansion) {
    EraEngine engine;
    engine.init();

    // 1965: Trust, CreditCards, International now available
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::CreditCards, 1965));
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::InternationalBanking, 1965));
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::TrustAndCustody, 1965));
    // Still no trading
    EXPECT_FALSE(engine.isDivisionAvailable(DivisionType::TradingDesk, 1965));
}

TEST(EraEngine, DivisionGatingBigBang) {
    EraEngine engine;
    engine.init();

    // 1995: Investment banking unlocked (Glass-Steagall eroding)
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::InvestmentBanking, 1995));
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::Securitization, 1995));
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::PrivateEquity, 1995));
    // No derivatives yet
    EXPECT_FALSE(engine.isDivisionAvailable(DivisionType::DerivativesDesk, 1995));
}

TEST(EraEngine, DivisionGatingModern) {
    EraEngine engine;
    engine.init();

    // 2025: Everything available
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::Fintech, 2025));
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::CryptoCustody, 2025));
    EXPECT_TRUE(engine.isDivisionAvailable(DivisionType::DerivativesDesk, 2025));
}

TEST(EraEngine, AllAvailableDivisionsAccumulates) {
    EraEngine engine;
    engine.init();

    auto divs1950 = engine.allAvailableDivisions(1950);
    EXPECT_EQ(divs1950.size(), 2); // Commercial + Mortgage

    auto divs1970 = engine.allAvailableDivisions(1970);
    EXPECT_EQ(divs1970.size(), 5); // +Trust, CreditCards, International

    auto divs2030 = engine.allAvailableDivisions(2030);
    EXPECT_EQ(divs2030.size(), 16); // All 16 division types
}

TEST(EraEngine, InstrumentGating) {
    EraEngine engine;
    engine.init();

    EXPECT_TRUE(engine.isInstrumentAvailable("treasuries", 1950));
    EXPECT_FALSE(engine.isInstrumentAvailable("forex", 1950));
    EXPECT_TRUE(engine.isInstrumentAvailable("forex", 1980));
    EXPECT_FALSE(engine.isInstrumentAvailable("crypto", 1980));
    EXPECT_TRUE(engine.isInstrumentAvailable("crypto", 2025));
    EXPECT_FALSE(engine.isInstrumentAvailable("cds", 1995));
    EXPECT_TRUE(engine.isInstrumentAvailable("cds", 2005));
}

TEST(EraEngine, EraSerializesToJson) {
    EraEngine engine;
    engine.init();

    nlohmann::json j = engine.currentEra();
    EXPECT_EQ(j["id"], "post_war");
    EXPECT_EQ(j["startYear"], 1945);
    EXPECT_EQ(j["endYear"], 1959);
    EXPECT_TRUE(j.contains("availableDivisions"));
    EXPECT_TRUE(j.contains("feel"));
}

// ── Integration: Era in Turn Manager ────────────────────────────────

TEST(EraIntegration, GameStartsIn1945) {
    SimulationConfig cfg;
    cfg.seed = 300;
    QuarterlyTurnManager mgr(cfg);

    EXPECT_EQ(mgr.state().currentYear, 1945);
    EXPECT_EQ(mgr.currentEra().id, "post_war");
}

TEST(EraIntegration, EraIncludedInJson) {
    SimulationConfig cfg;
    cfg.seed = 301;
    QuarterlyTurnManager mgr(cfg);

    auto j = mgr.toPlayerJson();
    EXPECT_TRUE(j.contains("era"));
    EXPECT_EQ(j["era"]["id"], "post_war");
}
