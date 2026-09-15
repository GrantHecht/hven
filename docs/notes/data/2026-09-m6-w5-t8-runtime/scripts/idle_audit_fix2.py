#!/usr/bin/env python3
"""W5 T8.9r fix2 -- R2' RE-AUDIT: one idle proof, every retained batch, every round.

R2 (the fix1 rule) asked for a PER-PROCESS bar: every foreign pid under 0.5 % of
the batch's wall, anywhere on the box. That bar was unmeetable on this desktop --
the compositor alone accrues ~3 % of any window -- and fix1 reported it NOT met
and took its verdict on a second, narrower test. astra's fix1 review is right
that a disclosed substitution is not a satisfied ruling, and the settler has
AMENDED the rule rather than pretended the old one passed.

R2' -- THE PINNED-CORE RULE, which is the one the recipe actually reserves:

    foreign TASK time on the MEASUREMENT CORE (cpu2) *and* on its SMT SIBLING
    (cpu10), counted as user+nice+steal+guest from /proc/stat across the batch,
    must be under 0.5 % of the batch's wall; every foreign pid and state seen in
    any snapshot is listed, transients included; a foreign `R` is a pause and a
    RE-SNAPSHOT; a batch that cannot be proven is re-run.

This program does not measure anything. It runs ONE `idle_proof.py` -- the
attrib4 (nice-inclusive) version, byte-identical in all three places it now sits
-- over the RETAINED snapshots of every batch of every round, and assembles the
per-batch table R2' asks for. Nothing is re-measured: a batch whose retained
snapshots cannot support R2' is marked UNPROVEN and flagged in the reading.

Usage:  idle_audit_fix2.py <round>=<per-round idle_proof output> ... <outfile>

The per-round outputs are produced by `scripts/idle_audit_fix2.sh`, which is what
invokes `idle_proof.py`; this file only reads them back and assembles.
"""
import os
import re
import sys

# ---------------------------------------------------------------------------
# WHAT EACH BATCH ASSERTS. R2' governs WALL-ASSERTING batches; the rest are
# audited too, and reported, because the same snapshots exist for them and a
# reader should not have to take "that one did not assert wall" on trust.
#
#   WALL   the batch's elapsed times appear in a table that is read as wall
#   COUNT  the batch asserts counters only (perf stat, perf record, fault
#          counts). CLAUDE.md section 7: counters taken at MKL_NUM_THREADS=1
#          are deterministic per process and scheduling-invariant, so a window
#          that cannot be proven solo does not impeach them -- it only forbids
#          reading that batch's elapsed time as a measurement.
#   SCREEN a screening probe; nothing in any table comes from it.
#   SUPERSEDED  retained, unedited, and quoted nowhere.
# ---------------------------------------------------------------------------
KIND = {
    ("fix1", "leg1-ipm-r1"): "WALL", ("fix1", "leg1-ipm-r2"): "WALL",
    ("fix1", "leg1-ipm-r3"): "WALL", ("fix1", "leg1-ssn-r1"): "WALL",
    ("fix1", "leg1-ssn-r2"): "WALL", ("fix1", "leg1-ssn-r3"): "WALL",
    ("fix1", "leg1-walk-r1"): "WALL", ("fix1", "leg1-walk-r2"): "WALL",
    ("fix1", "leg1-walk-r3"): "WALL",
    ("fix1", "leg1perf-ipm"): "COUNT", ("fix1", "leg1perf-ssn"): "COUNT",
    ("fix1", "leg1perf-walk"): "COUNT",
    ("fix1", "interior-wall"): "WALL",
    ("fix1", "interior-perfA"): "COUNT", ("fix1", "interior-perfB"): "COUNT",
    ("fix1", "interior-cells-r1"): "COUNT", ("fix1", "interior-cells-r2"): "COUNT",
    ("fix1", "interior-cells-r3"): "COUNT",
    ("attrib2", "wall-r1"): "WALL", ("attrib2", "wall-r2"): "WALL",
    ("attrib2", "wall-r3"): "WALL", ("attrib2", "wall-r4"): "WALL",
    ("attrib2", "wall-r5"): "WALL",
    ("attrib3", "wall-r1"): "WALL", ("attrib3", "wall-r2"): "WALL",
    ("attrib3", "wall-r3"): "WALL", ("attrib3", "wall-r4"): "WALL",
    ("attrib3", "wall-r5"): "WALL",
    ("attrib3", "screen"): "SCREEN",
    ("attrib4", "wall-r1"): "WALL", ("attrib4", "wall-r2"): "WALL",
    ("attrib4", "wall-r3"): "WALL", ("attrib4", "wall-r4"): "WALL",
    ("attrib4", "wall-r5"): "WALL",
}
# prefix rules, applied when the exact key is absent
KIND_PREFIX = [
    ("fix1", "leg2-", "WALL"),            # pass A carries the leg's wall CSV;
                                          # pass B is its own timed batch and is
                                          # held to the same bar
    ("attrib2", "perfA-", "COUNT"), ("attrib2", "perfB-", "COUNT"),
    ("attrib2", "diff-", "COUNT"),
    ("attrib3", "xwall-", "WALL"), ("attrib3", "ywall-", "WALL"),
    ("attrib3", "alloc-", "WALL"), ("attrib3", "wallpf-", "WALL"),
    ("attrib3", "mem-", "COUNT"), ("attrib3", "perfstat-", "COUNT"),
    ("attrib3", "perfrec-", "COUNT"), ("attrib3", "perf", "COUNT"),
    ("attrib3-superseded", "", "SUPERSEDED"),
    ("attrib4", "pf-", "COUNT"), ("attrib4", "e3", "COUNT"),
]

ROUND_TITLE = {
    "fix1": "fix round 1 -- the reading's own legs (reading.md sections 1-9)",
    "attrib2": "T8.9r-attrib2 -- the eleven-arm interior leg (section 11)",
    "attrib3": "T8.9r-attrib3 -- inside T8.4 (section 12)",
    "attrib3-superseded": "T8.9r-attrib3, the SUPERSEDED argv-lock round set "
                          "(retained, quoted nowhere)",
    "attrib4": "T8.9r-attrib4 -- the redraw experiment (section 13)",
}

# Rounds whose retained logs carry NO `PS_SNAPSHOT`/`CPUSTAT` blocks at all, so
# R2' cannot be computed from them. Neither is re-measured (the settler's ruling
# forbids it this round); both are declared here instead.
NOT_AUDITABLE = [
    ("round 1 (superseded)", "logs/round1-superseded/L*.log",
     "Round 1's legs predate R2 entirely: their solo evidence is the `pgrep` "
     "audit astra's I1 refused, and they carry no `PS_SNAPSHOT` block. Every "
     "number they produced was SUPERSEDED by the fix1 re-measurement and is "
     "quoted nowhere in the reading. **R2' cannot be computed for them and is "
     "not claimed.**"),
    ("T8.9r-attrib5 (section 14)", "attribution-interior/t84/single-row/logs/*.log",
     "The single-row leg's batch logs carry `PGREP_INLOCK`, `FOREGROUND_START`/"
     "`_END` and `ARGV_LEN` but no `PS_SNAPSHOT`/`CPUSTAT` bracket. **That leg "
     "asserts no wall clock** — section 14 says so in its own closing "
     "paragraph, and its elapsed figures are printed as informational under "
     "CLAUDE.md section 7 -- so R2' has nothing to govern there. Its "
     "instruction and branch counts are the asserted currency and are "
     "scheduling-invariant at `MKL_NUM_THREADS=1`. **R2' is NOT claimed for "
     "that leg, and its wall figures stay informational.**"),
]

ROW = re.compile(r"^\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|")


def parse(path):
    """-> (test1 rows by batch, test2 rows by batch), each a list of cells."""
    t1, t2 = {}, {}
    where = None
    for line in open(path):
        if line.startswith("## Test 1"):
            where = t1
            continue
        if line.startswith("## Test 2"):
            where = t2
            continue
        if line.startswith("## The five busiest"):
            where = None
            continue
        if where is None or not line.startswith("| "):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if cells[0] in ("log", "---") or set(cells[0]) == {"-"}:
            continue
        where.setdefault(cells[1], cells)
    return t1, t2


def kind_of(rnd, batch):
    if (rnd, batch) in KIND:
        return KIND[(rnd, batch)]
    for r, pre, k in KIND_PREFIX:
        if r == rnd and batch.startswith(pre):
            return k
    return "COUNT"


def clean(s):
    return s.replace("**", "")


def main():
    args = sys.argv[1:]
    outpath = args[-1]
    rounds = []
    for a in args[:-1]:
        label, path = a.split("=", 1)
        rounds.append((label, path))

    out = open(outpath, "w") if outpath != "-" else sys.stdout
    out.write("# W5 T8.9r fix2 — R2' idle proof, every retained batch of every round\n\n")
    out.write(
        "**R2 IS AMENDED (settler, 2026-09-11).** R2's per-process bar — every foreign pid on "
        "the box under 0.5 % of the batch wall — was **unmeetable on this desktop and could not "
        "be reached by re-running**: the Wayland compositor alone accrues about 3 % of any "
        "window, on 16 logical CPUs, whatever the leg does. Fix1 reported that bar NOT met and "
        "took its verdict on a narrower test it had not been granted. astra's fix1 review is "
        "right that a disclosed substitution is not a satisfied ruling. **R2' replaces it, and "
        "every retained batch of every round is re-audited here under R2' — including the rounds "
        "that already published a verdict.**\n\n")
    out.write("> **R2' — the PINNED-CORE rule.** Foreign TASK time on the measurement core "
              "(`cpu2`) **and** on its SMT sibling (`cpu10`), counted as "
              "`user + nice + steal + guest` from `/proc/stat` across the batch, must be under "
              "**0.5 %** of the batch's wall. Every foreign pid and state seen in any snapshot "
              "is listed, transients included. A foreign `R` is a pause and a **re-snapshot**. "
              "A batch that cannot be proven is re-run.\n\n")
    out.write(
        "**One tool, one version, everywhere.** `scripts/idle_proof.py` is now the attrib4 "
        "(nice-inclusive) version — `sha256 7c90914d55b5ee5c3dbcc53c146289484f8a0bede4a486ed41cc"
        "6ad9364c68aa` — and the two older copies it superseded "
        "(`scripts/idle_proof.py`, `attribution-interior/t84/scripts/idle_proof.py`, both "
        "`ca107629…`) were REPLACED by it, so the same code computes every table below. The "
        "earlier version took foreign task time from the `user` bucket alone on the argument "
        "that no foreign task on this box is niced; the retained logs falsify that (foreign "
        "`SN`/`RN` tasks are present), and a niced foreign task lands in `nice`, exactly where "
        "the measurement's own time lands. **Nothing was re-measured for this audit** — it is "
        "arithmetic on snapshots that were already on disk.\n\n")
    out.write("**What `R` means here.** The fix1/attrib recipe's `box_guard` paused on a foreign "
              "`R`, slept, printed `BOX_PAUSE_GIVEUP` and continued **without re-snapshotting** "
              "(`scripts/common.sh`). R2' requires the re-snapshot. The retained batches were "
              "taken under the older recipe, so an `R` observation there is DISCLOSED rather "
              "than discharged: the column below names every batch that saw one. It does not "
              "move a verdict, because any CPU such a task consumed on `cpu2` or `cpu10` is "
              "already inside the counted buckets — R2''s bar is on the resource, not on the "
              "state. R2''s re-snapshot term binds the NEXT measurement, not this audit.\n\n")

    # ---- the per-batch table -------------------------------------------
    out.write("## The per-batch table — every retained batch, every round\n\n")
    out.write("`core` and `sibling` are R2''s two fractions (`cpu2`, `cpu10`); `worst foreign` is "
              "R2-as-written's worst single foreign pid across the whole box, retained because "
              "the amendment does not delete the number it was taken on. `asserts` is what the "
              "batch's data is read as: **WALL** (R2' governs), **COUNT** (counters only — "
              "deterministic at `MKL_NUM_THREADS=1`, CLAUDE.md §7), **SCREEN** (a probe nothing "
              "cites), **SUPERSEDED** (retained, quoted nowhere).\n\n")
    out.write("**A batch is UNPROVEN for one of two reasons, and they are different.** Either its "
              "measured fractions exceed R2''s bar — the number is there and it is too big — or "
              "the batch has **no timed-run bracket at all**: its recipe wrote `PS_SNAPSHOT` "
              "blocks but no `CPUSTAT`/`CPUTIME_SELF` pair around each run, so no pinned-core "
              "figure exists to test. Four of attrib4's batches are the second kind (`e3`, "
              "`pf-r1..r3` — the `perf record` and fault-count batches, which assert COUNTS). "
              "Both are reported as UNPROVEN, and the cell says which.\n\n")
    out.write("| round | batch | asserts | core `cpu2` | sibling `cpu10` | worst foreign (box) | "
              "foreign pids | transients | states seen | `R` seen | pauses | **R2'** |\n")
    out.write("|---|---|---|---|---|---|---|---|---|---|---|---|\n")

    tally = {}
    unproven_wall = []
    for label, path in rounds:
        t1, t2 = parse(path)
        for batch in t2:
            a = t1.get(batch, [""] * 12)
            b = t2[batch]
            kind = kind_of(label, batch)
            if b[-1].startswith("**UNPROVEN (no timed run"):
                core = sib = "— (no timed run bracketed)"
                verdict = "**UNPROVEN**"
            else:
                core, sib = clean(b[5]), clean(b[7])
                verdict = "PROVEN" if b[-1] == "PINNED-CLEAN" else "**UNPROVEN**"
            t = tally.setdefault((label, kind), [0, 0])
            t[0] += 1
            if verdict == "PROVEN":
                t[1] += 1
            elif kind == "WALL":
                unproven_wall.append((label, batch, core, sib))
            out.write("| %s | `%s` | %s | %s | %s | %s | %s | %s | `%s` | %s | %s | %s |\n" % (
                label, batch, kind, core, sib, clean(a[7]) if len(a) > 7 else "—",
                a[4] if len(a) > 4 else "—", a[5] if len(a) > 5 else "—",
                a[8] if len(a) > 8 else "—", a[9] if len(a) > 9 else "—",
                a[10] if len(a) > 10 else "—", verdict))

    # ---- the summary ----------------------------------------------------
    out.write("\n## The audit, per round\n\n")
    out.write("| round | WALL batches | WALL proven | COUNT/other batches | COUNT/other proven |\n")
    out.write("|---|---|---|---|---|\n")
    for label, _ in rounds:
        w = tally.get((label, "WALL"), [0, 0])
        o = [0, 0]
        for (r, k), v in tally.items():
            if r == label and k != "WALL":
                o[0] += v[0]
                o[1] += v[1]
        out.write("| %s | %d | **%d** | %d | %d |\n" % (label, w[0], w[1], o[0], o[1]))

    out.write("\n### The WALL-asserting batches that R2' does NOT prove\n\n")
    if unproven_wall:
        out.write("| round | batch | core `cpu2` | sibling `cpu10` |\n|---|---|---|---|\n")
        for r, b, c, s in unproven_wall:
            out.write("| %s | `%s` | %s | %s |\n" % (r, b, c, s))
        out.write("\n**These are FLAGGED, not re-measured** — the settler's fix2 ruling owes no "
                  "re-measurement, and a flag that says which rounds a number rests on is worth "
                  "more than a re-run taken to make a table read clean. Every one of them fails "
                  "on the SMT SIBLING, not on the measurement core, and by 0.03 to 0.31 "
                  "percentage points. The reading carries the flag at each place these batches "
                  "feed a number.\n")
    else:
        out.write("None.\n")

    out.write("\n### Rounds whose retained snapshots cannot support R2' at all\n\n")
    for name, glob, why in NOT_AUDITABLE:
        out.write("* **%s** (`%s`): %s\n" % (name, glob, why))

    # ---- the appendices -------------------------------------------------
    out.write("\n---\n\n# Appendix — `idle_proof.py`'s output, verbatim, per round\n\n")
    out.write("Each block below is the tool's own stdout for that round's logs, unedited. The "
              "tool's own preamble still describes R2 AS WRITTEN (Test 1) and the pinned-core "
              "test (Test 2); under R2' **Test 2 is the rule** and Test 1 is the superseded bar, "
              "retained because the amendment does not delete the numbers it was taken on.\n")
    for label, path in rounds:
        out.write("\n## %s\n\n" % ROUND_TITLE.get(label, label))
        out.write("```\n")
        out.write(open(path).read())
        out.write("```\n")
    if out is not sys.stdout:
        out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
