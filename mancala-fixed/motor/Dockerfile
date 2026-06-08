# ── Build stage ───────────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git libomp-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel $(nproc)

# ── Runtime stage ─────────────────────────────────────────────────────────────
FROM ubuntu:24.04

# libgomp1 = OpenMP runtime (gcc), wget = needed by healthcheck
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgomp1 wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/build/mancala_server /app/mancala_server
COPY --from=builder /src/build/mancala_bench  /app/mancala_bench
COPY --from=builder /src/build/mancala_tests  /app/mancala_tests
COPY tests/ /app/tests/
COPY bench/ /app/bench/

# Validate all tests pass at build time
RUN /app/mancala_tests

EXPOSE 8001
ENV OMP_NUM_THREADS=4

ENTRYPOINT ["/app/mancala_server"]
CMD ["--port", "8001"]
