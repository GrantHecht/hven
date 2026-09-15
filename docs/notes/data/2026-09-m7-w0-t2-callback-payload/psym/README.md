# The P-SYM classification, completed — 2026-09-15 (M7 W0 T2 fix1)

An addendum, added at fix round 1. The retained transcript
`../logs/72f-psym.out` names WHICH symbols differ between the two library
objects of the T2 pair (`d8ed6b28` → `b8db4b98`); for the ones it reports as
STRUCTURAL it does not say WHAT differs, and the first report left
`IpmSolver::alg_impl` and the interior object's only-after symbol unclassified —
and named the latter wrongly. This directory closes that.

`classify.py` re-reads the SAME two object pairs the transcript was taken over
(they are retained under `.scratch/m7t2/psym-{old,new}-build/`, which is where
the numbers below come from) and classifies every differing symbol by one rule:

1. disassemble with GNU objdump — the disassembler P-SYM itself requires;
2. drop ALIGNMENT PADDING, counted separately;
3. normalise INTRA-FUNCTION branch targets, recording each target's delta, so a
   function that merely MOVED does not read as changed;
4. diff what is left — mnemonic and operands — and classify every remaining
   difference as
   * **MEMBER-DISPLACEMENT**: same mnemonic, same operands, one memory
     displacement larger by a CONSTANT. The constant is reported, never
     assumed;
   * **LABEL-FAMILY**: same instruction, different compiler-local label number
     (`.LCPI` / `.LBB` / `.Ltmp` / `GCC_except_table`) — a name codegen assigns
     per run, which reaches a listing only because objdump annotates a target;
   * **OTHER**: anything else, printed in full.

A symbol whose only differences are a pad count, a constant branch-target shift,
a constant member-displacement shift and label renumberings is **LAYOUT ONLY**.
Anything else is named.

    python3 classify.py <old-object> <new-object>   # exits 1 if anything DIFFERS

## The result

| object | byte-identical | layout-only | differing | only-before | only-after |
|---|---:|---:|---:|---:|---:|
| `ipm_solver.cpp.o` | 163 | **78** | **3** | 0 | 1 |
| `sqp_solver.cpp.o` | 246 | **47** | **6** | 1 | 2 |

**Every differing symbol is one this task EDITED, or a constructor that now
initialises the new members.** There is no third category.

* interior — `IpmSolver::IpmSolver(IpmOptions)` (two more members initialised),
  `apply_pending_iteration_callback` (the mask applied beside the callable) and
  `fire_iteration_event` (the guarded path, which IS the change).
* SQP — both constructors, `apply_pending_iteration_callback`,
  `enter_restoration` (the forwarded mask argument) and the two `run_major`
  symbols. `run_major` itself differs in **2 instructions out of 3239**, and
  both are a register swap at two adjacent spill slots
  (`mov %rbx,0x498(%rsp)` ↔ `mov %r14,0x498(%rsp)`) — register-allocation
  noise, not work.

### `IpmSolver::alg_impl` — the symbol the first report left out

```
LAYOUT   hven::solvers::IpmSolver::alg_impl(...)
  pads 208 -> 207; non-pad instructions 7502 -> 7502
  branch-target shifts: -16 x1091
  member-displacement pairs: +8 x39
```

**Not one instruction was added or removed.** The function's alignment padding
is one line shorter (its start offset moved from 32-byte-aligned to
16-byte-aligned), which shifts every one of its 1091 intra-function branch
targets by a constant −16; and 39 instructions reach a member past the new mask
fields, every one at exactly +8. P-SYM reported it as STRUCTURAL only because
its pad route is declined whenever a non-pad line ALSO differs (its rule 3), and
the displacements are non-pad lines.

### `sqp_solver.cpp.o`'s displacement constant is **+4**, not +8

`invoke_iteration_callback` reads `callback_in_flight_` at `0x358` before and
`0x35c` after: on the SQP driver ONE of the two new four-byte enums landed in
existing padding, so the shift downstream is four bytes rather than eight. The
first report's blanket "+8" was right for the interior object and wrong for this
one. The classifier reports the constant per symbol rather than assuming it, and
flags any symbol whose displacement deltas are not a single value.

### The interior object's only-after symbol — corrected

It is **`Eigen::Matrix<double,-1,1,0,-1,1>::Matrix(const&)`**, Eigen's `VectorXd`
copy constructor, emitted as a **weak (COMDAT) out-of-line definition** in the
after arm because the restructured `fire_iteration_event` gave the inliner a
copy site it declined to expand. It has exactly **one call site in the whole
object, inside `fire_iteration_event`** — verified by relocation
(`objdump -dr`) — so it is reachable only from the guarded, attached-only path.

The first report said this symbol was the two-argument
`set_iteration_callback`. **That was wrong**: the setter pair is the
only-before/only-after pair on the SQP object (the one-argument overload
becoming the two-argument one), and the SQP object's second only-after symbol
is a lambda the mask gating introduced in `run_major`. The interior object's
only-after symbol is the Eigen constructor above.

## What this addendum does not claim

It classifies the objects of ONE pair. It is not a statement about the fix-round
change, which moves `sqp_solver.o` again — that pair is reported in the task
report, not here.
