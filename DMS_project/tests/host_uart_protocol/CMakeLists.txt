cmake_minimum_required(VERSION 3.16)
project(dms_host_uart_protocol_tests C)

set(CMAKE_OBJECT_PATH_MAX 128)

enable_testing()

add_executable(host_uart_protocol_tests
    test_uart_protocol.c
    ../../esp32_firmware/common/dms_uart_protocol.c
)

target_compile_features(host_uart_protocol_tests PRIVATE c_std_11)
target_compile_options(host_uart_protocol_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(host_uart_protocol_tests PRIVATE ../../esp32_firmware/common)

add_test(NAME host_uart_protocol_tests COMMAND host_uart_protocol_tests)
