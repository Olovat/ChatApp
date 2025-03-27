cmake_minimum_required(VERSION 3.14)
project(ChatApp)

set(CMAKEWINDOWSEXPORTALLSYMBOLS ON)

# Включаем Google Test через FetchContent
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

# Подключаем Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Widgets)

# Указываем пути к заголовочным файлам
include_directories(${CMAKE_SOURCE_DIR}/server)
include_directories(${CMAKE_SOURCE_DIR}/client)

# Указываем файлы .ui
set(UI_FILES
    client/mainwindow.ui
    client/auth_window.ui
    client/reg_window.ui
    client/privatechatwindow.ui
)

# Обработка файлов .ui
qt6_wrap_ui(UI_HEADERS ${UI_FILES})

# Компилируем сервер
add_executable(server server/main.cpp server/server.cpp server/server.h)
target_link_libraries(server Qt6::Core Qt6::Network Qt6::Sql)

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
    ${UI_HEADERS}
)
target_link_libraries(client Qt6::Core Qt6::Network Qt6::Sql Qt6::Widgets)

# Компилируем тесты для сервера
add_executable(test_server tests/test_server/main.cpp server/server.cpp server/server.h)
target_link_libraries(test_server gtest gtest_main Qt6::Core Qt6::Network Qt6::Sql gmock gmock_main)

# Компилируем тесты для клиента
add_executable(test_client
    tests/test_client/main.cpp
    client/main.cpp
    client/mainwindow.cpp
    client/mainwindow.h
    client/auth_window.cpp
    client/auth_window.h
    client/reg_window.cpp
    client/reg_window.h
    client/privatechatwindow.cpp
    client/privatechatwindow.h
    ${UI_HEADERS}
)
target_link_libraries(test_client gtest gtest_main Qt6::Core Qt6::Network Qt6::Sql Qt6::Widgets gmock gmock_main)