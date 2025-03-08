cmake_minimum_required(VERSION 3.14)
project(ChatApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Network Sql Widgets REQUIRED)

# Подпапка сервер тестов
add_subdirectory(server)

# Подпапка клиент тестов
add_subdirectory(client)

# Вызов тестов
enable_testing()

# Добавление подпапки тесты
add_subdirectory(tests)

