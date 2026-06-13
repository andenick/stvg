#pragma once

// ════════════════════════════════════════════════════════════════════
// ServerRuntime — hosting-hardening helpers for the public playtest build
// (D1 deployment). Pure, portable C++20: env-driven config, a telemetry
// writer with caps + per-day directories + known-session gating, and a
// background housekeeping thread (gzip old dirs, delete very old dirs).
//
// No shell-outs. Gzip uses zlib when available (STVG_HAVE_ZLIB, wired by
// CMake find_package(ZLIB)); when zlib is absent the housekeeper still
// performs age-based deletion (the disk-protection guarantee) and simply
// skips compression. This keeps the MSVC dev build green without forcing a
// zlib dependency on Windows while giving the Linux container real gzip.
// ════════════════════════════════════════════════════════════════════

#include <string>
#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <atomic>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

#ifdef STVG_HAVE_ZLIB
#include <zlib.h>
#endif

namespace stvg::api {

namespace fs = std::filesystem;

// ── Environment helpers ─────────────────────────────────────────────
inline std::string envOr(const char* key, const std::string& dflt) {
    const char* v = std::getenv(key);
    return (v && *v) ? std::string(v) : dflt;
}
inline long envLongOr(const char* key, long dflt) {
    const char* v = std::getenv(key);
    if (!v || !*v) return dflt;
    try { return std::stol(v); } catch (...) { return dflt; }
}

// Runtime configuration resolved once at boot from the environment.
struct RuntimeConfig {
    std::string dataDir;          // STVG_DATA_DIR (saves + telemetry root)
    int maxSessions = 12;         // STVG_MAX_SESSIONS
    int perIpCap = 3;             // STVG_PER_IP_CAP
    long idleEvictSeconds = 1800; // 30 min
    long sweepIntervalSec = 60;
    long bodyCapBytes = 256 * 1024;        // 256 KB request body cap
    long sessionFileCapBytes = 20 * 1024 * 1024; // 20 MB per session file
    long dirQuotaBytes = 5LL * 1024 * 1024 * 1024; // 5 GB telemetry quota
    int gzipOlderThanDays = 2;
    int deleteOlderThanDays = 60;

    static RuntimeConfig fromEnv() {
        RuntimeConfig c;
        // Default ./data-runtime locally; the container sets /var/lib/stvg.
        c.dataDir = envOr("STVG_DATA_DIR", "./data-runtime");
        c.maxSessions = static_cast<int>(envLongOr("STVG_MAX_SESSIONS", 12));
        c.perIpCap = static_cast<int>(envLongOr("STVG_PER_IP_CAP", 3));
        // Idle eviction window (seconds). Default 30 min; overridable for tuning
        // and for fast eviction tests (STVG_IDLE_EVICT_SECONDS).
        c.idleEvictSeconds = envLongOr("STVG_IDLE_EVICT_SECONDS", 1800);
        c.sweepIntervalSec = envLongOr("STVG_SWEEP_INTERVAL_SECONDS", 60);
        return c;
    }

    std::string savesDir() const { return (fs::path(dataDir) / "saves").string(); }
    std::string telemetryDir() const { return (fs::path(dataDir) / "telemetry").string(); }
};

// ── Date helpers (UTC, portable) ────────────────────────────────────
inline std::string utcDateStamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}

// ── Telemetry writer with caps + known-session gating ───────────────
// Thread-safe. Sessions must be registered (registerSession) before their
// events are accepted — this kills random-POST disk-fill. Per-day dirs.
class TelemetryWriter {
public:
    explicit TelemetryWriter(RuntimeConfig cfg) : cfg_(std::move(cfg)) {
        std::error_code ec;
        fs::create_directories(cfg_.telemetryDir(), ec);
    }

    // Register a session id seen at session_start / new game (in-memory set).
    void registerSession(const std::string& sessionId) {
        std::lock_guard lock(mtx_);
        knownSessions_.insert(sessionId);
    }
    bool isKnown(const std::string& sessionId) {
        std::lock_guard lock(mtx_);
        return knownSessions_.count(sessionId) > 0;
    }

    enum class Result { Ok, UnknownSession, FileCapped, QuotaExceeded, WriteError };

    // Append one or more JSONL lines for a session. `lines` are already-
    // serialized JSON strings (no trailing newline). Returns a status.
    Result append(const std::string& sessionId, const std::vector<std::string>& lines) {
        std::lock_guard lock(mtx_);
        if (knownSessions_.count(sessionId) == 0) return Result::UnknownSession;

        // Dir quota (refuse + log once when exceeded).
        if (dirSizeBytes() > cfg_.dirQuotaBytes) {
            if (!quotaWarned_) {
                spdlog::warn("Telemetry dir quota ({} GB) exceeded — refusing further writes",
                             cfg_.dirQuotaBytes / (1024.0 * 1024 * 1024));
                quotaWarned_ = true;
            }
            return Result::QuotaExceeded;
        }

        std::string dayDir = (fs::path(cfg_.telemetryDir()) / utcDateStamp()).string();
        std::error_code ec;
        fs::create_directories(dayDir, ec);
        std::string path = (fs::path(dayDir) / ("session_" + safeId(sessionId) + ".jsonl")).string();

        // Per-session file cap (drop + log once).
        std::uintmax_t existing = 0;
        if (fs::exists(path, ec)) existing = fs::file_size(path, ec);
        if (static_cast<long>(existing) > cfg_.sessionFileCapBytes) {
            if (cappedFiles_.insert(path).second) {
                spdlog::warn("Telemetry session file at cap ({} MB) — dropping further events: {}",
                             cfg_.sessionFileCapBytes / (1024 * 1024), path);
            }
            return Result::FileCapped;
        }

        bool isNew = !fs::exists(path, ec);
        std::ofstream f(path, std::ios::app);
        if (!f.is_open()) return Result::WriteError;
        if (isNew) {
            f << R"({"type":"meta","schema":3})" << "\n";
        }
        for (const auto& l : lines) f << l << "\n";
        return Result::Ok;
    }

    static std::string safeId(const std::string& id) {
        std::string s;
        for (char c : id)
            s += (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
        if (s.empty()) s = "anon";
        if (s.size() > 64) s.resize(64);
        return s;
    }

    const RuntimeConfig& config() const { return cfg_; }

    // Aggregate counts for /admin/stats: total bytes + per-day file counts.
    nlohmann::json statsJson() {
        std::lock_guard lock(mtx_);
        nlohmann::json perDay = nlohmann::json::object();
        std::error_code ec;
        std::uintmax_t total = 0;
        for (auto& day : fs::directory_iterator(cfg_.telemetryDir(), ec)) {
            if (!day.is_directory()) continue;
            int files = 0; std::uintmax_t bytes = 0;
            for (auto& f : fs::directory_iterator(day.path(), ec)) {
                if (f.is_regular_file()) { files++; bytes += f.file_size(ec); }
            }
            perDay[day.path().filename().string()] = {{"files", files}, {"bytes", bytes}};
            total += bytes;
        }
        return {{"knownSessions", knownSessions_.size()},
                {"totalBytes", total},
                {"perDay", perDay}};
    }

private:
    std::uintmax_t dirSizeBytes() {
        std::error_code ec;
        std::uintmax_t total = 0;
        for (auto& e : fs::recursive_directory_iterator(cfg_.telemetryDir(), ec)) {
            if (e.is_regular_file()) total += e.file_size(ec);
        }
        return total;
    }

    RuntimeConfig cfg_;
    std::mutex mtx_;
    std::unordered_set<std::string> knownSessions_;
    std::unordered_set<std::string> cappedFiles_;
    bool quotaWarned_ = false;
};

// ── Housekeeping: gzip old day-dirs, delete very old ones ───────────
// Portable. Runs ~daily on a background thread. Compression via zlib when
// available; deletion is always performed (the disk guarantee).
class Housekeeper {
public:
    Housekeeper(RuntimeConfig cfg) : cfg_(std::move(cfg)) {}
    ~Housekeeper() { stop(); }

    void start() {
        running_ = true;
        thread_ = std::thread([this] { loop(); });
    }
    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

private:
    void loop() {
        // Run once shortly after boot, then roughly once a day. Sleep in 1s
        // slices (not one 60s/86400s block) so stop()/join() returns promptly on
        // shutdown — a coarse sleep would make docker stop wait out the slice.
        long slept = 0;
        runOnce();
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (++slept >= 24 * 3600) { slept = 0; if (running_.load()) runOnce(); }
        }
    }

    void runOnce() {
        std::error_code ec;
        if (!fs::exists(cfg_.telemetryDir(), ec)) return;
        auto today = todayDays();
        for (auto& day : fs::directory_iterator(cfg_.telemetryDir(), ec)) {
            if (!day.is_directory()) continue;
            long ageDays = today - parseDateDays(day.path().filename().string());
            if (ageDays < 0) continue; // unparseable / future
            if (ageDays > cfg_.deleteOlderThanDays) {
                fs::remove_all(day.path(), ec);
                spdlog::info("Housekeeper: deleted telemetry dir > {} days: {}",
                             cfg_.deleteOlderThanDays, day.path().string());
                continue;
            }
            if (ageDays > cfg_.gzipOlderThanDays) {
                gzipDir(day.path());
            }
        }
    }

    void gzipDir(const fs::path& dir) {
        std::error_code ec;
        for (auto& f : fs::directory_iterator(dir, ec)) {
            if (!f.is_regular_file()) continue;
            if (f.path().extension() == ".gz") continue;
#ifdef STVG_HAVE_ZLIB
            std::string in = f.path().string();
            std::string out = in + ".gz";
            if (fs::exists(out, ec)) continue;
            if (gzipFile(in, out)) { fs::remove(in, ec); }
#else
            (void)f; // zlib absent: leave uncompressed (deletion still protects disk)
#endif
        }
    }

#ifdef STVG_HAVE_ZLIB
    static bool gzipFile(const std::string& in, const std::string& out) {
        std::ifstream src(in, std::ios::binary);
        if (!src.is_open()) return false;
        gzFile dst = gzopen(out.c_str(), "wb6");
        if (!dst) return false;
        char buf[1 << 16];
        while (src.read(buf, sizeof(buf)) || src.gcount() > 0) {
            int n = static_cast<int>(src.gcount());
            if (gzwrite(dst, buf, n) != n) { gzclose(dst); return false; }
        }
        gzclose(dst);
        return true;
    }
#endif

    static long todayDays() { return parseDateDays(utcDateStamp()); }

    // Days since epoch from a YYYY-MM-DD string (portable, via timegm-equiv).
    static long parseDateDays(const std::string& s) {
        int y = 0, m = 0, d = 0;
        if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return -1;
        std::tm tm{};
        tm.tm_year = y - 1900; tm.tm_mon = m - 1; tm.tm_mday = d;
        tm.tm_hour = 12; // noon to dodge DST/rounding
        std::time_t t;
#ifdef _WIN32
        t = _mkgmtime(&tm);
#else
        t = timegm(&tm);
#endif
        if (t == static_cast<std::time_t>(-1)) return -1;
        return static_cast<long>(t / 86400);
    }

    RuntimeConfig cfg_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

} // namespace stvg::api
