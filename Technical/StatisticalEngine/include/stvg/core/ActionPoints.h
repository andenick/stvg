#pragma once

#include <nlohmann/json.hpp>
#include <algorithm>

namespace stvg {

struct ActionPointBudget {
    int total = 5;
    int spent = 0;
    int remaining() const { return total - spent; }

    static int budgetForBankSize(double totalAssets) {
        if (totalAssets < 50e9) return 5;
        if (totalAssets < 200e9) return 7;
        if (totalAssets < 500e9) return 8;
        return 10;
    }

    bool spend(int cost) {
        if (cost > remaining()) return false;
        spent += cost;
        return true;
    }

    void refund(int cost) {
        spent = std::max(0, spent - cost);
    }

    void reset(double totalAssets) {
        total = budgetForBankSize(totalAssets);
        spent = 0;
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ActionPointBudget, total, spent)

} // namespace stvg
