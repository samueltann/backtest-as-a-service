cmake_minimum_required(VERSION 3.20)
project(backtest_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(bt_core
  src/data_handler.cpp
  src/strategies.cpp
  src/portfolio.cpp
  src/execution.cpp
  src/metrics.cpp
  src/engine.cpp
)
target_include_directories(bt_core PUBLIC include third_party)
target_compile_options(bt_core PRIVATE -Wall -Wextra)

add_executable(backtest-engine src/main.cpp)
target_link_libraries(backtest-engine PRIVATE bt_core)

# Tests (Catch2 v3, vendored amalgamated build)
enable_testing()
add_library(catch2 STATIC third_party/catch2/catch_amalgamated.cpp)
target_include_directories(catch2 PUBLIC third_party)

add_executable(bt_tests
  tests/test_indicators.cpp
  tests/test_engine.cpp
  tests/test_metrics.cpp
)
target_link_libraries(bt_tests PRIVATE bt_core catch2)
add_test(NAME bt_tests COMMAND bt_tests)
