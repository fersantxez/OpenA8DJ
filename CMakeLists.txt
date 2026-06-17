cmake_minimum_required(VERSION 3.20)

project(OpenA8DJCppDriverKit
  VERSION 0.1.0
  DESCRIPTION "C++/DriverKit redesign line for Native Instruments Audio 8 DJ"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(opena8djcpp_core
  core/src/audio_model.cpp
  core/src/mode2_packet.cpp
  core/src/input_decode.cpp
  core/src/input_profile.cpp
  core/src/routing.cpp
  core/src/policies.cpp
  core/src/metrics.cpp
  core/src/prepared_transport.cpp
  core/src/timecode.cpp
  core/src/timecode_analysis.cpp
  core/src/driverkit_model.cpp)

target_include_directories(opena8djcpp_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include)

target_compile_options(opena8djcpp_core
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion)

add_library(opena8djcpp_driverkit_shell
  driverkit/src/audio_driver_skeleton.cpp)

target_include_directories(opena8djcpp_driverkit_shell
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/driverkit/include)

target_link_libraries(opena8djcpp_driverkit_shell
  PUBLIC
    opena8djcpp_core)

target_compile_options(opena8djcpp_driverkit_shell
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion)

add_executable(opena8djcpp_core_tests
  core/tests/core_contract_tests.cpp)

target_link_libraries(opena8djcpp_core_tests
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_offline_bench
  tools/offline_bench.cpp)

target_link_libraries(opena8djcpp_offline_bench
  PRIVATE
    opena8djcpp_core)

target_compile_options(opena8djcpp_offline_bench
  PRIVATE
    -O3)

add_executable(opena8djcpp_packet_matrix
  tools/packet_matrix.cpp)

target_link_libraries(opena8djcpp_packet_matrix
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_mode2_mainline_layout_parity
  tools/mode2_mainline_layout_parity.cpp)

target_link_libraries(opena8djcpp_mode2_mainline_layout_parity
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_mode2_cpp_oracle_dump
  tools/mode2_cpp_oracle_dump.cpp)

target_link_libraries(opena8djcpp_mode2_cpp_oracle_dump
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_protocol_contract
  tools/protocol_contract.cpp)

target_link_libraries(opena8djcpp_protocol_contract
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_simulated_output_matrix
  tools/simulated_output_matrix.cpp)

target_link_libraries(opena8djcpp_simulated_output_matrix
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_timecode_matrix
  tools/timecode_matrix.cpp)

target_link_libraries(opena8djcpp_timecode_matrix
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_timecode_signal_analysis
  tools/timecode_signal_analysis.cpp)

target_link_libraries(opena8djcpp_timecode_signal_analysis
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_realtime_audit
  tools/realtime_audit.cpp)

target_link_libraries(opena8djcpp_realtime_audit
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_driverkit_surface_model
  tools/driverkit_surface_model.cpp)

target_link_libraries(opena8djcpp_driverkit_surface_model
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_driverkit_shell_contract
  tools/driverkit_shell_contract.cpp)

target_link_libraries(opena8djcpp_driverkit_shell_contract
  PRIVATE
    opena8djcpp_driverkit_shell)

add_executable(opena8djcpp_driverkit_prepared_transport_contract
  tools/driverkit_prepared_transport_contract.cpp)

target_link_libraries(opena8djcpp_driverkit_prepared_transport_contract
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_dvs_signal_smoke
  tools/dvs_signal_smoke.cpp)

target_link_libraries(opena8djcpp_dvs_signal_smoke
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_dvs_packet_input_decode
  tools/dvs_packet_input_decode.cpp)

target_link_libraries(opena8djcpp_dvs_packet_input_decode
  PRIVATE
    opena8djcpp_core)

add_executable(opena8djcpp_jitter_model
  tools/jitter_model.cpp)

add_executable(opena8djcpp_transfer_pool_model
  tools/transfer_pool_model.cpp)

add_executable(opena8djcpp_transport_budget_model
  tools/transport_budget_model.cpp)

add_executable(opena8djcpp_static_policy_check
  tools/static_policy_check.cpp)

add_executable(opena8djcpp_hardware_lock_policy_check
  tools/hardware_lock_policy_check.cpp)

add_executable(opena8djcpp_packed_usb_audio_probe
  tools/packed_usb_audio_probe.cpp)

add_executable(opena8djcpp_evidence_schema_check
  tools/evidence_schema_check.cpp)

enable_testing()
add_test(NAME opena8djcpp_core_contract COMMAND opena8djcpp_core_tests)
add_test(NAME opena8djcpp_packet_matrix COMMAND opena8djcpp_packet_matrix)
add_test(NAME opena8djcpp_mode2_mainline_layout_parity COMMAND opena8djcpp_mode2_mainline_layout_parity)
add_test(NAME opena8djcpp_mode2_cpp_oracle_dump COMMAND opena8djcpp_mode2_cpp_oracle_dump)
add_test(NAME opena8djcpp_protocol_contract COMMAND opena8djcpp_protocol_contract)
add_test(NAME opena8djcpp_simulated_output_matrix COMMAND opena8djcpp_simulated_output_matrix)
add_test(NAME opena8djcpp_timecode_matrix COMMAND opena8djcpp_timecode_matrix)
add_test(NAME opena8djcpp_timecode_signal_analysis COMMAND opena8djcpp_timecode_signal_analysis)
add_test(NAME opena8djcpp_realtime_audit COMMAND opena8djcpp_realtime_audit)
add_test(NAME opena8djcpp_driverkit_surface_model COMMAND opena8djcpp_driverkit_surface_model)
add_test(NAME opena8djcpp_driverkit_shell_contract COMMAND opena8djcpp_driverkit_shell_contract)
add_test(NAME opena8djcpp_driverkit_prepared_transport_contract COMMAND opena8djcpp_driverkit_prepared_transport_contract)
add_test(NAME opena8djcpp_dvs_signal_smoke COMMAND opena8djcpp_dvs_signal_smoke)
add_test(NAME opena8djcpp_dvs_packet_input_decode COMMAND opena8djcpp_dvs_packet_input_decode)
add_test(NAME opena8djcpp_jitter_model COMMAND opena8djcpp_jitter_model)
add_test(NAME opena8djcpp_transfer_pool_model COMMAND opena8djcpp_transfer_pool_model)
add_test(NAME opena8djcpp_transport_budget_model COMMAND opena8djcpp_transport_budget_model)
add_test(NAME opena8djcpp_static_policy_check COMMAND opena8djcpp_static_policy_check)
add_test(NAME opena8djcpp_hardware_lock_policy_check COMMAND opena8djcpp_hardware_lock_policy_check)

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  add_test(NAME opena8djcpp_offline_bench COMMAND opena8djcpp_offline_bench)
endif()
