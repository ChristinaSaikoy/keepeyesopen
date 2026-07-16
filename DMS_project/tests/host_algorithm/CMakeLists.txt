cmake_minimum_required(VERSION 3.16)
project(host_algorithm C)

enable_testing()
add_executable(host_algorithm_tests
    test_algorithm.c
    ../../esp32_firmware/esp32_s3_fw/main/perclos.c
    ../../esp32_firmware/esp32_s3_fw/main/dms_decision.c)
target_include_directories(host_algorithm_tests PRIVATE ../../esp32_firmware/esp32_s3_fw/main)
target_compile_options(host_algorithm_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME host_algorithm_tests COMMAND host_algorithm_tests)
