# hven

hven is a high-performance sparse NLP solver library, providing an
interior-point engine and an SQP engine over shared infrastructure
(sparse linear algebra, KKT assembly, warm-start currency, and
globalization primitives). Written in C++20, licensed under
Apache-2.0, and under active development.

The primary consumer is [tycho](https://github.com/GrantHecht/tycho),
a trajectory design and optimal control library. hven builds
standalone with CMake and requires Intel MKL (Linux/Windows) or Apple
Accelerate (macOS) as its sparse linear algebra backend.

The name: Hven is the island where Tycho Brahe built his observatories
— the ground both his instruments stood on.
