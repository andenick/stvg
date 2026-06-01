#pragma once

#include "../core/BankState.h"
#include "../core/CEOProfile.h"
#include "../simulation/CrisisEngine.h"
#include <string>

namespace stvg::game {

namespace CrisisHandler {

    bool respond(
        const std::string& crisisId,
        const std::string& responseType,
        simulation::CrisisEngine& crisisEngine,
        ActionPointBudget& actionPoints,
        bool hasCeo,
        const CEOProfile& ceoProfile);

} // namespace CrisisHandler

} // namespace stvg::game
