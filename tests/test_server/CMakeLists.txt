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
if ($ENV{GOOGLETEST_DIR})
    get_filename_component(_GTEST_BASE $ENV{GOOGLETEST_DIR} REALPATH)
elseif (EXISTS "D:/Qt_project/googletest-main") # set by QC
    get_filename_component(_GTEST_BASE "D:/Qt_project/googletest-main" REALPATH) # set by QC
endif()

if (NOT GTEST_ROOT)
    if (_GTEST_BASE)
        message("Setting GTEST_ROOT to ${_GTEST_BASE}")
        set(GTEST_ROOT ${_GTEST_BASE})
    else()
        message("No GTEST_ROOT specified - using system defaults.")
    endif()
endif()

find_package(GTest REQUIRED)
if (NOT GTest_FOUND)
    message(FATAL_ERROR "No GTest Found")
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
