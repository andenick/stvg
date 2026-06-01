#pragma once

#include <cmath>
#include <algorithm>

namespace stvg::simulation {

struct MaturityProfile {
    double shortPct = 0.30;   // < 1yr: treasuries, interbank
    double mediumPct = 0.50;  // 1-5yr: MBS, corporate bonds
    double longPct = 0.20;    // 5-30yr: long bonds, fixed mortgages

    static constexpr double SHORT_DURATION = 0.5;
    static constexpr double MEDIUM_DURATION = 3.0;
    static constexpr double LONG_DURATION = 8.0;

    double portfolioDuration() const {
        return shortPct * SHORT_DURATION
             + mediumPct * MEDIUM_DURATION
             + longPct * LONG_DURATION;
    }

    double markToMarketImpact(double yieldChangeBps) const {
        double yieldChange = yieldChangeBps / 10000.0;
        return -portfolioDuration() * yieldChange;
    }

    double portfolioYield(double shortRate, double mediumRate, double longRate) const {
        return shortPct * shortRate + mediumPct * mediumRate + longPct * longRate;
    }

    void setTarget(double shortTarget, double mediumTarget, double longTarget) {
        double total = shortTarget + mediumTarget + longTarget;
        if (total <= 0) { shortPct = 0.33; mediumPct = 0.34; longPct = 0.33; return; }
        shortPct = shortTarget / total;
        mediumPct = mediumTarget / total;
        longPct = longTarget / total;
    }
};

enum class DurationTarget { Short, Balanced, Long };

struct DurationRiskState {
    MaturityProfile profile;
    DurationTarget target = DurationTarget::Balanced;
    double prevYield10Y = 0.03;
    double unrealizedGainsLosses = 0.0; // AOCI: positive = gains, negative = losses
    double cumulativeMtmPnL = 0.0;

    void applyTarget() {
        switch (target) {
            case DurationTarget::Short:
                profile.setTarget(0.60, 0.30, 0.10);
                break;
            case DurationTarget::Balanced:
                profile.setTarget(0.30, 0.50, 0.20);
                break;
            case DurationTarget::Long:
                profile.setTarget(0.10, 0.40, 0.50);
                break;
        }
    }

    void updateQuarterly(double currentYield10Y, double totalSecurities) {
        applyTarget();
        double yieldChange = currentYield10Y - prevYield10Y;
        double mtmImpact = profile.markToMarketImpact(yieldChange * 10000.0);
        double pnl = totalSecurities * mtmImpact;
        unrealizedGainsLosses += pnl;
        cumulativeMtmPnL += pnl;
        prevYield10Y = currentYield10Y;
    }

    bool forcedSaleTriggered(double capital) const {
        return unrealizedGainsLosses < -0.30 * capital;
    }

    double forcedSaleLoss(double capital) const {
        if (!forcedSaleTriggered(capital)) return 0;
        return std::min(std::fabs(unrealizedGainsLosses) * 0.5, capital * 0.20);
    }
};

} // namespace stvg::simulation
