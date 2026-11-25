
# ===== PF-1 SCRATCH (not for merge): dump replay under the walk observer =====
add_executable(pf1b_replay
    ${HVEN_SRC_DIR}/core/enum_names.cpp
    ${HVEN_SRC_DIR}/core/ledger.cpp
    ${HVEN_SRC_DIR}/core/pattern_hash.cpp
    ${HVEN_SRC_DIR}/interior/kkt_factorization.cpp
    ${HVEN_SRC_DIR}/kkt/kkt_calls.cpp
    ${HVEN_SRC_DIR}/linear/dense_symmetric_factor.cpp
    ${HVEN_SRC_DIR}/qp/qp_engine.cpp
    ${HVEN_SRC_DIR}/qp/ws_independence.cpp
    ${HVEN_SRC_DIR}/linear/pardiso_session.cpp
    ${HVEN_SRC_DIR}/linear/symmetric_factor_mkl.cpp
    qp/pf1b_replay.cpp)
target_compile_definitions(pf1b_replay PRIVATE HVEN_TESTING=1)
target_include_directories(pf1b_replay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/qp ${INCLUDE_DIRS})
target_link_libraries(pf1b_replay PRIVATE Eigen3::Eigen fmt::fmt-header-only Threads::Threads ${LINK_LIBS})
target_compile_options(pf1b_replay PRIVATE ${COMPILE_FLAGS})
