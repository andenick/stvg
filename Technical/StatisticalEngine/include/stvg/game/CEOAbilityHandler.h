#pragma once

#include "../core/BankState.h"
#include "../core/CEOProfile.h"
#include "../simulation/PoliticalEngine.h"
#include <string>

namespace stvg::game {

namespace CEOAbilityHandler {

    struct AbilityState {
        int& cooldown;
        bool& abilityActive;
        bool& revenueBoostActive;
        bool& regulatoryShieldActive;
        bool& crisisShieldActive;
    };

    bool useAbility(
        const CEOProfile& ceo,
        Bank& bank,
        ActionPointBudget& actionPoints,
        simulation::PoliticalEngine& politicalEngine,
        AbilityState state);

} // namespace CEOAbilityHandler

} // namespace stvg::game
