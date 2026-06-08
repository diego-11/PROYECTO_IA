cmake_minimum_required(VERSION 3.16)
project(mancala_motor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release build by default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

# ── OpenMP ────────────────────────────────────────────────────────────────────
find_package(OpenMP REQUIRED)

# ── nlohmann/json (header-only, fetched automatically) ───────────────────────
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

include_directories(src)

# ── Server binary ─────────────────────────────────────────────────────────────
add_executable(mancala_server src/server.cpp)
target_link_libraries(mancala_server PRIVATE OpenMP::OpenMP_CXX nlohmann_json::nlohmann_json)

# ── Benchmark binary ─────────────────────────────────────────────────────────
add_executable(mancala_bench bench/benchmark.cpp)
target_link_libraries(mancala_bench PRIVATE OpenMP::OpenMP_CXX)

# ── Test binary ───────────────────────────────────────────────────────────────
add_executable(mancala_tests tests/test_main.cpp)
target_link_libraries(mancala_tests PRIVATE OpenMP::OpenMP_CXX)

enable_testing()
add_test(NAME unit_tests COMMAND mancala_tests)
