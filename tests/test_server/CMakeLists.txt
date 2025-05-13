cmake_minimum_required(VERSION 3.16)

project(test LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    message (FATAL_ERROR "No GTest Found")
endif()

add_test(NAME test COMMAND test)

target_link_libraries(test PRIVATE GTest::GTest)
if (GMock_FOUND)
    target_link_libraries(test INTERFACE GTest::GMock)
endif()


set(SERVER_SOURCES_DIR ${CMAKE_SOURCE_DIR}/server)
set(TEST_SERVER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_server
    main.cpp
    tst_authentication.cpp
    tst_registration.cpp
    tst_test.cpp
    ${TEST_SERVER_DIR}/MockDatabase.h
    ${TEST_SERVER_DIR}/MockNetworkClient.h
    ${TEST_SERVER_DIR}/MockNetworkServer.h
    ${SERVER_SOURCES_DIR}/ChatLogicServer.cpp
)

target_include_directories(test_server PRIVATE
    ${SERVER_SOURCES_DIR}
    ${TEST_SERVER_DIR}    
    ${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include
    ${CMAKE_BINARY_DIR}/_deps/googletest-src/googlemock/include
)
include(GoogleTest)
gtest_discover_tests(test_server)
