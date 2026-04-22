cmake_minimum_required(VERSION 3.20)
project(cardputer_atari800_tests C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable warnings for every test executable. Locked in now so the pattern
# sticks as new tests are added in M2+.
add_compile_options(-Wall -Wextra -Wpedantic)

# All host tests live here. Each test is a standalone executable for easy
# isolation — ctest runs them and reports failures individually.
enable_testing()

add_executable(test_sanity test_sanity.c)
add_test(NAME sanity COMMAND test_sanity)

add_executable(test_palette test_palette.c ../src/display/palette.cpp)
set_target_properties(test_palette PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME palette COMMAND test_palette)

add_executable(test_projector test_projector.c ../src/display/projector.cpp)
set_target_properties(test_projector PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME projector COMMAND test_projector)

add_executable(test_loader test_loader.c ../src/storage/loader.cpp)
set_target_properties(test_loader PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME loader COMMAND test_loader)

add_executable(test_keymap test_keymap.c ../src/input/keymap.c)
add_test(NAME keymap COMMAND test_keymap)

add_executable(test_joystick test_joystick.c ../src/input/joystick.c)
add_test(NAME joystick COMMAND test_joystick)

add_executable(test_mode test_mode.c ../src/input/mode.c)
add_test(NAME mode COMMAND test_mode)

add_executable(test_pokey_glue test_pokey_glue.c ../src/audio/pokey_glue.c)
add_test(NAME pokey_glue COMMAND test_pokey_glue)
