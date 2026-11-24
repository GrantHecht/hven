# hven

hven is a sparse nonlinear-programming solver library written in C++20. It has
two engines behind one problem type and one result shape: an interior-point
engine (`hven::IpmSolver`) and an SQP engine (`hven::SqpSolver`), sharing sparse
linear algebra, KKT assembly, warm-start currency and globalization
primitives. Sparse linear algebra comes from Intel MKL (Linux, Windows) or Apple
Accelerate (macOS). Licensed under Apache-2.0.

**Status.** Both engines are in the repository and tested, the API names are
frozen for the 0.2 line, and the library is under active development. It is
consumed with CMake, either by `add_subdirectory` or as an installed package
(`find_package(hven)` then `target_link_libraries(app hven::hven)`); a consumer
includes `<hven/hven.h>`.

- Start here: [docs/getting-started.md](docs/getting-started.md), from install to
  a read trace.
- All documentation: [docs/README.md](docs/README.md).
- Runnable programs, one problem each: [examples/README.md](examples/README.md).

The name: Hven is the island where Tycho Brahe built his observatories, the
ground both his instruments stood on.
