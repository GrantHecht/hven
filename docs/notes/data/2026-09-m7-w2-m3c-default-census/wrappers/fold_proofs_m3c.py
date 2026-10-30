#!/usr/bin/env python3
"""M7 W2 M3c -- the IDENTITY GATE's fold proofs, run BEFORE the gate reads a leg.

A checker that has never been seen to fail is not a checker (M3a fix1's ruling
on three vacuous checkers, and RULE.md section 4.3). This script constructs
inputs -- good ones, and one for every refusal the gate declares -- hands each
to `identity_m3c.py` as a subprocess, and asserts BOTH the per-cell verdict and
the process exit code.

Nothing here reads a real leg. The only real files it touches are the
harness's committed CSVs, READ-ONLY, and the synthetic rows it writes under a
temporary directory of its own.

  usage: fold_proofs_m3c.py
  exit   0  every fold landed where it was declared to land
         1  a fold landed somewhere else -- the gate is not the gate this rule
            describes, and no leg may be read with it
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CHECKER = os.path.join(HERE, "identity_m3c.py")
BENCH = "/home/ghecht/Projects/hven-bench"
RUN = os.path.join(BENCH, "runs/2026-09-m7-w2-h-repin/run-A")
RESULTS_CSV = os.path.join(RUN, "results.csv")
COUNTERS_CSV = os.path.join(RUN, "cutest/counters-hven-sqp.csv")

# M3b Corrections C8: every path this script hands a sibling is checked to exist.
for p in (CHECKER, RESULTS_CSV, COUNTERS_CSV):
    if not os.path.exists(p):
        print(f"FOLD SETUP REFUSED: missing {p}")
        sys.exit(1)

sys.path.insert(0, HERE)
import identity_m3c as G  # noqa: E402  -- the checker's own pins, not a copy of them


def harness_values(cell):
    """The committed row for a cell, read the way the checker reads it."""
    res = G.read_csv_rows(RESULTS_CSV, G.RESULTS_HEADER, "results.csv")
    cnt = G.read_csv_rows(COUNTERS_CSV, G.COUNTERS_HEADER, "counters")
    return res[G.results_key(cell)], cnt[G.harness_key(cell, "default")]


def synth_row(cell, tweaks=None, drop_keys=(), extra_keys=(), drop_blocks=(),
              no_end=False, double_begin=False, empty=False):
    """A synthetic probe row that AGREES with the committed harness row for
    `cell` on every field the gate compares -- then perturbed as asked.

    It is built from the checker's OWN pinned key set and field lists, so a
    fold cannot pass by agreeing with a stale copy of them."""
    if empty:
        return ""
    res, cnt = harness_values(cell)
    kv = {}
    for k in sorted(G.EXPECTED_KEY_BLOCK):
        kv[k] = "0"
    kv["problem"] = cell
    kv["status"] = res["status"]
    kv["f"] = res["objective"]
    kv["stationarity"] = res["kkt_stationarity"]
    kv["feasibility_e"] = res["kkt_primal_eq"]
    kv["feasibility_i"] = res["kkt_primal_ineq"]
    kv["complementarity"] = res["kkt_complementarity"]
    kv["ladder"] = "off"
    kv["ledger"] = "on"
    kv["x_k_source"] = "n/a (--ladder off: the callback copied no iterate)"
    kv["restoration_sub_status"] = "not_run"
    counters = {f"c_{h}": cnt[h] for _, h, s in
                (G.CORE_INT + G.EXTENDED_INT) if s == "counters"}
    counters["c_field_count"] = str(G.EXPECTED_COUNTER_FIELDS)
    for k, v in (tweaks or {}).items():
        if k in kv:
            kv[k] = v
        else:
            counters[k] = v
    for k in drop_keys:
        kv.pop(k, None)
    for k in extra_keys:
        kv[k] = "1"

    out = ["PROBE_BEGIN"]
    if double_begin:
        out.append("PROBE_BEGIN")
    out += [f"{k}={v}" for k, v in kv.items()]
    blocks = {
        "LADDER": ["depth,event_iteration,f_event,radius,step_norm,f_main,h_main,dx_inf,prox_rel"],
        "OPTIONS": ["opt_tr_min=1e-10", "opt_feas_tol=1e-06", "opt_qp_failure_budget=3"],
        "COUNTERS": [f"{k}={v}" for k, v in counters.items()],
        "HISTORY": [f"h_field_count={G.EXPECTED_ITERATE_FIELDS}", "h_rows_total=0",
                    "h_rows_printed=0", "h_printed_from_index=-1", "index,trial"],
        "LEDGER": ["ledger_attached=1", "ledger_qp_records=0"],
    }
    for name, body in blocks.items():
        if name in drop_blocks:
            continue
        out.append(f"{name}_BEGIN")
        out += body
        out.append(f"{name}_END")
    if not no_end:
        out.append("PROBE_END")
    return "\n".join(out) + "\n"


def run_gate(rows_dir, cells, env=None, bar=False):
    e = dict(os.environ)
    if env:
        e.update(env)
    cmd = [sys.executable, "-B", CHECKER, "--rows", rows_dir, "--cells", ",".join(cells)]
    if bar:
        cmd.append("--bar")
    p = subprocess.run(cmd, capture_output=True, text=True, env=e)
    return p.returncode, p.stdout + p.stderr


FOLDS = []


def fold(name, expect_rc, expect_verdicts, build, env=None, cells=None, bar=False):
    FOLDS.append((name, expect_rc, expect_verdicts, build, env, cells, bar))


# ---------------------------------------------------------------------------
# The folds. Each names the refusal or verdict it is built to produce.
# ---------------------------------------------------------------------------
fold("a good row matching the committed harness row", 0, {"STEENBRB": "PASS"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB")))

fold("a counter moved by one -- a FINDING, not a refusal", 1,
     {"STEENBRB": "DIFFERS"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB",
                                              tweaks={"c_factorizations": "25974"})))

fold("the terminal status moved", 1, {"STEENBRB": "DIFFERS"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", tweaks={"status": "optimal"})))

# The near-ulp bar on this cell's objective is 1e-12 * 4.0909e12 = 4.09 in
# ABSOLUTE terms, so the two folds below sit deliberately on either side of it:
# a gap of 20 must be a DIFFERENCE and a gap of 5e-4 must be accepted.
fold("the objective moved well past the near-ulp gate", 1, {"STEENBRB": "DIFFERS"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", tweaks={"f": "4090860686462.3218"})))

fold("the objective moved WITHIN the near-ulp gate -- accepted, still PASS", 0,
     {"STEENBRB": "PASS"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB",
                                              tweaks={"f": "4090860686442.3223"})))

fold("a nan objective against the pin's nan -- a BYTE fact, PASS", 0,
     {"STEENBRD": "PASS"},
     lambda d: write(d, "STEENBRD", synth_row("STEENBRD")), cells=["STEENBRD"])

fold("a -nan objective against the pin's nan -- both are NaN, PASS", 0,
     {"STEENBRE": "PASS"},
     lambda d: write(d, "STEENBRE", synth_row("STEENBRE", tweaks={"f": "-nan"})),
     cells=["STEENBRE"])

fold("a nan objective against a FINITE pin value -- still DIFFERS", 1,
     {"STEENBRB": "DIFFERS"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", tweaks={"f": "nan"})))

fold("an inf objective against the pin's nan -- NOT the same token, DIFFERS", 1,
     {"STEENBRE": "DIFFERS"},
     lambda d: write(d, "STEENBRE", synth_row("STEENBRE", tweaks={"f": "inf"})),
     cells=["STEENBRE"])

fold("no leg row at all -- CENSORED is never agreement", 2, {"STEENBRB": "REFUSED"},
     lambda d: None)

fold("an EMPTY leg row", 2, {"STEENBRB": "REFUSED"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", empty=True)))

fold("a row with no PROBE_END -- truncated mid-write", 2, {"STEENBRB": "REFUSED"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", no_end=True)))

fold("a row with two PROBE_BEGINs", 2, {"STEENBRB": "REFUSED"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", double_begin=True)))

fold("a row missing the HISTORY block", 2, {"STEENBRB": "REFUSED"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", drop_blocks=("HISTORY",))))

fold("a row missing the LEDGER block", 2, {"STEENBRB": "REFUSED"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", drop_blocks=("LEDGER",))))

fold("a key STRIPPED from the key block", 2, {"STEENBRB": "REFUSED"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", drop_keys=("qp_maxiter_exits",))))

fold("an UNEXPECTED key in the key block", 2, {"STEENBRB": "REFUSED"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", extra_keys=("a_new_key",))))

fold("a short COUNTERS block (c_field_count wrong)", 2, {"STEENBRB": "REFUSED"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB", tweaks={"c_field_count": "48"})))

fold("a short HISTORY block (h_field_count wrong)", 2, {"STEENBRB": "REFUSED"},
     lambda d: write(d, "STEENBRB", synth_row("STEENBRB").replace(
         f"h_field_count={G.EXPECTED_ITERATE_FIELDS}", "h_field_count=28")))

fold("a group-W cell -- NOT COMPARABLE, never PASS", 0, {"SAWPATH": "NOT_COMPARABLE"},
     lambda d: write(d, "SAWPATH", synth_row("STEENBRB", tweaks={"problem": "SAWPATH"})),
     cells=["SAWPATH"])

fold("EVERY comparable cell differing -- RULE.md 3.2's structural bar", 3,
     {"STEENBRB": "DIFFERS", "YORKNET": "DIFFERS"},
     lambda d: (write(d, "STEENBRB", synth_row("STEENBRB", tweaks={"c_major_iters": "99"})),
                write(d, "YORKNET", synth_row("YORKNET", tweaks={"c_major_iters": "99"}))),
     cells=["STEENBRB", "YORKNET"], bar=True)

fold("one cell differing, one passing -- a FINDING, not the bar", 1,
     {"STEENBRB": "DIFFERS", "YORKNET": "PASS"},
     lambda d: (write(d, "STEENBRB", synth_row("STEENBRB", tweaks={"c_major_iters": "99"})),
                write(d, "YORKNET", synth_row("YORKNET"))),
     cells=["STEENBRB", "YORKNET"])

fold("the same two failing cells WITHOUT the bar -- still only a FINDING", 1,
     {"STEENBRB": "DIFFERS", "YORKNET": "DIFFERS"},
     lambda d: (write(d, "STEENBRB", synth_row("STEENBRB", tweaks={"c_major_iters": "99"})),
                write(d, "YORKNET", synth_row("YORKNET", tweaks={"c_major_iters": "99"}))),
     cells=["STEENBRB", "YORKNET"])

fold("a TAMPERED harness header -- the gate refuses to parse by position", 2, {},
     lambda d: tamper_csv(d),
     env="TAMPER", cells=["STEENBRB"])


def write(d, cell, text):
    with open(os.path.join(d, f"{cell}.txt"), "w") as fh:
        fh.write(text)


def tamper_csv(d):
    write(d, "STEENBRB", synth_row("STEENBRB"))
    bad = os.path.join(d, "counters-tampered.csv")
    with open(COUNTERS_CSV) as src, open(bad, "w") as dst:
        for line in src:
            if line.startswith("cell_id,"):
                line = line.replace("major_iters", "majors", 1)
            dst.write(line)
    return bad


def main():
    failed = 0
    print("M7 W2 M3c -- IDENTITY GATE FOLD PROOFS (RULE.md section 4.3)")
    print(f"checker: {CHECKER}")
    print("")
    for i, (name, expect_rc, expect_verdicts, build, env, cells, bar) in enumerate(FOLDS, 1):
        d = tempfile.mkdtemp(prefix="m3c-fold-")
        try:
            produced = build(d)
            use_env = None
            if env == "TAMPER":
                use_env = {"M3C_COUNTERS_CSV": produced}
            rc, out = run_gate(d, cells or ["STEENBRB"], use_env, bar)
            ok = (rc == expect_rc)
            for cell, verdict in expect_verdicts.items():
                if not re.search(rf"^IDENTITY {re.escape(cell)} {verdict}$", out, re.M):
                    ok = False
            mark = "ok  " if ok else "FAIL"
            print(f"{mark} fold {i:2d}: {name}")
            print(f"        expected rc={expect_rc} verdicts={expect_verdicts}; got rc={rc}")
            if not ok:
                failed += 1
                for line in out.splitlines():
                    print(f"        | {line}")
        finally:
            shutil.rmtree(d, ignore_errors=True)
    print("")
    distinct = sorted({v for _, _, vs, _, _, _, _ in FOLDS for v in vs.values()})
    print(f"FOLDS={len(FOLDS)}  FAILED={failed}  DISTINCT_VERDICTS={distinct}")
    refusals = sum(1 for _, rc, _, _, _, _, _ in FOLDS if rc == 2)
    print(f"REFUSAL_FOLDS={refusals}  (a gate with no reachable refusal is not a gate)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
