cmake_minimum_required(VERSION 3.10)

project(CPUsim)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

add_executable(CPUsim main.cpp)

# Add logger tests
add_executable(logger_tests unittest/logger.cpp)
target_include_directories(logger_tests PRIVATE include)