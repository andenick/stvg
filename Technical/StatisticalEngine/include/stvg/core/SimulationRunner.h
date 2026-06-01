#pragma once

#include "Types.h"
#include "QuarterlyTurnManager.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>
#include <spdlog/spdlog.h>

namespace stvg {

class SimulationRunner {
public:
    using BroadcastFn = std::function<void(const std::string& type, const nlohmann::json& payload)>;

    SimulationRunner(QuarterlyTurnManager& game, std::mutex& gameMtx, BroadcastFn broadcast)
        : game_(game), gameMtx_(gameMtx), broadcastFn_(std::move(broadcast))
        , baseTicksPerSecond_(std::max(1, static_cast<int>(game.config().ticksPerSecond > 0
            ? game.config().ticksPerSecond : 4)))
    {
        game_.setRealTimeSimulation(true);
    }

    ~SimulationRunner() { stop(); }

    void start() {
        if (running_.load()) return;
        running_ = true;
        simThread_ = std::thread([this] { runLoop(); });
        spdlog::info("SimulationRunner started");
    }

    void stop() {
        running_ = false;
        if (simThread_.joinable()) simThread_.join();
    }

    void play() {
        paused_ = false;
        spdlog::info("SimulationRunner: play (speed={}x)", speed_.load());
    }

    void pause() {
        paused_ = true;
        spdlog::info("SimulationRunner: paused");
    }

    void setSpeed(int speed) {
        speed_ = std::clamp(speed, 1, 8);
        spdlog::info("SimulationRunner: speed set to {}x", speed_.load());
    }

    bool isPaused() const { return paused_.load(); }
    int speed() const { return speed_.load(); }

    std::mutex& mutex() { return gameMtx_; }

private:
    QuarterlyTurnManager& game_;
    std::mutex& gameMtx_;
    BroadcastFn broadcastFn_;

    std::thread simThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{true};
    std::atomic<int> speed_{1};
    bool simPrepared_ = false;
    int baseTicksPerSecond_ = 4;

    void runLoop() {
        while (running_.load()) {
            if (paused_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            int tickIntervalMs = 1000 / (baseTicksPerSecond_ * speed_.load());
            auto start = std::chrono::steady_clock::now();

            {
                std::lock_guard lock(gameMtx_);
                try {
                    processOneTick();
                } catch (const std::exception& e) {
                    spdlog::error("SimulationRunner tick error: {}", e.what());
                    paused_ = true;
                } catch (...) {
                    spdlog::error("SimulationRunner tick: unknown error");
                    paused_ = true;
                }
            }

            auto elapsed = std::chrono::steady_clock::now() - start;
            auto remaining = std::chrono::milliseconds(tickIntervalMs) - elapsed;
            if (remaining > std::chrono::milliseconds(1)) {
                std::this_thread::sleep_for(remaining);
            }
        }
    }

    void processOneTick() {
        auto phase = game_.currentPhase();

        switch (phase) {
            case TurnPhase::QuarterStart: {
                game_.advancePhase();
                broadcastFn_("quarter_boundary", {
                    {"year", game_.state().currentYear},
                    {"quarter", game_.state().currentQuarter},
                    {"phase", "decision_phase"},
                    {"pendingDecisions", game_.pendingDecisions()},
                    {"messages", game_.messages()},
                    {"events", game_.quarterEvents()},
                    {"state", game_.toPlayerJson()}
                });
                paused_ = true;
                break;
            }

            case TurnPhase::DecisionPhase: {
                game_.advancePhase();
                game_.prepareSimulation();
                simPrepared_ = true;
                break;
            }

            case TurnPhase::Simulation: {
                if (!simPrepared_) {
                    game_.prepareSimulation();
                    simPrepared_ = true;
                }

                bool done = game_.tickSimulationDay();
                broadcastFn_("market_tick", game_.marketTickPayload());

                if (done) {
                    game_.completeSimulationPhase();
                    simPrepared_ = false;

                    game_.advancePhase();

                    if (game_.currentPhase() == TurnPhase::CrisisResponse) {
                        broadcastFn_("quarter_boundary", {
                            {"year", game_.state().currentYear},
                            {"quarter", game_.state().currentQuarter},
                            {"phase", "crisis_response"},
                            {"activeCrises", game_.crisisEngine().activeCrises()},
                            {"report", game_.lastReport()},
                            {"state", game_.toPlayerJson()}
                        });
                        paused_ = true;
                    } else {
                        broadcastFn_("quarter_boundary", {
                            {"year", game_.state().currentYear},
                            {"quarter", game_.state().currentQuarter},
                            {"phase", "quarter_complete"},
                            {"report", game_.lastReport()},
                            {"state", game_.toPlayerJson()}
                        });
                        game_.advancePhase();
                    }

                    auto endState = game_.checkGameEnd();
                    if (endState.reason != GameEndReason::None) {
                        broadcastFn_("game_over", {
                            {"reason", endState.reason},
                            {"isVictory", endState.isVictory},
                            {"title", endState.title},
                            {"narrative", endState.narrative},
                            {"state", game_.toPlayerJson()}
                        });
                        paused_ = true;
                    }
                }
                break;
            }

            case TurnPhase::QuarterEnd:
                game_.advancePhase();
                break;

            case TurnPhase::CrisisResponse:
                game_.advancePhase();
                game_.advancePhase();
                break;

            case TurnPhase::QuarterComplete:
                game_.advancePhase();
                break;
        }
    }
};

} // namespace stvg
