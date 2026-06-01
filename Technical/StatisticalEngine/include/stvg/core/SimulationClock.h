#pragma once

#include <chrono>
#include <cstdint>

namespace stvg {

// Manages the mapping between real wall-clock time and simulated game time.
// The simulation runs in "trading days" within quarters/years.
class SimulationClock {
public:
    SimulationClock(int startYear = 2024, int startQuarter = 1, int daysPerQuarter = 63)
        : year_(startYear), quarter_(startQuarter)
        , daysPerQuarter_(daysPerQuarter) {}

    // Advance by one simulated trading day. Returns true if quarter ended.
    bool advanceDay() {
        ++day_;
        ++totalDays_;
        if (day_ >= daysPerQuarter_) {
            day_ = 0;
            ++quarter_;
            if (quarter_ > 4) {
                quarter_ = 1;
                ++year_;
            }
            return true; // Quarter boundary
        }
        return false;
    }

    int year() const { return year_; }
    int quarter() const { return quarter_; }
    int dayInQuarter() const { return day_; }
    int totalDays() const { return totalDays_; }
    int daysPerQuarter() const { return daysPerQuarter_; }

    // Fractional year for continuous-time models
    double yearFraction() const {
        return year_ + (quarter_ - 1) * 0.25 + (double)day_ / (daysPerQuarter_ * 4);
    }

    void reset(int year, int quarter) {
        year_ = year;
        quarter_ = quarter;
        day_ = 0;
        totalDays_ = 0;
    }

    // Restore full clock state from a save (Phase 3.2 save/load support)
    void set(int year, int quarter, int day, int totalDays) {
        year_ = year;
        quarter_ = quarter;
        day_ = day;
        totalDays_ = totalDays;
    }

private:
    int year_;
    int quarter_;
    int day_ = 0;
    int totalDays_ = 0;
    int daysPerQuarter_;
};

} // namespace stvg
