# I18 corpus instrument, a consumer of an installed hven (find_package) plus the evidence worktree's tests/ include root.
cmake_minimum_required(VERSION 3.24)
project(i18_corpus_replay LANGUAGES CXX)
set(I18_TESTS_ROOT "" CACHE PATH "the tests/ directory whose qp/support/qp_dump.h is read")
find_package(hven REQUIRED)
add_executable(corpus_replay corpus_replay.cpp)
target_compile_features(corpus_replay PRIVATE cxx_std_20)
target_include_directories(corpus_replay PRIVATE "${I18_TESTS_ROOT}")
target_link_libraries(corpus_replay PRIVATE hven::hven)
