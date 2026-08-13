# Build notes

What a developer needs to know about how this library compiles, beyond
`cmake --preset … && cmake --build …`.

## The precompiled header

`src/hven_pch.h` is a precompiled header used by six of the `hven`
target's translation units. It exists because this library's build time
is dominated by header parsing, not by how much code any one file
contains.

The numbers behind that claim (Linux, clang, Release `-O3`, idle box,
ccache disabled):

- Parsing the header set in `src/hven_pch.h` costs **3.01 s**, and that
  cost is paid once per TU that includes it.
- `src/drivers/interior_point_solver.cpp` is the largest TU at 3722
  lines and 7.02 s. **3.01 s of that is the headers**; only 4.01 s is
  its own body.
- `src/core/pattern_hash.cpp` is **39 lines** and still costs 2.98 s.

So file length barely predicts compile cost here, and the lever that
works is amortizing the shared header floor rather than moving code
between files. Building the PCH once costs 3.38 s; each participating TU
then drops 1.5–2.0 s. Measured effect on a clean `-j2` build of the
library target, median of three runs each: **28.16 s → 23.20 s
(−17.6%)**.

The same measurements are why the engine's large TUs are *not* split
into smaller ones — a split multiplies the 3.01 s floor by the number of
pieces instead of amortizing it, and (with `LINK_TIME_OPT` off) turns
helper calls inside the interior-point hot loop into cross-TU references
the compiler can no longer inline.

### Which TUs use it, and the rule for joining

Membership is **opt-in**, listed in `src/CMakeLists.txt`. Everything not
named there is opted out, so a newly added source gets no PCH until
someone measures it.

A TU qualifies only if it clears both bars:

1. it compiles **faster** with the PCH, and
2. its object file stays **byte-identical** to the non-PCH build.

Bar 2 is the one that matters most. The translation-unit section of
`CLAUDE.md` requires runtime neutrality to be proven rather than
presumed for anything touching engine code, and a byte-identical object
is the strongest available proof: identical bytes cannot move a
golden-rig row or a counter. With the current list, all 18 object files
*and* `libhven.a` itself are byte-identical with and without the PCH.

Several TUs get faster with the PCH but produce a *non*-byte-identical
object, and are excluded for that reason alone. Their code is not wrong
— per-symbol disassembly comparison shows identical instructions, with
only function layout order and local label numbering shifting. It
happens because those TUs include a different header set in a different
order (`interior_point_solver_globalization.cpp`, for example, orders
its includes alphabetically and pulls in two headers outside the shared
set), so prefixing the shared block changes template instantiation order
and therefore emission order. Admitting them would buy roughly 5.4 s
more; doing so means relaxing bar 2, which is a numerics-governance
decision, not a build decision.

### If you edit `src/hven_pch.h`

The header's include list is the include block of
`src/drivers/interior_point_solver.cpp`, verbatim and in the same order.
That ordering is load-bearing — it is what makes the byte-identity
property hold. After any edit, re-run the byte-identity check against a
non-PCH build and update the participating-TU list to whatever still
qualifies. `src/CMakeLists.txt` fails configuration outright if a name
in the opt-in list no longer matches a real source, so a rename cannot
silently widen or narrow the PCH's coverage.

## ccache

The build uses ccache automatically when it is on `PATH`. Getting useful
hit rates on a PCH-using tree needs a little configuration, because
ccache is conservative about precompiled headers by default — it will
refuse to cache PCH-consuming compiles unless told how to hash them.

Put this in `~/.config/ccache/ccache.conf`:

```ini
max_size = 40G
sloppiness = pch_defines,time_macros
pch_external_checksum = true
```

What each line is for:

- **`max_size`** — the default (5G) is too small for a tree of this
  shape; a full build writes several GiB and the cache thrashes rather
  than helping.
- **`sloppiness = pch_defines,time_macros`** — permits caching of
  compiles that consume a PCH at all. Without it those compiles are
  simply uncacheable, which on this tree is most of the expensive ones.
- **`pch_external_checksum`** — makes consumers hash the PCH's
  *content*. This is what makes the sloppiness above safe: a
  regenerated-but-byte-identical PCH costs nothing, while a genuinely
  changed PCH still invalidates everything downstream.

The build pairs with this by compiling the PCH with `-Xclang
-fno-pch-timestamp` under Clang (`src/CMakeLists.txt`), so a regenerated
PCH that happens to be identical does not force every consumer to
recompile on a timestamp alone.

One consequence worth stating plainly: a timestamp-free PCH means clang
itself will no longer reject a stale PCH whose input headers changed
only in content and mtime (a size change is still caught). The content
checksum from `pch_external_checksum` is the guard that replaces that
check. That is why the setting is recommended rather than merely
optional on a Clang development machine.
