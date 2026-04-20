cmake_minimum_required(VERSION 3.20)
project(cardputer_atari800_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# All host tests live here. Each test is a standalone executable for easy
# isolation — ctest runs them and reports failures individually.
enable_testing()

add_executable(test_sanity test_sanity.c)
add_test(NAME sanity COMMAND test_sanity)
