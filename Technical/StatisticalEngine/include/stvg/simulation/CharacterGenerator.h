#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../core/EmployeeCandidate.h"
#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Character Generator — procedural creation of unique employees
// Types (Archetype, EmployeeStats, EmployeeCandidate, etc.) are in
// core/EmployeeCandidate.h for use by BankState without circular deps.
// ════════════════════════════════════════════════════════════════════

class CharacterGenerator {
public:
    explicit CharacterGenerator(math::RandomEngine& rng) : rng_(rng) {}

    // Generate N candidates for the current era
    std::vector<EmployeeCandidate> generateCandidates(int count, int gameYear,
                                                        double bankReputation = 50.0) {
        std::vector<EmployeeCandidate> candidates;
        for (int i = 0; i < count; ++i) {
            candidates.push_back(generateOne(gameYear, bankReputation));
        }
        return candidates;
    }

    EmployeeCandidate generateOne(int gameYear, double bankReputation = 50.0) {
        EmployeeCandidate emp;
        emp.id = "emp_" + std::to_string(++counter_);

        // 1. Pick archetype (era-weighted)
        emp.archetype = pickArchetype(gameYear);

        // 2. Generate stats from archetype + budget
        generateStats(emp);

        // 3. Generate experience and age
        generateExperienceAndAge(emp);

        // 4. Pick traits (2-4)
        generateTraits(emp);

        // 5. Maybe add incapability (10%)
        if (rng_.bernoulli(0.10)) {
            emp.incapability = pickIncapability();
        }

        // 6. Generate hidden stats
        emp.hiddenPotential = std::clamp((int)(rng_.normalSample(50, 20)), 10, 95);
        emp.hiddenIntegrity = archetypeBaseIntegrity(emp.archetype)
                            + (int)(rng_.normalSample(0, 12));
        emp.hiddenIntegrity = std::clamp(emp.hiddenIntegrity, 10, 95);

        // 7. Generate name + backstory
        emp.name = generateName(gameYear);
        emp.education = generateEducation(emp.archetype, gameYear);
        emp.specialty = archetypeSpecialty(emp.archetype);
        emp.backstory = generateBackstory(emp, gameYear);

        // 8. Calculate salary
        emp.annualSalary = calculateSalary(emp);
        emp.level = emp.suggestedLevel();

        // 9. Better candidates for higher reputation banks
        if (bankReputation > 70) {
            // Slight stat boost for prestigious banks
            for (int i = 0; i < 10; ++i) {
                emp.stats.stat(i) = std::min(95, emp.stats.stat(i) + (int)((bankReputation - 70) * 0.1));
            }
        }

        return emp;
    }

private:
    math::RandomEngine& rng_;
    int counter_ = 0;

    // ── Archetype Selection ─────────────────────────────────────

    Archetype pickArchetype(int gameYear) {
        // Era-weighted distribution
        std::array<double, 8> weights;
        if (gameYear < 1960) {
            weights = {0.35, 0.00, 0.00, 0.15, 0.20, 0.10, 0.10, 0.10};
        } else if (gameYear < 1980) {
            weights = {0.20, 0.05, 0.00, 0.25, 0.15, 0.15, 0.10, 0.10};
        } else if (gameYear < 2000) {
            weights = {0.10, 0.15, 0.15, 0.20, 0.10, 0.10, 0.15, 0.05};
        } else {
            weights = {0.05, 0.10, 0.25, 0.15, 0.10, 0.10, 0.20, 0.05};
        }

        double roll = rng_.uniform();
        double cum = 0;
        for (int i = 0; i < 8; ++i) {
            cum += weights[i];
            if (roll < cum) return static_cast<Archetype>(i);
        }
        return Archetype::Lifer;
    }

    // ── Stat Generation ─────────────────────────────────────────

    void generateStats(EmployeeCandidate& emp) {
        // Budget: 300-600 total points (wider range = more variety)
        int budget = 420 + (int)(rng_.normalSample(0, 60));
        budget = std::clamp(budget, 300, 600);

        // Archetype weights (which stats get more of the budget)
        auto weights = archetypeStatWeights(emp.archetype);

        // Base allocation from archetype + wider noise (±15, not ±8)
        for (int i = 0; i < 10; ++i) {
            double alloc = budget * weights[i];
            emp.stats.stat(i) = (int)(alloc + rng_.normalSample(0, 15));
            emp.stats.stat(i) = std::clamp(emp.stats.stat(i), 5, 95);
        }

        // "Wild stat" mechanic: 1-2 stats get a big random swing
        // This creates uniqueness WITHIN archetypes
        // (a Patrician who happens to be surprisingly analytical, or one with terrible stamina)
        int numWild = 1 + (rng_.bernoulli(0.4) ? 1 : 0);
        for (int w = 0; w < numWild; ++w) {
            int idx = (int)(rng_.uniform() * 10) % 10;
            int swing = (int)(rng_.normalSample(0, 20)); // ±20 point swing
            emp.stats.stat(idx) = std::clamp(emp.stats.stat(idx) + swing, 5, 95);
        }
    }

    std::array<double, 10> archetypeStatWeights(Archetype arch) {
        // Order: analytical, intuition, judgment, persuasion, networking,
        //        leadership, ambition, resilience, stamina, adaptability
        switch (arch) {
            case Archetype::Patrician:  return {0.06, 0.08, 0.14, 0.10, 0.20, 0.12, 0.08, 0.10, 0.06, 0.06};
            case Archetype::Gunslinger: return {0.10, 0.18, 0.10, 0.08, 0.06, 0.04, 0.18, 0.12, 0.10, 0.04};
            case Archetype::Quant:      return {0.25, 0.10, 0.12, 0.04, 0.03, 0.04, 0.10, 0.08, 0.08, 0.16};
            case Archetype::Dealmaker:  return {0.08, 0.10, 0.12, 0.20, 0.16, 0.08, 0.12, 0.06, 0.04, 0.04};
            case Archetype::Operator:   return {0.10, 0.08, 0.16, 0.08, 0.08, 0.20, 0.08, 0.10, 0.08, 0.04};
            case Archetype::Rainmaker:  return {0.05, 0.10, 0.10, 0.18, 0.22, 0.10, 0.08, 0.06, 0.05, 0.06};
            case Archetype::Prodigy:    return {0.18, 0.12, 0.10, 0.08, 0.04, 0.04, 0.14, 0.06, 0.08, 0.16};
            case Archetype::Lifer:      return {0.08, 0.08, 0.12, 0.06, 0.10, 0.10, 0.04, 0.18, 0.16, 0.08};
        }
        return {0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10};
    }

    int archetypeBaseIntegrity(Archetype arch) {
        switch (arch) {
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

    std::string archetypeSpecialty(Archetype arch) {
        switch (arch) {
            case Archetype::Patrician:  return "Commercial Lending, Trust";
            case Archetype::Gunslinger: return "Trading, Derivatives";
            case Archetype::Quant:      return "Trading, Securitization";
            case Archetype::Dealmaker:  return "Investment Banking, M&A";
            case Archetype::Operator:   return "Operations, Management";
            case Archetype::Rainmaker:  return "Wealth Management, Client Relations";
            case Archetype::Prodigy:    return "Any (adaptable)";
            case Archetype::Lifer:      return "Any (reliable)";
        }
        return "General";
    }

    // ── Experience & Age ────────────────────────────────────────

    void generateExperienceAndAge(EmployeeCandidate& emp) {
        switch (emp.archetype) {
            case Archetype::Prodigy:
                emp.yearsExperience = (int)(rng_.uniform() * 3);  // 0-2 years
                emp.age = 22 + emp.yearsExperience;
                break;
            case Archetype::Rainmaker:
            case Archetype::Lifer:
                emp.yearsExperience = 10 + (int)(rng_.uniform() * 20); // 10-30
                emp.age = 32 + emp.yearsExperience;
                break;
            default:
                emp.yearsExperience = 2 + (int)(rng_.uniform() * 15); // 2-17
                emp.age = 24 + emp.yearsExperience;
                break;
        }
    }

    // ── Traits ──────────────────────────────────────────────────

    void generateTraits(EmployeeCandidate& emp) {
        // 1-2 positive + 1-2 negative
        int numPositive = 1 + (rng_.bernoulli(0.5) ? 1 : 0);
        int numNegative = 1 + (rng_.bernoulli(0.4) ? 1 : 0);

        static const std::vector<Trait> positivePool = {
            // Cognitive
            {"Meticulous", "Exceptional attention to detail", true, "analytical", 0.15},
            {"Visionary", "Sees opportunities others miss", true, "intuition", 0.20},
            {"Photographic Memory", "Recalls every number perfectly", true, "analytical", 0.10},
            {"Pattern Spotter", "Finds hidden correlations in data", true, "intuition", 0.15},
            {"Decisive", "Makes quick, confident judgments", true, "judgment", 0.15},
            {"Strategic Thinker", "Plans three moves ahead", true, "judgment", 0.20},
            // Social
            {"Silver Tongue", "Extraordinarily persuasive", true, "persuasion", 0.20},
            {"Old Money", "Family connections open doors", true, "networking", 0.25},
            {"Team Builder", "Inspires and elevates teams", true, "leadership", 0.15},
            {"Connected", "Brings clients when hired", true, "networking", 0.15},
            {"Charming", "Everyone likes them instantly", true, "persuasion", 0.10},
            {"Mentor", "Develops junior talent brilliantly", true, "leadership", 0.20},
            {"Diplomat", "Resolves conflicts effortlessly", true, "leadership", 0.10},
            {"Club Member", "Country club and board connections", true, "networking", 0.20},
            // Drive
            {"Iron Constitution", "Tireless worker, never sick", true, "stamina", 0.20},
            {"Relentless", "Never gives up on a deal", true, "ambition", 0.15},
            {"Cool Under Fire", "Thrives in crisis", true, "resilience", 0.25},
            {"Workaholic", "Lives at the office, 100-hour weeks", true, "stamina", 0.25},
            {"Comeback Kid", "Bounces back from any setback", true, "resilience", 0.20},
            {"Hungry", "Burning desire to prove themselves", true, "ambition", 0.20},
            // Wildcard
            {"Quick Study", "Learns new systems in days", true, "adaptability", 0.20},
            {"Polymath", "Expertise across multiple domains", true, "adaptability", 0.15},
            {"Polyglot", "Speaks 4+ languages fluently", true, "networking", 0.10},
            // Special
            {"Lucky", "Uncanny knack for being in the right place", true, "special", 0.05},
            {"War Hero", "Military service builds respect", true, "resilience", 0.15},
            {"Former Athlete", "Competitive spirit and discipline", true, "stamina", 0.10},
            {"Published Author", "Intellectual credibility", true, "analytical", 0.05},
            {"Philanthropist", "Community reputation", true, "networking", 0.10},
        };

        static const std::vector<Trait> negativePool = {
            // Social
            {"Arrogant", "Dismissive of subordinates", false, "leadership", -0.10},
            {"Abrasive", "Alienates clients regularly", false, "persuasion", -0.15},
            {"Trophy Hunter", "Steals credit for team wins", false, "morale", -0.10},
            {"Lone Wolf", "Refuses to collaborate", false, "leadership", -0.15},
            {"Gossip", "Spreads rumors, erodes trust", false, "morale", -0.10},
            {"Sycophant", "Only agrees with superiors", false, "judgment", -0.10},
            // Drive
            {"Reckless", "Takes unnecessary risks", false, "risk", 0.20},
            {"Burned Out", "Running on fumes", false, "stamina", -0.25},
            {"Volatile", "Unpredictable mood swings", false, "morale", -0.15},
            {"Distracted", "Side projects and outside interests", false, "stamina", -0.15},
            {"Coasting", "Does minimum required work", false, "ambition", -0.20},
            {"Glory Hound", "Only chases high-profile deals", false, "judgment", -0.10},
            // Integrity
            {"Corner Cutter", "Takes compliance shortcuts", false, "integrity", -0.20},
            {"Creative Accountant", "Questionable bookkeeping", false, "integrity", -0.25},
            {"Loyalty to a Fault", "Won't report problems upward", false, "integrity", -0.10},
            {"Gambler", "Personal gambling habit, judgment clouded", false, "judgment", -0.15},
            // Adaptability
            {"Rigid", "Resists change fiercely", false, "adaptability", -0.30},
            {"Technophobe", "Can't adapt to new systems", false, "adaptability", -0.25},
            {"Old Guard", "Believes past methods are always best", false, "adaptability", -0.20},
            // Cost/Other
            {"Expensive Tastes", "Demands premium compensation", false, "salary", 0.30},
            {"Night Owl", "Misses morning meetings", false, "stamina", -0.10},
            {"Chronic Complainer", "Drags down team morale", false, "morale", -0.20},
            {"Prima Donna", "Requires special treatment", false, "salary", 0.20},
            {"Grudge Holder", "Never forgives a slight", false, "leadership", -0.10},
            {"Anxious", "Freezes under pressure", false, "resilience", -0.20},
        };

        std::vector<int> usedP, usedN;
        for (int i = 0; i < numPositive; ++i) {
            int idx;
            do { idx = (int)(rng_.uniform() * positivePool.size()); }
            while (std::find(usedP.begin(), usedP.end(), idx) != usedP.end());
            usedP.push_back(idx);
            emp.traits.push_back(positivePool[idx]);
        }
        for (int i = 0; i < numNegative; ++i) {
            int idx;
            do { idx = (int)(rng_.uniform() * negativePool.size()); }
            while (std::find(usedN.begin(), usedN.end(), idx) != usedN.end());
            usedN.push_back(idx);
            emp.traits.push_back(negativePool[idx]);
        }
    }

    std::string pickIncapability() {
        static const std::vector<std::string> incaps = {
            "Cannot do client-facing work",
            "Cannot do detail work",
            "Cannot manage teams",
            "Cannot work under pressure",
            "Cannot follow rules",
        };
        return incaps[(int)(rng_.uniform() * incaps.size()) % incaps.size()];
    }

    // ── Name Generation (Era-Appropriate) ───────────────────────

    std::string generateName(int gameYear) {
        bool female = false;
        if (gameYear < 1960) female = rng_.bernoulli(0.05);
        else if (gameYear < 1980) female = rng_.bernoulli(0.15);
        else if (gameYear < 2000) female = rng_.bernoulli(0.30);
        else female = rng_.bernoulli(0.45);

        std::string first = female ? pickFemaleName(gameYear) : pickMaleName(gameYear);
        std::string last = pickSurname(gameYear);
        return first + " " + last;
    }

    std::string pickMaleName(int year) {
        static const std::vector<std::string> names1945 = {
            "William", "Robert", "James", "Charles", "Edward", "Thomas", "Richard",
            "George", "Henry", "John", "Frederick", "Arthur", "Theodore", "Albert"};
        static const std::vector<std::string> names1970 = {
            "Michael", "David", "Steven", "Daniel", "Mark", "Jeffrey", "Kevin",
            "Gregory", "Brian", "Scott", "Timothy", "Patrick", "Dennis"};
        static const std::vector<std::string> names1990 = {
            "Matthew", "Christopher", "Andrew", "Joshua", "Ryan", "Justin",
            "Brandon", "Tyler", "Nicholas", "Jonathan", "Nathan"};
        static const std::vector<std::string> names2010 = {
            "Ethan", "Aiden", "Liam", "Noah", "Mason", "Alexander",
            "Raj", "Wei", "Carlos", "Omar", "Kai", "Dmitri"};

        const auto& pool = (year < 1960) ? names1945 : (year < 1985) ? names1970
                         : (year < 2005) ? names1990 : names2010;
        return pool[(int)(rng_.uniform() * pool.size()) % pool.size()];
    }

    std::string pickFemaleName(int year) {
        static const std::vector<std::string> names1945 = {
            "Margaret", "Elizabeth", "Dorothy", "Helen", "Virginia", "Ruth", "Eleanor"};
        static const std::vector<std::string> names1970 = {
            "Susan", "Patricia", "Linda", "Nancy", "Karen", "Sandra", "Carol", "Barbara"};
        static const std::vector<std::string> names1990 = {
            "Jennifer", "Jessica", "Amanda", "Stephanie", "Nicole", "Megan", "Lauren"};
        static const std::vector<std::string> names2010 = {
            "Emma", "Sophia", "Priya", "Yuki", "Aisha", "Isabella", "Olivia", "Fatima"};

        const auto& pool = (year < 1960) ? names1945 : (year < 1985) ? names1970
                         : (year < 2005) ? names1990 : names2010;
        return pool[(int)(rng_.uniform() * pool.size()) % pool.size()];
    }

    std::string pickSurname(int year) {
        static const std::vector<std::string> sn1945 = {
            "Hartford", "Aldrich", "Prescott", "Whitfield", "Morgan", "Whitney",
            "Brooks", "Cabot", "Lowell", "Winthrop", "Harriman", "Astor"};
        static const std::vector<std::string> sn1970 = {
            "Johnson", "Williams", "Anderson", "Thompson", "Martinez", "Garcia",
            "O'Brien", "Sullivan", "Murphy", "Rosenberg", "Goldstein", "Romano"};
        static const std::vector<std::string> sn1990 = {
            "Kim", "Chen", "Patel", "Singh", "Nakamura", "Rodriguez", "Petrov",
            "Okonkwo", "Walsh", "Schneider", "Yamamoto", "Kowalski"};
        static const std::vector<std::string> sn2010 = {
            "Zhang", "Chakraborty", "Al-Rashid", "Okafor", "Fernandez",
            "Bergstrom", "Volkov", "Tanaka", "Gupta", "Hassan", "Park"};

        const auto& pool = (year < 1960) ? sn1945 : (year < 1985) ? sn1970
                         : (year < 2005) ? sn1990 : sn2010;
        return pool[(int)(rng_.uniform() * pool.size()) % pool.size()];
    }

    // ── Education ───────────────────────────────────────────────

    std::string generateEducation(Archetype arch, int year) {
        int gradYear = year - 4 - (int)(rng_.uniform() * 10);
        std::string school;

        if (arch == Archetype::Quant) {
            static const std::vector<std::string> quantSchools = {
                "MIT", "Caltech", "Princeton", "Stanford", "Chicago"};
            school = quantSchools[(int)(rng_.uniform() * quantSchools.size()) % quantSchools.size()];
            return "PhD in Applied Mathematics from " + school;
        }

        if (year < 1970) {
            static const std::vector<std::string> schools = {
                "Yale", "Harvard", "Princeton", "Dartmouth", "Williams", "Amherst"};
            school = schools[(int)(rng_.uniform() * schools.size()) % schools.size()];
            return school + " '" + std::to_string(gradYear % 100);
        }

        static const std::vector<std::string> mbaSchools = {
            "Harvard Business School", "Wharton", "Stanford GSB",
            "Columbia Business School", "Chicago Booth", "MIT Sloan"};
        school = mbaSchools[(int)(rng_.uniform() * mbaSchools.size()) % mbaSchools.size()];
        return school + " MBA '" + std::to_string(gradYear % 100);
    }

    // ── Backstory ───────────────────────────────────────────────

    std::string generateBackstory(const EmployeeCandidate& emp, int gameYear) {
        static const std::vector<std::string> firms = {
            "First Continental Trust", "Atlantic Securities", "Pacific National Bank",
            "Metropolitan Finance", "Republic Investment Group", "Commonwealth Capital",
            "Sterling Brothers", "Meridian Partners", "Vanguard Trust Co.",
            "Heritage Financial", "Cornerstone Capital", "Summit Partners",
            "Whitfield & Sons", "Transcontinental Bancorp", "Founders National",
            "Liberty Capital Partners", "Eagle Rock Investments", "Hudson River Trust",
            "Chesapeake Financial", "Pinnacle Securities", "Northwind Capital",
            "Olympus Asset Management", "Granite State Trust", "Broadmoor Group",
            "Westfield Capital", "Continental Illinois Trust", "Federal Commerce Bank",
            "Ironbridge Partners", "Bayshore Trust", "Keystone Financial Group"};

        std::string firm = firms[(int)(rng_.uniform() * firms.size()) % firms.size()];

        std::string career;
        if (emp.yearsExperience < 3) {
            static const std::vector<std::string> juniorTemplates = {
                "Recently completed a training program at ",
                "Just finished an analyst rotation at ",
                "Completed a two-year associate program at ",
                "Interned and was hired full-time at ",
            };
            career = juniorTemplates[(int)(rng_.uniform() * juniorTemplates.size()) % juniorTemplates.size()]
                   + firm + ".";
        } else {
            static const std::vector<std::string> seniorTemplates = {
                "Spent " + std::to_string(emp.yearsExperience) + " years at " + firm + " building their " + emp.specialty + " practice.",
                "Rose through the ranks at " + firm + " over " + std::to_string(emp.yearsExperience) + " years, most recently heading " + emp.specialty + ".",
                "Joined " + firm + " as an analyst and grew into a " + emp.specialty + " specialist over " + std::to_string(emp.yearsExperience) + " years.",
                "Left " + firm + " after " + std::to_string(emp.yearsExperience) + " years to seek new challenges in " + emp.specialty + ".",
                "Was recruited away from " + firm + " where they led a team of " + std::to_string(3 + emp.yearsExperience / 2) + " in " + emp.specialty + ".",
            };
            career = seniorTemplates[(int)(rng_.uniform() * seniorTemplates.size()) % seniorTemplates.size()];
        }

        // Personality note from traits
        std::string personality;
        if (!emp.traits.empty()) {
            auto& posT = emp.traits[0]; // First trait (usually positive)
            personality = "Known for being " + posT.description + ".";
            if (emp.traits.size() > 1) {
                for (size_t i = 1; i < emp.traits.size(); ++i) {
                    if (!emp.traits[i].positive) {
                        personality += " Colleagues note: " + emp.traits[i].description + ".";
                        break;
                    }
                }
            }
        }

        return emp.education + ". " + career + " " + personality;
    }

    // ── Salary Calculation ──────────────────────────────────────

    double calculateSalary(const EmployeeCandidate& emp) {
        double base = 40000 + emp.yearsExperience * 8000;
        double skillPremium = (emp.stats.analytical + emp.stats.persuasion + emp.stats.networking)
                            / 3.0 * 500;
        double archetypeMult = 1.0;
        switch (emp.archetype) {
            case Archetype::Prodigy: archetypeMult = 0.6; break;
            case Archetype::Lifer: archetypeMult = 0.8; break;
            case Archetype::Operator: archetypeMult = 1.2; break;
            case Archetype::Patrician: archetypeMult = 1.3; break;
            case Archetype::Quant: archetypeMult = 1.4; break;
            case Archetype::Gunslinger: archetypeMult = 1.5; break;
            case Archetype::Dealmaker: archetypeMult = 1.6; break;
            case Archetype::Rainmaker: archetypeMult = 1.7; break;
        }

        // Check for "Expensive Tastes" trait
        for (const auto& t : emp.traits) {
            if (t.name == "Expensive Tastes") archetypeMult *= 1.3;
        }

        return (base + skillPremium) * archetypeMult;
    }
};

} // namespace stvg::simulation
