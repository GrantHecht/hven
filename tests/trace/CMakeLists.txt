################################################################################
# trace/ tests: hven_trace_tests: the trace schema, the sinks and the serializer; the human trace
# rendered from real solves (golden/human_trace/), the v1 reader fixtures (golden/trace_v1/, held
# byte-exact for outside readers) and the cost contract's structural proof. The golden directories
# and the source root are absolute, so ctest's working directory is irrelevant.
# Flags: RELEASE_O1 (the directory rule in tests/CMakeLists.txt).
################################################################################

set(_hven_trace_sources
    test_console_sink.cpp
    test_human_trace_goldens.cpp
    test_print_tables.cpp
    test_trace_messages.cpp
    test_trace_off_path.cpp
    test_trace_v1_contract.cpp
    test_trace_v1_fixtures.cpp
)
hven_sources_not_on_windows(_hven_trace_sources
    test_json_lines_trace_sink.cpp
)

hven_add_test_executable(hven_trace_tests
    SOURCES ${_hven_trace_sources}
    RELEASE_O1
    DEFINITIONS
        HVEN_SRC_ROOT="${HVEN_REPO_ROOT}/src"
        HVEN_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/human_trace"
        HVEN_TRACE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/trace_v1")
