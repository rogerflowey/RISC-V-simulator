cmake_minimum_required(VERSION 3.13)

project(CPUsim)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(common_settings INTERFACE)

target_include_directories(common_settings INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_options(common_settings INTERFACE 
    -Wall 
    -Wextra
    -Wpedantic
)

add_executable(CPUsim main.cpp)
target_link_libraries(CPUsim PRIVATE common_settings)

add_executable(STD standard/main.cpp)

enable_testing()

file(GLOB test_sources CONFIGURE_DEPENDS "unittest/*.cpp")

# Loop through each discovered test file and create a test for it.
foreach(test_source ${test_sources})
    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name} ${test_source})

    target_link_libraries(${test_name} PRIVATE common_settings)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

