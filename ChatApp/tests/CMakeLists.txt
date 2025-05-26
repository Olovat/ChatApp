
cmake_minimum_required(VERSION 3.16)
project(chat_app_tests)

# Add the server tests
add_subdirectory(test_server)

# Add the client tests
add_subdirectory(test_client)