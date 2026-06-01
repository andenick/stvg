#include "stvg/game/CrisisHandler.h"
#include <spdlog/spdlog.h>

namespace stvg::game {

bool CrisisHandler::respond(
    const std::string& crisisId,
    const std::string& responseType,
    simulation::CrisisEngine& crisisEngine,
    ActionPointBudget& actionPoints,
    bool hasCeo,
    const CEOProfile& ceoProfile)
{
    if (responseType == "aggressive") {
        if (!actionPoints.spend(1)) {
            spdlog::warn("Not enough action points for aggressive crisis response");
            return false;
        }
    }

    double ceoStress = hasCeo ? ceoProfile.personality.stressResponse : 0.5;
    crisisEngine.resolveWithResponse(crisisId, responseType, ceoStress);
    spdlog::info("Crisis {} responded with: {} (CEO stress {:.2f})",
        crisisId, responseType, ceoStress);

    return true;
}

} // namespace stvg::game
