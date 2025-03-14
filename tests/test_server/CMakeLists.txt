cmake_minimum_required(VERSION 3.16)

project(test_server LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt packages
find_package(Qt6 COMPONENTS Core Network Sql REQUIRED)

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

# Server source files
set(SERVER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../server")
file(GLOB SERVER_SOURCES "${SERVER_SRC_DIR}/*.cpp")
file(GLOB SERVER_HEADERS "${SERVER_SRC_DIR}/*.h")
# Remove server's main.cpp as we have our own test main
list(FILTER SERVER_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

# Add test files
set(TEST_SOURCES
    main.cpp
    tst_registration.cpp
    tst_authentication.cpp
)

add_executable(test_server ${TEST_SOURCES} ${SERVER_SOURCES})

target_include_directories(test_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${SERVER_SRC_DIR}
)

target_link_libraries(test_server PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    GTest::GTest
    GTest::Main
    GTest::gmock
    GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(test_server)
