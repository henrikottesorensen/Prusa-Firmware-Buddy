# Common dependencies for all tests
link_libraries(SG14 freertos_tests buddy_utils logging-mock)

# Everything connect_tests compiles except its own Catch2 test files - shared with the rig target
# (connect_rig) below so the two cannot drift on the firmware source list.
set(CONNECT_SHARED_SOURCES
  ${CMAKE_CURRENT_BINARY_DIR}/http_resp_automaton.cpp
  ${CMAKE_SOURCE_DIR}/src/common/automata/core.cpp
  ${CMAKE_SOURCE_DIR}/src/common/directory.cpp
  ${CMAKE_SOURCE_DIR}/src/common/crc32.cpp
  ${CMAKE_SOURCE_DIR}/src/common/error_code_mangle.cpp
  ${CMAKE_SOURCE_DIR}/src/common/json_encode.cpp
  ${CMAKE_SOURCE_DIR}/src/common/segmented_json.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filepath_operation.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filename_type.cpp
  ${CMAKE_SOURCE_DIR}/src/common/printer_model.cpp
  ${CMAKE_SOURCE_DIR}/src/common/gcode/gcode_reader_any.cpp
  ${CMAKE_SOURCE_DIR}/src/common/gcode/gcode_reader_binary.cpp
  ${CMAKE_SOURCE_DIR}/src/common/gcode/gcode_reader_interface.cpp
  ${CMAKE_SOURCE_DIR}/src/common/gcode/gcode_reader_plaintext.cpp
  ${CMAKE_SOURCE_DIR}/src/common/gcode/gcode_buffer.cpp
  ${CMAKE_SOURCE_DIR}/src/common/gcode/meatpack.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/resp_parser.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/httpc.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/socket.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/connection.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/socket_connection_factory.cpp
  ${CMAKE_SOURCE_DIR}/src/common/stat_retry.cpp
  ${CMAKE_SOURCE_DIR}/src/common/random_sw.cpp
  ${CMAKE_SOURCE_DIR}/src/common/general_response.cpp
  ${CMAKE_SOURCE_DIR}/src/common/path_utils.cpp
  ${CMAKE_SOURCE_DIR}/src/common/e2ee/e2ee.cpp
  ${CMAKE_SOURCE_DIR}/src/common/e2ee/sha256_multiuse.cpp
  ${CMAKE_SOURCE_DIR}/src/common/e2ee/movable_aes_context.cpp
  ${CMAKE_SOURCE_DIR}/src/common/e2ee/key.cpp
  ${CMAKE_SOURCE_DIR}/src/common/e2ee/decryptor.cpp
  ${CMAKE_SOURCE_DIR}/src/common/e2ee/utils.cpp
  ${CMAKE_SOURCE_DIR}/src/common/e2ee/hmac.cpp
  ${CMAKE_SOURCE_DIR}/src/common/crash_dump/secret.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/background.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/render.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/command.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/planner.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/printer.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/hostname.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/sleep.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/decrypt.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/download.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/monitor.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/files.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/changed_path.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/transfer.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/transfer_file_check.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/transfer_recovery.cpp
  ${CMAKE_SOURCE_DIR}/src/common/str_utils.cpp
  ${CMAKE_SOURCE_DIR}/lib/heatshrink/heatshrink_decoder.c
  missing_functions.cpp
  partial_file_mock.cpp
  gui_media_events_mock.cpp
  xbuddy_extension_mock.cpp
  side_strip_control_mock.cpp
  cancel_object_mock.cpp
  ${CMAKE_SOURCE_DIR}/tests/stubs/async_job/async_job.cpp
  ${CMAKE_SOURCE_DIR}/tests/unit/common/printer_state_mock.cpp
  ${CMAKE_SOURCE_DIR}/tests/stubs/jsmn_impl.c
  ${CMAKE_SOURCE_DIR}/tests/stubs/strlcpy.c
  ${CMAKE_SOURCE_DIR}/tests/stubs/printer_type.cpp
  ${CMAKE_SOURCE_DIR}/tests/unit/mock/bsod.cpp
  # LwIP parts
  # See reason why we do it this way in
  # tests/unit/lib/WUI/nhttp/CMakeLists.txt
  ${CMAKE_SOURCE_DIR}/tests/unit/lib/WUI/nhttp/missing_functions.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/altcp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/altcp_tcp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/def.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/pbuf.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ip.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_addr.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/icmp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/etharp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_frag.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/udp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/tcp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/dns.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/tcp_in.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/tcp_out.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/timeouts.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/memp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/netif.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/netif/ethernet.c
  ${CMAKE_SOURCE_DIR}/src/feature/cork/tracker.cpp
  )

add_executable(
  connect_tests
  ${CONNECT_SHARED_SOURCES}
  # This target's own Catch2 test files, plus the frozen-clock stub they rely on.
  missing_functions_time.cpp
  command.cpp
  render.cpp
  hostname.cpp
  printer.cpp
  )
target_include_directories(
  connect_tests
  PRIVATE .
          include
          ${CMAKE_CURRENT_SOURCE_DIR}/mock
          ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_SOURCE_DIR}/tests/stubs # must be before ${CMAKE_SOURCE_DIR}/include
          ${CMAKE_BINARY_DIR}/include
          ${CMAKE_SOURCE_DIR}/include
          ${CMAKE_SOURCE_DIR}/src
          ${CMAKE_SOURCE_DIR}/src/lang
          ${CMAKE_SOURCE_DIR}/src/connect
          ${CMAKE_SOURCE_DIR}/src/common
          ${CMAKE_SOURCE_DIR}/src/common/http
          ${CMAKE_SOURCE_DIR}/src/common/gcode
          ${CMAKE_SOURCE_DIR}/src/common/utils
          ${CMAKE_SOURCE_DIR}/src/common/marlin_server_types
          ${CMAKE_SOURCE_DIR}/src/persistent_stores
          ${CMAKE_SOURCE_DIR}/lib/jsmn
          ${CMAKE_SOURCE_DIR}/lib/WUI
          ${CMAKE_SOURCE_DIR}/lib/heatshrink
          ${CMAKE_SOURCE_DIR}/src/gui
          ${CMAKE_SOURCE_DIR}/include/usb_host
          ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/include
          ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/system
          ${CMAKE_SOURCE_DIR}/lib/libbgcode/src/LibBGCode
          ${CMAKE_SOURCE_DIR}/lib/Marlin/Marlin/src/
          ${CMAKE_BINARY_DIR}/lib/libbgcode/src/
  )
target_link_libraries(
  connect_tests
  mbedTLS
  bgcode_core
  raii
  xbuddy_extension
  otp
  random
  random_sw
  base64
  )

file(GLOB AUTOMATA_GENERATORS ${PROJECT_ROOT_DIR}/utils/gen-automata/*.py)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/http_resp_automaton.cpp
         ${CMAKE_CURRENT_BINARY_DIR}/http_resp_automaton.h
  DEPENDS ${AUTOMATA_GENERATORS}
  COMMAND ${Python3_EXECUTABLE} ${PROJECT_ROOT_DIR}/utils/gen-automata/http_client.py
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Generating http client automata"
  )
add_custom_target(
  generate-httpc-automata-tests-connect DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/http_resp_automaton.cpp
                                                ${CMAKE_CURRENT_BINARY_DIR}/http_resp_automaton.h
  )
add_dependencies(connect_tests generate-httpc-automata-tests-connect)

# --- connect rig -------------------------------------------------------------------------------
# The real Connect client as a runnable binary, talking to a Connect server over real TCP. Adds
# the three firmware files connect_tests does not compile - the top-level loop, the websocket
# framing and the buffered connection - plus the rig's own entry point, Printer implementation and
# a REAL clock (missing_functions_time.cpp freezes time at 0, which would stop the planner ever
# scheduling telemetry and would spin the idle loop).
# The rig brings its own PartialFile, so the stub one must not also be linked in.
# REMOVE_ITEM on a copy, leaving CONNECT_SHARED_SOURCES intact for the other two targets.
set(CONNECT_RIG_SOURCES ${CONNECT_SHARED_SOURCES})
list(REMOVE_ITEM CONNECT_RIG_SOURCES partial_file_mock.cpp)

add_executable(
  connect_rig
  ${CONNECT_RIG_SOURCES}
  ${CMAKE_SOURCE_DIR}/src/connect/connect.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/connection_cache.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/registrator.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/status.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/json_out.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/connect_error.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/websocket.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/buffered_conn.cpp
  ${CMAKE_SOURCE_DIR}/src/common/utils/string_builder.cpp
  rig/rig_main.cpp
  rig/rig_time.cpp
  # A real PartialFile, backed by an ordinary file. partial_file_mock.cpp (in
  # CONNECT_SHARED_SOURCES) returns "not implemented" from create/open, which makes
  # Transfer::begin refuse every download before any byte moves - so the rig cannot exercise
  # transfers at all without this. Its symbols replace the mock's at link time.
  rig/rig_partial_file.cpp
  # PartialFile's sector pool holds a CountingSemaphore, which the freertos_tests object library
  # does not build - nothing needed it until the rig constructed a PartialFile for real. Same
  # firmware source, against the same FreeRTOS stubs the rest of the target already uses.
  ${CMAKE_SOURCE_DIR}/src/freertos/counting_semaphore.cpp
  # The real TLS stack, replacing the rig_tls.cpp stub that used to refuse every connection. Both
  # files are firmware sources built unmodified; net_sockets.cpp does not touch lwIP despite its
  # includes, because it delegates every operation to http::socket_con, which already builds here.
  ${CMAKE_SOURCE_DIR}/src/connect/tls/tls.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/tls/net_sockets.cpp
  # Two small helpers tls.cpp calls that nothing else in this target needed. Real firmware sources
  # rather than stubs: proxy.cpp is the HTTP CONNECT path a proxied config uses, and conserve_cpu is
  # a singleton the TLS handshake consults to yield while the maths runs.
  ${CMAKE_SOURCE_DIR}/src/common/http/proxy.cpp
  ${CMAKE_SOURCE_DIR}/src/common/conserve_cpu.cpp
  # A real entropy source. The stub below returns 42 and fills nothing, which makes seeding fail and
  # tls.cpp give up before it opens a socket - see the file's own comment.
  rig/rig_entropy.cpp
  )
# tests/unit/lib/WUI/nhttp/missing_functions.c stubs mkdir() as `return 0` - harmless for tests that
# never touch a filesystem, fatal for the rig: Transfer::begin believes it created the transfer
# directory and then fails opening the backup file inside it ("Failed to create backup file").
# Renaming the stub's symbol for this target only lets the real mkdir link from libc. The file is
# left alone, so connect_tests and the nhttp suite keep the stub they expect.
#
# mbedtls_hardware_poll is renamed for the same reason and by the same trick: its stub returns 42
# with the buffer untouched, so a DRBG seeded from it fails and TLS never reaches the socket.
# rig/rig_entropy.cpp provides the real one.
set_source_files_properties(
  ${CMAKE_SOURCE_DIR}/tests/unit/lib/WUI/nhttp/missing_functions.c
  TARGET_DIRECTORY connect_rig
  PROPERTIES COMPILE_DEFINITIONS
             "mkdir=rig_unused_stub_mkdir;mbedtls_hardware_poll=rig_unused_stub_mbedtls_hardware_poll"
  )

target_include_directories(connect_rig PRIVATE $<TARGET_PROPERTY:connect_tests,INCLUDE_DIRECTORIES>)
target_link_libraries(connect_rig $<TARGET_PROPERTY:connect_tests,LINK_LIBRARIES>)
add_dependencies(connect_rig generate-httpc-automata-tests-connect)

# Compile the vendored mbedtls against the firmware's own config rather than mbedtls' stock defaults,
# the same way addMBEDTLS.cmake does for the firmware target - otherwise the rig would silently speak
# 16384-byte records where the printer speaks 1024, and nothing measured here about record sizes
# would describe the printer's TLS stack. rig_mbedtls_config.h is that config with the single value
# that cannot survive 64-bit pointers adjusted; see its own comment for why.
target_compile_definitions(
  connect_rig PRIVATE MBEDTLS_CONFIG_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/rig/rig_mbedtls_config.h\"
  )

# --- render dumper -----------------------------------------------------------------------------
# Runs the real renderer over render.cpp's scenarios and prints the JSON it produces, for testing a
# server implementation against actual renderer output. Needs no socket, no Connect and no clock of
# its own - just the shared sources plus the SAME frozen time stub connect_tests links, which is
# what keeps the emitted durations reproducible. Not registered with add_catch_test: it has its own
# main() and is a tool, not a test.
add_executable(connect_render_dump ${CONNECT_SHARED_SOURCES} missing_functions_time.cpp
                                   rig/render_dump.cpp
  )
target_include_directories(
  connect_render_dump PRIVATE $<TARGET_PROPERTY:connect_tests,INCLUDE_DIRECTORIES>
  )
target_link_libraries(connect_render_dump $<TARGET_PROPERTY:connect_tests,LINK_LIBRARIES>)
add_dependencies(connect_render_dump generate-httpc-automata-tests-connect)

# --- keystream dumper --------------------------------------------------------------------------
# Prints the AES-CTR keystream the real transfer decryptor produces at a set of starting offsets,
# for testing a server that has to encrypt ranges the same way. See the file's own header for why
# it emits the keystream rather than a decrypted file, and why the offsets are the interesting part.
#
# A much smaller dependency set than the two targets above: the decryptor needs AES and a bsod that
# throws rather than halting, and nothing else - no Connect, no printer, no clock. Listed explicitly
# rather than reusing CONNECT_SHARED_SOURCES, which would drag in the whole client for three files.
add_executable(
  connect_keystream_dump ${CMAKE_SOURCE_DIR}/src/transfers/decrypt.cpp
                         ${CMAKE_SOURCE_DIR}/tests/unit/mock/bsod.cpp rig/keystream_dump.cpp
  )
target_include_directories(
  connect_keystream_dump PRIVATE $<TARGET_PROPERTY:connect_tests,INCLUDE_DIRECTORIES>
  )
target_link_libraries(connect_keystream_dump $<TARGET_PROPERTY:connect_tests,LINK_LIBRARIES>)

# Planner tests are separate. They mock time and don't want to poison that for others.
add_executable(
  connect_planner_tests
  ${CMAKE_CURRENT_BINARY_DIR}/http_resp_automaton.cpp
  ${CMAKE_SOURCE_DIR}/src/common/automata/core.cpp
  ${CMAKE_SOURCE_DIR}/src/common/crc32.cpp
  ${CMAKE_SOURCE_DIR}/src/common/directory.cpp
  ${CMAKE_SOURCE_DIR}/src/common/error_code_mangle.cpp
  ${CMAKE_SOURCE_DIR}/src/common/json_encode.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filepath_operation.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filename_type.cpp
  ${CMAKE_SOURCE_DIR}/src/common/printer_model.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/resp_parser.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/httpc.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/socket.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/connection.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/socket_connection_factory.cpp
  ${CMAKE_SOURCE_DIR}/src/common/stat_retry.cpp
  ${CMAKE_SOURCE_DIR}/src/common/random_sw.cpp
  ${CMAKE_SOURCE_DIR}/src/common/general_response.cpp
  ${CMAKE_SOURCE_DIR}/src/common/utils/string_builder.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/background.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/planner.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/command.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/printer.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/sleep.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/decrypt.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/monitor.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/download.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/files.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/changed_path.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/transfer.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/transfer_file_check.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/transfer_recovery.cpp
  ${CMAKE_SOURCE_DIR}/tests/stubs/jsmn_impl.c
  ${CMAKE_SOURCE_DIR}/tests/stubs/strlcpy.c
  ${CMAKE_SOURCE_DIR}/tests/stubs/printer_type.cpp
  ${CMAKE_SOURCE_DIR}/tests/unit/mock/bsod.cpp
  ${CMAKE_SOURCE_DIR}/tests/unit/common/printer_state_mock.cpp
  time_mock.cpp
  missing_functions.cpp
  planner.cpp
  partial_file_mock.cpp
  gui_media_events_mock.cpp
  xbuddy_extension_mock.cpp
  side_strip_control_mock.cpp
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/mbedtls/library/aes.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/mbedtls/library/aesni.c
  # LwIP parts
  # See reason why we do it this way in
  # tests/unit/lib/WUI/nhttp/CMakeLists.txt
  ${CMAKE_SOURCE_DIR}/tests/unit/lib/WUI/nhttp/missing_functions.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/altcp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/altcp_tcp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/def.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/pbuf.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ip.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_addr.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/icmp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/etharp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_frag.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/udp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/tcp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/dns.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/tcp_in.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/tcp_out.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/timeouts.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/memp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/netif.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/netif/ethernet.c
  ${CMAKE_SOURCE_DIR}/src/feature/cork/tracker.cpp
  )
target_include_directories(
  connect_planner_tests
  PRIVATE .
          include
          ${CMAKE_CURRENT_SOURCE_DIR}/mock
          ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_SOURCE_DIR}/tests/stubs # must be before ${CMAKE_SOURCE_DIR}/include
          ${CMAKE_BINARY_DIR}/include
          ${CMAKE_SOURCE_DIR}/include
          ${CMAKE_SOURCE_DIR}/src/connect
          ${CMAKE_SOURCE_DIR}/src/common
          ${CMAKE_SOURCE_DIR}/src/common/http
          ${CMAKE_SOURCE_DIR}/src/common/marlin_server_types
          ${CMAKE_SOURCE_DIR}/src/common/utils
          ${CMAKE_SOURCE_DIR}/src/gui
          ${CMAKE_SOURCE_DIR}/src
          ${CMAKE_SOURCE_DIR}/src/lang
          ${CMAKE_SOURCE_DIR}/lib/jsmn
          ${CMAKE_SOURCE_DIR}/lib/WUI
          ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/mbedtls/include
          ${CMAKE_SOURCE_DIR}/include/usb_host
          ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/include
          ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/system
          ${CMAKE_SOURCE_DIR}/lib/libbgcode/src/LibBGCode
          ${CMAKE_SOURCE_DIR}/lib/Marlin/Marlin/src/
          ${CMAKE_BINARY_DIR}/lib/libbgcode/src/
  )
add_dependencies(connect_planner_tests generate-httpc-automata-tests-connect)

target_link_libraries(connect_planner_tests raii xbuddy_extension otp random random_sw)

add_executable(
  connect_registration_tests
  ${CMAKE_CURRENT_BINARY_DIR}/http_resp_automaton.cpp
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/mbedtls/library/aes.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/mbedtls/library/aesni.c
  ${CMAKE_SOURCE_DIR}/src/common/automata/core.cpp
  ${CMAKE_SOURCE_DIR}/src/common/crc32.cpp
  ${CMAKE_SOURCE_DIR}/src/common/error_code_mangle.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/connection.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/httpc.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/resp_parser.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/socket.cpp
  ${CMAKE_SOURCE_DIR}/src/common/http/socket_connection_factory.cpp
  ${CMAKE_SOURCE_DIR}/src/common/json_encode.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filename_type.cpp
  ${CMAKE_SOURCE_DIR}/src/common/filepath_operation.cpp
  ${CMAKE_SOURCE_DIR}/src/common/printer_model.cpp
  ${CMAKE_SOURCE_DIR}/src/common/stat_retry.cpp
  ${CMAKE_SOURCE_DIR}/src/common/segmented_json.cpp
  ${CMAKE_SOURCE_DIR}/src/common/random_sw.cpp
  ${CMAKE_SOURCE_DIR}/src/common/utils/string_builder.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/background.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/json_out.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/planner.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/printer.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/registrator.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/sleep.cpp
  ${CMAKE_SOURCE_DIR}/src/connect/status.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/decrypt.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/download.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/files.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/monitor.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/changed_path.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/transfer.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/transfer_file_check.cpp
  ${CMAKE_SOURCE_DIR}/src/transfers/transfer_recovery.cpp
  ${CMAKE_SOURCE_DIR}/tests/stubs/strlcpy.c
  ${CMAKE_SOURCE_DIR}/tests/stubs/printer_type.cpp
  ${CMAKE_SOURCE_DIR}/tests/unit/mock/bsod.cpp
  ${CMAKE_SOURCE_DIR}/tests/unit/common/printer_state_mock.cpp
  missing_functions.cpp
  ${CMAKE_SOURCE_DIR}/tests/unit/lib/WUI/nhttp/missing_functions.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/altcp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/altcp_tcp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/def.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/pbuf.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ip.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_addr.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/icmp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/etharp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_frag.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/udp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/tcp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/dns.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/tcp_in.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/tcp_out.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/timeouts.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/memp.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/core/netif.c
  ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/netif/ethernet.c
  ${CMAKE_SOURCE_DIR}/src/feature/cork/tracker.cpp
  registrator.cpp
  time_mock.cpp
  partial_file_mock.cpp
  gui_media_events_mock.cpp
  )
target_include_directories(
  connect_registration_tests
  PRIVATE .
          include
          ${CMAKE_CURRENT_SOURCE_DIR}/mock
          ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_SOURCE_DIR}/tests/stubs
          ${CMAKE_BINARY_DIR}/include
          ${CMAKE_SOURCE_DIR}/include
          ${CMAKE_SOURCE_DIR}/src
          ${CMAKE_SOURCE_DIR}/src/lang
          ${CMAKE_SOURCE_DIR}/src/common
          ${CMAKE_SOURCE_DIR}/src/common/http
          ${CMAKE_SOURCE_DIR}/src/common/marlin_server_types
          ${CMAKE_SOURCE_DIR}/src/common/utils
          ${CMAKE_SOURCE_DIR}/src/gui
          ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/mbedtls/include
          ${CMAKE_SOURCE_DIR}/include/usb_host
          ${CMAKE_SOURCE_DIR}/lib/WUI
          ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/mbedtls/include
          ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/src/include
          ${CMAKE_SOURCE_DIR}/lib/Middlewares/Third_Party/LwIP/system
          ${CMAKE_SOURCE_DIR}/lib/libbgcode/src/LibBGCode
          ${CMAKE_SOURCE_DIR}/lib/Marlin/Marlin/src/
          ${CMAKE_BINARY_DIR}/lib/libbgcode/src/
  )
add_dependencies(connect_registration_tests generate-httpc-automata-tests-connect)

target_link_libraries(connect_registration_tests raii xbuddy_extension otp random random_sw)

add_catch_test(connect_tests)
add_catch_test(connect_planner_tests)
add_catch_test(connect_registration_tests)
