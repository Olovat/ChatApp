cmake_minimum_required(VERSION 3.16)

project(client_tests LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt packages
find_package(Qt6 COMPONENTS Core Network Sql Widgets Test REQUIRED)

# Find GTest
if(DEFINED ENV{GOOGLETEST_DIR})
    set(GOOGLETEST_DIR $ENV{GOOGLETEST_DIR})
elseif(EXISTS "/usr/src/googletest")
    set(GOOGLETEST_DIR "/usr/src/googletest")
else()
    message(STATUS "Using FetchContent to get GoogleTest")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
endif()

if(NOT TARGET GTest::gtest)
    find_package(GTest REQUIRED)
endif()

# Add client sources - excluding main.cpp to avoid multiple main functions
set(CLIENT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../client")
file(GLOB CLIENT_SOURCES "${CLIENT_SRC_DIR}/*.cpp")
file(GLOB CLIENT_HEADERS "${CLIENT_SRC_DIR}/*.h")
file(GLOB CLIENT_FORMS "${CLIENT_SRC_DIR}/*.ui")

# Remove client's main.cpp as we have our own test main
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

# Add test sources
file(GLOB TEST_SOURCES "*.cpp")

# Create test executable
add_executable(client_tests ${TEST_SOURCES} ${CLIENT_SOURCES} ${CLIENT_FORMS})

target_include_directories(client_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CLIENT_SRC_DIR})
target_link_libraries(client_tests PRIVATE 
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

# Add tests
include(GoogleTest)
gtest_discover_tests(client_tests)
