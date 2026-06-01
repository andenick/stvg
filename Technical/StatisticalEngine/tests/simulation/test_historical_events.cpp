#include <gtest/gtest.h>
#include <stvg/simulation/HistoricalEventLoader.h>
#include <stvg/math/RandomEngine.h>

using namespace stvg::simulation;
using namespace stvg::math;

// ── Conversion Tests (non-overlapping with test_historical.cpp) ──

TEST(HistoricalEventConversion, CategoryStringToEnumMapping) {
    RandomEngine rng(42);

    struct TestCase {
        std::string category;
        EventCategory expected;
    };
    std::vector<TestCase> cases = {
        {"market",      EventCategory::Market},
        {"opportunity", EventCategory::Opportunity},
        {"regulatory",  EventCategory::Regulatory},
        {"crisis",      EventCategory::Crisis},
        {"personnel",   EventCategory::Personnel},
        {"competitive", EventCategory::Competitive},
        {"strategic",   EventCategory::Strategic},
        {"operational", EventCategory::Operational},
        {"unknown",     EventCategory::Market},  // default
        {"gibberish",   EventCategory::Market},  // default
    };

    for (const auto& tc : cases) {
        HistoricalEvent hist;
        hist.id = "test";
        hist.title = "Test";
        hist.category = tc.category;
        hist.weight = 1.0;

        GameEvent ge = HistoricalEventLoader::toGameEvent(hist, rng);
        EXPECT_EQ(ge.category, tc.expected)
            << "Failed for category: " << tc.category;
    }
}

TEST(HistoricalEventConversion, WeightRandomizedWithin08To12) {
    HistoricalEvent hist;
    hist.id = "test";
    hist.title = "Test";
    hist.category = "market";
    hist.weight = 1.0;

    for (int seed = 0; seed < 1000; ++seed) {
        RandomEngine rng(seed);
        GameEvent ge = HistoricalEventLoader::toGameEvent(hist, rng);

        // weight * (0.8 + uniform*0.4) → [0.8, 1.2]
        EXPECT_GE(ge.baseWeight, 0.8) << "Weight too low at seed " << seed;
        EXPECT_LE(ge.baseWeight, 1.2) << "Weight too high at seed " << seed;
    }
}

TEST(HistoricalEventConversion, IsHistoricalFlagAndNoteSet) {
    RandomEngine rng(42);

    HistoricalEvent hist;
    hist.id = "korean_war";
    hist.title = "Korean War";
    hist.historicalNote = "The Korean War began in June 1950.";
    hist.category = "crisis";
    hist.weight = 1.0;

    GameEvent ge = HistoricalEventLoader::toGameEvent(hist, rng);
    EXPECT_TRUE(ge.isHistorical);
    EXPECT_EQ(ge.historicalNote, hist.historicalNote);
    EXPECT_EQ(ge.id, hist.id);
    EXPECT_EQ(ge.title, hist.title);
}

// ── Loader Edge Cases ───────────────────────────────────────────

TEST(HistoricalEventLoader, LoadFromFileReturnsFalseForMissing) {
    HistoricalEventLoader loader;
    bool result = loader.loadFromFile("nonexistent_file_that_does_not_exist.json");
    EXPECT_FALSE(result);
    EXPECT_EQ(loader.totalEvents(), 0u);
}

TEST(HistoricalEventLoader, GetEventsForYearEmptyForOutOfRange) {
    HistoricalEventLoader loader;
    // Load real events file
    loader.loadFromFile("data/historical_events.json");

    // Year 1900 is before any event (earliest is 1945)
    auto events = loader.getEventsForYear(1900);
    EXPECT_TRUE(events.empty());

    // Year 3000 is after all events
    auto events2 = loader.getEventsForYear(3000);
    EXPECT_TRUE(events2.empty());
}
