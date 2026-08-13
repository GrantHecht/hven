# hven

hven is a high-performance sparse NLP solver library, being built around
an interior-point engine and an SQP engine over shared infrastructure
(sparse linear algebra, KKT assembly, warm-start currency, and
globalization primitives). Written in C++20, licensed under
Apache-2.0, and under active development.

**What is here today** is the foundation those engines will sit on: the
core and linear-algebra components — the sparse symmetric factor/solve
surface with its MKL and Accelerate backends, the dense factor, and the
structural pattern hash. Both engines arrive with later milestones,
migrated in from the projects they originated in.

The primary consumer is [tycho](https://github.com/GrantHecht/tycho),
a trajectory design and optimal control library. hven builds
standalone with CMake and requires Intel MKL (Linux/Windows) or Apple
Accelerate (macOS) as its sparse linear algebra backend. It is consumed
either by `add_subdirectory` plus the `hven::hven` target, or as an
installed CMake package via `find_package(hven)` +
`target_link_libraries(app hven::hven)` (see
`scripts/check_install_smoke.sh`).

The name: Hven is the island where Tycho Brahe built his observatories
— the ground both his instruments stood on.
