#pragma once

// ════════════════════════════════════════════════════════════════════
// EndgameResolver — out-of-line definitions for QuarterlyTurnManager's
// endgame methods. Extracted from QuarterlyTurnManager.h in Phase 3.1
// to keep that header navigable. Included from the BOTTOM of
// QuarterlyTurnManager.h, after the class definition is complete.
//
// Contains:
//   - checkGameEnd()  — pre-endgame loss/win checks + endgame trigger
//   - resolveEndgame() — picks one of 6 paths based on accumulated state
//
// Both are const member functions of QuarterlyTurnManager.
// ════════════════════════════════════════════════════════════════════

namespace stvg {

inline GameEndState QuarterlyTurnManager::checkGameEnd() const {
    GameEndState end;
    end.quartersPlayed = quartersCompleted_;
    end.avgScore = averageScore();

    // Loss conditions (checked first)
    if (bank_.capital <= 0) {
        end.reason = GameEndReason::Bankrupt;
        end.isVictory = false;
        end.title = "BANKRUPT";
        end.narrative = "The morning papers carry the headline nobody wanted to see. "
            + bank_.name + " has run out of capital. Depositors line up at branches, "
            "but there is nothing left. The FDIC moves in to manage the wind-down.";
        return end;
    }
    // Regulatory seizure: CET1-based post-1988 (Basel), leverage-based pre-1988
    {
        bool seized = false;
        if (state_.currentYear >= 1988) {
            double cet1 = RiskWeightTable::cet1Ratio(bank_, state_.currentYear);
            double required = RiskWeightTable::requiredCET1(state_.currentYear);
            seized = (cet1 < required * 0.5); // seizure at half the minimum
        } else {
            seized = (bank_.capitalRatio() < kBalance.regulatorySeizureCapRatio);
        }
        if (seized) {
            end.reason = GameEndReason::RegulatorySeizure;
            end.isVictory = false;
            end.title = "REGULATORY SEIZURE";
            end.narrative = "The FDIC team arrives at headquarters before dawn. "
                + bank_.name + "'s capital ratio has fallen below the regulatory minimum. "
                "The bank is placed into receivership. Your tenure as CEO is over.";
            return end;
        }
    }
    if (bank_.reputation <= 15.0) {  // Phase 4 A1: raised from 5 to 15
        end.reason = GameEndReason::ReputationCollapse;
        end.isVictory = false;
        end.title = "REPUTATION COLLAPSE";
        end.narrative = "Clients have been quietly moving their accounts for months. "
            "The board of " + bank_.name + " can no longer ignore the exodus. "
            "With a reputation score of just " + std::to_string((int)bank_.reputation)
            + ", the bank's franchise value has been destroyed.";
        return end;
    }
    // Phase 4 A1: BoardFired now triggers at 4 poor quarters OR 8 mediocre quarters
    if (consecutivePoorQuarters_ >= 4 || consecutiveMediocreQuarters_ >= 8) {
        end.reason = GameEndReason::BoardFired;
        end.isVictory = false;
        end.title = "FIRED BY THE BOARD";
        end.narrative = consecutivePoorQuarters_ >= 4
            ? "The board convenes an emergency session. Four consecutive "
              "quarters of dismal performance have exhausted their patience. "
              "\"We appreciate your service, but it's time for new leadership at "
              + bank_.name + ".\""
            : "After two years of mediocre results, the board has lost confidence. "
              "\"The shareholders deserve better,\" the chair says. Your tenure at "
              + bank_.name + " is over.";
        return end;
    }
    // Phase 4 A1: LiquidityCrisis — deposit-to-asset ratio collapse (bank run)
    if (bank_.totalDeposits < bank_.totalAssets * 0.4) {
        end.reason = GameEndReason::LiquidityCrisis;
        end.isVictory = false;
        end.title = "LIQUIDITY CRISIS";
        end.narrative = "The lines at the branches tell the story. Depositors have lost faith in "
            + bank_.name + ". With deposits at just " + std::to_string((int)(bank_.totalDeposits / 1e9))
            + "B against " + std::to_string((int)(bank_.totalAssets / 1e9))
            + "B in assets, the bank cannot fund its operations. The FDIC steps in.";
        return end;
    }
    // Phase 4 A1: FraudScandal — hidden risk surfaced as fraud
    if (fraudTriggered_) {
        end.reason = GameEndReason::FraudScandal;
        end.isVictory = false;
        end.title = "FRAUD SCANDAL";
        end.narrative = "The front page of the Wall Street Journal carries the devastating headline: "
            "'" + bank_.name + " FRAUD EXPOSED.' Years of hidden risk and deception by division "
            "heads have finally come to light. Federal investigators arrive at dawn.";
        return end;
    }

    // Win conditions
    if (bank_.totalAssets >= 2e8) {   // $200M win target (rescaled from $2T)
        end.reason = GameEndReason::GrowthTarget;
        end.isVictory = true;
        end.title = "TOO BIG TO FAIL";
        end.narrative = "Wall Street can scarcely believe it. From a one-branch shop to a "
            "$" + std::to_string((int)(bank_.totalAssets / 1e6)) + " million institution. "
            + bank_.name + " is now a systemically important financial institution. "
            "The regulators are nervous, but the shareholders are ecstatic.";
        return end;
    }
    if (progression_.level >= 40) {
        end.reason = GameEndReason::LegendStatus;
        end.isVictory = true;
        end.title = "WALL STREET LEGEND";
        end.narrative = "After navigating every storm the market could throw at you, "
            "you've earned the title that every banker dreams of. " + progression_.title
            + " — a name that will be remembered long after you step down from " + bank_.name + ".";
        return end;
    }
    // Survivor win: only if a quarter cap is set (quartersPerGame > 0)
    if (config_.quartersPerGame > 0 && quartersCompleted_ >= config_.quartersPerGame && quartersCompleted_ > 0) {
        double avg = totalScore_ / quartersCompleted_;
        if (avg >= 35.0) {
            end.reason = GameEndReason::SurvivorWin;
            end.isVictory = true;
            end.title = "SURVIVOR";
            end.narrative = "The final quarterly report lands with quiet confidence. "
                + bank_.name + " has survived " + std::to_string(quartersCompleted_)
                + " quarters of market turbulence with an average score of "
                + std::to_string((int)avg) + ". Not everyone can say that.";
            return end;
        }
    }

    // Endgame: any doom meter at 100, or year 2040 reached.
    // Outcome is a function of accumulated state (multiple paths).
    const auto& dm = state_.doomMeters;
    if (dm.aiDisplacement >= 100 || dm.climateCatastrophe >= 100 ||
        dm.globalTensions >= 100 || dm.inequality >= 100 ||
        state_.currentYear >= 2040) {
        return resolveEndgame();
    }

    return end; // reason == None
}

// Resolve endgame into one of 6 paths based on accumulated state.
// Order matters: hardest/best paths checked first so easier ones don't shadow them.
inline GameEndState QuarterlyTurnManager::resolveEndgame() const {
    GameEndState end;
    end.quartersPlayed = quartersCompleted_;
    end.avgScore = averageScore();

    const auto& dm = state_.doomMeters;
    int metersMaxed = 0;
    int metersHigh = 0;  // > 70
    for (double m : {dm.aiDisplacement, dm.climateCatastrophe,
                      dm.globalTensions, dm.inequality}) {
        if (m >= 100) metersMaxed++;
        else if (m > 70) metersHigh++;
    }

    double capRatio = bank_.capital / std::max(startingCapital_, 1.0);
    double rep = bank_.reputation;
    int divs = (int)bank_.divisions.size();

    // Path 6: ABOLITION — hardest. All meters cool, deliberately small bank, high rep.
    if (metersMaxed == 0 && metersHigh == 0 && rep > 90 && capRatio < 1.0 && divs <= 2) {
        end.reason = GameEndReason::EndgameAbolition;
        end.isVictory = true;
        end.title = "ABOLITION";
        end.narrative = "You used your accumulated power to dismantle concentrated finance itself. "
            + bank_.name + " is now a public utility. You step down as CEO. "
            "The age of oligarchic banking is over.";
        return end;
    }
    // Path 5: TRANSFORMATION — actively reshaped the system, big bank, big rep, calm world.
    if (metersMaxed == 0 && metersHigh == 0 && rep > 80 && capRatio > 2.0) {
        end.reason = GameEndReason::EndgameTransformation;
        end.isVictory = true;
        end.title = "TRANSFORMATION";
        end.narrative = "Against every prediction, you steered " + bank_.name + " — and the world — "
            "through the gauntlet. Climate stabilized. AI integrated. Tensions managed. "
            "You reshaped global finance into something durable.";
        return end;
    }
    // Path 4: RECONSTRUCTION — burned capital fighting crises, world recovered.
    if (metersMaxed <= 1 && capRatio < 1.0 && rep > 70) {
        end.reason = GameEndReason::EndgameReconstruction;
        end.isVictory = true;
        end.title = "RECONSTRUCTION";
        end.narrative = "You spent the bank's capital fighting the crises of the century. "
            + bank_.name + " is smaller than when you started, but it stands — and so does "
            "much of the world it helped rebuild. The Marshall Plan model, vindicated.";
        return end;
    }
    // Path 3: MANAGED DECLINE — meters elevated but not catastrophic, bank survived.
    if (metersMaxed == 0 && metersHigh >= 2) {
        end.reason = GameEndReason::EndgameManagedDecline;
        end.isVictory = false; // bittersweet
        end.title = "MANAGED DECLINE";
        end.narrative = "The world is diminished. The bank survived. The status quo barely held. "
            "Historians will debate whether " + bank_.name + " was part of the problem "
            "or the only thing that kept it from getting worse.";
        return end;
    }
    // Path 2: FORTRESS — one meter maxed but bank thrived. Techno-feudalism.
    if (metersMaxed >= 1 && capRatio > 1.5 && rep < 60) {
        end.reason = GameEndReason::EndgameFortress;
        end.isVictory = false;
        end.title = "FORTRESS";
        end.narrative = "Outside the walls, the world burned. Inside them, " + bank_.name +
            " flourished. You saved yourself and your shareholders. The history books — "
            "if anyone writes them — will not be kind.";
        return end;
    }
    // Path 1: COLLAPSE — default. Multiple meters maxed or low everything.
    end.reason = GameEndReason::EndgameCollapse;
    end.isVictory = false;
    end.title = "COLLAPSE";
    end.narrative = "Too many crises, too little action. " + bank_.name +
        " could not navigate the convergence of catastrophes that ended the era. "
        "Banking — and much else — will need to be rebuilt from the wreckage.";
    return end;
}

} // namespace stvg
