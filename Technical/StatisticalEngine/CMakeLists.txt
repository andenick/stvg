cmake_minimum_required(VERSION 3.20)
project(STVGStatisticalEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include cmake modules
include(cmake/CompilerOptions.cmake)
include(cmake/Dependencies.cmake)

# ── Core library (all engine code) ──────────────────────────────────
# Explicit source list — add new .cpp files here when created.
# Header-only engine uses placeholder.cpp to satisfy the static lib requirement.
set(ENGINE_SOURCES
    src/core/placeholder.cpp
    src/game/DecisionHandler.cpp
    src/game/CEOAbilityHandler.cpp
    src/game/CrisisHandler.cpp
    src/game/AnnualReportBuilder.cpp
)

add_library(stvg_engine STATIC ${ENGINE_SOURCES})
target_include_directories(stvg_engine PUBLIC include)
target_link_libraries(stvg_engine PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog)
target_compile_features(stvg_engine PUBLIC cxx_std_20)

# ── Server executable ───────────────────────────────────────────────
add_executable(stvg_server src/main.cpp)
target_link_libraries(stvg_server PRIVATE stvg_engine Crow::Crow)

# Optional zlib: when present, the telemetry housekeeper gzips old day-dirs
# (STVG_HAVE_ZLIB). Absent (e.g. a stock MSVC dev box), housekeeping still does
# age-based deletion — the disk-protection guarantee — and skips compression.
# This keeps Windows building without a zlib dependency while the Linux
# container (ubuntu:24.04 + zlib1g-dev) gets real gzip.
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(stvg_server PRIVATE ZLIB::ZLIB)
    target_compile_definitions(stvg_server PRIVATE STVG_HAVE_ZLIB)
    message(STATUS "zlib found — telemetry gzip housekeeping enabled")
else()
    message(STATUS "zlib not found — telemetry housekeeping will delete-only (no gzip)")
endif()

if(WIN32)
    target_link_libraries(stvg_server PRIVATE ws2_32 wsock32)
endif()

# ── Autoplay game-testing executable (no Crow dependency) ──────────
add_executable(stvg_autoplay src/autoplay_main.cpp)
target_link_libraries(stvg_autoplay PRIVATE stvg_engine)

# ── Tests ───────────────────────────────────────────────────────────
option(STVG_BUILD_TESTS "Build tests" ON)
if(STVG_BUILD_TESTS)
    enable_testing()
    file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)
    add_executable(stvg_tests ${TEST_SOURCES})
    target_link_libraries(stvg_tests PRIVATE stvg_engine GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(stvg_tests DISCOVERY_TIMEOUT 120)
endif()
