################################################################################
# trace/ tests: hven_trace_tests (RS-6a): the trace schema, the sinks and the serializer; the human trace rendered from real solves (golden/human_trace/), the v1 reader fixtures
# (golden/trace_v1/, held byte-exact for outside readers), the cost contract's structural proof. The golden directories and the source root are absolute, so ctest's working directory is irrelevant.
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

# The former hven_sqp_tests sources: UNIX-only (its subprocess wall-deadline tests drive the
# corpus runner, whose deadline enforcement is fork/exec orchestration, and the suite has never
# been built or derived on Windows; RS-6b is the stage that lifts the gate).
set(_hven_trace_unix_sources
    test_trace_writer.cpp
)
if(NOT WIN32)
    list(APPEND _hven_trace_sources ${_hven_trace_unix_sources})
else()
    hven_test_sources_absent("Windows" ${_hven_trace_unix_sources})
endif()

# Compiled at -O1 in Release, as the retired interior/drivers/model targets compiled them
# (compile time; the sources of the former hven_tests and hven_sqp_tests were not).
set(_hven_trace_o1_sources
    test_console_sink.cpp
    test_human_trace_goldens.cpp
    test_print_tables.cpp
    test_trace_messages.cpp
    test_trace_off_path.cpp
    test_trace_v1_contract.cpp
    test_trace_v1_fixtures.cpp
)

hven_add_test_executable(hven_trace_tests
    SOURCES ${_hven_trace_sources}
    O1_SOURCES ${_hven_trace_o1_sources}
    DEFINITIONS
        HVEN_SRC_ROOT="${HVEN_REPO_ROOT}/src"
        HVEN_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/human_trace"
        HVEN_TRACE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/trace_v1")
