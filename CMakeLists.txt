cmake_minimum_required(VERSION 3.14)
project(ChatApp)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Включаем Google Test через FetchContent
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# При сборке под Windows с флагом /MD, Google Test требует флаг /MD
# Поэтому устанавливаем флаг gtest_force_shared_crt в ON
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Подключаем Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Widgets Test)

set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/client)

# Указываем пути к заголовочным файлам
include_directories(${CMAKE_SOURCE_DIR}/server)
include_directories(${CMAKE_SOURCE_DIR}/client)
include_directories(${CMAKE_SOURCE_DIR}/tests)
include_directories(${CMAKE_BINARY_DIR})

# Указываем файлы .ui
set(UI_FILES
    client/mainwindow.ui
    client/auth_window.ui
    client/reg_window.ui
    client/privatechatwindow.ui
    client/groupchatwindow.ui
    client/transitwindow.ui
)

# Обработка файлов .ui
qt6_wrap_ui(UI_HEADERS ${UI_FILES})

# Компилируем сервер
add_executable(server server/main.cpp server/server.cpp server/server.h)
target_link_libraries(server gtest gtest_main Qt6::Core Qt6::Network Qt6::Sql)

# Компилируем клиент
add_executable(client
    client/main.cpp
    client/mainwindow.cpp
    client/mainwindow.h
    client/auth_window.cpp
    client/auth_window.h
    client/reg_window.cpp
    client/reg_window.h
    client/privatechatwindow.cpp
    client/privatechatwindow.h
    client/transitwindow.cpp
    client/transitwindow.h
    client/groupchatwindow.h
    client/groupchatwindow.cpp
    ${UI_HEADERS}
)
target_link_libraries(client gtest gtest_main Qt6::Core Qt6::Network Qt6::Sql Qt6::Widgets Qt6::Test)

# Компилируем тесты для сервера
add_executable(test_server tests/test_server/main.cpp server/server.cpp server/server.h)
target_link_libraries(test_server gtest gtest_main Qt6::Core Qt6::Network Qt6::Sql gmock gmock_main)

# Компилируем тесты для клиента
    add_executable(test_client
    tests/test_client/main.cpp
    client/main.cpp
    client/mainwindow.cpp
    client/auth_window.cpp
    client/reg_window.cpp
    client/privatechatwindow.cpp
    client/transitwindow.cpp
    client/transitwindow.h
    client/groupchatwindow.h
    client/groupchatwindow.cpp
    ${UI_HEADERS}
    tests/test_client/MockDatabase.h
)
target_link_libraries(test_client gtest gtest_main Qt6::Core Qt6::Network Qt6::Sql Qt6::Widgets Qt6::Test gmock gmock_main)


# Добавляем определение для тестов
target_compile_definitions(test_client PRIVATE TESTING=1)

# Включаем тестирование
enable_testing()
add_test(NAME client_tests COMMAND test_client)

if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
endif()