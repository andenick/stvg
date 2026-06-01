#include <stvg/api/WebSocketServer.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_running{true};

void signalHandler(int sig) {
    spdlog::info("Received signal {}, shutting down...", sig);
    g_running = false;
}

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("STVG Statistical Engine v2.0");
    spdlog::info("========================================");

    uint16_t port = 8080;
    std::string staticDir = "static";
    if (argc > 1) port = static_cast<uint16_t>(std::stoi(argv[1]));
    if (argc > 2) staticDir = argv[2];

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    stvg::api::Server server(port, staticDir);

    spdlog::info("Game UI:     http://localhost:{}/", port);
    spdlog::info("REST API:    http://localhost:{}/api/health", port);
    spdlog::info("WebSocket:   ws://localhost:{}/ws", port);
    spdlog::info("Static dir:  {}", staticDir);
    spdlog::info("========================================");
    spdlog::info("Create a game: POST /api/game/new");
    spdlog::info("Then advance:  POST /api/game/<id>/advance");
    spdlog::info("========================================");

    // Run server in a thread so we can handle signals
    std::thread serverThread([&]() { server.run(); });

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.stop();
    if (serverThread.joinable()) serverThread.join();

    spdlog::info("STVG shut down cleanly");
    return 0;
}
