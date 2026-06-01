#include "stvg/game/CEOAbilityHandler.h"
#include <spdlog/spdlog.h>

namespace stvg::game {

bool CEOAbilityHandler::useAbility(
    const CEOProfile& ceo,
    Bank& bank,
    ActionPointBudget& actionPoints,
    simulation::PoliticalEngine& politicalEngine,
    AbilityState state)
{
    if (state.cooldown > 0) return false;
    if (!actionPoints.spend(3)) return false;

    state.cooldown = ceo.specialAbilityCooldown;
    state.abilityActive = true;

    if (ceo.id == "sterling") {
        state.revenueBoostActive = true;
        spdlog::info("ABILITY: Victoria Sterling activates Double Down!");
    } else if (ceo.id == "webb") {
        if (!politicalEngine.canAfford(10.0)) {
            spdlog::warn("Webb's Political Cover requires 10 political capital (have {:.1f})",
                politicalEngine.currentCapital());
            actionPoints.refund(3);
            state.cooldown = 0;
            return false;
        }
        politicalEngine.spendCapital(10.0);
        politicalEngine.earnCapital(3.0, "Political Cover relationship building");
        state.regulatoryShieldActive = true;
        spdlog::info("ABILITY: Marcus Webb activates Political Cover! (cost 10 political capital)");
    } else if (ceo.id == "okonkwo") {
        for (auto& div : bank.divisions) {
            div.inspectedThisQuarter = true;
        }
        spdlog::info("ABILITY: Dr. Amara Okonkwo activates Full Spectrum Analysis!");
    } else if (ceo.id == "bancroft") {
        state.crisisShieldActive = true;
        spdlog::info("ABILITY: Russell Bancroft activates Fortress Balance Sheet!");
    } else if (ceo.id == "sullivan") {
        for (auto& div : bank.divisions) {
            div.headHonesty = HeadHonesty::Honest;
        }
        spdlog::info("ABILITY: Grace Sullivan activates Truth Serum!");
    }

    return true;
}

} // namespace stvg::game
