# ════════════════════════════════════════════════════════════════════
# STVG — multi-stage container for the game.volcker.org public playtest
# (D1/D2 deployment). Build stage compiles a Release binary via CMake
# FetchContent and RUNS THE UNIT SUITE (the image only ships if tests pass);
# the runtime stage is a slim ubuntu:24.04 with just the binary + web assets
# + game data + curl for the healthcheck.
#
#   docker build -t stvg:local .
#   docker run --rm -p 8080:8080 -v stvg_data:/var/lib/stvg stvg:local
#
# Entrypoint: stvg_server 8080 /srv/static  (binds 0.0.0.0 in-container).
# ════════════════════════════════════════════════════════════════════

# ── Stage 1: build + test ───────────────────────────────────────────
FROM ubuntu:24.04 AS build

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy the engine sources (CMake pulls Crow/asio/spdlog/json/gtest via
# FetchContent at configure time, so a network-enabled build is required).
COPY CMakeLists.txt ./
COPY cmake/ ./cmake/
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/
COPY data/ ./data/
COPY static/ ./static/

# Configure Release with IPO, build the three executables.
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
    && cmake --build build --target stvg_tests stvg_server stvg_autoplay -j "$(nproc)"

# Gate: run the unit suite IN THE BUILD STAGE. The engine reads ./data at
# startup, so run from /src (where data/ lives). A non-zero exit fails the
# build and the image is never produced.
RUN cd /src && SPDLOG_LEVEL=off ./build/stvg_tests

# ── Stage 2: runtime ────────────────────────────────────────────────
FROM ubuntu:24.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive
# curl for HEALTHCHECK; zlib1g for the telemetry gzip housekeeper; ca-certs
# are harmless and tiny. --no-install-recommends keeps the layer lean.
RUN apt-get update && apt-get install -y --no-install-recommends \
        curl \
        zlib1g \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /srv

# Binary + web assets + game data the engine reads at startup
# (archetypes, ceos, events, calibration, deals, scenarios, config).
COPY --from=build /src/build/stvg_server /usr/local/bin/stvg_server
COPY --from=build /src/static/ /srv/static/
COPY --from=build /src/data/   /srv/data/

# Persistent saves + telemetry live here (mount a named volume in production).
ENV STVG_DATA_DIR=/var/lib/stvg
RUN mkdir -p /var/lib/stvg

EXPOSE 8080
STOPSIGNAL SIGTERM
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -fsS http://localhost:8080/healthz || exit 1

# stvg_server reads ./data relative to its working dir; /srv has data/ + static/.
WORKDIR /srv
ENTRYPOINT ["stvg_server", "8080", "/srv/static"]
