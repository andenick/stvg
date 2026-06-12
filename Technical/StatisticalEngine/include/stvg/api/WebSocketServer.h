#pragma once

#include "../core/Types.h"
#include "../core/GameConfig.h"
#include "../core/QuarterlyTurnManager.h"
#include "../core/SimulationRunner.h"
#include <crow.h>
#include <spdlog/spdlog.h>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>

namespace stvg::api {

class Server {
public:
    explicit Server(uint16_t port = 8080, const std::string& staticDir = "static")
        : port_(port), staticDir_(staticDir) {
        setupRoutes();
    }

    void run() {
        spdlog::info("STVG server starting on port {}", port_);
        app_.port(port_).multithreaded().run();
    }

    void stop() {
        spdlog::info("Server stopping — shutting down {} simulation runners", simRunners_.size());
        for (auto& [id, runner] : simRunners_) {
            if (runner) runner->stop();
        }
        simRunners_.clear();
        app_.stop();
    }

    // Create a new turn-based game session using global config
    std::string createGame(const std::string& ceoId = "",
                           StartingPosition startPos = StartingPosition::CommercialBank) {
        SimulationConfig simConfig;
        simConfig.seed = config_.timing.seed;
        simConfig.startYear = config_.timing.startYear;
        simConfig.startQuarter = config_.timing.startQuarter;
        simConfig.quarterDurationDays = config_.timing.quarterDurationDays;
        simConfig.ticksPerSecond = config_.timing.ticksPerSecond;
        simConfig.daysPerTick = config_.timing.daysPerTick;
        simConfig.marketVolScale = config_.markets.volScale;
        // STAR_02 P5 §5.3: the live player server enables event→market coupling
        // (default-off in the engine so deterministic tests are unaffected;
        // marketVolScale precedent). Headlines now genuinely lead price action.
        simConfig.eventMarketCoupling = true;

        // Pass difficulty-tunable reputation params from GameConfig
        simConfig.reputationRecoveryRate = config_.crisis.reputationRecoveryRate;
        simConfig.reputationCrisisDamageMultiplier = config_.crisis.reputationDamagePerSeverity / 1.5;

        auto bankConfig = bankConfigForPosition(startPos);

        std::lock_guard lock(gameMtx_);
        auto game = std::make_unique<QuarterlyTurnManager>(simConfig, bankConfig, ceoId, startPos);
        std::string id = "game_" + std::to_string(simConfig.seed);
        games_[id] = std::move(game);

        auto mtx = std::make_unique<std::mutex>();
        auto* mtxPtr = mtx.get();
        auto* gamePtr = games_[id].get();
        gameRunnerMtx_[id] = std::move(mtx);

        auto runner = std::make_unique<SimulationRunner>(
            *gamePtr, *mtxPtr,
            [this, id](const std::string& type, const nlohmann::json& payload) {
                broadcast(id, type, payload);
            }
        );
        runner->start();
        simRunners_[id] = std::move(runner);

        spdlog::info("Created game {} with SimulationRunner{}", id,
            ceoId.empty() ? "" : " (CEO: " + ceoId + ")");
        return id;
    }

    QuarterlyTurnManager* getGame(const std::string& id) {
        std::lock_guard lock(gameMtx_);
        auto it = games_.find(id);
        return it != games_.end() ? it->second.get() : nullptr;
    }

    // Acquire the per-game mutex for thread-safe access alongside SimulationRunner.
    // Releases gameMtx_ before blocking on the per-game lock to avoid priority inversion.
    std::unique_lock<std::mutex> lockGame(const std::string& gameId) {
        std::mutex* mtx = nullptr;
        {
            std::lock_guard mapLock(gameMtx_);
            auto it = gameRunnerMtx_.find(gameId);
            if (it != gameRunnerMtx_.end()) mtx = it->second.get();
        }
        if (mtx) return std::unique_lock<std::mutex>(*mtx);
        return std::unique_lock<std::mutex>();
    }

    // Get the SimulationRunner for a game (thread-safe).
    SimulationRunner* getRunner(const std::string& gameId) {
        std::lock_guard mapLock(gameMtx_);
        auto it = simRunners_.find(gameId);
        return it != simRunners_.end() ? it->second.get() : nullptr;
    }

    void broadcast(const std::string& gameId, const std::string& type, const nlohmann::json& payload) {
        auto msg = WebSocketMessage{type, payload, now_iso8601()};
        broadcastToGame(gameId, msg.to_json().dump());
    }

private:
    crow::SimpleApp app_;
    uint16_t port_;
    std::string staticDir_;

    GameConfig config_;  // Global config — applied to new games
    mutable std::mutex gameMtx_;
    std::unordered_map<std::string, std::unique_ptr<QuarterlyTurnManager>> games_;

    std::unordered_map<std::string, std::unique_ptr<std::mutex>> gameRunnerMtx_;
    std::unordered_map<std::string, std::unique_ptr<SimulationRunner>> simRunners_;

    mutable std::mutex wsMtx_;
    std::unordered_map<std::string, std::unordered_set<crow::websocket::connection*>> wsClients_;

    // Helper: JSON error response
    static crow::response jsonError(int code, const std::string& msg) {
        nlohmann::json j{{"success", false}, {"error", {{"message", msg}}}, {"timestamp", now_iso8601()}};
        auto resp = crow::response(code, j.dump());
        resp.add_header("Content-Type", "application/json");
        resp.add_header("Access-Control-Allow-Origin", "*");
        return resp;
    }

    // Helper: JSON success response
    static crow::response jsonOk(const nlohmann::json& data) {
        nlohmann::json j{{"success", true}, {"data", data}, {"timestamp", now_iso8601()}};
        auto resp = crow::response(200, j.dump());
        resp.add_header("Content-Type", "application/json");
        resp.add_header("Access-Control-Allow-Origin", "*");
        return resp;
    }

    std::string readFile(const std::string& path) const {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            // Try with backslashes on Windows
            std::string winPath = path;
            for (auto& c : winPath) if (c == '/') c = '\\';
            f.open(winPath, std::ios::binary);
            if (!f.is_open()) return "";
        }
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    void setupRoutes() {
        // ── Static files: serve index.html at root ────────────────
        CROW_ROUTE(app_, "/")
        ([this]() {
            std::string html = readFile(staticDir_ + "/index.html");
            if (html.empty()) {
                return crow::response(200, "text/html",
                    "<h1>STVG Server Running</h1><p>Place index.html in static/ directory.</p>"
                    "<p><a href='/api/health'>Health Check</a></p>");
            }
            return crow::response(200, "text/html", html);
        });

        // ── Static files: serve assets (JS/CSS bundles from Vite build) ──
        CROW_ROUTE(app_, "/assets/<string>")
        ([this](const std::string& filename) {
            if (filename.find("..") != std::string::npos) return crow::response(403);
            std::string content = readFile(staticDir_ + "/assets/" + filename);
            if (content.empty()) return crow::response(404);

            std::string ct = "application/octet-stream";
            if (filename.rfind(".js") == filename.length() - 3) ct = "application/javascript";
            else if (filename.rfind(".css") == filename.length() - 4) ct = "text/css";

            auto resp = crow::response(200, ct, content);
            resp.add_header("Cache-Control", "public, max-age=31536000");
            return resp;
        });

        // ── Static files: character portraits (P4 §4.4) ──
        // Serves generated portrait PNGs (+ the manifest.json) from
        // static/portraits/. Until the P9 art pipeline backfills real assets the
        // manifest is `{}` and the client falls back to local DiceBear SVGs, so
        // this route mostly serves the empty manifest. One extra path segment, so
        // it needs its own route (the single-segment /assets/<string> won't match).
        CROW_ROUTE(app_, "/assets/portraits/<string>")
        ([this](const std::string& filename) {
            if (filename.find("..") != std::string::npos) return crow::response(403);
            std::string content = readFile(staticDir_ + "/portraits/" + filename);
            if (content.empty()) return crow::response(404);

            std::string ct = "application/octet-stream";
            if (filename.rfind(".json") == filename.length() - 5) ct = "application/json";
            else if (filename.rfind(".png") == filename.length() - 4) ct = "image/png";
            else if (filename.rfind(".svg") == filename.length() - 4) ct = "image/svg+xml";
            else if (filename.rfind(".webp") == filename.length() - 5) ct = "image/webp";

            auto resp = crow::response(200, ct, content);
            resp.add_header("Cache-Control", "public, max-age=3600");
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });

        // ── Static files: favicon and icons ──
        CROW_ROUTE(app_, "/favicon.svg")
        ([this]() {
            std::string content = readFile(staticDir_ + "/favicon.svg");
            if (content.empty()) return crow::response(404);
            return crow::response(200, "image/svg+xml", content);
        });

        // ── Health check ──────────────────────────────────────────
        CROW_ROUTE(app_, "/api/health")
        ([this]() {
            std::lock_guard lock(gameMtx_);
            return jsonOk({{"status", "ok"}, {"games", games_.size()}});
        });

        // ── List available CEOs ────────────────────────────────────
        CROW_ROUTE(app_, "/api/ceos").methods(crow::HTTPMethod::GET)
        ([]() {
            auto profiles = CEOProfile::allProfiles();
            nlohmann::json j = profiles;
            auto resp = crow::response(200, nlohmann::json{{"success", true}, {"data", j}, {"timestamp", now_iso8601()}}.dump());
            resp.add_header("Content-Type", "application/json");
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });

        // ── List available scenarios ──────────────────────────────
        CROW_ROUTE(app_, "/api/scenarios").methods(crow::HTTPMethod::GET)
        ([]() {
            nlohmann::json scenarios = nlohmann::json::array();
            for (const auto& path : {"data/scenarios/tutorial.json",
                                      "data/scenarios/crisis_mode.json",
                                      "data/scenarios/full_game.json",
                                      "data/scenarios/speedrun.json",
                                      "data/scenarios/ai_endgame.json"}) {
                std::ifstream f(path);
                if (!f.is_open()) f.open(std::string("../") + path);
                if (f.is_open()) {
                    try { scenarios.push_back(nlohmann::json::parse(f)); } catch (...) {}
                }
            }
            auto resp = crow::response(200, nlohmann::json{
                {"success", true}, {"data", scenarios}, {"timestamp", now_iso8601()}}.dump());
            resp.add_header("Content-Type", "application/json");
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });

        // ── List available starting positions ────────────────────
        CROW_ROUTE(app_, "/api/starting-positions").methods(crow::HTTPMethod::GET)
        ([]() {
            auto positions = allStartingPositionMetadata();
            auto resp = crow::response(200, nlohmann::json{{"success", true}, {"data", positions}, {"timestamp", now_iso8601()}}.dump());
            resp.add_header("Content-Type", "application/json");
            resp.add_header("Access-Control-Allow-Origin", "*");
            return resp;
        });

        // ── Create new game ───────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/new").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            std::string ceoId;
            StartingPosition startPos = StartingPosition::CommercialBank;
            // Apply any per-request overrides to config
            try {
                if (!req.body.empty()) {
                    auto body = nlohmann::json::parse(req.body);
                    if (body.contains("seed")) config_.timing.seed = body["seed"].get<uint64_t>();
                    if (body.contains("startYear")) config_.timing.startYear = body["startYear"];
                    if (body.contains("ceoId")) ceoId = body["ceoId"].get<std::string>();
                    if (body.contains("startingPosition")) {
                        startPos = body["startingPosition"].get<StartingPosition>();
                    }
                    if (body.contains("preset")) {
                        std::string p = body["preset"];
                        if (p == "sandbox") config_ = GameConfig::Sandbox();
                        else if (p == "easy") config_ = GameConfig::Easy();
                        else if (p == "medium" || p == "normal") config_ = GameConfig::Medium();
                        else if (p == "hard") config_ = GameConfig::Hard();
                        else if (p == "crisis") config_ = GameConfig::Crisis();
                    }
                    if (body.contains("marketVolScale"))
                        config_.markets.volScale = body["marketVolScale"].get<double>();
                }
            } catch (...) {}

            // Live player games get a livelier, more exaggerated intra-quarter
            // market than the raw historical calibration (tests/autoplay stay 1.0).
            // A preset reset above clears volScale, so (re)assert it here unless
            // the request explicitly overrode it.
            if (config_.markets.volScale <= 1.0) config_.markets.volScale = 2.0;

            std::string id = createGame(ceoId, startPos);
            return jsonOk({{"gameId", id}, {"config", config_.toJson()}, {"startingPosition", nlohmann::json(startPos)}});
        });

        // ── Get game state (player view — filtered by organizational complexity) ──
        CROW_ROUTE(app_, "/api/game/<string>/state").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->toPlayerJson());
        });

        // ── Get full truth (debug/post-mortem) ───────────────────
        CROW_ROUTE(app_, "/api/game/<string>/truth").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->toGodJson());
        });

        // ── Advance turn phase ────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/advance").methods(crow::HTTPMethod::POST)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");

            auto* runner = getRunner(gameId);
            if (runner) {
                runner->play();
                return jsonOk({{"delegated", true},
                               {"message", "Real-time mode active — use play/pause controls"}});
            }

            auto gameLock = lockGame(gameId);
            auto newPhase = game->advancePhase();

            broadcast(gameId, "turn/phase/changed", {
                {"phase", newPhase},
                {"year", game->state().currentYear},
                {"quarter", game->state().currentQuarter}
            });

            return jsonOk(game->toPlayerJson());
        });

        // ── Get pending decisions ─────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/decisions").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->pendingDecisions());
        });

        // ── Submit a decision ─────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/decision/<string>").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, const std::string& gameId, const std::string& decisionId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");

            std::string optionId;
            try {
                auto body = nlohmann::json::parse(req.body);
                optionId = body.value("optionId", "");
            } catch (...) {
                return jsonError(400, "Missing optionId");
            }

            auto gameLock = lockGame(gameId);
            bool ok = game->submitDecision(decisionId, optionId);
            if (!ok) return jsonError(400, "Decision failed (insufficient AP or not found)");

            return jsonOk({{"resolved", true}, {"decisionId", decisionId}, {"optionId", optionId},
                           {"actionPoints", game->actionPoints()}});
        });

        // ── Get character messages ────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/messages").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->messages());
        });

        // ── Get quarterly report ──────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/report").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->lastReport());
        });

        // ── Get bank state ────────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/bank").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->bank());
        });

        // ── Get traders ───────────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/traders").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->traders());
        });

        // ── Get events ────────────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/events").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->quarterEvents());
        });

        // ── Macro history (P3.2) ──────────────────────────────────
        // Per-quarter macro snapshots + market closes for chart hydration.
        // Shape: { quarters: [ { year, quarter, econ:{...}, closes:{id:px} } ] }
        CROW_ROUTE(app_, "/api/game/<string>/macro-history").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->macroHistoryJson());
        });

        // ── Hiring pool ───────────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/hiring").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->hiringPool());
        });

        // ── Hire employee ─────────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/hire").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            try {
                auto body = nlohmann::json::parse(req.body);
                std::string candidateId = body.value("candidateId", "");
                std::string divisionId = body.value("divisionId", "");
                auto gameLock = lockGame(gameId);
                bool ok = game->hireEmployee(candidateId, divisionId);
                if (!ok) return jsonError(400, "Hire failed");
                return jsonOk({{"hired", true}, {"candidateId", candidateId}, {"divisionId", divisionId}});
            } catch (...) {
                return jsonError(400, "Bad request");
            }
        });

        // ── Poach a rival division (STAR_02 P6) ───────────────────
        CROW_ROUTE(app_, "/api/game/<string>/poach").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            try {
                auto body = nlohmann::json::parse(req.body);
                std::string offerId = body.value("offerId", "");
                if (offerId.empty()) return jsonError(400, "Missing offerId");
                auto gameLock = lockGame(gameId);
                bool ok = game->acceptPoach(offerId);
                if (!ok) return jsonError(400, "Poach failed (unknown/expired offer or unaffordable)");
                return jsonOk({{"poached", true}, {"offerId", offerId},
                               {"bankCapital", game->bank().capital}});
            } catch (...) {
                return jsonError(400, "Bad request");
            }
        });

        // ── Poach offers list (STAR_02 P6) ────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/poach-offers").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->poachOffers());
        });

        // ── Fire employee ─────────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/fire").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            try {
                auto body = nlohmann::json::parse(req.body);
                std::string employeeId = body.value("employeeId", "");
                auto gameLock = lockGame(gameId);
                bool ok = game->fireEmployee(employeeId);
                if (!ok) return jsonError(400, "Fire failed");
                return jsonOk({{"fired", true}, {"employeeId", employeeId}});
            } catch (...) {
                return jsonError(400, "Bad request");
            }
        });

        // ── Set division autonomy ─────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/division/<string>/autonomy").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, const std::string& gameId, const std::string& divId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            try {
                auto body = nlohmann::json::parse(req.body);
                double level = body.value("level", -1.0);
                if (level < 0 || level > 1) return jsonError(400, "Level must be 0-1");
                auto gameLock = lockGame(gameId);
                bool ok = game->setDivisionAutonomy(divId, level);
                if (!ok) return jsonError(400, "Failed (insufficient AP or division not found)");
                return jsonOk({{"divisionId", divId}, {"autonomyLevel", level},
                               {"actionPoints", game->actionPoints()}});
            } catch (...) {
                return jsonError(400, "Bad request");
            }
        });

        // ── Use CEO special ability ──────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/ability").methods(crow::HTTPMethod::POST)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            if (!game->hasCeo()) return jsonError(400, "No CEO selected");
            bool ok = game->useSpecialAbility();
            if (!ok) {
                int cd = game->specialAbilityCooldownRemaining();
                if (cd > 0) return jsonError(400, "Ability on cooldown (" + std::to_string(cd) + "Q remaining)");
                return jsonError(400, "Insufficient action points (costs 3 AP)");
            }
            return jsonOk({
                {"abilityUsed", game->ceoProfile().specialAbilityName},
                {"cooldown", game->specialAbilityCooldownRemaining()},
                {"actionPoints", game->actionPoints()}
            });
        });

        // ── Respond to crisis ──────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/crisis/respond").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            try {
                auto body = nlohmann::json::parse(req.body);
                std::string crisisId = body.value("crisisId", "");
                std::string responseType = body.value("responseType", "measured");
                if (crisisId.empty()) return jsonError(400, "Missing crisisId");
                if (responseType != "aggressive" && responseType != "measured" && responseType != "gamble") {
                    return jsonError(400, "Invalid responseType (aggressive|measured|gamble)");
                }
                auto gameLock = lockGame(gameId);
                bool ok = game->respondToCrisis(crisisId, responseType);
                if (!ok) return jsonError(400, "Cannot respond to crisis (wrong phase or insufficient AP)");
                return jsonOk({
                    {"crisisId", crisisId},
                    {"responseType", responseType},
                    {"phase", game->currentPhase()},
                    {"actionPoints", game->actionPoints()}
                });
            } catch (...) {
                return jsonError(400, "Invalid JSON");
            }
        });

        // ── Available deals ────────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/deals").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk({
                {"available", game->availableDeals()},
                {"active", game->activeDeals()}
            });
        });

        // ── Accept deal ───────────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/deal/<string>/accept").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, const std::string& gameId, const std::string& dealId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            try {
                auto body = nlohmann::json::parse(req.body);
                double amount = body.value("investmentAmount", 0.0);
                if (amount <= 0) return jsonError(400, "Investment amount required");
                auto gameLock = lockGame(gameId);
                bool ok = game->acceptDeal(dealId, amount);
                if (!ok) return jsonError(400, "Deal acceptance failed");
                return jsonOk({{"accepted", true}, {"dealId", dealId}, {"amount", amount}});
            } catch (...) {
                return jsonError(400, "Bad request");
            }
        });

        // ── Personal trade (STAR_02 P7) ───────────────────────────
        // Market order on the CEO's off-balance-sheet personal account.
        // Body: { marketId, side: "buy"|"sell", amount } (amount = $ notional).
        CROW_ROUTE(app_, "/api/game/<string>/trade").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            try {
                auto body = nlohmann::json::parse(req.body);
                std::string marketId = body.value("marketId", "");
                std::string side = body.value("side", "");
                double amount = body.value("amount", 0.0);
                if (marketId.empty()) return jsonError(400, "Missing marketId");
                if (side != "buy" && side != "sell") return jsonError(400, "side must be buy or sell");
                if (!(amount > 0.0)) return jsonError(400, "amount must be positive");
                auto gameLock = lockGame(gameId);
                auto r = game->trade(marketId, side, amount);
                if (!r.ok) return jsonError(400, r.err);
                return jsonOk({
                    {"traded", true}, {"marketId", marketId}, {"side", side},
                    {"qtyDelta", r.qtyDelta}, {"realizedPnl", r.realizedPnl},
                    {"positionQtyAfter", r.positionQtyAfter},
                    {"tradeBook", game->tradeBookJson()},
                });
            } catch (...) {
                return jsonError(400, "Bad request");
            }
        });

        // ── Personal trade book (STAR_02 P7) ──────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/trade-book").methods(crow::HTTPMethod::GET)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->tradeBookJson());
        });

        // ── Player telemetry (local JSONL) ────────────────────────
        // Appends play-event(s) to telemetry/session_<id>.jsonl. Body is either
        // a single event object or { sessionId, events: [...] }. Local-only for
        // now; a remote shipper can be added at the big release.
        CROW_ROUTE(app_, "/api/telemetry").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto body = nlohmann::json::parse(req.body);
                std::string sessionId = body.value("sessionId", std::string("anon"));
                std::string safe;
                for (char c : sessionId) safe += (std::isalnum((unsigned char)c) ? c : '_');
                if (safe.empty()) safe = "anon";
                std::error_code ec;
                std::filesystem::create_directories("telemetry", ec);
                std::string path = "telemetry/session_" + safe + ".jsonl";
                // P1/P4: if this is a brand-new session file, prepend a
                // self-describing meta line so the JSONL is self-documenting.
                bool isNewFile = !std::filesystem::exists(path, ec);
                std::ofstream f(path, std::ios::app);
                if (isNewFile) {
                    nlohmann::json meta = {{"type", "meta"}, {"schema", 2}};
                    f << meta.dump() << "\n";
                }
                if (body.contains("events") && body["events"].is_array()) {
                    for (const auto& e : body["events"]) f << e.dump() << "\n";
                } else {
                    f << body.dump() << "\n";
                }
                return jsonOk({{"logged", true}});
            } catch (...) {
                return jsonError(400, "Bad telemetry");
            }
        });

        // ── Config: Get current config ──────────────────────────
        CROW_ROUTE(app_, "/api/config").methods(crow::HTTPMethod::GET)
        ([this]() {
            return jsonOk(config_.toJson());
        });

        // ── Config: Update (partial merge) ──────────────────────
        CROW_ROUTE(app_, "/api/config").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req) {
            try {
                auto j = nlohmann::json::parse(req.body);
                config_.mergeFromJson(j);
                auto validation = config_.validate();
                nlohmann::json resp = config_.toJson();
                resp["validation"] = {
                    {"valid", validation.valid()},
                    {"errors", validation.errors},
                    {"warnings", validation.warnings}
                };
                return jsonOk(resp);
            } catch (const std::exception& e) {
                return jsonError(400, std::string("Bad config JSON: ") + e.what());
            }
        });

        // ── Config: Load preset ─────────────────────────────────
        CROW_ROUTE(app_, "/api/config/preset/<string>").methods(crow::HTTPMethod::POST)
        ([this](const std::string& preset) {
            if (preset == "sandbox") config_ = GameConfig::Sandbox();
            else if (preset == "easy") config_ = GameConfig::Easy();
            else if (preset == "medium" || preset == "normal") config_ = GameConfig::Medium();
            else if (preset == "hard") config_ = GameConfig::Hard();
            else if (preset == "crisis") config_ = GameConfig::Crisis();
            else return jsonError(400, "Unknown preset: " + preset);
            spdlog::info("Config preset loaded: {}", preset);
            return jsonOk(config_.toJson());
        });

        // ── Config: Validate ────────────────────────────────────
        CROW_ROUTE(app_, "/api/config/validate").methods(crow::HTTPMethod::GET)
        ([this]() {
            auto v = config_.validate();
            return jsonOk({{"valid", v.valid()}, {"errors", v.errors}, {"warnings", v.warnings}});
        });

        // ── Save game state ───────────────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/save").methods(crow::HTTPMethod::POST)
        ([this](const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            auto gameLock = lockGame(gameId);
            return jsonOk(game->saveGame());
        });

        // ── Load game state (Phase 4 B2) ────────────────────────────
        CROW_ROUTE(app_, "/api/game/<string>/load").methods(crow::HTTPMethod::POST)
        ([this](const crow::request& req, const std::string& gameId) {
            auto* game = getGame(gameId);
            if (!game) return jsonError(404, "Game not found");
            try {
                auto body = nlohmann::json::parse(req.body);
                auto gameLock = lockGame(gameId);
                if (!game->loadGame(body)) return jsonError(400, "Invalid save data");
                return jsonOk({{"loaded", true}});
            } catch (const std::exception& e) {
                return jsonError(400, std::string("Parse error: ") + e.what());
            }
        });

        // ── CORS preflight ────────────────────────────────────────
        CROW_ROUTE(app_, "/api/<path>").methods(crow::HTTPMethod::OPTIONS)
        ([](const std::string&) {
            auto resp = crow::response(204);
            resp.add_header("Access-Control-Allow-Origin", "*");
            resp.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            resp.add_header("Access-Control-Allow-Headers", "Content-Type");
            return resp;
        });

        // ── WebSocket ─────────────────────────────────────────────
        CROW_WEBSOCKET_ROUTE(app_, "/ws")
            .onopen([this](crow::websocket::connection& conn) {
                spdlog::info("WebSocket client connected");
                std::lock_guard lock(wsMtx_);
                wsClients_["_global"].insert(&conn);
            })
            .onclose([this](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
                spdlog::info("WebSocket disconnected: {} ({})", reason, code);
                std::lock_guard lock(wsMtx_);
                for (auto& [_, clients] : wsClients_) clients.erase(&conn);
            })
            .onmessage([this](crow::websocket::connection& conn, const std::string& data, bool) {
                try {
                    auto msg = nlohmann::json::parse(data);
                    std::string type = msg.value("type", "");

                    if (type == "subscribe") {
                        std::string gid = msg.value("gameId", "");
                        if (!gid.empty()) {
                            std::lock_guard lock(wsMtx_);
                            wsClients_[gid].insert(&conn);
                            wsClients_["_global"].erase(&conn);
                        }
                    }
                    else if (type == "simulation_control") {
                        std::string gid = msg.value("gameId", "");
                        std::string action = msg.value("action", "");
                        auto* runner = getRunner(gid);
                        if (runner) {
                            if (action == "play") {
                                runner->play();
                            } else if (action == "pause") {
                                runner->pause();
                            } else if (action == "set_speed") {
                                int speed = msg.value("speed", 1);
                                runner->setSpeed(speed);
                            }
                            broadcast(gid, "simulation_state", {
                                {"paused", runner->isPaused()},
                                {"speed", runner->speed()}
                            });
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::warn("Bad WS message: {}", e.what());
                }
            });
    }

    void broadcastToGame(const std::string& gameId, const std::string& data) {
        std::lock_guard lock(wsMtx_);
        for (const auto& key : {gameId, std::string("_global")}) {
            if (auto it = wsClients_.find(key); it != wsClients_.end()) {
                for (auto* conn : it->second) conn->send_text(data);
            }
        }
    }
};

} // namespace stvg::api
