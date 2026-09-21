#!/usr/bin/env python3
"""M7 W2 M3b -- RULE.md section 2's ABORT CONDITIONS, as a checker.

THE GATE IS THIS COMPARISON, and it runs BEFORE any predicate of the rule is
evaluated. Two things are checked and they are independent:

  2.1 THE IDENTITY GATE. The traced solve's first QP-ledger record must equal
      M7 W2 M3a's COMMITTED `rows/f/<cell>@F-trinf.txt` record 0 on ALL 20
      `QpCounters` fields plus the record's status. The authoritative values are
      the BYTES OF THAT FILE -- RULE.md's own table of them is a declaration,
      not the gate, and a disagreement between the two is reported as a
      transcription defect in the rule rather than resolved in the table's
      favour.

  2.2 TRACE INTEGRITY. The observer's record count must equal the record's
      `minor_iters`; `truncated` must be false; the walk must have exited
      `max_iter`; no record may carry the `unset` action; `solve` must be 0 and
      `warm` false on the traced solve.

STRUCTURALLY NON-VACUOUS, on M7 W2 M3a fix round 1's hardened template (the
review finding "the classifier can pass vacuously"). A checker that compares
zero fields and exits 0 is exactly the failure this file exists to forbid, so
every refusal below has its own exit code and `--selftest` FOLD-PROVES each of
them against a synthetic input:

  R1  a file that does not exist, or carries no `PROBE_BEGIN`          -> 3
  R2  a file with no `LEDGER_FIRST_BEGIN ... LEDGER_FIRST_END` block   -> 3
  R3  fewer than 20 counter keys present in either block               -> 4
  R4  the compared field list is not the declared one                  -> 4
  R5  any field differs, or the status differs                         -> 1
  R6  a trace-integrity condition fails (only when --trace is given)   -> 5

Nothing here reads a verdict, chooses a threshold or touches RULE.md.
"""
import argparse
import os
import sys
import tempfile

# QpCounters' twenty fields in DECLARATION ORDER (core/solver_counters.h), the
# same list m3b_probe.cpp and M3a's probe print. It is DECLARED here so that a
# comparison over some other list is a refusal (R4) rather than a quiet pass.
FIELDS = (
    "factorizations",
    "schur_updates",
    "minor_iters",
    "eqp_refine_steps",
    "border_refine_steps",
    "verdict_refine_steps",
    "suspect_escalations",
    "k0_reused",
    "symbolic_analyses",
    "ws_adds",
    "ws_drops",
    "shift_adds",
    "degenerate_steps",
    "degenerate_run_max",
    "ws_adds_bound",
    "ws_drops_bound",
    "distinct_ineq_added",
    "distinct_bound_added",
    "drop_ties",
    "ratio_ties",
)
STATUS_KEY = "first_qp_record_status"


def read_kv(path):
    """Every `key=value` line of a probe row, last occurrence winning."""
    if not os.path.isfile(path):
        return None, "no such file"
    kv, saw_begin, saw_first = {}, False, False
    with open(path, errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line == "PROBE_BEGIN":
                saw_begin = True
            elif line == "LEDGER_FIRST_BEGIN":
                saw_first = True
            elif "=" in line:
                k, v = line.split("=", 1)
                kv[k] = v
    if not saw_begin:
        return None, "carries no PROBE_BEGIN -- this is not a probe row"
    if not saw_first:
        return None, "carries no LEDGER_FIRST_BEGIN block -- there is no record 0 in it"
    return kv, None


def check_identity(m3a_path, probe_path, out=sys.stdout):
    """Returns (exit_code, number_of_fields_compared)."""
    a, err = read_kv(m3a_path)
    if a is None:
        print("REFUSED (R1/R2): M3a row {}: {}".format(m3a_path, err), file=out)
        return 3, 0
    b, err = read_kv(probe_path)
    if b is None:
        print("REFUSED (R1/R2): probe row {}: {}".format(probe_path, err), file=out)
        return 3, 0

    keys = ["first_" + f for f in FIELDS] + [STATUS_KEY]
    missing_a = [k for k in keys if k not in a]
    missing_b = [k for k in keys if k not in b]
    if missing_a or missing_b:
        print("REFUSED (R3): the comparison would cover {} of {} keys -- missing from the M3a "
              "row {}, from the probe row {}".format(len(keys) - max(len(missing_a), len(missing_b)),
                                                     len(keys), missing_a, missing_b), file=out)
        return 4, 0
    if len(FIELDS) != 20 or len(set(FIELDS)) != 20:
        print("REFUSED (R4): the declared field list is not twenty distinct names", file=out)
        return 4, 0

    compared, differing = 0, []
    for k in keys:
        compared += 1
        if a[k] != b[k]:
            differing.append((k, a[k], b[k]))

    print("IDENTITY GATE  M3a={}  probe={}".format(os.path.basename(m3a_path),
                                                   os.path.basename(probe_path)), file=out)
    print("  fields compared: {}  (20 QpCounters fields + the record status)".format(compared),
          file=out)
    for k in keys:
        print("    {:<34} {:>12}".format(k, a[k]), file=out)
    if differing:
        print("  DIFFERING: {}".format(len(differing)), file=out)
        for k, va, vb in differing:
            print("    {:<34} M3a={:<14} probe={}".format(k, va, vb), file=out)
        print("REFUSED (R5): the traced solve is NOT M3a's record 0 -- RULE.md section 2.1 "
              "ABORTS the leg", file=out)
        return 1, compared
    print("  DIFFERING: 0 -- the rebuilt HVEN_TESTING binary reproduces M3a's committed "
          "record 0 on all 20 fields and the status", file=out)
    return 0, compared


def check_trace(trace_path, probe_path, out=sys.stdout):
    """RULE.md section 2.2. Returns an exit code."""
    b, err = read_kv(probe_path)
    if b is None:
        print("REFUSED (R1/R2): probe row {}: {}".format(probe_path, err), file=out)
        return 3
    if not os.path.isfile(trace_path):
        print("REFUSED (R6): no trace at {}".format(trace_path), file=out)
        return 5

    header, rows, cols = {}, 0, None
    unset, bad_solve = 0, 0
    with open(trace_path, errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("#"):
                if "=" in line:
                    k, v = line[1:].strip().split("=", 1)
                    header[k] = v
                continue
            if cols is None:
                cols = line.split(",")
                continue
            if not line:
                continue
            rows += 1
            cells = line.split(",")
            if len(cells) != len(cols):
                print("REFUSED (R6): trace row {} has {} cells, the header has {}"
                      .format(rows, len(cells), len(cols)), file=out)
                return 5
            rec = dict(zip(cols, cells))
            if rec.get("action") == "unset":
                unset += 1
            if rec.get("solve") != "0":
                bad_solve += 1

    minor_iters = b.get("first_minor_iters")
    problems = []
    if cols is None:
        problems.append("the trace has no header row")
    if minor_iters is None:
        problems.append("the probe row carries no first_minor_iters")
    elif str(rows) != minor_iters:
        problems.append("the trace has {} records and record 0 reports minor_iters={} -- a pass "
                        "went unrecorded".format(rows, minor_iters))
    if header.get("truncated") != "0":
        problems.append("the observer reports truncated={}".format(header.get("truncated")))
    if header.get("exit_status") != "max_iter":
        problems.append("the walk exited {} -- RULE.md section 2.2 requires max_iter"
                        .format(header.get("exit_status")))
    if header.get("warm") != "0":
        problems.append("the traced solve reports warm={} -- this must be the cold first "
                        "subproblem".format(header.get("warm")))
    if header.get("solve_index") != "0":
        problems.append("the traced solve_index is {}".format(header.get("solve_index")))
    if unset:
        problems.append("{} record(s) carry the `unset` action, which is an instrumentation "
                        "bug".format(unset))
    if bad_solve:
        problems.append("{} record(s) do not belong to solve 0".format(bad_solve))

    print("TRACE INTEGRITY  {}".format(os.path.basename(trace_path)), file=out)
    print("  records={}  minor_iters={}  truncated={}  exit={}  warm={}  solve_index={}  "
          "incoming_ws_size={}  solves_seen={}"
          .format(rows, minor_iters, header.get("truncated"), header.get("exit_status"),
                  header.get("warm"), header.get("solve_index"),
                  header.get("incoming_ws_size"), header.get("solves_seen")), file=out)
    if problems:
        for p in problems:
            print("  PROBLEM: {}".format(p), file=out)
        print("REFUSED (R6): RULE.md section 2.2 ABORTS the leg", file=out)
        return 5
    print("  OK -- one record per pass, nothing truncated, the cold first subproblem, "
          "exited on the cap", file=out)
    return 0


# ---------------------------------------------------------------------------
# THE FOLD PROOFS. Each one is an input this checker MUST refuse, with the exit
# code it must refuse with. A checker that returns 0 for all of them compares
# nothing, which is the defect this whole file is shaped against.
# ---------------------------------------------------------------------------
def _row(fields, status="max_iter", head="PROBE_BEGIN\n", first=True):
    s = head
    if first:
        s += "LEDGER_FIRST_BEGIN\n"
    s += "first_label=f_qp_0\nfirst_warm=0\n{}={}\n".format(STATUS_KEY, status)
    for k, v in fields.items():
        s += "first_{}={}\n".format(k, v)
    if first:
        s += "LEDGER_FIRST_END\n"
    return s


def selftest():
    import io
    base = {f: (10 if f != "minor_iters" else 4680) for f in FIELDS}
    folds, failed = 0, 0

    def fold(name, want, fn):
        nonlocal folds, failed
        folds += 1
        buf = io.StringIO()
        got = fn(buf)
        ok = got == want
        if not ok:
            failed += 1
        print("FOLD {:02d}  {:<8} want={} got={}  {}".format(
            folds, "ACCEPTED" if want == 0 else "REFUSED", want, got, name))
        for line in buf.getvalue().splitlines():
            if line.startswith("REFUSED") or "PROBLEM" in line or "DIFFERING:" in line:
                print("          " + line.strip())

    with tempfile.TemporaryDirectory() as d:
        good = os.path.join(d, "good.txt")
        open(good, "w").write(_row(base))
        same = os.path.join(d, "same.txt")
        open(same, "w").write(_row(base))
        moved = dict(base)
        moved["ws_adds"] = 11
        movedp = os.path.join(d, "moved.txt")
        open(movedp, "w").write(_row(moved))
        statusp = os.path.join(d, "status.txt")
        open(statusp, "w").write(_row(base, status="optimal"))
        short = dict(base)
        del short["ratio_ties"]
        shortp = os.path.join(d, "short.txt")
        open(shortp, "w").write(_row(short))
        noblock = os.path.join(d, "noblock.txt")
        open(noblock, "w").write(_row(base, first=False))
        empty = os.path.join(d, "empty.txt")
        open(empty, "w").write("")

        fold("identity: two identical record-0 blocks", 0,
             lambda o: check_identity(good, same, o)[0])
        fold("identity: ONE field moved (ws_adds 10 -> 11)", 1,
             lambda o: check_identity(good, movedp, o)[0])
        fold("identity: the STATUS moved (max_iter -> optimal)", 1,
             lambda o: check_identity(good, statusp, o)[0])
        fold("identity: a row missing one of the twenty keys", 4,
             lambda o: check_identity(good, shortp, o)[0])
        fold("identity: a row with no LEDGER_FIRST block", 3,
             lambda o: check_identity(good, noblock, o)[0])
        fold("identity: an EMPTY file (no PROBE_BEGIN)", 3,
             lambda o: check_identity(good, empty, o)[0])
        fold("identity: a file that does not exist", 3,
             lambda o: check_identity(good, os.path.join(d, "nope.txt"), o)[0])

        hdr = ("# truncated=0\n# exit_status=max_iter\n# warm=0\n# solve_index=0\n"
               "# solves_seen=1\n# incoming_ws_size=7\n")
        cols = "solve,minor,action\n"

        def trace(path, header, nrows, action="add", solve="0"):
            with open(path, "w") as fh:
                fh.write(header)
                fh.write(cols)
                for i in range(nrows):
                    fh.write("{},{},{}\n".format(solve, i, action))
            return path

        tiny = dict(base)
        tiny["minor_iters"] = 5
        tinyp = os.path.join(d, "tiny.txt")
        open(tinyp, "w").write(_row(tiny))

        fold("trace: five records against minor_iters=5", 0,
             lambda o: check_trace(trace(os.path.join(d, "t_ok.csv"), hdr, 5), tinyp, o))
        fold("trace: FOUR records against minor_iters=5 (a pass went unrecorded)", 5,
             lambda o: check_trace(trace(os.path.join(d, "t_short.csv"), hdr, 4), tinyp, o))
        fold("trace: an `unset` action", 5,
             lambda o: check_trace(trace(os.path.join(d, "t_unset.csv"), hdr, 5, action="unset"),
                                   tinyp, o))
        fold("trace: truncated=1", 5,
             lambda o: check_trace(trace(os.path.join(d, "t_trunc.csv"),
                                         hdr.replace("truncated=0", "truncated=1"), 5), tinyp, o))
        fold("trace: the walk exited optimal, not max_iter", 5,
             lambda o: check_trace(trace(os.path.join(d, "t_opt.csv"),
                                         hdr.replace("exit_status=max_iter",
                                                     "exit_status=optimal"), 5), tinyp, o))
        fold("trace: a WARM traced solve", 5,
             lambda o: check_trace(trace(os.path.join(d, "t_warm.csv"),
                                         hdr.replace("warm=0", "warm=1"), 5), tinyp, o))
        fold("trace: records belonging to another solve", 5,
             lambda o: check_trace(trace(os.path.join(d, "t_s1.csv"), hdr, 5, solve="1"),
                                   tinyp, o))
        fold("trace: a trace file that does not exist", 5,
             lambda o: check_trace(os.path.join(d, "nope.csv"), tinyp, o))

    print()
    print("IDENTITY_FOLDS_TOTAL={}  IDENTITY_FOLDS_FAILED={}".format(folds, failed))
    return 0 if failed == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--m3a-row")
    ap.add_argument("--probe-row")
    ap.add_argument("--trace")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()
    if not args.m3a_row or not args.probe_row:
        ap.error("--m3a-row and --probe-row are required (or use --selftest)")

    rc, _ = check_identity(args.m3a_row, args.probe_row)
    if rc != 0:
        return rc
    if args.trace:
        trc = check_trace(args.trace, args.probe_row)
        if trc != 0:
            return trc
    return 0


if __name__ == "__main__":
    sys.exit(main())
