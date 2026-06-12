#include <gtest/gtest.h>
#include <stvg/core/ArchetypeRegistry.h>
#include <stvg/simulation/CharacterGenerator.h>
#include <stvg/math/RandomEngine.h>
#include <array>
#include <map>

using namespace stvg;
using namespace stvg::simulation;

// ════════════════════════════════════════════════════════════════════
// STAR_02 P5 / §4.2 — ArchetypeRegistry load + pure-refactor proof.
//
// The JSON families[] were migrated VERBATIM from the CharacterGenerator
// literals. These tests pin that contract: the registry values MUST equal the
// pre-refactor hardcoded constants exactly. If a mismatch is ever found, the
// JSON is wrong and must be fixed to match the code (never the reverse).
// ════════════════════════════════════════════════════════════════════

namespace {

// The hardcoded pre-refactor literals (copied from the original
// CharacterGenerator.h, P0 state) — the ground truth the JSON must reproduce.
std::array<double, 10> hardWeights(Archetype a) {
    switch (a) {
        case Archetype::Patrician:  return {0.06, 0.08, 0.14, 0.10, 0.20, 0.12, 0.08, 0.10, 0.06, 0.06};
        case Archetype::Gunslinger: return {0.10, 0.18, 0.10, 0.08, 0.06, 0.04, 0.18, 0.12, 0.10, 0.04};
        case Archetype::Quant:      return {0.25, 0.10, 0.12, 0.04, 0.03, 0.04, 0.10, 0.08, 0.08, 0.16};
        case Archetype::Dealmaker:  return {0.08, 0.10, 0.12, 0.20, 0.16, 0.08, 0.12, 0.06, 0.04, 0.04};
        case Archetype::Operator:   return {0.10, 0.08, 0.16, 0.08, 0.08, 0.20, 0.08, 0.10, 0.08, 0.04};
        case Archetype::Rainmaker:  return {0.05, 0.10, 0.10, 0.18, 0.22, 0.10, 0.08, 0.06, 0.05, 0.06};
        case Archetype::Prodigy:    return {0.18, 0.12, 0.10, 0.08, 0.04, 0.04, 0.14, 0.06, 0.08, 0.16};
        case Archetype::Lifer:      return {0.08, 0.08, 0.12, 0.06, 0.10, 0.10, 0.04, 0.18, 0.16, 0.08};
    }
    return {};
}

int hardIntegrity(Archetype a) {
    switch (a) {
        case Archetype::Patrician:  return 75;
        case Archetype::Gunslinger: return 40;
        case Archetype::Quant:      return 65;
        case Archetype::Dealmaker:  return 55;
        case Archetype::Operator:   return 70;
        case Archetype::Rainmaker:  return 60;
        case Archetype::Prodigy:    return 60;
        case Archetype::Lifer:      return 80;
    }
    return 60;
}

double hardSalaryMult(Archetype a) {
    switch (a) {
        case Archetype::Prodigy: return 0.6;
        case Archetype::Lifer: return 0.8;
        case Archetype::Operator: return 1.2;
        case Archetype::Patrician: return 1.3;
        case Archetype::Quant: return 1.4;
        case Archetype::Gunslinger: return 1.5;
        case Archetype::Dealmaker: return 1.6;
        case Archetype::Rainmaker: return 1.7;
    }
    return 1.0;
}

std::array<double, 8> hardEraWeights(int year) {
    if (year < 1960) return {0.35, 0.00, 0.00, 0.15, 0.20, 0.10, 0.10, 0.10};
    if (year < 1980) return {0.20, 0.05, 0.00, 0.25, 0.15, 0.15, 0.10, 0.10};
    if (year < 2000) return {0.10, 0.15, 0.15, 0.20, 0.10, 0.10, 0.15, 0.05};
    return {0.05, 0.10, 0.25, 0.15, 0.10, 0.10, 0.20, 0.05};
}

const std::array<Archetype, 8> kArchetypes = {
    Archetype::Patrician, Archetype::Gunslinger, Archetype::Quant, Archetype::Dealmaker,
    Archetype::Operator, Archetype::Rainmaker, Archetype::Prodigy, Archetype::Lifer};

} // namespace

TEST(ArchetypeRegistry, LoadsOrSkips) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP() << "archetypes.json not reachable from CWD";
    EXPECT_EQ(reg.families().size(), 8u);
    EXPECT_GE(reg.specializations().size(), 30u);
}

TEST(ArchetypeRegistry, FamilyOrderMatchesEnum) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();
    // Families MUST load in enum order so index == Archetype value.
    static const char* ids[] = {"patrician", "gunslinger", "quant", "dealmaker",
                                "operator", "rainmaker", "prodigy", "lifer"};
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(reg.families()[i].id, ids[i]) << "family index " << i;
}

TEST(ArchetypeRegistry, StatWeightsVerbatim) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();
    for (auto a : kArchetypes) {
        const auto* fam = reg.family(a);
        ASSERT_NE(fam, nullptr);
        auto expected = hardWeights(a);
        for (int i = 0; i < 10; ++i)
            EXPECT_DOUBLE_EQ(fam->statWeights[i], expected[i])
                << "archetype " << (int)a << " stat " << i;
    }
}

TEST(ArchetypeRegistry, IntegrityVerbatim) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();
    for (auto a : kArchetypes)
        EXPECT_EQ(reg.family(a)->integrityBase, hardIntegrity(a)) << "archetype " << (int)a;
}

TEST(ArchetypeRegistry, SalaryMultVerbatim) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();
    for (auto a : kArchetypes)
        EXPECT_DOUBLE_EQ(reg.family(a)->salaryMult, hardSalaryMult(a)) << "archetype " << (int)a;
}

TEST(ArchetypeRegistry, EraSpawnWeightsVerbatim) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();
    for (int year : {1945, 1955, 1965, 1979, 1985, 1999, 2005, 2024}) {
        auto expected = hardEraWeights(year);
        for (int i = 0; i < 8; ++i)
            EXPECT_DOUBLE_EQ(ArchetypeRegistry::spawnWeight(reg.families()[i], year), expected[i])
                << "year " << year << " family " << i;
    }
}

// macroBetas / sigma must be present for every family (P5 needs them).
TEST(ArchetypeRegistry, MacroBetasAndSigmaPresent) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();
    for (const auto& fam : reg.families()) {
        EXPECT_GT(fam.sigma, 0.0) << fam.id;
        for (const auto& axis : ArchetypeRegistry::macroAxes())
            EXPECT_TRUE(fam.macroBetas.count(axis) > 0) << fam.id << " missing beta " << axis;
    }
}

// ── End-to-end pure-refactor determinism ────────────────────────────
// Generating candidates with a fixed seed must produce the SAME stats and
// salaries whether or not the registry is present, because the JSON values
// equal the hardcoded fallback. We prove the registry-fed path reproduces the
// known per-archetype salary multiplier exactly across a large sample.
TEST(ArchetypeRegistry, GeneratedSalaryMatchesFamilyMult) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();

    math::RandomEngine rng(12345);
    CharacterGenerator gen(rng);
    auto cands = gen.generateCandidates(400, 1990, 50.0);
    ASSERT_FALSE(cands.empty());

    for (const auto& c : cands) {
        // Reconstruct the deterministic salary formula and confirm the family
        // multiplier embedded in annualSalary matches the registry value
        // (modulo the "Expensive Tastes" 1.3x trait).
        double base = 40000 + c.yearsExperience * 8000;
        double skillPremium = (c.stats.analytical + c.stats.persuasion + c.stats.networking) / 3.0 * 500;
        double traitMult = 1.0;
        for (const auto& t : c.traits) if (t.name == "Expensive Tastes") traitMult *= 1.3;
        double famMult = reg.family(c.archetype)->salaryMult;
        double expected = (base + skillPremium) * famMult * traitMult;
        EXPECT_NEAR(c.annualSalary, expected, 1e-6)
            << "archetype " << (int)c.archetype;
    }
}

// Specialization rolls are deterministic in the candidate id and era-valid.
TEST(ArchetypeRegistry, SpecializationDeterministicAndEraValid) {
    const auto& reg = ArchetypeRegistry::instance();
    if (!reg.loaded()) GTEST_SKIP();

    math::RandomEngine rng(777);
    CharacterGenerator gen(rng);
    int gameYear = 2005;
    auto cands = gen.generateCandidates(200, gameYear, 50.0);

    int withSpec = 0;
    for (const auto& c : cands) {
        if (c.specializationId.empty()) continue;
        ++withSpec;
        // Find the spec — must exist, match the family, and be era-valid.
        const ArchetypeSpecialization* sp = nullptr;
        for (const auto& s : reg.specializations())
            if (s.id == c.specializationId) { sp = &s; break; }
        ASSERT_NE(sp, nullptr) << c.specializationId;
        nlohmann::json fam = c.archetype;
        EXPECT_EQ(sp->family, fam.get<std::string>());
        EXPECT_LE(sp->eraFrom, gameYear);
        EXPECT_GE(sp->eraTo, gameYear);
    }
    // At least some candidates should pick up a specialization at 2005.
    EXPECT_GT(withSpec, 0);
}
