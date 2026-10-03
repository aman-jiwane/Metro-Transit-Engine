cmake_minimum_required(VERSION 3.14)

project(MetroTransitSystem LANGUAGES C CXX)

# Set C++ Standard to C++17 (Required for std::shared_mutex)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# SQLite
# ---------------------------------------------------------------------------
FetchContent_Declare(
    sqlite3_amalgamation
    URL https://sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
)
FetchContent_MakeAvailable(sqlite3_amalgamation)

add_library(sqlite3 STATIC ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
if(UNIX)
    target_link_libraries(sqlite3 PUBLIC ${CMAKE_DL_LIBS} Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# metro_core: Shared logic
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ALL_SOURCES "src/*.cpp")
list(REMOVE_ITEM ALL_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
list(REMOVE_ITEM ALL_SOURCES "${CMAKE_SOURCE_DIR}/src/api/api_main.cpp")

add_library(metro_core STATIC ${ALL_SOURCES})
target_include_directories(metro_core PUBLIC include)
target_link_libraries(metro_core PUBLIC Threads::Threads sqlite3)

# ---------------------------------------------------------------------------
# Main application executable (CLI)
# ---------------------------------------------------------------------------
add_executable(MetroTransitSystem src/main.cpp)
target_link_libraries(MetroTransitSystem PRIVATE metro_core)

# ---------------------------------------------------------------------------
# REST API server (Phase 6) Dependencies
# ---------------------------------------------------------------------------
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.51.0
)
FetchContent_GetProperties(httplib)
if(NOT httplib_POPULATED)
    FetchContent_Populate(httplib)
endif()

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# REST API Executable
# ---------------------------------------------------------------------------
add_executable(MetroApiServer src/api/api_main.cpp)
target_include_directories(MetroApiServer PRIVATE ${httplib_SOURCE_DIR})
target_link_libraries(MetroApiServer PRIVATE metro_core nlohmann_json::nlohmann_json)

# Explicitly link Windows Socket libraries and enforce static runtime binding for MinGW
if(WIN32)
    target_link_libraries(MetroApiServer PRIVATE ws2_32 mswsock)
    target_link_options(MetroApiServer PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------
# Tests (Catch2)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.15.1
)
FetchContent_MakeAvailable(Catch2)

file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
add_executable(MetroTests ${TEST_SOURCES})
target_include_directories(MetroTests PRIVATE include tests)
target_link_libraries(MetroTests PRIVATE metro_core Catch2::Catch2WithMain)

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(MetroTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})