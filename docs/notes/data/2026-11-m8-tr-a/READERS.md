# TR-A reader-move list (first commit; spec 2.9 counted 36 by a wider grep)

Files under tests/ examples/ bench/ that name the v0 sink API, v0 event/function names, or the console tables (strict grep, 25 hits), listed at base fa97d6bf0. Each is re-pointed in TR-A. Console-text-only readers (tests that capture console output without naming the API) are added as they fail; the final list is in the TR-A report.

- bench/counting_trace_sink.h
- examples/08_results_trace_options.cpp
- tests/common_support/console_capture.h
- tests/drivers/console_sink_base_transcript.inc
- tests/drivers/test_callback.cpp
- tests/drivers/test_console_sink.cpp
- tests/drivers/test_dense_arm_engines.cpp
- tests/drivers/test_evaluation_failure.cpp
- tests/drivers/test_infinite_bound.cpp
- tests/drivers/test_stop_token.cpp
- tests/drivers/test_warm_protocol.cpp
- tests/install_smoke/include_console_trace_sink.cpp
- tests/install_smoke/main.cpp
- tests/interior/test_ipm_solver_entry.cpp
- tests/interior/test_ipm_stop_reason.cpp
- tests/interior/test_ipm_trace.cpp
- tests/sqp/test_column_scaling.cpp
- tests/sqp/test_ipm_dense_guard_seams.cpp
- tests/sqp/test_problem_scaling.cpp
- tests/sqp/test_qp_cap_levers.cpp
- tests/sqp/test_qp_mode_sites.cpp
- tests/sqp/test_sqp_solver.cpp
- tests/sqp/test_stop_poll_seams.cpp
- tests/sqp/test_trace_writer.cpp
- tests/sqp/test_warm_start.cpp
