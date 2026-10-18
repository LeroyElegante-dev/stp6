cmake_minimum_required(VERSION 3.20)
project(DataConverter VERSION 1.0.0 LANGUAGES CXX)

# Убирает предупреждение CMP0135 при распаковке скачанных архивов
cmake_policy(SET CMP0135 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DATACONV_BUILD_TESTS "Build unit tests" ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# nlohmann/json — локальный заголовок, ничего не качаем
# ---------------------------------------------------------------------------
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

# ---------------------------------------------------------------------------
# yaml-cpp 0.9.0 (в 0.8.0 не хватало #include <cstdint> для GCC 15+)
# ---------------------------------------------------------------------------
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(yaml-cpp
  URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.9.0.tar.gz)
FetchContent_MakeAvailable(yaml-cpp)

# ---------------------------------------------------------------------------
# GoogleTest
# ---------------------------------------------------------------------------
if(DATACONV_BUILD_TESTS)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz)
  FetchContent_MakeAvailable(googletest)
endif()

# ---------------------------------------------------------------------------
# Проект
# ---------------------------------------------------------------------------
add_library(dataconverter src/converter.cpp)
target_include_directories(dataconverter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(dataconverter PUBLIC
  nlohmann_json::nlohmann_json
  yaml-cpp::yaml-cpp)

add_executable(dataconv src/main.cpp)
target_link_libraries(dataconv PRIVATE dataconverter)

if(DATACONV_BUILD_TESTS)
  enable_testing()
  add_executable(dataconv_tests tests/converter_tests.cpp)
  target_link_libraries(dataconv_tests PRIVATE dataconverter GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(dataconv_tests)
endif()