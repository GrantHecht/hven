# The piece assembly protocol

A problem built from pieces is an `AssemblyDeclaration` (`hven/model/assembly_declaration.h`)
wrapped in a `Problem`. `IpmSolver` solves it; `SqpSolver` refuses it by name. Everything a
consumer needs is in `#include "hven/model/pieces.h"`.

There are two ways to write a piece:

- **The local-block layer** (`hven/model/local_blocks.h`), the default. A piece states its sizes
  as compile-time constants and evaluates small dense blocks over its own inputs. The library
  writes the KKT claims, the scatter, the lock protocol, the elimination of fixed variables and
  the partitioning. Start here.
- **The raw protocol**, the expert route, specified on this page. A raw piece writes its own
  claims and scatters its own derivatives. Use it when a piece's sizes are only known at run
  time, when its blocks are too large to hold densely, or when it carries an existing erased
  function representation. Every local-block piece becomes a raw piece
  (`LocalConstraintBlock<P>`, `LocalObjectiveBlock<P>`), so the two routes meet in the same
  handles and the same layout.

Example 4 (`examples/04_partitioned_chain.cpp`) is the local-block route. The same chain written
on the raw protocol is `tests/model/test_pieces.cpp` (namespace `raw`), which also pins that the
two forms solve identically and claim the same slots.

## 1. The declaration

The builder fills a declaration and derives what it can:

```cpp
hven::solvers::AssemblyDeclaration d(n);            // n variables
d.add_objective(term, vars);                        // vars: inputs x applications
d.add_equality(piece, vars, rows);                  // rows: outputs x applications; c(x) = 0
d.add_inequality(piece, vars, rows);                // c(x) <= 0
d.set_bounds(first, last, lower, upper);            // variables first..last, inclusive
d.set_partitions(k);                                // the one place the count is given
hven::solvers::Problem problem(std::move(d));
```

- **Index maps.** Column `V` of `vars` lists the variables application `V` reads, in the piece's
  input order. Column `V` of `rows` lists the rows of the block that application `V`'s outputs
  are, in the piece's output order. Equality and inequality rows are two separate row spaces,
  each numbered from 0.
- **Derived counts.** A block's row space is `0 .. (largest row named)`. Rows named by more than
  one application are summed; the excess is the block's shared-row overcount, derived as
  `sum(rows claimed) - rows`. Every row of the space must be named by some application: a row
  with no piece is the constraint `0 = 0`, and `validate()` refuses it, naming the row.
- **Refusals.** Each builder call refuses, by name and with both numbers, an index map whose row
  count is not the piece's input (or output) count, a pair of maps with different application
  counts, a map with no application, a variable outside `[0, n)` and a negative row. `set_bounds`
  refuses an index outside the declaration, an empty range, a NaN side and an inverted pair.
  Repeated bounds on one variable intersect, tightest wins.
- **Partitions.** `set_partitions(k)` asks the interior-point layout for `k` evaluation
  partitions. The layout may clamp the request down, never up: it adopts at most one partition
  per 1000 claimed KKT elements, and at least one. `Problem::partitions()` reports the count
  adopted. Partitions are evaluated concurrently (§6).

The raw route writes the same fields directly: `objectives_`, `equality_constraints_` and
`inequality_constraints_` (lists of `ObjectiveFunction` / `ConstraintFunction`, each a handle plus
its index maps), `primal_vars_`, `equality_rows_`, `inequality_rows_`, the two
`*_shared_row_overcount_` fields, `partition_count_` and `variable_bounds_`. On that route the
counts are stated, not derived, and `validate()` checks them: each block's pieces must claim
exactly `rows + overcount` rows. `fixing_rows_` is internal (a fixed-variable treatment's rows)
and stays 0 in a consumer's declaration.

## 2. The handles

A piece enters the layout through one of two type-erased handles
(`hven/model/solver_interface_specs.h`): `ConstraintInterface` for a constraint piece and
`ObjectiveInterface` for an objective piece. Both take any piece type by converting constructor.
What gets stored is decided by `SolverInterfaceAdapter<T>` (`hven/solver_interface_adapter.h`):

- **No registration is needed.** The default stores a plain type as itself: one erasure, one
  virtual dispatch per evaluation call.
- **An erased handle with the hooks** (`pack_into_constraint_interface(ConstraintInterface &)
  const`, and `pack_into_objective_interface(ObjectiveInterface &) const` for the objective route)
  is detected, and the hook emplaces the handle's concrete payload. Still one dispatch.
- **An erased handle without the hooks** is stored whole, and every evaluation then pays two
  dispatches, the handle's and the stored model's. Give a handle the hooks before freezing its
  representation; they must be reachable from the handle's own erasure.
- **An explicit specialization** of `SolverInterfaceAdapter<T>` replaces the default for `T`. It
  lives in the file that defines `T`, and it provides both installs (inherit
  `ConstraintUnsupported<T>` or `ObjectiveUnsupported<T>` to refuse a direction in words).
- **Any movable type is legal.** A small piece is stored inline and relocated by move
  construction, so a piece may own a `std::string`, a `std::vector` or any other movable member.

Every refusal on these routes is an authored compile-time message.

## 3. What a raw piece provides

Every piece reports itself and claims its slots:

| member | meaning |
|---|---|
| `std::string name() const` | names the piece in diagnostics |
| `int input_rows() const` | inputs per application (the rows of its variable map) |
| `bool thread_safe() const` | reported; the layout does not read it (§6 states the rule) |
| `int num_kkt_elements(bool do_jac, bool do_hess) const` | KKT slots one application claims |
| `void get_kkt_space(rows, cols, int &free_slot, int con_offset, bool do_jac, bool do_hess, SolverIndexingData &d)` | claims them (§4) |

A **constraint** piece adds `int output_rows() const` (outputs per application) and five
evaluations, each a superset of the one before:

| member | writes |
|---|---|
| `constraints(X, FX, d)` | residuals |
| `constraints_adjointgradient(X, L, FX, AGX, d)` | residuals, `J^T lambda` |
| `constraints_jacobian(X, FX, kkt, locations, clashes, locks, d)` | residuals, Jacobian |
| `constraints_jacobian_adjointgradient(X, L, FX, AGX, kkt, locations, clashes, locks, d)` | residuals, `J^T lambda`, Jacobian |
| `constraints_jacobian_adjointgradient_adjointhessian(X, L, FX, AGX, kkt, locations, clashes, locks, d)` | the above and `sum_j lambda_j d2c_j` |

An **objective** piece adds three evaluations and nothing of the constraint kind (no
`output_rows`, no `constraints*` members):

| member | writes |
|---|---|
| `objective(scale, X, val, d)` | adds `scale * f` to `val` |
| `objective_gradient(scale, X, val, GX, d)` | the value, and `scale * grad f` |
| `objective_gradient_hessian(scale, X, val, GX, kkt, locations, clashes, locks, d)` | the above and `scale * hess f` |

Each evaluation loops over the applications `V = 0 .. d.num_appl() - 1` of the piece it was handed
(a partition hands a piece a contiguous share of its applications, §6).

## 4. `SolverIndexingData`: maps, arenas and claims

`SolverIndexingData` (`hven/model/indexing_data.h`) is the per-piece record the layout fills.

**Reading inputs.** `X[d.v_loc(i, V)]` is input `i` of application `V`. `X` is the full primal
vector, in the problem's own variable space, always.

**Multipliers.** `L[d.c_loc(j, V)]` is the multiplier of output `j` of application `V`. `L` is
the block's own multiplier vector (equality or inequality), addressed by row.

**Residuals and gradients are SLOTS, not rows.** `FX`, `GX` and `AGX` are claim arenas: each
application owns a contiguous run of slots, and the engine sums every slot into its row after
the pass. Application `V`'s slots start at

- `d.inner_constraint_starts_[V]` in `FX` (`output_rows()` slots, one per output),
- `d.inner_gradient_starts_[V]` in `GX` and `AGX` (`input_rows()` slots, one per input).

The engine zeroes the arenas before every pass. Write `FX[start + j]` and `GX[start + i]`, and
add into `AGX[start + i]`. An input whose variable the engine eliminated still owns its gradient
slot; the engine skips it when it sums.

**Claims.** `get_kkt_space` records, for each application `V`, `d.inner_kkt_starts_[V] =
free_slot`, and then claims that application's slots contiguously, advancing `free_slot` once per
slot and writing each slot's coordinates into `rows[slot]`, `cols[slot]`:

- a Jacobian slot (output `j`, input `i`): `(d.c_loc(j, V) + con_offset, d.v_scatter_loc(i, V))`,
  or `(-1, -1)` when `d.v_scatter_loc(i, V) < 0`;
- a Hessian slot (inputs `r`, `c`): `(d.v_scatter_loc(r, V), d.v_scatter_loc(c, V))`, in either
  orientation, or `(-1, -1)` when either is negative.

`d.v_scatter_loc(i, V)` is the input's column in the space the engine factorizes; it is `-1` for a
variable the engine eliminated (a fixed variable under `FixedVariableTreatment::kMakeParameter`).
An eliminated slot is still claimed, so the scatter's cursor stays in lockstep with the claims,
and it names no matrix entry. Constraint pieces are claimed with `do_jac` and `do_hess` both true
and the block's `con_offset`; objective pieces with `do_hess` only. Two claims may name one
matrix entry; their values are summed.

`num_kkt_elements` must be **additive** in its two flags (`f(true, true) == f(true, false) +
f(false, true)`), and must equal the slots `get_kkt_space` claims per application for the same
flags. The layout counts with the both-flags call and splits per domain with the two single-flag
calls; a piece that answers otherwise is refused by name when the claim stream is restated.

## 5. The scatter and the lock protocol

A scatter walks the claims in claim order: application `V`'s `k`-th claimed slot is
`d.inner_kkt_starts_[V] + k`, and its value goes to `kkt.valuePtr()[locations[slot]] += value`.
Skip a slot whose claim was `(-1, -1)`. The engine zeroes the KKT values before every pass.

When the problem is partitioned, two partitions may write one matrix entry. `clashes[col]` is
`-1` for an uncontested column and otherwise the index of its mutex in `locks`. The lock column
of an entry is fixed, so every writer of one entry takes the same mutex:

- a **Hessian** entry `(r, c)` locks `kkt_canonical_lock_col(r, c)` (the smaller of the two),
  whenever that column is contested;
- a **Jacobian** entry `(row, col)` locks `col` (constraint rows sort above every variable
  column), when the column is contested and `!d.unique_constraints_`.

Within one partition writes are serial; a single-partition problem takes no lock. Do not
introduce another keying.

**A stated hazard.** `unique_constraints_` is true for every piece in this release, so Jacobian
writes never lock. Two applications that write the SAME Jacobian entry (one row and one variable,
which takes a shared row) from DIFFERENT partitions would race. A provider whose shared rows can
do that keeps the pieces involved in one partition with `ConstraintFunction::set_thread_mode`
(for example `ThreadingFlags::MainThread`). Shared rows on distinct variables are safe: they name
distinct entries.

## 6. Threading

A partitioned layout evaluates its partitions concurrently on the library's thread pool, one task
per partition, and the last partition on the calling thread. A piece's members are therefore
called from several threads at once, on disjoint applications, and must be safe to call
concurrently: every evaluation member is `const`. By default a piece's applications are split
across the partitions (`ThreadingFlags::ByApplication`); `set_thread_mode` on its
`ObjectiveFunction` / `ConstraintFunction` pins it instead (`MainThread`, `RoundRobin` or a fixed
partition index), and is part of the declaration: set it before the problem is built.

## 7. Evaluation failure

A piece follows the library's contract (`hven/core/errors.h`): at a point it cannot evaluate,
throw `hven::solvers::EvaluationError`, or produce a non-finite value, and the engine treats the
trial as rejected and backs off. At the solve's start point an `EvaluationError` propagates and a
non-finite value ends the solve `kNumericalError`. Any other exception propagates unchanged, with
its own type, also from a pool thread.

## 8. The local-block layer, for reference

`LocalConstraintBlock<P>` and `LocalObjectiveBlock<P>` are this protocol, written once. Per
application they gather the inputs into stack buffers, call the piece's blocks into zeroed
buffers, and write: residuals and gradients into their slots, `J^T lambda` into the adjoint
gradient slots, and the pattern entries of the Jacobian and Hessian blocks through the claimed
slots, under §5's locks. The claims of one application are the Jacobian pattern's entries in
order, then the Hessian pattern's. A Hessian block is read in its lower triangle; an off-diagonal
block entry whose two inputs name one variable lands on that variable's diagonal with weight 2.
