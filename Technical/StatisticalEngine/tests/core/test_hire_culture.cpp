#include <gtest/gtest.h>
#include <stvg/core/PathEngine.h>
#include <stvg/core/ArchetypeRegistry.h>
#include <stvg/core/EmployeeCandidate.h>

using namespace stvg;
using namespace stvg::simulation;

// ════════════════════════════════════════════════════════════════════
// STAR_02 P5 Item 3 — PathEngine::applyHireEffect culture shift (NORTH_STAR
// "hiring gunslingers shifts culture" un-break).
//
// Hires emit FAMILY ids; the old code expected "aggressive"/"conservative"/…
// strings the enum never produced, so the shift never fired. With the registry
// cultureShift vectors keyed by family id:
//   • hiring Gunslingers measurably RAISES riskCulture
//   • hiring Lifers measurably LOWERS it
// ════════════════════════════════════════════════════════════════════

TEST(HireCulture, GunslingersRaiseRiskCulture) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();

    PathEngine engine;
    double before = engine.state().riskCulture;
    // Hire 3 gunslingers.
    engine.applyHireEffect(Archetype::Gunslinger);
    engine.applyHireEffect(Archetype::Gunslinger);
    engine.applyHireEffect(Archetype::Gunslinger);
    EXPECT_GT(engine.state().riskCulture, before)
        << "hiring gunslingers must raise riskCulture";
}

TEST(HireCulture, LifersLowerRiskCulture) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();

    PathEngine engine;
    // Start above the floor so a downward shift is observable.
    engine.applyHireEffect(Archetype::Gunslinger);
    engine.applyHireEffect(Archetype::Gunslinger);
    double before = engine.state().riskCulture;
    engine.applyHireEffect(Archetype::Lifer);
    engine.applyHireEffect(Archetype::Lifer);
    engine.applyHireEffect(Archetype::Lifer);
    EXPECT_LT(engine.state().riskCulture, before)
        << "hiring lifers must lower riskCulture";
}

// String overload still works with the real family id.
TEST(HireCulture, StringFamilyIdShiftsCulture) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();
    PathEngine engine;
    double before = engine.state().riskCulture;
    engine.applyHireEffect(std::string("gunslinger"));
    EXPECT_GT(engine.state().riskCulture, before);
}

// Legacy vocabulary still works (fallback path) even without the registry
// value, so older callers don't regress.
TEST(HireCulture, LegacyVocabularyFallback) {
    PathEngine engine;
    double before = engine.state().riskCulture;
    engine.applyHireEffect(std::string("aggressive"));
    EXPECT_GT(engine.state().riskCulture, before);
}
