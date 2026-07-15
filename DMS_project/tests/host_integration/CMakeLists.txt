cmake_minimum_required(VERSION 3.16)
project(dms_host_integration_tests C)

set(CMAKE_OBJECT_PATH_MAX 128)
enable_testing()

add_executable(host_integration_tests
    test_integration.c
    ../../esp32_firmware/common/dms_uart_protocol.c
    ../../esp32_firmware/esp32_s3_fw/main/perclos.c
    ../../esp32_firmware/esp32_s3_fw/main/dms_s3_pipeline.c
)
target_compile_features(host_integration_tests PRIVATE c_std_11)
target_compile_options(host_integration_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(host_integration_tests PRIVATE
    ../../esp32_firmware/common
    ../../esp32_firmware/esp32_s3_fw/main
)
if(NOT MSVC)
    target_link_libraries(host_integration_tests PRIVATE m)
endif()
add_test(NAME host_integration_tests COMMAND host_integration_tests)
