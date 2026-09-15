#!/usr/bin/env python3
"""W5 T8.9r -- the group-1 runtime comparator (fix round 1, 2026-09-11).

Reproduces every number in reading.md from raw/ and perf/ alone. No network,
no state, no writes outside --out. Exit status:

    0   every input present, every manifest satisfied, nothing unparseable
    1   a MANIFEST failure -- a required round, arm or leg is missing
    2   any other problem (unreadable input, unparseable perf capture, ...)

The caller worst-folds that status into LEG_RC, so a comparator that cannot
read its inputs fails the leg instead of silently reporting an empty table.

THE RULES IT IMPLEMENTS are docs/notes/2026-09-m6-w5-t6-ownership.md 11.1/11.2:

  FLAT        per-cell median ratio in [0.99, 1.01] for every cell AND the
              corpus figure within +/-0.5 %.
  MOVED       corpus beyond +/-1 %, OR >= 3 cells outside [0.98, 1.02] with
              the same sign in all three alternating runs.
  UNRESOLVED  anything that is neither.

  LAYOUT-MOVED  instructions AND branches identical within 1e-4, with the
                cycle delta accounted for by the icache / branch-miss deltas.
  WORK-MOVED    instructions UP outside that identity band. THE VETO.

AGGREGATION (11.3), and it is not a mean of ratios: per cell, the MEDIAN of
the alternating runs per arm; the ratio of those two medians; the corpus
figure is the SUM of the per-cell medians, and the corpus ratio is the ratio
of the two sums.

================================================================================
TWO RULES WERE FIXED BY THE SETTLER BEFORE THIS ROUND'S RUNS AND ARE WRITTEN
HERE, IN THE INSTRUMENT, RATHER THAN APPLIED TO THE NUMBERS AFTERWARDS.
Both are recorded in PROVENANCE.txt's fix1 block with the same wording, and
logs/F0-predeclaration.log records the hash of this file and of PROVENANCE.txt
at the moment they were fixed -- which is BEFORE the first timed run of the
round. (astra I5 (a) and (e); settler rulings R1 and R3, 2026-09-11.)

  R1 -- THE VETO'S "REPRODUCIBLE SLOWDOWN" IS THE BANDED READING.
  11.1's sentence "a reproducible slowdown -- three solo runs, same sign -- on
  ANY cell" admits a strict reading (any cell slower in all three paired
  rounds, however small) and a banded one (a cell OUTSIDE 0.99-1.01, slower in
  all three). A fair coin gives ~3.4 same-sign cells of 27 by chance, so the
  strict count fires on sampling alone and cannot be the trigger.
  **A cell is a reproducible slowdown when its median ratio is outside
  0.99-1.01 AND it is slower in all three rounds.** This tool prints BOTH
  counts, CLASSIFIES ON THE BANDED ONE, and says so on the line it prints.
  The strict count is informational.

  R3 -- THE INTERIOR LEG'S FIRST-ROW RULE, PRE-DECLARED.
  The interior leg runs in ONE process and writes its rows in order. The FIRST
  row each `--engine interior` process writes is that process's warm-up -- MKL's
  first call, the allocator's first growth, the page faults for the whole
  working set -- and is EXCLUDED FROM THE BAND BY POSITION, not by name and not
  after looking at its value. It is reported separately with its own three
  paired ratios, and the corpus figure is printed BOTH ways: WITHOUT it
  (PRIMARY, by this rule) and WITH it. The rule keys on the row's POSITION in
  every raw CSV of the leg; if the arms or the rounds disagree about which row
  came first, that is a MANIFEST failure, not a choice for this tool to make.

FOUR DEFECTS OF THE ROUND-1 INSTRUMENT ARE FIXED HERE (astra I5):
  (a) the interior primary population is the R3 one, and both figures print;
  (b) leg 2's noise floor is the MAXIMUM per-(mode, trace) same-arm pass A /
      pass B disagreement -- never a median across different modes, traces or
      repeat counts, which is an average of incomparable instruments;
  (c) pass B prints NO verdict wherever pass A carries none -- the floor gate
      applies to both passes, and an unmatched population carries neither;
  (d) the three-round manifest is ENFORCED: a missing round exits 1.
"""

import argparse
import csv
import math
import os
import re
import statistics
import sys

FLAT_CELL = (0.99, 1.01)
MOVED_CELL = (0.98, 1.02)
CORPUS_FLAT = 0.005
CORPUS_MOVED = 0.01
IDENTITY = 1.0e-4
ROUNDS = ("r1", "r2", "r3")

PROBLEMS = []
MANIFEST = []

# ---------------------------------------------------------------------------
# THE EXPLICIT MANIFEST (settler ruling R9, fix2; astra's fix1 review item 4).
#
# The fix1 manifest checked ROUNDS within whatever populations it FOUND ON DISK:
# it listed `perf/leg1/<mode>/` to learn which cells existed, and `raw/leg2/` to
# learn which combinations existed, and then demanded r1/r2/r3 of each. A whole
# missing cell, or a whole missing (mode, trace) combination, therefore produced
# a SMALLER discovered population and exit 0 -- the tool reported nothing at all
# about the thing that was not there. A manifest that is discovered from the
# data cannot detect a missing population; it can only detect a hole inside one
# it was told about.
#
# So the expected population is WRITTEN DOWN here, in full, and checked against
# what is on disk before anything is scored. Every member below is a member this
# leg RAN; a missing one is a MANIFEST failure naming it, and exit 1.
#
# FIX ROUND 3 -- THE MANIFEST IS BY IDENTITY, NOT BY COUNT (settler ruling R14;
# astra's fix2 review, item 3). The fix2 manifest declared leg 1's population as
# THE NUMBER 27. A number cannot tell a substitution from the real thing:
# renaming a required cell to an undeclared name consistently across all six
# files kept the count at 27 and the tool exited 0, having scored a corpus
# nobody declared. Three holes are closed here:
#
#   (a) LEG1_CELLS is the 27 cell ids themselves, taken from the committed
#       t10b control's key list (`bench/baselines/2026-09-02-t10b-ipm/
#       ipm_baseline.csv`, in that file's row order). Every leg-1 raw CSV must
#       carry exactly that key set -- so a cell missing from ONE ROUND is a
#       MANIFEST failure naming the cell and the file, not a downstream
#       "did not carry every cell of the earlier runs" PROBLEM at exit 2.
#   (b) the interior arms are declared BY SHA, with the retained provenance
#       token that identifies each -- the schema width the arm's tree emits and
#       the build directory its invocation line records. Substituting one arm's
#       CSVs for another's is then a MANIFEST failure, not a silently different
#       comparison.
#   (c) the per-cell interior perf population is checked as (cell, arm, round)
#       over the DECLARED cells, so the absence of the whole
#       `perf/interior_cells` tree is a MANIFEST failure naming it -- at fix2
#       that directory's absence skipped the check block entirely and surfaced
#       as exit 2.
#
# Exit 1 is the manifest. Exit 2 is reserved for usage and read errors.
# ---------------------------------------------------------------------------
LEG1_MODES = ("ipm", "ssn", "walk")
LEG1_ARMS = ("base", "head")
# The U0 corpus, scored per mode: the 27 cell ids of the committed t10b control,
# in that baseline's own row order.
LEG1_CELLS = (
    "f7_n1000_bound_neutral", "f7_n1000_bound_physics", "f7_n1000_bound_corrupted",
    "f7_n1000_bound_activity", "f7_n1000_bound_warm",
    "f7_n2000_bound_neutral", "f7_n2000_bound_physics", "f7_n2000_bound_corrupted",
    "f7_n2000_bound_activity", "f7_n2000_bound_warm",
    "f7_n5000_bound_neutral", "f7_n5000_bound_physics", "f7_n5000_bound_corrupted",
    "f7_n5000_bound_activity", "f7_n5000_bound_warm",
    "f7_n10000_bound_neutral", "f7_n10000_bound_physics", "f7_n10000_bound_corrupted",
    "f7_n10000_bound_activity", "f7_n10000_bound_warm",
    "f7_n20000_bound_neutral", "f7_n20000_bound_physics", "f7_n20000_bound_corrupted",
    "f7_n20000_bound_activity", "f7_n20000_bound_warm",
    "f7_n1000_path_warm", "f7_n800_path_warm")
# Listed in the order the tables are EMITTED in (lexicographic, which is the
# order round 1's `sorted(...)` discovery produced), so writing the population
# down does not reorder a single line of the output.
LEG1_PERF_CELLS = ("f7_n1000_bound_neutral", "f7_n20000_bound_neutral",
                   "f7_n5000_bound_neutral")
LEG1_PERF_PASSES = ("passA", "passB")
LEG2_COMBOS = (("ipm", "off"), ("ipm", "sink"), ("ssn", "off"), ("ssn", "sink"),
               ("walk", "off"), ("walk", "sink"))
INTERIOR_ARMS = ("arm102", "armb98", "head")
# R14: the arms BY SHA, each with the token its retained provenance carries.
# `schema` is the column count the arm's own tree emits (19 at 102f729, 31 from
# b9848bf on); `build` is the extraction directory the invocation line records.
# Both are written by the measured binary into every CSV it produced, so a
# substituted arm cannot satisfy its own row of this table.
INTERIOR_ARM_IDENTITY = {
    "arm102": dict(sha="102f729", schema="19", build="arm-base"),
    "armb98": dict(sha="b9848bf", schema="31", build="arm-interior-base"),
    "head":   dict(sha="e51a7e0", schema="31", build="arm-head"),
}
INTERIOR_CELLS = ("f7_n10000_bound_neutral", "f7_n10000_bound_physics",
                  "f7_n1000_bound_neutral", "f7_n1000_bound_physics",
                  "f7_n20000_bound_neutral", "f7_n20000_bound_physics",
                  "f7_n2000_bound_neutral", "f7_n2000_bound_physics",
                  "f7_n5000_bound_neutral", "f7_n5000_bound_physics",
                  "hs071_x1_fixed")

# The interior counter columns COMMON to the 19-column schema at 102f729 and
# the 31-column schema at b9848bf / e51a7e0 (R4). `wall_s` is compared as a
# TIME, separately; these twelve must be identical.
INTERIOR_COMMON_COUNTERS = ("status", "iter_num", "obj_val", "kkt_inf", "barr_inf", "econ_inf",
                            "icon_inf", "factorizations", "solves", "analyses", "soc_steps",
                            "watchdog_activations")


def problem(msg):
    PROBLEMS.append(msg)


def manifest_fail(msg):
    MANIFEST.append(msg)


def read_rows(path):
    """Rows of one bench CSV, '#' provenance lines dropped, header keyed, IN FILE ORDER."""
    if not os.path.exists(path):
        problem("missing raw file: %s" % path)
        return []
    with open(path, newline="") as fh:
        lines = [ln for ln in fh if not ln.startswith("#")]
    if not lines:
        problem("empty raw file: %s" % path)
        return []
    return list(csv.DictReader(lines))


def first_row_key(path, key_col="cell_id"):
    rows = read_rows(path)
    return rows[0].get(key_col) if rows else None


def cell_series(paths, key_col, val_col):
    """{cell: [value per run]} over the runs named by `paths`, in order."""
    series = {}
    for idx, path in enumerate(paths):
        rows = read_rows(path)
        for row in rows:
            key = row.get(key_col)
            raw = row.get(val_col)
            if key is None or raw is None:
                problem("%s: row without %s/%s" % (path, key_col, val_col))
                continue
            try:
                val = float(raw)
            except ValueError:
                problem("%s: %s=%r is not a number" % (path, val_col, raw))
                continue
            series.setdefault(key, []).append(val)
        if rows and len(set(len(v) for v in series.values())) > 1:
            problem("%s: run %d did not carry every cell of the earlier runs" % (path, idx + 1))
    return series


def band_of(cells, corpus_ratio):
    outside_flat = [c for c in cells if not (FLAT_CELL[0] <= c["ratio"] <= FLAT_CELL[1])]
    outside_moved_same_sign = [
        c for c in cells
        if not (MOVED_CELL[0] <= c["ratio"] <= MOVED_CELL[1])
        and (c["slower_all"] or c["faster_all"])]
    if abs(corpus_ratio - 1.0) > CORPUS_MOVED or len(outside_moved_same_sign) >= 3:
        band = "MOVED"
    elif not outside_flat and abs(corpus_ratio - 1.0) <= CORPUS_FLAT:
        band = "FLAT"
    else:
        band = "UNRESOLVED"
    return band, outside_flat, outside_moved_same_sign


def aggregate(cells, label, only_base=(), only_head=()):
    base_corpus = sum(c["base_median"] for c in cells)
    head_corpus = sum(c["head_median"] for c in cells)
    corpus_ratio = head_corpus / base_corpus if base_corpus else float("nan")
    band, outside_flat, outside_moved = band_of(cells, corpus_ratio)
    # R1: the veto classification is the BANDED count. The strict count is
    # printed beside it and is informational.
    veto_strict = [c["cell"] for c in cells if c["slower_all"]]
    veto_banded = [c["cell"] for c in cells if c["slower_all"] and c["ratio"] > FLAT_CELL[1]]
    return dict(label=label, cells=cells, base_corpus=base_corpus, head_corpus=head_corpus,
                corpus_ratio=corpus_ratio, band=band, outside_flat=outside_flat,
                outside_moved_same_sign=outside_moved, veto_strict=veto_strict,
                veto_banded=veto_banded, only_base=list(only_base), only_head=list(only_head))


def score(base_paths, head_paths, key_col="cell_id", val_col="wall_s", label=""):
    base = cell_series(base_paths, key_col, val_col)
    head = cell_series(head_paths, key_col, val_col)
    keys = [k for k in base if k in head]
    only_base = sorted(set(base) - set(head))
    only_head = sorted(set(head) - set(base))
    cells = []
    for key in keys:
        b, h = base[key], head[key]
        if len(b) != len(h):
            problem("%s %s: %d base runs vs %d head runs" % (label, key, len(b), len(h)))
        bm, hm = statistics.median(b), statistics.median(h)
        ratio = hm / bm if bm else float("nan")
        paired = [hv / bv for hv, bv in zip(h, b) if bv]
        slower_all = len(paired) == len(b) and all(p > 1.0 for p in paired)
        faster_all = len(paired) == len(b) and all(p < 1.0 for p in paired)
        cells.append(
            dict(cell=key, base_runs=b, head_runs=h, base_median=bm, head_median=hm,
                 ratio=ratio, paired=paired, slower_all=slower_all, faster_all=faster_all))
    cells.sort(key=lambda c: c["cell"])
    return aggregate(cells, label, only_base, only_head)


PERF_RE = re.compile(r"^\s*([0-9][0-9,]*|<not counted>|<not supported>)\s+([A-Za-z0-9_.:\-]+)\s*$")


def read_perf(path):
    """{event: count} from one `perf stat` capture."""
    if not os.path.exists(path):
        problem("missing perf file: %s" % path)
        return {}
    out = {}
    with open(path) as fh:
        for line in fh:
            m = PERF_RE.match(line.rstrip())
            if not m:
                continue
            val, ev = m.group(1), m.group(2)
            if val.startswith("<"):
                problem("%s: event %s was %s -- multiplexed or unsupported" % (path, ev, val))
                continue
            out[ev] = int(val.replace(",", ""))
    if not out:
        problem("%s: no perf events parsed" % path)
    return out


def perf_medians(paths):
    per_event = {}
    for path in paths:
        for ev, val in read_perf(path).items():
            per_event.setdefault(ev, []).append(val)
    return {ev: statistics.median(vals) for ev, vals in per_event.items()}, per_event


def classify_perf(base_paths, head_paths):
    bmed, braw = perf_medians(base_paths)
    hmed, hraw = perf_medians(head_paths)
    ratios = {}
    for ev in sorted(set(bmed) & set(hmed)):
        ratios[ev] = hmed[ev] / bmed[ev] if bmed[ev] else float("nan")
    verdict = "UNCLASSIFIED"
    ins = ratios.get("instructions:u")
    bra = ratios.get("branches:u")
    if ins is not None:
        identical = abs(ins - 1.0) <= IDENTITY and (bra is None or abs(bra - 1.0) <= IDENTITY)
        if ins > 1.0 + IDENTITY:
            verdict = "WORK-MOVED (instructions UP) -- THE VETO"
        elif ins < 1.0 - IDENTITY:
            verdict = "INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)"
        elif identical:
            verdict = "instructions+branches inside the 1e-4 identity band"
        else:
            verdict = "instructions inside the band, branches outside -- UNRESOLVED"
    return dict(ratios=ratios, base=bmed, head=hmed, base_raw=braw, head_raw=hraw,
                verdict=verdict)


def same_arm_disagreement(files, arms=("base", "head")):
    """{arm: |passA/passB - 1|} on `instructions:u` within ONE population."""
    out = {}
    for arm in arms:
        fa = [f for f in files if os.path.basename(f).startswith("passA-" + arm)]
        fb = [f for f in files if os.path.basename(f).startswith("passB-" + arm)]
        if not fa or not fb:
            continue
        ma, _ = perf_medians(fa)
        mb, _ = perf_medians(fb)
        if ma.get("instructions:u") and mb.get("instructions:u"):
            out[arm] = abs(ma["instructions:u"] / mb["instructions:u"] - 1.0)
    return out


def no_verdict_text(floor, original):
    return ("NO VERDICT AT 1e-4 -- this population's own two passes of the SAME binary disagree by "
            "%.4f %%, above the identity band; the ratio below is reported, not classified "
            "(original label: %s)" % (100.0 * floor, original))


def apply_floor(perf, floor):
    """Gate a perf verdict on a noise floor computed by the CALLER.

    The floor is passed in rather than derived here, because what counts as
    "this leg's own noise" is a property of the POPULATION the verdict is about
    (astra I5 (b)): leg 1 scores one cell at a time and its floor is that
    cell's; leg 2's populations are six incomparable (mode, trace, N)
    combinations and its floor is the WORST of them, never their median.
    """
    perf["floor"] = floor
    if floor > IDENTITY:
        perf["verdict"] = no_verdict_text(floor, perf["verdict"])
    return perf


def suppress_verdict(perf, why):
    perf["verdict"] = "NO VERDICT -- %s (the ratios below are reported, not classified)" % why
    return perf


def fmt_cells(res, out):
    out.write("| cell | base median (s) | head median (s) | ratio | paired per round |\n")
    out.write("|---|---|---|---|---|\n")
    for c in res["cells"]:
        out.write("| `%s` | %.6f | %.6f | %.4f | %s |\n" % (
            c["cell"], c["base_median"], c["head_median"], c["ratio"],
            " ".join("%.4f" % p for p in c["paired"])))


def emit_perf_table(perf, out):
    out.write("| event | base median | head median | ratio |\n|---|---|---|---|\n")
    for ev in sorted(perf["ratios"]):
        out.write("| `%s` | %d | %d | %.5f |\n" % (ev, perf["base"][ev], perf["head"][ev],
                                                   perf["ratios"][ev]))


def emit(res, out, perf=None):
    out.write("\n### %s\n\n" % res["label"])
    if res["only_base"]:
        out.write("Keys only in the BASE arm (no band): %s\n\n"
                  % ", ".join("`%s`" % k for k in res["only_base"]))
    if res["only_head"]:
        out.write("Keys only in the HEAD arm (head-only, informational, no band): %s\n\n"
                  % ", ".join("`%s`" % k for k in res["only_head"]))
    out.write("corpus base %.4f s -> head %.4f s, **ratio %.4f** (%+.3f %%); cells compared %d; "
              "outside 0.99-1.01: **%d**; band **%s**\n\n" % (
                  res["base_corpus"], res["head_corpus"], res["corpus_ratio"],
                  100.0 * (res["corpus_ratio"] - 1.0), len(res["cells"]),
                  len(res["outside_flat"]), res["band"]))
    out.write("veto check (R1: the CLASSIFICATION is the BANDED count; the strict count is "
              "informational) -- cells slower in ALL alternating rounds: **banded (also outside "
              "0.99-1.01) %d %s**; strict, informational, %d %s\n\n" % (
                  len(res["veto_banded"]), res["veto_banded"] or "",
                  len(res["veto_strict"]), res["veto_strict"] or ""))
    fmt_cells(res, out)
    if perf:
        out.write("\nperf: %s\n\n" % perf["verdict"])
        emit_perf_table(perf, out)


def runs(root, *parts, **kw):
    """The measurement files in one directory, in a stable order."""
    ext = kw.pop("ext", ".csv")
    d = os.path.join(root, *parts)
    if not os.path.isdir(d):
        problem("missing directory: %s" % d)
        return []
    return [os.path.join(d, f) for f in sorted(os.listdir(d)) if f.endswith(ext)]


def arm_files(files, arm, prefix=""):
    return [f for f in files if os.path.basename(f).startswith(prefix + arm + "-")]


def check_rounds(label, files, arm, prefix=""):
    """MANIFEST (astra I5 (d)): exactly r1, r2, r3 for this arm. Missing = exit 1."""
    got = set()
    for f in files:
        b = os.path.basename(f)
        if not b.startswith(prefix + arm + "-"):
            continue
        m = re.search(r"-(r[123])\.[a-z]+$", b)
        if m:
            got.add(m.group(1))
    missing = [r for r in ROUNDS if r not in got]
    if missing:
        manifest_fail("%s / arm %s: rounds %s are missing (have %s) -- the three-round manifest "
                      "is not satisfied" % (label, arm, ",".join(missing), ",".join(sorted(got))
                                            or "none"))
    return not missing


def check_members(label, kind, expected, found):
    """R9: the EXPECTED population, written down, checked against what is on disk.

    `expected` and `found` are sequences of comparable members (names, or
    (mode, trace) pairs). A member that is expected and absent is a MANIFEST
    failure naming it; a member that is present and NOT expected is also named,
    because an unexpected population is a population nobody declared and the
    tables would silently include it.
    """
    missing = [m for m in expected if m not in found]
    extra = [m for m in found if m not in expected]
    if missing:
        manifest_fail("%s: %s %s are MISSING (expected %d, found %d) -- the explicit "
                      "manifest is not satisfied"
                      % (label, kind, ", ".join(str(m) for m in missing),
                         len(expected), len(found)))
    if extra:
        manifest_fail("%s: %s %s are present but NOT in the declared manifest"
                      % (label, kind, ", ".join(str(m) for m in extra)))
    return not missing and not extra


def check_count(label, kind, expected, found):
    """R9: a scored population's SIZE, declared rather than accepted."""
    if expected != found:
        manifest_fail("%s: %d %s scored, %d expected -- the explicit manifest is not satisfied"
                      % (label, found, kind, expected))
        return False
    return True


def check_keys(label, path, expected, key_col="cell_id"):
    """R14: the KEY SET of one raw CSV, by identity, against the declared population.

    A member missing from THIS file -- even when every other round has it -- is a
    MANIFEST failure naming the file and the member, and so is a key the manifest
    never declared. This is what makes a SUBSTITUTION detectable: a count cannot
    tell a renamed cell from the cell it replaced.
    """
    if not os.path.exists(path):
        manifest_fail("%s: %s is MISSING -- the explicit manifest is not satisfied"
                      % (label, os.path.basename(path)))
        return False
    with open(path, newline="") as fh:
        lines = [ln for ln in fh if not ln.startswith("#")]
    got = [r.get(key_col) for r in csv.DictReader(lines)] if lines else []
    missing = [m for m in expected if m not in got]
    extra = [m for m in got if m not in expected]
    if missing:
        manifest_fail("%s / %s: rows %s are MISSING (expected %d, found %d) -- the explicit "
                      "manifest is not satisfied"
                      % (label, os.path.basename(path), ", ".join(missing), len(expected),
                         len(got)))
    if extra:
        manifest_fail("%s / %s: rows %s are present but NOT in the declared manifest"
                      % (label, os.path.basename(path), ", ".join(sorted(set(extra)))))
    return not missing and not extra


def check_arm_identity(label, paths, arm):
    """R14: an interior arm identified by the provenance its own binary wrote.

    Every CSV of the arm must carry the declared schema width and the declared
    build directory in its invocation line. An arm's files standing in for
    another arm's fails here rather than being scored as if they were the arm
    the table names.
    """
    want = INTERIOR_ARM_IDENTITY.get(arm)
    if want is None:
        manifest_fail("%s: arm %s is not in the declared identity table" % (label, arm))
        return False
    ok = True
    for path in paths:
        schema = build = None
        with open(path, errors="replace") as fh:
            for line in fh:
                if not line.startswith("#"):
                    break
                if line.startswith("# schema:"):
                    schema = line.split(":", 1)[1].strip()
                elif line.startswith("# invocation:"):
                    for tok in line.split():
                        for part in tok.split("/"):
                            if part == want["build"]:
                                build = part
        if schema != want["schema"]:
            manifest_fail("%s / arm %s (%s): %s declares schema %s, the manifest declares %s -- "
                          "this file was not written by the arm the table names"
                          % (label, arm, want["sha"], os.path.basename(path), schema,
                             want["schema"]))
            ok = False
        if build is None:
            manifest_fail("%s / arm %s (%s): %s carries no `%s` in its invocation line -- this "
                          "file was not written by the arm the table names"
                          % (label, arm, want["sha"], os.path.basename(path), want["build"]))
            ok = False
    return ok


def first_row_rule(paths, label):
    """R3: the first row every process of this leg wrote. MANIFEST if they disagree."""
    firsts = {}
    for p in paths:
        k = first_row_key(p)
        if k is not None:
            firsts.setdefault(k, []).append(os.path.basename(p))
    if not firsts:
        return None
    if len(firsts) > 1:
        manifest_fail("%s: the runs disagree about which row the process writes FIRST (%s); R3's "
                      "warm-up exclusion keys on POSITION and cannot be applied" % (
                          label, "; ".join("%s <- %s" % (k, ",".join(v))
                                           for k, v in sorted(firsts.items()))))
        return None
    return next(iter(firsts))


def split_first_row(res, first_key, out, label):
    """Print the R3 warm-up row on its own and return the PRIMARY (without-it) result."""
    first = [c for c in res["cells"] if c["cell"] == first_key]
    kept = [c for c in res["cells"] if c["cell"] != first_key]
    out.write("\n#### R3 -- the process's FIRST row, excluded from the band by the pre-declared "
              "rule: `%s`\n\n" % first_key)
    if not first:
        out.write("The first row this leg's processes write is `%s`, which is not in the banded "
                  "population of %s (it is not an F7 row); nothing is excluded here.\n\n"
                  % (first_key, label))
        return res
    c = first[0]
    out.write("base runs %s; head runs %s; paired ratios %s; median ratio **%.4f**. "
              "The base arm's own three runs span %.3fx.\n\n" % (
                  ", ".join("%.6f" % v for v in c["base_runs"]),
                  ", ".join("%.6f" % v for v in c["head_runs"]),
                  " ".join("%.4f" % p for p in c["paired"]),
                  c["ratio"], max(c["base_runs"]) / min(c["base_runs"])))
    prim = aggregate(kept, res["label"], res["only_base"], res["only_head"])
    out.write("corpus WITHOUT it (**PRIMARY**, %d rows): base %.4f -> head %.4f, ratio **%.4f** "
              "(%+.3f %%); outside 0.99-1.01: **%d**; band **%s**\n\n" % (
                  len(kept), prim["base_corpus"], prim["head_corpus"], prim["corpus_ratio"],
                  100.0 * (prim["corpus_ratio"] - 1.0), len(prim["outside_flat"]), prim["band"]))
    out.write("corpus WITH it (%d rows, reported for completeness): base %.4f -> head %.4f, ratio "
              "**%.4f** (%+.3f %%); outside 0.99-1.01: **%d**; band **%s**\n\n" % (
                  len(res["cells"]), res["base_corpus"], res["head_corpus"], res["corpus_ratio"],
                  100.0 * (res["corpus_ratio"] - 1.0), len(res["outside_flat"]), res["band"]))
    return prim


def interior_counter_identity(base_paths, head_paths, columns, out, label):
    """R4: every counter column common to the two schemas, arm against arm, per round."""
    diffs = {}
    ncells = 0
    nrounds = 0
    for bp, hp in zip(base_paths, head_paths):
        brows = {r["cell_id"]: r for r in read_rows(bp)}
        hrows = {r["cell_id"]: r for r in read_rows(hp)}
        keys = [k for k in brows if k in hrows]
        if not keys:
            continue
        nrounds += 1
        ncells = len(keys)
        for k in keys:
            for col in columns:
                bv = brows[k].get(col)
                hv = hrows[k].get(col)
                if bv is None or hv is None:
                    diffs.setdefault("<column absent: %s>" % col, set()).add(k)
                elif bv != hv:
                    diffs.setdefault(col, set()).add(k)
    out.write("\n**Counter identity across the two schemas (R4).** %d common rows x %d common "
              "counter columns x %d rounds, `wall_s` excluded: %s\n\n" % (
                  ncells, len(columns), nrounds,
                  ", ".join("`%s` (%d rows)" % (k, len(v)) for k, v in sorted(diffs.items()))
                  or "**NONE -- every counter column is identical**"))
    return diffs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True, help="the artifact directory")
    ap.add_argument("--out", default="-", help="where the reading tables go")
    args = ap.parse_args()
    root = args.root
    out = sys.stdout if args.out == "-" else open(args.out, "w")

    out.write("# W5 T8.9r comparator output (fix round 1)\n")
    out.write("\nRegenerate: `python3 comparator.py --root . --out -`\n")
    out.write("\nR1 (the banded veto reading) and R3 (the interior first-row rule) are "
              "PRE-DECLARED in this file's docstring and in `PROVENANCE.txt`; "
              "`logs/F0-predeclaration.log` records that they were fixed before the first timed "
              "run of this round.\n")

    results = {}

    # ---- leg 1: the U0 27-cell corpus, three SQP modes -------------------
    out.write("\n## Leg 1 -- the U0 27-cell corpus (102f729 -> e51a7e0)\n")
    # R9: the modes themselves are a declared population, not a discovered one.
    leg1root = os.path.join(root, "raw", "leg1")
    check_members("leg 1", "modes", LEG1_MODES,
                  sorted(os.listdir(leg1root)) if os.path.isdir(leg1root) else [])
    for mode in LEG1_MODES:
        files = runs(root, "raw", "leg1", mode)
        for arm in LEG1_ARMS:
            check_rounds("leg 1 / %s" % mode, files, arm)
        b = arm_files(files, "base")
        h = arm_files(files, "head")
        # R14: every raw CSV of this mode carries EXACTLY the 27 declared cell
        # ids. A cell missing from one round, or a cell the manifest never
        # declared standing in for one that is, fails HERE and names itself.
        for path in b + h:
            check_keys("leg 1 / %s" % mode, path, LEG1_CELLS)
        res = score(b, h, label="leg 1 / %s" % mode)
        # R9/R14: 27 cells, declared BY IDENTITY. A short corpus is a MANIFEST
        # failure, not a quietly smaller table.
        check_members("leg 1 / %s" % mode, "cells", LEG1_CELLS,
                      [c["cell"] for c in res["cells"]])
        check_count("leg 1 / %s" % mode, "cells", len(LEG1_CELLS), len(res["cells"]))
        emit(res, out)
        perf_files = runs(root, "perf", "leg1", mode, ext=".txt")
        cells = sorted(set(
            os.path.basename(f).split("-", 2)[2].rsplit("-r", 1)[0] for f in perf_files))
        # R9: the three perf cells are declared. Round 1's tool learned them
        # from the directory, so an omitted cell shrank the population silently.
        check_members("leg 1 perf / %s" % mode, "cells", LEG1_PERF_CELLS, cells)
        pa_all = {}
        for cell in LEG1_PERF_CELLS:
            if cell not in cells:
                continue
            cell_files = [f for f in perf_files if ("-" + cell + "-r") in os.path.basename(f)]
            for pn in LEG1_PERF_PASSES:          # R9: both passes, both arms,
                for arm in LEG1_ARMS:            # three rounds each -- declared
                    check_rounds("leg 1 perf / %s / %s" % (mode, cell), cell_files,
                                 arm, pn + "-")
            # I5 (b): the floor is a property of THIS population -- one cell,
            # one mode, one binary measured twice.
            dis = same_arm_disagreement(cell_files)
            floor = max(dis.values()) if dis else 0.0
            for pass_name, label in (("passA", "pass A"), ("passB", "pass B (Zen 3 front end)")):
                pb = apply_floor(classify_perf(
                    [f for f in cell_files
                     if os.path.basename(f).startswith(pass_name + "-base-" + cell + "-r")],
                    [f for f in cell_files
                     if os.path.basename(f).startswith(pass_name + "-head-" + cell + "-r")]),
                    floor)
                out.write("\n%s, cell `%s`: %s\n\n" % (label, cell, pb["verdict"]))
                emit_perf_table(pb, out)
                if pass_name == "passA":
                    pa_all[cell] = pb
                else:
                    for name, d in (("base", pb["base"]), ("head", pb["head"])):
                        line = []
                        if d.get("cycles:u"):
                            line.append("front-end-bound (dq-empty/cycles) = %.4f"
                                        % (d.get("de_dis_uop_queue_empty_di0:u", 0) / d["cycles:u"]))
                        hit = d.get("op_cache_hit_miss.op_cache_hit:u")
                        miss = d.get("op_cache_hit_miss.op_cache_miss:u")
                        if hit is not None and miss is not None and (hit + miss):
                            line.append("op-cache miss share = %.4f" % (miss / (hit + miss)))
                        if line:
                            out.write("%s arm: %s\n" % (name, "; ".join(line)))
        order = {"WORK-MOVED (instructions UP) -- THE VETO": 3,
                 "instructions inside the band, branches outside -- UNRESOLVED": 2,
                 "INSTRUCTIONS DOWN outside the identity band -- owner classification (11.1.1)": 1}
        worst = None
        for cell, pb in pa_all.items():
            if worst is None or order.get(pb["verdict"], 0) > order.get(worst["verdict"], 0):
                worst = pb
        results["leg1/%s" % mode] = (res, worst)

    # ---- leg 2: the 27 HS problems --------------------------------------
    out.write("\n## Leg 2 -- the 27 Hock-Schittkowski problems, --repeat N\n")
    leg2root = os.path.join(root, "raw", "leg2")
    found_combos = []
    if os.path.isdir(leg2root):
        for mode in sorted(os.listdir(leg2root)):
            for trace in sorted(os.listdir(os.path.join(leg2root, mode))):
                found_combos.append((mode, trace))
    else:
        problem("missing directory: %s" % leg2root)
    # R9: the six combinations are DECLARED. Round 1's tool discovered them from
    # the directory tree, so deleting `raw/leg2/ipm/off` wholesale produced a
    # five-combination table and exit 0 -- the population nobody declared was the
    # population it scored.
    check_members("leg 2", "combinations", LEG2_COMBOS, found_combos)
    combos = [c for c in LEG2_COMBOS if c in found_combos]

    # I5 (b): the floor is the MAXIMUM per-(mode, trace) same-arm disagreement.
    # A median across modes, traces and repeat counts averages instruments that
    # measure different workloads at different N, which is not a noise floor of
    # anything.
    per_combo_floor = {}
    for mode, trace in combos:
        pf = runs(root, "perf", "leg2", mode, trace, ext=".txt")
        dis = same_arm_disagreement(pf)
        per_combo_floor[(mode, trace)] = max(dis.values()) if dis else 0.0
    leg2_floor = max(per_combo_floor.values()) if per_combo_floor else 0.0
    if combos:
        out.write("\n**Leg 2's noise floor (I5 (b)).** The same binary measured twice, per "
                  "(mode, trace) population; the floor this leg is gated on is the MAXIMUM, not a "
                  "median across combinations:\n\n")
        out.write("| mode / trace | worst same-arm passA/passB disagreement |\n|---|---|\n")
        for k in sorted(per_combo_floor):
            out.write("| %s / %s | %.6f %% |\n" % (k[0], k[1], 100.0 * per_combo_floor[k]))
        out.write("\n**Leg 2 floor = %.6f %%** (the maximum above); the 1e-4 identity band is "
                  "0.01 %%.\n" % (100.0 * leg2_floor))

    for mode, trace in combos:
        files = runs(root, "raw", "leg2", mode, trace)
        wall_files = [f for f in files if not os.path.basename(f).startswith("passB-")]
        check_rounds("leg 2 / %s / %s" % (mode, trace), wall_files, "base")
        check_rounds("leg 2 / %s / %s" % (mode, trace), wall_files, "head")
        pf = runs(root, "perf", "leg2", mode, trace, ext=".txt")
        check_rounds("leg 2 perf / %s / %s" % (mode, trace), pf, "base", "passA-")
        check_rounds("leg 2 perf / %s / %s" % (mode, trace), pf, "head", "passA-")
        check_rounds("leg 2 perf / %s / %s" % (mode, trace), pf, "base", "passB-")
        check_rounds("leg 2 perf / %s / %s" % (mode, trace), pf, "head", "passB-")
        b = arm_files(wall_files, "base")
        h = arm_files(wall_files, "head")
        res = score(b, h, key_col="hs", val_col="wall_median_s",
                    label="leg 2 / %s / trace=%s" % (mode, trace))
        pa = apply_floor(classify_perf(arm_files(pf, "base", "passA-"),
                                       arm_files(pf, "head", "passA-")), leg2_floor)
        emit(res, out, pa)
        pbb = arm_files(pf, "base", "passB-")
        pbh = arm_files(pf, "head", "passB-")
        if pbb and pbh:
            # I5 (c): pass B is gated on the SAME floor. A leg that carries no
            # instruction verdict on pass A carries none on pass B either.
            pb = apply_floor(classify_perf(pbb, pbh), leg2_floor)
            out.write("\npass B: %s\n\n" % pb["verdict"])
            emit_perf_table(pb, out)
        results["leg2/%s/%s" % (mode, trace)] = (res, pa)

    # ---- the interior leg, THREE ARMS (R4) -------------------------------
    out.write("\n## The interior leg -- THREE arms\n")
    out.write("\nThe leg exists at 102f729 (astra I2: `149f29b` is an ancestor of `102f729`), with "
              "33 rows and the 19-column schema. It therefore gets a base arm at the SQP legs' own "
              "base for those 33 keys, and keeps b9848bf as the 41-key arm. What is measured at "
              "102f729 -> e51a7e0 IS the top-level IPM's runtime across the whole of group 1 on "
              "those 33 rows.\n")

    ifiles = runs(root, "raw", "interior")
    # R9: three arms x three rounds, declared.
    check_members("interior", "arms", INTERIOR_ARMS,
                  sorted({os.path.basename(f).rsplit("-r", 1)[0] for f in ifiles
                          if not os.path.basename(f).startswith("pass")}))
    for arm in INTERIOR_ARMS:
        check_rounds("interior", ifiles, arm)
        # R14: and the arm is the arm it says it is.
        check_arm_identity("interior", arm_files(ifiles, arm), arm)
    first_key = first_row_rule(ifiles, "the interior leg")

    interior_results = {}
    for arm, armlabel, span in (("arm102", "102f729", "102f729 -> e51a7e0 (33 base keys)"),
                                ("armb98", "b9848bf", "b9848bf -> e51a7e0 (41 keys)")):
        bpaths = arm_files(ifiles, arm)
        hpaths = arm_files(ifiles, "head")
        res_all = score(bpaths, hpaths, label="interior / %s / ALL comparable rows" % span)
        f7 = [c for c in res_all["cells"] if c["cell"].startswith("f7_")]
        res_f7 = aggregate(f7, "interior / %s / F7 rows (banded, leg-1 rules)" % span,
                           res_all["only_base"], res_all["only_head"])
        emit(res_f7, out)
        prim = split_first_row(res_f7, first_key, out, res_f7["label"]) if first_key else res_f7
        ms = [c for c in res_all["cells"] if not c["cell"].startswith("f7_")]
        out.write("\n**The millisecond-scale rows of this arm -- NO BAND (A7 (ii)).**\n\n")
        out.write("| row | base median (s) | head median (s) | ratio | paired per round |\n")
        out.write("|---|---|---|---|---|\n")
        for c in ms:
            out.write("| `%s` | %.6f | %.6f | %.4f | %s |\n" % (
                c["cell"], c["base_median"], c["head_median"], c["ratio"],
                " ".join("%.4f" % p for p in c["paired"])))
        if arm == "arm102":
            interior_counter_identity(bpaths, hpaths, INTERIOR_COMMON_COUNTERS, out,
                                      "interior / 102f729 -> e51a7e0")
        interior_results[arm] = prim
        results["interior/%s" % armlabel] = (prim, None)

    # whole-process perf on the interior leg: NO verdict, the populations differ
    out.write("\n### The interior leg's WHOLE-PROCESS instruction counts -- NO VERDICT\n\n")
    ipf = runs(root, "perf", "interior", ext=".txt")
    for arm in INTERIOR_ARMS:                    # R9: three arms, both passes
        for pn in LEG1_PERF_PASSES:
            check_rounds("interior perf", ipf, arm, pn + "-")
    for arm, span in (("arm102", "102f729 -> e51a7e0"), ("armb98", "b9848bf -> e51a7e0")):
        for pass_name in ("passA", "passB"):
            pb = classify_perf(arm_files(ipf, arm, pass_name + "-"),
                               arm_files(ipf, "head", pass_name + "-"))
            # I5 (c): an unmatched population carries no verdict in EITHER pass.
            suppress_verdict(pb, "the two arms' processes do not run the same rows (%s), so "
                                 "`perf stat`, which counts the PROCESS, is not like-for-like"
                             % span)
            out.write("\n%s, %s: %s\n\n" % (pass_name, span, pb["verdict"]))
            emit_perf_table(pb, out)

    # ---- I4: the per-cell interior instruction measurement ---------------
    out.write("\n### I4 -- the per-cell interior instruction measurement, LIKE-FOR-LIKE\n\n")
    out.write("Every `--engine interior` process also runs an UNCONDITIONAL row set (the "
              "`hs071_x1_fixed` treatments, the `cap1`/`solve_optimize`/warm variants, the two "
              "`infeas2` rows) whichever cell is requested, and at the head TWO MORE (`parts2`) "
              "that no flag suppresses -- so a bare per-cell process is NOT like-for-like across "
              "arms. The instrument that is: run the cell, run `--cells hs071_x1_fixed` (which "
              "adds no cell of its own and is exactly that unconditional set), and DIFFERENCE the "
              "two on the same arm. The unconditional set -- the head's `parts2` rows included -- "
              "and process start-up cancel, and what is left is that cell's own three rows.\n\n")
    cellroot = os.path.join(root, "perf", "interior_cells")
    base_key = "hs071_x1_fixed"
    # R14: the ABSENCE OF THE WHOLE TREE IS A MANIFEST FAILURE NAMING IT. At
    # fix2 this `if` simply skipped the block, so deleting the directory left
    # the explicit member check at the bottom of it unreached and surfaced as a
    # downstream exit 2. The per-(cell, arm, round) population is declared, so
    # its container's absence is the manifest's business.
    if not os.path.isdir(cellroot):
        manifest_fail("interior_cells: the declared population's directory `perf/interior_cells` "
                      "is MISSING -- %d cells x %d arms x %d rounds are declared and none of "
                      "them is on disk" % (len(INTERIOR_CELLS), len(INTERIOR_ARMS), len(ROUNDS)))
        for cell in INTERIOR_CELLS:
            manifest_fail("interior_cells: cell `%s` is MISSING (its whole directory)" % cell)
    if os.path.isdir(cellroot):
        # R9: the eleven per-cell populations are DECLARED. Round 1's tool
        # listed the directory, so an omitted cell simply left the table.
        check_members("interior_cells", "cells", INTERIOR_CELLS,
                      sorted(d for d in os.listdir(cellroot)
                             if os.path.isdir(os.path.join(cellroot, d))))
        # R14: and the population is (cell, arm, round), checked over the
        # DECLARED cells rather than over whichever of them happen to be there.
        for cell in INTERIOR_CELLS:
            cdir = os.path.join(cellroot, cell)
            if not os.path.isdir(cdir):
                continue           # already named by check_members above
            cf = runs(root, "perf", "interior_cells", cell, ext=".txt")
            for arm in INTERIOR_ARMS:
                check_rounds("interior_cells / %s" % cell, cf, arm, "passA-")
        commons = {}
        for arm in INTERIOR_ARMS:
            cf = runs(root, "perf", "interior_cells", base_key, ext=".txt")
            check_rounds("interior_cells / %s" % base_key, cf, arm, "passA-")
            m, _ = perf_medians(arm_files(cf, arm, "passA-"))
            commons[arm] = m
        out.write("The unconditional set alone (`--cells hs071_x1_fixed`), median of three:\n\n")
        out.write("| arm | rows | instructions | branches |\n|---|---|---|---|\n")
        for arm, nrows in (("arm102", 3), ("armb98", 11), ("head", 13)):
            if commons.get(arm):
                out.write("| %s | %d | %d | %d |\n" % (
                    arm, nrows, commons[arm].get("instructions:u", 0),
                    commons[arm].get("branches:u", 0)))
        present = {d for d in os.listdir(cellroot)
                   if os.path.isdir(os.path.join(cellroot, d))}
        cells = [c for c in INTERIOR_CELLS if c != base_key and c in present]
        # First the RAW whole-process counts, which are data whatever the
        # differencing turns out to be worth.
        out.write("\nThe per-cell processes, whole-process `instructions:u`, median of three:\n\n")
        out.write("| cell | arm102 | armb98 | head |\n|---|---|---|---|\n")
        raw = {}
        for cell in cells:
            cf = runs(root, "perf", "interior_cells", cell, ext=".txt")
            row = {}
            for arm in INTERIOR_ARMS:
                check_rounds("interior_cells / %s" % cell, cf, arm, "passA-")
                m, _ = perf_medians(arm_files(cf, arm, "passA-"))
                row[arm] = m
            raw[cell] = row
            out.write("| `%s` | %d | %d | %d |\n" % (
                cell, row["arm102"].get("instructions:u", 0),
                row["armb98"].get("instructions:u", 0), row["head"].get("instructions:u", 0)))

        # THE SOUNDNESS CHECK ON THE DIFFERENCING, AND IT FAILS.
        # The instrument assumes the unconditional row set costs the same inside
        # a cell process as it does alone. It does not: alone, `hs071_x1_fixed`
        # is the FIRST solve of the process and pays MKL's first call, the
        # allocator's first growth and the page faults for the working set;
        # inside a cell process those same rows run AFTER a large F7 solve has
        # already paid them. The residual is a per-arm constant of the same
        # order as a small cell's whole cost, and it shows up as a NEGATIVE
        # difference -- a cell process with FEWER instructions than the
        # unconditional set it contains, which is impossible. Rather than
        # publish a ratio built on that, the tool prints the arithmetic, names
        # the violation and refuses the verdict.
        out.write("\n| cell | arm | instructions (differenced) | branches (differenced) | "
                  "ratio vs head |\n|---|---|---|---|---|\n")
        unsound = []
        i4 = {}
        for cell in cells:
            diff = {}
            for arm in ("arm102", "armb98", "head"):
                m = raw[cell][arm]
                if not m or not commons.get(arm):
                    continue
                diff[arm] = {ev: m[ev] - commons[arm].get(ev, 0)
                             for ev in ("instructions:u", "branches:u", "cycles:u") if ev in m}
            hd = diff.get("head", {})
            for arm in ("arm102", "armb98"):
                d = diff.get(arm)
                if not d or not hd or not d.get("instructions:u"):
                    continue
                ins = hd["instructions:u"] / d["instructions:u"]
                for label, vals in (("%s/%s" % (cell, arm), d), ("%s/head" % cell, hd)):
                    if vals.get("instructions:u", 0) <= 0 or vals.get("branches:u", 0) <= 0:
                        unsound.append(label)
                out.write("| `%s` | %s | %+d | %+d | %.5f |\n" % (
                    cell, arm, d["instructions:u"], d.get("branches:u", 0), ins))
                i4["%s/%s" % (cell, arm)] = ins
        unsound = sorted(set(unsound))
        if unsound:
            out.write("\n**THE DIFFERENCING IS UNSOUND AND CARRIES NO VERDICT.** %d of the "
                      "differenced quantities are NEGATIVE -- a cell process with fewer "
                      "instructions or branches than the unconditional row set it contains, which "
                      "cannot happen if the set cost the same in both processes. It does not: run "
                      "alone it is the process's first solve and pays MKL's first call, the "
                      "allocator's first growth and the working set's page faults, which inside a "
                      "cell process a large F7 solve has already paid. The offending entries are "
                      "%s. **No per-cell interior instruction verdict is issued.**\n\n"
                      % (len(unsound), ", ".join("`%s`" % x for x in unsound)))
            out.write("AND THE UNCONDITIONAL SETS ARE NOT COMPARABLE EITHER, which is the deeper "
                      "reason: the head's 13-row set costs FEWER instructions (%d) than "
                      "b9848bf's 11-row set (%d), so even the term being subtracted is not the "
                      "same quantity across the arms. **A7 (ii)'s per-row instructions-only "
                      "reading for the interior leg is NOT AVAILABLE from this harness**, by "
                      "either route; the gap is REGISTERED for W6 with the rest of A7, and the "
                      "numbers above are retained as data, not as a verdict.\n"
                      % (commons["head"].get("instructions:u", 0),
                         commons["armb98"].get("instructions:u", 0)))
        out.write("\n**`hs071_x1_fixed` and the two `infeas2` rows carry NO like-for-like "
                  "instruction verdict** and none is printed: they ARE the unconditional set, so "
                  "there is nothing to difference them against, and the set itself differs between "
                  "the arms by the two head-only `parts2` rows. Registered as a harness gap, not "
                  "resolved here.\n")
    else:
        problem("missing directory: %s" % cellroot)

    # ---- the counter identity, leg 1 ------------------------------------
    out.write("\n## Counter identity, leg 1 (recomputed from raw/)\n\n")
    out.write("| mode | cells | columns compared | rounds | differing columns |\n")
    out.write("|---|---|---|---|---|\n")
    for mode in ("ipm", "ssn", "walk"):
        files = runs(root, "raw", "leg1", mode)
        diffs = {}
        ncells = ncols = nrounds = 0
        for tag in sorted(set(os.path.basename(f).split("-")[1] for f in files)):
            bf = [f for f in files if os.path.basename(f) == "base-" + tag]
            hf = [f for f in files if os.path.basename(f) == "head-" + tag]
            if not bf or not hf:
                continue
            nrounds += 1
            brows = {r["cell_id"]: r for r in read_rows(bf[0])}
            hrows = {r["cell_id"]: r for r in read_rows(hf[0])}
            ncells = len(brows)
            for cid, b in brows.items():
                h = hrows.get(cid)
                if h is None:
                    diffs.setdefault("<missing row>", set()).add(cid)
                    continue
                ncols = len(b) - 1
                for k in b:
                    if k == "wall_s":
                        continue
                    if b[k] != h[k]:
                        diffs.setdefault(k, set()).add(cid)
        out.write("| %s | %d | %d | %d | %s |\n" % (
            mode, ncells, ncols, nrounds,
            ", ".join("`%s` (%d cells)" % (k, len(v)) for k, v in sorted(diffs.items()))
            or "**NONE -- byte-identical**"))

    # ---- the instrument's own checks -------------------------------------
    out.write("\n## Instrument checks\n")
    out.write("\nThe same arm, measured twice -- pass A against pass B. A population whose own two "
              "passes disagree by more than 1e-4 cannot carry a 1e-4 instruction verdict.\n\n")
    out.write("| population | arm | pass A instructions | pass B instructions | A/B |\n"
              "|---|---|---|---|---|\n")

    def selfcheck(label, files, arms=("base", "head")):
        for arm in arms:
            fa = arm_files(files, arm, "passA-")
            fb = arm_files(files, arm, "passB-")
            if not fa or not fb:
                continue
            ma, _ = perf_medians(fa)
            mb, _ = perf_medians(fb)
            if ma.get("instructions:u") and mb.get("instructions:u"):
                out.write("| %s | %s | %d | %d | %.5f |\n" % (
                    label, arm, ma["instructions:u"], mb["instructions:u"],
                    ma["instructions:u"] / mb["instructions:u"]))

    for mode in ("ipm", "ssn", "walk"):
        files = runs(root, "perf", "leg1", mode, ext=".txt")
        for cell in sorted(set(os.path.basename(f).split("-", 2)[2].rsplit("-r", 1)[0]
                               for f in files)):
            selfcheck("leg 1 / %s / %s" % (mode, cell),
                      [f for f in files if ("-" + cell + "-r") in os.path.basename(f)])
    for mode, trace in combos:
        selfcheck("leg 2 / %s / %s" % (mode, trace),
                  runs(root, "perf", "leg2", mode, trace, ext=".txt"))
    selfcheck("interior (whole process)", runs(root, "perf", "interior", ext=".txt"),
              arms=("arm102", "armb98", "head"))

    # ---- the summary ------------------------------------------------------
    out.write("\n## Summary\n\n")
    out.write("Bands are R1-classified: the veto column is the BANDED count.\n\n")
    out.write("| leg | corpus ratio | cells outside 0.99-1.01 | band | banded veto cells | "
              "perf verdict |\n|---|---|---|---|---|---|\n")
    for name in sorted(results):
        res, pa = results[name]
        out.write("| %s | %.4f | %d/%d | %s | %d | %s |\n" % (
            name, res["corpus_ratio"], len(res["outside_flat"]), len(res["cells"]),
            res["band"], len(res["veto_banded"]), pa["verdict"] if pa else "n/a"))

    if MANIFEST:
        out.write("\n## MANIFEST FAILURES (%d) -- exit 1\n\n" % len(MANIFEST))
        for m in MANIFEST:
            out.write("- %s\n" % m)
    if PROBLEMS:
        out.write("\n## PROBLEMS (%d) -- this reading is NOT complete\n\n" % len(PROBLEMS))
        for p in PROBLEMS:
            out.write("- %s\n" % p)
    if out is not sys.stdout:
        out.close()
    for m in MANIFEST:
        sys.stderr.write("comparator: MANIFEST: %s\n" % m)
    for p in PROBLEMS:
        sys.stderr.write("comparator: %s\n" % p)
    # MANIFEST FIRST. A missing round makes every downstream complaint a
    # consequence rather than a cause, so the status names the cause: exit 1.
    if MANIFEST:
        return 1
    if PROBLEMS:
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
