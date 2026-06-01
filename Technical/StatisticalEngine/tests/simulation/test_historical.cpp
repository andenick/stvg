#include <gtest/gtest.h>
#include <stvg/simulation/HistoricalEventLoader.h>
#include <stvg/math/RandomEngine.h>

using namespace stvg;
using namespace stvg::simulation;

static const std::string DATA_PATH = "data/events/historical_events.json";
static const std::string ALT_PATH = "../data/events/historical_events.json";

// Try to load from known paths
static bool tryLoad(HistoricalEventLoader& loader) {
    if (loader.loadFromFile(DATA_PATH)) return true;
    if (loader.loadFromFile(ALT_PATH)) return true;
    // Try from build directory
    if (loader.loadFromFile("../../data/events/historical_events.json")) return true;
    return false;
}

TEST(HistoricalEvents, LoaderLoadsFile) {
    HistoricalEventLoader loader;
    bool loaded = tryLoad(loader);
    if (!loaded) {
        GTEST_SKIP() << "Historical events JSON not found (not a failure)";
    }
    EXPECT_GT(loader.totalEvents(), 0);
}

TEST(HistoricalEvents, EventsHaveTitleAndDescription) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    for (const auto& evt : loader.allEvents()) {
        EXPECT_FALSE(evt.title.empty()) << "Event " << evt.id << " has empty title";
        EXPECT_FALSE(evt.description.empty()) << "Event " << evt.id << " has empty description";
    }
}

TEST(HistoricalEvents, EraFilteringWorks) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    // 2024 should have events (most events cover modern era)
    auto events2024 = loader.getEventsForYear(2024);
    EXPECT_GT(events2024.size(), 0);

    // 1930 should have no events (earliest is 1945)
    auto events1930 = loader.getEventsForYear(1930);
    EXPECT_EQ(events1930.size(), 0);
}

TEST(HistoricalEvents, EraFilterRespectsRange) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    auto events = loader.getEventsForYear(2024);
    for (const auto* evt : events) {
        EXPECT_LE(evt->eraEarliest, 2024);
        EXPECT_GE(evt->eraLatest, 2024);
    }
}

TEST(HistoricalEvents, DrawReturnsCorrectCount) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    math::RandomEngine rng(42);
    auto drawn = loader.drawHistoricalEvents(2024, 2, rng);
    EXPECT_LE(drawn.size(), 2u);
    // Should have drawn something if events exist for 2024
    if (!loader.getEventsForYear(2024).empty()) {
        EXPECT_GT(drawn.size(), 0u);
    }
}

TEST(HistoricalEvents, ToGameEventProducesValidEvent) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    math::RandomEngine rng(42);
    auto drawn = loader.drawHistoricalEvents(2024, 1, rng);
    if (drawn.empty()) GTEST_SKIP();

    const auto& ge = drawn[0];
    EXPECT_FALSE(ge.title.empty());
    EXPECT_FALSE(ge.description.empty());
    EXPECT_TRUE(ge.isHistorical);
}

TEST(HistoricalEvents, NoDuplicatesInDraw) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    math::RandomEngine rng(42);
    auto drawn = loader.drawHistoricalEvents(2024, 5, rng);

    // Check no duplicate IDs
    std::set<std::string> ids;
    for (const auto& ge : drawn) {
        EXPECT_TRUE(ids.insert(ge.id).second) << "Duplicate event: " << ge.id;
    }
}

// ═══════════════════════════════════════════════════════════════
// Coverage Tests — verify the expanded event set covers all eras
// ═══════════════════════════════════════════════════════════════

TEST(HistoricalEvents, AtLeast100Events) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();
    EXPECT_GE(loader.totalEvents(), 100)
        << "Should have at least 100 historical events for rich gameplay";
}

TEST(HistoricalEvents, AllErasHaveEvents) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    // Check each era has events
    struct EraRange { int start; int end; const char* name; };
    std::vector<EraRange> eras = {
        {1945, 1959, "Post-War"},
        {1960, 1972, "Expansion"},
        {1973, 1986, "Volatility"},
        {1987, 1999, "Big Bang"},
        {2000, 2007, "Shadow Banking"},
        {2008, 2019, "Crisis & Reform"},
        {2020, 2040, "Modern & Endgame"}
    };

    for (const auto& era : eras) {
        int count = 0;
        for (int y = era.start; y <= era.end; y++) {
            auto events = loader.getEventsForYear(y);
            count += (int)events.size();
        }
        EXPECT_GE(count, 5)
            << "Era " << era.name << " (" << era.start << "-" << era.end
            << ") should have at least 5 event-years of coverage";
    }
}

TEST(HistoricalEvents, NoGapLongerThan5Years) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    int lastYearWithEvent = 1945;
    for (int y = 1945; y <= 2040; y++) {
        auto events = loader.getEventsForYear(y);
        if (!events.empty()) {
            EXPECT_LE(y - lastYearWithEvent, 5)
                << "Gap too long: no events between " << lastYearWithEvent << " and " << y;
            lastYearWithEvent = y;
        }
    }
}

TEST(HistoricalEvents, Era2014to2019HasEvents) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    int count = 0;
    for (int y = 2014; y <= 2019; y++) {
        count += (int)loader.getEventsForYear(y).size();
    }
    EXPECT_GE(count, 3) << "2014-2019 gap should now be filled with events";
}

TEST(HistoricalEvents, Era2031to2040HasEvents) {
    HistoricalEventLoader loader;
    if (!tryLoad(loader)) GTEST_SKIP();

    int count = 0;
    for (int y = 2031; y <= 2040; y++) {
        count += (int)loader.getEventsForYear(y).size();
    }
    EXPECT_GE(count, 3) << "2031-2040 endgame should have events";
}
