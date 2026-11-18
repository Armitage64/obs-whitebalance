# Standalone test project for the OBS-independent math.
# cmake -S tests -B build_tests && cmake --build build_tests && ctest --test-dir build_tests
cmake_minimum_required(VERSION 3.20)
project(obs_whitebalance_tests C)

enable_testing()

add_executable(test_wb_math test_wb_math.c ../src/wb-math.c)
target_include_directories(test_wb_math PRIVATE ../src)
if(NOT MSVC)
  target_link_libraries(test_wb_math PRIVATE m)
  target_compile_options(test_wb_math PRIVATE -Wall -Wextra)
endif()

add_test(NAME wb_math COMMAND test_wb_math)
