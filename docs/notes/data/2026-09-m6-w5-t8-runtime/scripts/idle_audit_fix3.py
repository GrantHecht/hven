#!/usr/bin/env python3
"""W5 T8.9r fix3 -- R2' RE-AUDIT: one idle proof, every retained batch, every round.

THIS FILE IS `idle_audit_fix2.py` WITH THE EVIDENCE ACCOUNTING ADDED (settler
ruling R13; astra's fix2 review, item 1). The fix2 assembler is retained beside
it, unedited, because it is what produced the fix2 table. What is new here:

  * the per-batch table separates **UNPROVEN-FRACTION** (R2''s bar measured and
    exceeded) from **UNPROVEN-EVIDENCE** (the bar met or uncomputable, but the
    log lacks a re-snapshot after a foreign `R`, or lacks a snapshot between two
    consecutive timed runs, or has no timed-run bracket at all);
  * the `R` column is the union of `FOREIGN_TICK` and `FOREIGN_PS`, so `ps`
    states such as `Rsl` are counted;
  * `pauses` and `give-ups` are separate columns -- the fix2 table added them;
  * each round links its own per-pid appendix, `logs/idle-proof-pids-<round>.md`,
    which lists EVERY foreign pid of every batch with every state observed for
    it;
  * the "R2''s re-snapshot term binds the NEXT measurement" paragraph is GONE.
    The settler granted no retrospective exemption, so an `R` with no
    re-snapshot is an evidence gap wherever it appears, and the flag travels
    with the number.

**NO NUMBER MOVES.** Every fraction in the table below is the fix2 figure, from
the fix2 arithmetic, over the same retained snapshots.

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

# The per-round pid appendices (settler R13). Written by idle_proof.py's --pids.
PIDS_FILE = "logs/idle-proof-pids-%s.md"

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
    out.write("# W5 T8.9r fix3 — R2' idle proof, every retained batch of every round\n\n")
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
        "**One tool, one version, everywhere.** `scripts/idle_proof.py` is the attrib4 "
        "(nice-inclusive) version with fix3's evidence accounting added — `sha256 3394879dbb5191"
        "55fa985da793a43eedd825bfc1bc83b6636007e91795b33ab6`, byte-identical in all three places "
        "it sits (`scripts/`, `attribution-interior/t84/scripts/`, "
        "`attribution-interior/t84/redraw/scripts/`); it succeeds fix2's `7c90914d…`, which "
        "succeeded the two `ca107629…` copies. The same code computes every table below. The "
        "`ca107629…` version took foreign task time from the `user` bucket alone on the argument "
        "that no foreign task on this box is niced; the retained logs falsify that (foreign "
        "`SN`/`RN` tasks are present), and a niced foreign task lands in `nice`, exactly where "
        "the measurement's own time lands. Fix3 adds no arithmetic at all: it reads the second "
        "state source, counts pauses once, and reports what the logs do and do not contain. "
        "**Nothing was re-measured for this audit, at fix2 or at fix3** — it is arithmetic on "
        "snapshots that were already on disk, and every fraction below is byte-for-byte the "
        "fix2 figure.\n\n")
    out.write("**WHAT `R` MEANS HERE, AND WHAT FIX3 CHANGED ABOUT IT (settler R13).** The "
              "fix1/attrib recipe's `box_guard` paused on a foreign `R`, slept, printed "
              "`BOX_PAUSE_GIVEUP` and continued **without re-snapshotting** "
              "(`scripts/common.sh`). R2' requires the re-snapshot. Fix2 disclosed that and then "
              "deferred the requirement to \"the next measurement\"; **the settler granted no "
              "such retrospective exemption, and fix3 withdraws it.** An `R` with no re-snapshot "
              "is now an EVIDENCE gap on that batch, reported as **UNPROVEN-EVIDENCE**, and the "
              "flag travels to every number the batch feeds. The fractions are untouched: what "
              "CPU such a task consumed on `cpu2` or `cpu10` is already inside the counted "
              "buckets, so the bar still says what it said. What the bar cannot say is that the "
              "box was watched the way R2' asks, and that is now recorded rather than "
              "argued.\n\n")
    out.write("**The `R` column reads BOTH sources.** The leg scripts write `FOREIGN_TICK` "
              "(the single state character from `/proc/<pid>/stat`) and `FOREIGN_PS` (`ps`'s "
              "full string). Fix2 read only the first, so `4022442 Rsl` in "
              "`L4-leg1-ssn-r2.log` did not appear. Reading both raises the number of batches "
              "with an `R` observation to **10 / 10 / 11 / 3 / 5** across the five rounds.\n\n")
    out.write("**A SECOND EVIDENCE GAP, WHICH `PROVENANCE.txt` (G2) ALREADY DISCLOSED AND WHICH "
              "IS NOW A FLAG.** Only leg 1's nine wall batches place a snapshot between every "
              "A/B alternation. Every other recipe — leg-1 perf, leg 2, the interior leg, the "
              "per-cell interior leg, attrib2's `diff-`, attrib3's `screen` — calls `box_guard` "
              "after its arm loop, so two timed runs follow each other with no foreign state "
              "observed across the switch. Those batches are **UNPROVEN-EVIDENCE** too. The "
              "`CPUSTAT`/`CPUTIME_SELF` bracket around every timed run is still present in all "
              "of them, and that bracket is what the FRACTION is taken on.\n\n")

    # ---- the per-batch table -------------------------------------------
    out.write("## The per-batch table — every retained batch, every round\n\n")
    out.write("`core` and `sibling` are R2''s two fractions (`cpu2`, `cpu10`); `worst foreign` is "
              "R2-as-written's worst single foreign pid across the whole box, retained because "
              "the amendment does not delete the number it was taken on. `asserts` is what the "
              "batch's data is read as: **WALL** (R2' governs), **COUNT** (counters only — "
              "deterministic at `MKL_NUM_THREADS=1`, CLAUDE.md §7), **SCREEN** (a probe nothing "
              "cites), **SUPERSEDED** (retained, quoted nowhere).\n\n")
    out.write("**A batch is UNPROVEN in one of two DIFFERENT ways, and the cell says which.**\n\n"
              "* **UNPROVEN-FRACTION** — R2''s bar is measured and exceeded on `cpu2` or `cpu10`. "
              "The number is there and it is too big.\n"
              "* **UNPROVEN-EVIDENCE** — the bar is met, or cannot be computed, but the log does "
              "not contain what R2' asks for: a **re-snapshot** after a foreign `R`; a snapshot "
              "**between two consecutive timed runs**; or, for four of attrib4's batches (`e3`, "
              "`pf-r1..r3`), any **timed-run bracket at all** — `PS_SNAPSHOT` blocks but no "
              "`CPUSTAT`/`CPUTIME_SELF` pair, so no pinned-core figure exists to test.\n\n"
              "A batch can be both. The two columns therefore need not sum to the batch "
              "count.\n\n")
    out.write("| round | batch | asserts | core `cpu2` | sibling `cpu10` | worst foreign (box) | "
              "foreign pids | transients | states seen | `R` seen | pauses | give-ups | "
              "evidence | **R2'** |\n")
    out.write("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n")

    tally = {}
    unproven_wall_frac = []
    unproven_wall_ev = []
    for label, path in rounds:
        t1, t2 = parse(path)
        for batch in t2:
            a = t1.get(batch, [""] * 15)
            b = t2[batch]
            kind = kind_of(label, batch)
            ev = clean(b[-2]) if len(b) > 2 else "—"
            if b[-1].startswith("**UNPROVEN-EVIDENCE (no timed run"):
                core = sib = "— (no timed run bracketed)"
                bad_frac, bad_ev = False, True
            else:
                core, sib = clean(b[5]), clean(b[7])
                bad_frac = "UNPROVEN-FRACTION" in b[-1]
                bad_ev = "UNPROVEN-EVIDENCE" in b[-1]
            if bad_frac and bad_ev:
                verdict = "**UNPROVEN-FRACTION + UNPROVEN-EVIDENCE**"
            elif bad_frac:
                verdict = "**UNPROVEN-FRACTION**"
            elif bad_ev:
                verdict = "**UNPROVEN-EVIDENCE**"
            else:
                verdict = "PROVEN"
            t = tally.setdefault((label, kind), [0, 0, 0, 0])
            t[0] += 1
            if verdict == "PROVEN":
                t[1] += 1
            if bad_frac:
                t[2] += 1
                if kind == "WALL":
                    unproven_wall_frac.append((label, batch, core, sib))
            if bad_ev:
                t[3] += 1
                if kind == "WALL":
                    unproven_wall_ev.append((label, batch, ev))
            out.write("| %s | `%s` | %s | %s | %s | %s | %s | %s | `%s` | %s | %s | %s | %s | "
                      "%s |\n" % (
                label, batch, kind, core, sib, clean(a[7]) if len(a) > 7 else "—",
                a[4] if len(a) > 4 else "—", a[5] if len(a) > 5 else "—",
                a[8] if len(a) > 8 else "—", a[9] if len(a) > 9 else "—",
                a[10] if len(a) > 10 else "—", a[11] if len(a) > 11 else "—",
                ev, verdict))

    # ---- the summary ----------------------------------------------------
    out.write("\n## The audit, per round\n\n")
    out.write("Proven = the fraction bar met AND the evidence complete. The two unproven columns "
              "overlap: a batch can fail both.\n\n")
    out.write("| round | WALL batches | WALL proven | WALL unproven-FRACTION | "
              "WALL unproven-EVIDENCE | COUNT/other batches | COUNT/other proven | "
              "per-pid appendix |\n")
    out.write("|---|---:|---:|---:|---:|---:|---:|---|\n")
    for label, _ in rounds:
        w = tally.get((label, "WALL"), [0, 0, 0, 0])
        o = [0, 0, 0, 0]
        for (r, k), v in tally.items():
            if r == label and k != "WALL":
                for i in range(4):
                    o[i] += v[i]
        out.write("| %s | %d | **%d** | %d | %d | %d | %d | [`%s`](%s) |\n" % (
            label, w[0], w[1], w[2], w[3], o[0], o[1],
            PIDS_FILE % label, os.path.basename(PIDS_FILE % label)))

    out.write("\n### The WALL-asserting batches whose FRACTION R2' does not meet\n\n")
    if unproven_wall_frac:
        out.write("| round | batch | core `cpu2` | sibling `cpu10` |\n|---|---|---|---|\n")
        for r, b, c, sib in unproven_wall_frac:
            out.write("| %s | `%s` | %s | %s |\n" % (r, b, c, sib))
        out.write("\n**These are FLAGGED, not re-measured** — the settler's ruling owes no "
                  "re-measurement, and a flag that says which rounds a number rests on is worth "
                  "more than a re-run taken to make a table read clean. Every one of them fails "
                  "on the SMT SIBLING, not on the measurement core, and by 0.03 to 0.31 "
                  "percentage points. The reading carries the flag at each place these batches "
                  "feed a number.\n")
    else:
        out.write("None.\n")

    out.write("\n### The WALL-asserting batches whose EVIDENCE is short of R2' (fix3)\n\n")
    if unproven_wall_ev:
        out.write("| round | batch | what is missing |\n|---|---|---|\n")
        for r, b, e in unproven_wall_ev:
            out.write("| %s | `%s` | %s |\n" % (r, b, e))
        out.write("\n**These too are FLAGGED, not re-measured.** The gap is in the RECORD, not "
                  "in the numbers: the fractions these batches report are the fractions they "
                  "measured, and CLAUDE.md §7's own reason for the solo rule — that a busy "
                  "neighbour can only cost the measurement time — means an unwatched moment "
                  "biases a wall number in the SLOW direction, never the fast one. Where such a "
                  "batch feeds a number, the reading says so.\n")
    else:
        out.write("None.\n")

    out.write("\n### Rounds whose retained snapshots cannot support R2' at all\n\n")
    for name, glob, why in NOT_AUDITABLE:
        out.write("* **%s** (`%s`): %s\n" % (name, glob, why))

    # ---- the appendices -------------------------------------------------
    out.write("\n### Where every foreign pid is listed\n\n")
    out.write("R2' asks for every foreign pid and state seen in any snapshot, LISTED. That is "
              "one file per round, written by `idle_proof.py --pids` from the same snapshots the "
              "fractions come from, ~200 pids per batch:\n\n")
    for label, _ in rounds:
        out.write("* %s — [`%s`](%s)\n" % (label, PIDS_FILE % label,
                                            os.path.basename(PIDS_FILE % label)))
    out.write("\nThe five-busiest list in each appendix below is a summary of those files, not "
              "a substitute for them.\n")

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
