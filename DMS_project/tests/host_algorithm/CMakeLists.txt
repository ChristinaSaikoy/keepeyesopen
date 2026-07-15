cmake_minimum_required(VERSION 3.16)
project(dms_host_algorithm_tests C)

set(CMAKE_OBJECT_PATH_MAX 128)
enable_testing()

add_executable(host_algorithm_tests
    test_algorithm.c
    ../../esp32_firmware/esp32_s3_fw/main/perclos.c
)
target_compile_features(host_algorithm_tests PRIVATE c_std_11)
target_compile_options(host_algorithm_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(host_algorithm_tests PRIVATE ../../esp32_firmware/esp32_s3_fw/main)
if(NOT MSVC)
    target_link_libraries(host_algorithm_tests PRIVATE m)
endif()
add_test(NAME host_algorithm_tests COMMAND host_algorithm_tests)
