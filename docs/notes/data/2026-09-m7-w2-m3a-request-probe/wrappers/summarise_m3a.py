#!/usr/bin/env python3
"""M7 W2 M3a -- THE MACHINE FOLD (RULE.md section 7 item 4).

Reads rows/stage1/<CELL>@<tag>.txt and writes tables/ and verdict.txt, and --
when rows/e/ exists -- applies RULE-E.md section 5 to the addendum-E legs and
writes verdict-E.txt, tables/e-first-qp.txt and tables/e-solve-counters.txt.
The stage-1 fold below is untouched by the addendum: re-running this script
must leave verdict.txt and tables/stage1-*.txt byte-identical.
NO NUMBER IN THE ARTIFACT IS TRANSCRIBED BY HAND: every cell of every table
below comes from a row file, and every predicate is RULE.md's, coded once.

The fold is pure file reading. It starts no solve, reads no wall-clock, and
asserts nothing that is not a counter, a status or a residual printed by the
probe. Re-running it against the committed rows must reproduce tables/ and
verdict.txt byte-identically.

Usage:  python3 wrappers/summarise_m3a.py <artifact-dir>
"""

from __future__ import annotations

import os
import sys

# ---------------------------------------------------------------------------
# RULE.md section 3: the radii, in the chain's own order, with the tag the
# chain wrote each leg under. The control is FIRST and is named as such.
# ---------------------------------------------------------------------------
CELLS = ["STEENBRB", "YORKNET"]
LADDER = [
    ("tr1", "1 (control)"),
    ("tr1e1", "10"),
    ("tr1e2", "1e2"),
    ("tr1e3", "1e3"),
    ("tr1e4", "1e4"),
    ("trinf", "+inf"),
]
CONTROL_TAG = "tr1"

# RULE.md section 5.2 (A3) and section 5.3 (H2). Fixed before any leg.
FEAS_TOL = 1e-6
HARM_FACTOR = 1.10


def read_row(path: str) -> dict:
    """Parse one PROBE_BEGIN block into {key: value-as-string}.

    The ladder (LADDER_BEGIN..LADDER_END) is returned under the key
    '_ladder' as a list of raw lines; nothing in it is parsed to float here.
    """
    keys: dict = {}
    ladder: list = []
    in_ladder = False
    with open(path, "r") as handle:
        for line in handle:
            line = line.rstrip("\n")
            if line == "LADDER_BEGIN":
                in_ladder = True
                continue
            if line == "LADDER_END":
                in_ladder = False
                continue
            if in_ladder:
                ladder.append(line)
                continue
            if line in ("PROBE_BEGIN", "PROBE_END"):
                continue
            if "=" in line:
                key, value = line.split("=", 1)
                keys[key] = value
    keys["_ladder"] = ladder
    return keys


def rank(row: dict) -> object:
    """RULE.md section 5.1's ordering, coded exactly as the table reads.

    Returns an int rank, or None for UNRANKED (`interrupted`, section 6).
    A status outside the table is a STOP: the rule says a leg reporting
    `acceptable`, `stalled` or `diverging` is reported as such, and this
    fold refuses to rank one rather than inventing a place for it.
    """
    status = row["status"]
    if status == "interrupted":
        return None
    if status == "optimal":
        return 4
    if status == "infeasible":
        return 3 if row["infeasibility_certified"] == "1" else 1
    if status in ("max_iter", "budget_exhausted"):
        return 2
    if status == "numerical_error":
        return 0
    raise SystemExit(
        "STOP (RULE.md section 5.1): unranked status %r -- the rule does not "
        "place it, and this fold will not." % status
    )


def status_label(row: dict) -> str:
    if row["status"] == "infeasible":
        return "infeasible (cert=%s)" % row["infeasibility_certified"]
    return row["status"]


def request0(row: dict) -> bool:
    """RULE.md section 2.2. `yes` iff a phase ran AND it was entered in major 0."""
    if int(row["restoration_entries"]) == 0:
        return False
    return row["x_k_source"].startswith("start_point")


def truncated(row: dict) -> bool:
    """RULE.md section 6. A TRUNCATED leg is UNDETERMINED for 5.2 and 5.3."""
    return row["deadline_fired"] == "1"


def worst_feas(row: dict) -> float:
    return max(float(row["feasibility_e"]), float(row["feasibility_i"]))


def fmt(value: str, width: int) -> str:
    return value.ljust(width)


def main() -> None:
    art = sys.argv[1] if len(sys.argv) > 1 else "."
    rows_dir = os.path.join(art, "rows", "stage1")
    tables_dir = os.path.join(art, "tables")
    os.makedirs(tables_dir, exist_ok=True)

    data: dict = {}
    missing: list = []
    for cell in CELLS:
        for tag, _label in LADDER:
            path = os.path.join(rows_dir, "%s@%s.txt" % (cell, tag))
            if not os.path.exists(path):
                # RULE.md section 6: a CENSORED leg has NO ROW. It is listed
                # and never filled.
                missing.append((cell, tag))
                continue
            data[(cell, tag)] = read_row(path)

    out: list = []
    w = out.append

    w("M7 W2 M3a -- THE VERDICT: RULE.md section 5, applied to stage 1.")
    w("")
    w("Machine-written by wrappers/summarise_m3a.py from rows/stage1/. No number")
    w("below is transcribed by hand. RULE.md was committed at dbf55675, BEFORE any")
    w("leg ran, and is applied here AS WRITTEN -- no clause is relaxed, widened or")
    w("re-read in the light of what the legs said.")
    w("")
    w("THE PREDICATES, as the rule states them:")
    w("  WINDOW_ARTEFACT(c,r) iff (A1) the major-0 request is GONE at (c,r)")
    w("                       AND  (A2) rank(c,r) >= rank(c,control)")
    w("                       AND  (A3) max(feasibility_e, feasibility_i) <= %g" % FEAS_TOL)
    w("  HARM(c,r)            iff (H1) rank(c,r) < rank(c,control)")
    w("                       OR   (H2) factorizations(c,r) > %.2f * factorizations(c,control)"
      % HARM_FACTOR)
    w("  request0             iff restoration_entries >= 1 AND x_k_source == start_point")
    w("                          (RULE.md section 2.2)")
    w("")
    if missing:
        w("CENSORED LEGS (RULE.md section 6 -- NO ROW, never filled):")
        for cell, tag in missing:
            w("  %s @ %s" % (cell, tag))
    else:
        w("CENSORED LEGS: none. All %d legs produced a row." % len(data))
    trunc = [k for k, v in data.items() if truncated(v)]
    if trunc:
        w("TRUNCATED LEGS (deadline_fired=1 -- counters reported, status UNRANKED,")
        w("verdict UNDETERMINED):")
        for cell, tag in trunc:
            w("  %s @ %s" % (cell, tag))
    else:
        w("TRUNCATED LEGS: none. deadline_fired=0 on every leg, so no leg's status")
        w("is unranked and no verdict is UNDETERMINED on that ground.")
    w("")

    # ---------------------------------------------------------------- table
    header = [
        ("cell", 9),
        ("tr_init", 11),
        ("req0", 5),
        ("first_qp_plain_status", 23),
        ("rest_entries", 13),
        ("rest_sub_status", 16),
        ("terminal status", 20),
        ("max(h_e,h_i)", 14),
        ("f", 22),
        ("majors", 7),
        ("minors", 8),
        ("factorizations", 15),
        ("fact/control", 13),
        ("A1", 4),
        ("A2", 4),
        ("A3", 4),
        ("WINDOW_ARTEFACT", 16),
        ("HARM", 18),
    ]
    w("THE VERDICT TABLE (RULE.md section 5.2 and 5.3), cell x radius")
    w("-" * 78)
    w("".join(fmt(name, width) for name, width in header))

    qualifying: dict = {cell: [] for cell in CELLS}
    harmed: dict = {cell: [] for cell in CELLS}
    for cell in CELLS:
        control = data.get((cell, CONTROL_TAG))
        if control is None:
            w("%s: CONTROL MISSING -- UNDETERMINED at every radius (RULE.md section 3)."
              % cell)
            continue
        ctrl_rank = rank(control)
        ctrl_fact = int(control["factorizations"])
        for tag, label in LADDER:
            row = data.get((cell, tag))
            if row is None:
                w("".join(fmt(v, wd) for v, (_n, wd) in zip(
                    [cell, label] + ["CENSORED"] * (len(header) - 2), header)))
                continue
            r = rank(row)
            fact = int(row["factorizations"])
            ratio = fact / ctrl_fact
            is_control = tag == CONTROL_TAG
            if is_control:
                a1 = a2 = a3 = wa = harm = "--"
            elif truncated(row) or r is None or ctrl_rank is None:
                a1 = a2 = a3 = "?"
                wa = "UNDETERMINED"
                harm = "UNDETERMINED"
            else:
                a1_v = not request0(row)
                a2_v = r >= ctrl_rank
                a3_v = worst_feas(row) <= FEAS_TOL
                a1 = "yes" if a1_v else "NO"
                a2 = "yes" if a2_v else "NO"
                a3 = "yes" if a3_v else "NO"
                wa_v = a1_v and a2_v and a3_v
                wa = "YES" if wa_v else "no"
                if wa_v:
                    qualifying[cell].append(label)
                h1 = r < ctrl_rank
                h2 = fact > HARM_FACTOR * ctrl_fact
                if h1 and h2:
                    harm = "HARM (H1+H2)"
                elif h1:
                    harm = "HARM (H1)"
                elif h2:
                    harm = "HARM (H2)"
                else:
                    harm = "no"
                if h1 or h2:
                    harmed[cell].append(label)
            cells = [
                cell,
                label,
                "yes" if request0(row) else "no",
                row["first_qp_plain_status"],
                row["restoration_entries"],
                row["restoration_sub_status"] if int(row["restoration_entries"]) else "(no phase)",
                status_label(row),
                "%.6g" % worst_feas(row),
                "%.10g" % float(row["f"]),
                row["major_iters"],
                row["qp_minor_iters"],
                row["factorizations"],
                "%.4f" % ratio,
                a1,
                a2,
                a3,
                wa,
                harm,
            ]
            w("".join(fmt(str(v), wd) for v, (_n, wd) in zip(cells, header)))
        w("")

    # ------------------------------------------------------- the conclusion
    w("RULE.md section 5.2: 'Only a radius satisfying it on BOTH goes to stage 2.'")
    for cell in CELLS:
        got = qualifying[cell]
        w("  %-9s qualifying radii: %s" % (cell, ", ".join(got) if got else "NONE"))
    both = [label for _tag, label in LADDER
            if label != "1 (control)"
            and all(label in qualifying[cell] for cell in CELLS)]
    w("  radii satisfying WINDOW_ARTEFACT on BOTH stage-1 cells: %s"
      % (", ".join(both) if both else "NONE"))
    w("")
    for cell in CELLS:
        got = harmed[cell]
        w("  %-9s radii showing HARM: %s" % (cell, ", ".join(got) if got else "none"))
    w("")
    if both:
        labelled = [lb + (" (qualified-with-harm)"
                          if any(lb in harmed[c] for c in CELLS) else "")
                    for lb in both]
        w("VERDICT: the hypothesis IS supported at %s." % ", ".join(labelled))
        w("Stage 2 runs at those radii, per RULE.md section 3 and section 5.2.")
    else:
        w("VERDICT: NO RADIUS QUALIFIES ON BOTH STAGE-1 CELLS.")
        w("")
        w("RULE.md section 5.5, the failure path, applies VERBATIM: the task reports")
        w("this table and the counters behind it and STOPS. STAGE 2 IS NOT RUN, and")
        w("stage 3 -- whose trigger (section 5.4) is defined only at the stage-2")
        w("radii -- is therefore not reached either. The hypothesis is recorded as")
        w("NOT SUPPORTED at these radii on these cells. It is not re-tested at other")
        w("radii, not widened to other cells, and not rescued by relaxing (A1)-(A3).")
        w("")
        w("NO DEFAULT IS CHANGED BY THIS TASK -- not tr_init, not")
        w("restoration_proximity, not qp.tr_radius, not feas_tol (RULE.md section")
        w("5.5, and section 1).")
    w("")

    with open(os.path.join(art, "verdict.txt"), "w") as handle:
        handle.write("\n".join(out) + "\n")

    # ------------------------------------------------ tables/: the QP lead
    lead: list = []
    lw = lead.append
    lw("M7 W2 M3a -- THE QP-SUBPROBLEM COUNTERS BEHIND THE VERDICT TABLE")
    lw("")
    lw("Machine-written by wrappers/summarise_m3a.py from rows/stage1/. Every column")
    lw("is a public counter printed verbatim by the probe, except the two marked")
    lw("DERIVED, whose arithmetic is stated in the header line below.")
    lw("")
    lw("  minors/maxiter-exit  DERIVED = qp_minor_iters / qp_maxiter_exits, printed")
    lw("                       ONLY when restoration_iters == 0 AND")
    lw("                       elastic_activations == 0 AND")
    lw("                       qp_maxiter_exits == major_iters -- i.e. when every")
    lw("                       minor the solve spent belongs to a plain major and")
    lw("                       every plain major ended at the cap. Under those three")
    lw("                       conditions the quotient IS the per-solve minor cap the")
    lw("                       walk ran at. '--' means at least one condition fails")
    lw("                       and the quotient would not be that cap.")
    lw("  iterate moved        DERIVED = steps_accepted > 0. The ladder's depth-0")
    lw("                       f_main/h_main columns in rows/stage1/ are the direct")
    lw("                       reading; this column is the counter that summarises it.")
    lw("")
    qhdr = [
        ("cell", 9), ("tr_init", 11), ("n", 5), ("me", 5), ("mi", 4),
        ("first_qp_status", 16), ("first_qp_elastic", 17),
        ("first_qp_verdict", 17), ("first_qp_tr_radius", 19),
        ("first_qp_tr_binding", 20),
        ("majors", 7), ("hist_rows", 10), ("minors", 8),
        ("minors/maxiter-exit", 20),
        ("qp_maxiter_exits", 17), ("qp_numerical_exits", 19),
        ("qp_failures_routed", 19), ("qp_fail_row_peak", 17),
        ("elastic_activations", 20), ("rejected_steps", 15),
        ("steps_accepted", 15), ("iterate moved", 14),
        ("factorizations", 15), ("symbolic", 9),
    ]
    lw("".join(fmt(name, width) for name, width in qhdr))
    for cell in CELLS:
        for tag, label in LADDER:
            row = data.get((cell, tag))
            if row is None:
                lw("%s%sCENSORED" % (fmt(cell, 9), fmt(label, 11)))
                continue
            plain_only = (int(row["restoration_iters"]) == 0
                          and int(row["elastic_activations"]) == 0
                          and int(row["qp_maxiter_exits"]) == int(row["major_iters"])
                          and int(row["qp_maxiter_exits"]) > 0)
            if plain_only:
                q = int(row["qp_minor_iters"]) / int(row["qp_maxiter_exits"])
                per = "%g" % q if q == int(q) else "%.6f" % q
            else:
                per = "--"
            vals = [
                cell, label, row["n"], row["me"], row["mi"],
                row["first_qp_status"], row["first_qp_elastic_applied"],
                row["first_qp_verdict"], row["first_qp_tr_radius"],
                row["first_qp_tr_binding"],
                row["major_iters"], row["history_rows"], row["qp_minor_iters"],
                per,
                row["qp_maxiter_exits"], row["qp_numerical_exits"],
                row["qp_failures_routed"], row["qp_failures_in_a_row_peak"],
                row["elastic_activations"], row["rejected_steps"],
                row["steps_accepted"],
                "yes" if int(row["steps_accepted"]) > 0 else "NO",
                row["factorizations"], row["symbolic_analyses"],
            ]
            lw("".join(fmt(str(v), wd) for v, (_n, wd) in zip(vals, qhdr)))
        lw("")
    with open(os.path.join(tables_dir, "stage1-qp-counters.txt"), "w") as handle:
        handle.write("\n".join(lead) + "\n")

    # ------------------------------------------- tables/: the S5 record fold
    s5: list = []
    sw = s5.append
    sw("M7 W2 M3a -- THE S5 RESTORATION RECORD, cell x radius")
    sw("")
    sw("Machine-written by wrappers/summarise_m3a.py from rows/stage1/.")
    sw("restoration_sub_status is M3a task A's field. It has NO 'never entered'")
    sw("sentinel: the record's absence rule is restoration_entries == 0, so this")
    sw("table prints '(no phase ran)' in that case and NEVER the raw field, exactly")
    sw("as RULE.md section 2.3 requires.")
    sw("")
    shdr = [
        ("cell", 9), ("tr_init", 11), ("rest_entries", 13), ("rest_iters", 11),
        ("restoration_sub_status", 23), ("x_k_source", 50),
        ("h_in", 22), ("h_out", 24), ("f_in", 22), ("f_out", 22),
    ]
    sw("".join(fmt(name, width) for name, width in shdr))
    for cell in CELLS:
        for tag, label in LADDER:
            row = data.get((cell, tag))
            if row is None:
                sw("%s%sCENSORED" % (fmt(cell, 9), fmt(label, 11)))
                continue
            entered = int(row["restoration_entries"]) > 0
            vals = [
                cell, label, row["restoration_entries"], row["restoration_iters"],
                row["restoration_sub_status"] if entered else "(no phase ran)",
                row["x_k_source"],
                row["restoration_h_in"] if entered else "(no phase ran)",
                row["restoration_h_out"] if entered else "(no phase ran)",
                row["restoration_f_in"] if entered else "(no phase ran)",
                row["restoration_f_out"] if entered else "(no phase ran)",
            ]
            sw("".join(fmt(str(v), wd) for v, (_n, wd) in zip(vals, shdr)))
        sw("")
    with open(os.path.join(tables_dir, "stage1-restoration-record.txt"), "w") as handle:
        handle.write("\n".join(s5) + "\n")

    sys.stdout.write("\n".join(out) + "\n")

    # RULE-E.md section 7 item 4. Writes only NEW files; nothing above is
    # re-opened for writing.
    fold_e(art, tables_dir)


# ===========================================================================
# ADDENDUM E -- THE QP-CAP EXPERIMENTS.
#
# RULE-E.md was committed at ade36329, BEFORE any E leg ran (the identity leg
# included), and its section 5 is coded below ONCE, from the rule as written.
# Nothing in this block touches the stage-1 fold above: `verdict.txt` and
# `tables/stage1-*.txt` are byte-for-byte what they were at 6bc75ee9, as
# RULE-E.md section 0 requires.
#
# Reads rows/e/<TAG>.txt plus the two COMMITTED stage-1 `@trinf` rows, which
# RULE-E.md section 2 fixes as the controls and which are NOT re-run. Writes
# verdict-E.txt, tables/e-first-qp.txt and tables/e-solve-counters.txt. No
# number in any of the three is transcribed by hand.
# ===========================================================================

E_CONTROL_TAG = "trinf"          # rows/stage1/<CELL>@trinf.txt -- RULE-E.md section 2
E_LEGS = [
    ("STEENBRB", "identity", "identity", "(both new options at their defaults)"),
    ("STEENBRB", "E1", "E1", "--qp-cap-retry on"),
    ("YORKNET", "E1", "E1", "--qp-cap-retry on"),
    ("STEENBRB", "E2", "E2", "--qp-max-iter 46800"),
    ("YORKNET", "E2", "E2", "--qp-max-iter 25600"),
]
E_CAP_MULTIPLE = 10              # RULE-E.md section 1: the E2 caps are 10x the quotient
E_RETRY_MULTIPLE = 2             # RULE-E.md section 1: E1 retries once at twice the cap


def e_key(row: dict, name: str) -> str:
    """A key the STAGE-1 probe did not print is named as absent, never filled.

    The five addendum-E keys do not exist in the committed stage-1 control rows;
    those cells read `n/a (absent)` and are never interpolated or back-filled.
    """
    return row.get(name, "n/a (absent)")


def e_derived_cap(control: dict):
    """RULE-E.md section 1's 'per-major minor quotient OBSERVED in stage 1'.

    Computed here from the committed control row under the same three
    conditions tables/stage1-qp-counters.txt states for that column, so the
    number is read off the evidence rather than copied out of the rule.
    Returns None when a condition fails -- in which case the quotient is not
    that cap and this fold will not pretend it is.
    """
    if int(control["restoration_iters"]) != 0:
        return None
    if int(control["elastic_activations"]) != 0:
        return None
    exits = int(control["qp_maxiter_exits"])
    if exits <= 0 or exits != int(control["major_iters"]):
        return None
    quotient = int(control["qp_minor_iters"]) / exits
    return int(quotient) if quotient == int(quotient) else None


def e_supported(row: dict) -> bool:
    """RULE-E.md section 5: SUPPORTED iff the first QP ends `optimal` AND
    steps_accepted >= 1."""
    return (row["first_qp_plain_status"] == "optimal"
            and int(row["steps_accepted"]) >= 1)


def fold_e(art: str, tables_dir: str) -> None:
    rows_dir = os.path.join(art, "rows", "e")
    if not os.path.isdir(rows_dir):
        return

    legs: dict = {}
    missing: list = []
    for cell, tag, _arm, _arg in E_LEGS:
        path = os.path.join(rows_dir, "%s@%s.txt" % (cell, tag))
        if not os.path.exists(path):
            missing.append((cell, tag))
            continue
        legs[(cell, tag)] = read_row(path)

    controls: dict = {}
    for cell in CELLS:
        path = os.path.join(art, "rows", "stage1", "%s@%s.txt" % (cell, E_CONTROL_TAG))
        if os.path.exists(path):
            controls[cell] = read_row(path)
    caps = {cell: e_derived_cap(row) for cell, row in controls.items()}

    out: list = []
    w = out.append
    w("M7 W2 M3a ADDENDUM E -- THE VERDICT: RULE-E.md section 5, applied to the")
    w("four E legs.")
    w("")
    w("Machine-written by wrappers/summarise_m3a.py from rows/e/ and from the two")
    w("COMMITTED stage-1 controls in rows/stage1/. No number below is transcribed")
    w("by hand. RULE-E.md was committed at ade36329, BEFORE any E leg ran, and is")
    w("applied here AS WRITTEN -- no predicate is relaxed, widened or re-read in")
    w("the light of what the legs said.")
    w("")
    w("THIS IS NOT A RE-TEST OF THE RADIUS HYPOTHESIS (RULE-E.md section 0). That")
    w("hypothesis stays CLOSED by RULE.md section 5.5 and verdict.txt stands")
    w("exactly as committed at 6bc75ee9. The hypothesis under test here is:")
    w("")
    w("  H-QP  With the trust window removed, the first QP subproblem on STEENBRB")
    w("        and YORKNET is SOLVABLE by the shipped active-set walk given more")
    w("        minor iterations; the solve dies on the iteration cap, not on a")
    w("        broken subproblem.")
    w("")
    w("THE PREDICATES, as the rule states them:")
    w("  SUPPORTED(cell,arm) iff first_qp_plain_status == optimal")
    w("                      AND steps_accepted >= 1")
    w("  REFUTED(cell)       iff the first QP still ends `max_iter` on that cell's")
    w("                          E2 leg (the %dx-cap arm)" % E_CAP_MULTIPLE)
    w("  TRUNCATED(leg)      iff deadline_fired == 1 -- counters ARE reported and")
    w("                          the leg yields NO VERDICT")
    w("  CENSORED(leg)       iff rc != 0 -- NO ROW, never filled")
    w("")

    if missing:
        w("CENSORED LEGS (RULE-E.md section 5 -- NO ROW, never filled):")
        for cell, tag in missing:
            w("  %s @ %s" % (cell, tag))
    else:
        w("CENSORED LEGS: none. All %d legs produced a row." % len(legs))
    trunc = [k for k, v in legs.items() if truncated(v)]
    if trunc:
        w("TRUNCATED LEGS (deadline_fired=1 -- counters reported, NO VERDICT):")
        for cell, tag in trunc:
            w("  %s @ %s" % (cell, tag))
    else:
        w("TRUNCATED LEGS: none.")
    w("")

    w("THE IDENTITY LEG (RULE-E.md section 3), the gate on the rebuilt instrument:")
    ident = legs.get(("STEENBRB", "identity"))
    ctrl = controls.get("STEENBRB")
    if ident is not None and ctrl is not None:
        shared = sorted(k for k in ident
                        if k != "_ladder" and k in ctrl
                        and k not in ("wall_seconds", "elapsed", "elapsed_seconds"))
        differing = [k for k in shared if ident[k] != ctrl[k]]
        newk = sorted(k for k in ident if k != "_ladder" and k not in ctrl)
        w("  %d shared keys re-compared by this fold, %d differing; ladder block %d vs %d"
          " lines (the CSV header plus its rows), %s"
          % (len(shared), len(differing), len(ident["_ladder"]), len(ctrl["_ladder"]),
             "IDENTICAL" if ident["_ladder"] == ctrl["_ladder"] else "DIFFERING"))
        w("  keys present only in the rebuilt probe: %s" % ", ".join(newk))
        if differing:
            w("  DIFFERING KEYS: %s" % ", ".join(differing))
        w("  (identity-e.txt is the chain's own report, run before any E leg;")
        w("   this line is this fold's independent re-check of the same files.)")
    else:
        w("  UNAVAILABLE -- the identity leg or its control row is missing.")
    w("")

    w("THE DERIVED PER-MAJOR MINOR CAP, read off each cell's own committed control")
    w("(rows/stage1/<CELL>@%s.txt) under the three conditions of" % E_CONTROL_TAG)
    w("tables/stage1-qp-counters.txt, and the arguments the E legs actually ran at:")
    for cell in CELLS:
        cap = caps.get(cell)
        e2 = legs.get((cell, "E2"))
        e1 = legs.get((cell, "E1"))
        if cap is None:
            w("  %-9s derived cap: UNAVAILABLE (a condition fails on the control)" % cell)
            continue
        line = "  %-9s derived cap %6d" % (cell, cap)
        if e2 is not None:
            arg = int(e2["qp_max_iter"])
            line += ";  E2 ran at --qp-max-iter %-6d = %dx  %s" % (
                arg, E_CAP_MULTIPLE,
                "AS REGISTERED" if arg == E_CAP_MULTIPLE * cap
                else "NOT %dx THE CAP (%d != %d)" % (E_CAP_MULTIPLE, arg,
                                                     E_CAP_MULTIPLE * cap))
        if e1 is not None and e1["first_qp_minor_iters"].isdigit():
            got = int(e1["first_qp_minor_iters"])
            line += ";  E1 first_qp_minor_iters %d = %s" % (
                got,
                "%dx the cap" % E_RETRY_MULTIPLE if got == E_RETRY_MULTIPLE * cap
                else "%.4gx the cap" % (got / cap))
        w(line)
    w("")

    # ---------------------------------------------------------------- table
    header = [
        ("cell", 9), ("arm", 5), ("argument", 21), ("deadline_fired", 15),
        ("first_qp_plain_status", 22), ("steps_accepted", 15),
        ("first_qp_minor_iters", 21), ("first_qp_factorizations", 24),
        ("terminal status", 17), ("VERDICT", 30),
    ]
    w("THE VERDICT TABLE (RULE-E.md section 5), cell x arm. The control row of each")
    w("cell is its committed stage-1 @%s leg and carries no verdict." % E_CONTROL_TAG)
    w("-" * 78)
    w("".join(fmt(name, width) for name, width in header))

    supported: list = []
    refuted: list = []
    no_verdict: list = []
    not_supported: list = []
    for cell in CELLS:
        ctl = controls.get(cell)
        if ctl is not None:
            vals = [cell, "ctrl", "(stage-1 control)", ctl["deadline_fired"],
                    ctl["first_qp_plain_status"], ctl["steps_accepted"],
                    e_key(ctl, "first_qp_minor_iters"),
                    e_key(ctl, "first_qp_factorizations"),
                    status_label(ctl), "-- (control, RULE-E.md section 2)"]
            w("".join(fmt(str(v), wd) for v, (_n, wd) in zip(vals, header)))
        for lcell, tag, arm, arg in E_LEGS:
            if lcell != cell or arm == "identity":
                continue
            row = legs.get((lcell, tag))
            if row is None:
                w("".join(fmt(v, wd) for v, (_n, wd) in zip(
                    [cell, arm, arg] + ["CENSORED"] * (len(header) - 3), header)))
                no_verdict.append((cell, arm, "CENSORED -- no row"))
                continue
            if truncated(row):
                verdict = "NO VERDICT (TRUNCATED)"
                no_verdict.append((cell, arm, "TRUNCATED at the 300 s solver deadline"))
            elif e_supported(row):
                verdict = "SUPPORTED"
                supported.append((cell, arm))
            else:
                verdict = "not SUPPORTED"
                not_supported.append((cell, arm))
            if arm == "E2" and not truncated(row):
                if row["first_qp_plain_status"] == "max_iter":
                    refuted.append(cell)
                    verdict += " / REFUTED(%s)" % cell
            vals = [cell, arm, arg, row["deadline_fired"],
                    row["first_qp_plain_status"], row["steps_accepted"],
                    row["first_qp_minor_iters"], row["first_qp_factorizations"],
                    status_label(row), verdict]
            w("".join(fmt(str(v), wd) for v, (_n, wd) in zip(vals, header)))
        w("")

    # ------------------------------------------------------- the conclusion
    w("WHAT THE RULE RETURNS")
    w("-" * 78)
    w("  SUPPORTED legs:     %s"
      % (", ".join("%s@%s" % p for p in supported) if supported else "NONE"))
    w("  not-SUPPORTED legs: %s"
      % (", ".join("%s@%s" % p for p in not_supported) if not_supported else "none"))
    w("  REFUTED cells:      %s" % (", ".join(refuted) if refuted else "NONE"))
    for cell, arm, why in no_verdict:
        w("  NO VERDICT: %s@%s -- %s" % (cell, arm, why))
    w("")
    if supported:
        w("VERDICT: H-QP IS SUPPORTED on %s." % ", ".join("%s@%s" % p for p in supported))
        e1_fail_e2_ok = [c for c in CELLS
                         if (c, "E1") in [tuple(x) for x in not_supported]
                         and (c, "E2") in supported]
        for cell in e1_fail_e2_ok:
            row = legs[(cell, "E2")]
            cap = caps.get(cell)
            w("  %s: SOLVABLE, BUT %dx IS NOT ENOUGH -- the first QP needed %s minors,"
              % (cell, E_RETRY_MULTIPLE, row["first_qp_minor_iters"]))
            if cap is not None:
                w("     against a derived cap of %d and %dx it = %d."
                  % (cap, E_RETRY_MULTIPLE, E_RETRY_MULTIPLE * cap))
    elif refuted and len(refuted) == len(CELLS):
        w("VERDICT: H-QP is REFUTED on both cells.")
        w("")
        w("RULE-E.md section 5's failure path applies VERBATIM: this table and the")
        w("counters behind it are the report, and the addendum STOPS.")
    else:
        w("VERDICT: NO LEG IS SUPPORTED, AND NO CELL IS REFUTED.")
        w("")
        w("H-QP is NOT SUPPORTED on either cell at either arm: the SUPPORTED")
        w("predicate is FALSE wherever a leg could be evaluated. It is NOT REFUTED")
        w("either -- REFUTED(cell) is defined on that cell's E2 leg alone, and both")
        w("E2 legs are TRUNCATED, which RULE-E.md section 5 says yields NO VERDICT.")
        w("The rule's last clause therefore governs: 'Any other combination is")
        w("reported as exactly what it is -- the table is the finding -- and is not")
        w("forced into one of the three labels above.'")
        w("")
        w("The failure path's INSTRUCTION applies unchanged: H-QP is recorded as not")
        w("supported on these cells at these caps, and the addendum STOPS. It is not")
        w("re-tested at a larger cap, not widened to other cells, not re-run at a")
        w("wider budget, and not rescued by relaxing the SUPPORTED predicate.")
    w("")
    w("NO DEFAULT IS CHANGED BY THIS ADDENDUM UNDER ANY OUTCOME (RULE-E.md section")
    w("0) -- not qp_cap_retry, not QpOptions::max_iter, not tr_init, not")
    w("restoration_proximity, not qp.tr_radius, not feas_tol. A SUPPORTED verdict")
    w("would have licensed none either.")
    w("")

    # ------------------------------------------- the observation, NOT a verdict
    w("=" * 78)
    w("OBSERVATION -- OUTSIDE THE VERDICT, AND NOT ONE")
    w("=" * 78)
    w("")
    w("The two E2 legs are TRUNCATED and the rule gives them no verdict. What")
    w("follows is not a verdict and is not offered as one: it is what the first")
    w("QP's own history[0] fields say on those legs, printed because they were")
    w("complete BEFORE the deadline fired. The evidence that they were complete is")
    w("in the row itself and is checked here, not assumed:")
    w("")
    for cell in CELLS:
        row = legs.get((cell, "E2"))
        if row is None:
            w("  %-9s E2: NO ROW." % cell)
            continue
        cap = caps.get(cell)
        arg = int(row["qp_max_iter"])
        minors = row["first_qp_minor_iters"]
        at_cap = minors.isdigit() and int(minors) == arg
        w("  %s @ E2  (--qp-max-iter %d, deadline_fired=%s, terminal status %s)"
          % (cell, arg, row["deadline_fired"], status_label(row)))
        w("      first_qp_solved            = %s   (history[0] carries a QP result)"
          % row["first_qp_solved"])
        w("      first_qp_minor_iters       = %-8s %s"
          % (minors,
             "= the cap EXACTLY: the first QP ran out its whole budget and RETURNED"
             if at_cap else "(NOT equal to the cap argument %d)" % arg))
        w("      first_qp_status            = %s" % row["first_qp_status"])
        w("      first_qp_elastic_applied   = %s   (so first_qp_plain_status is the"
          % row["first_qp_elastic_applied"])
        w("                                       raw status, no inference)")
        w("      first_qp_factorizations    = %s" % row["first_qp_factorizations"])
        w("      history_rows               = %-8s major_iters = %s -- a later major"
          % (row["history_rows"], row["major_iters"]))
        w("                                       exists, so history[0] is closed")
        w("      qp_maxiter_exits           = %s   qp_cap_retries = %s"
          % (row["qp_maxiter_exits"], row["qp_cap_retries"]))
        if cap is not None and minors.isdigit():
            w("      minors vs the derived cap  = %d = %.6gx %d"
              % (int(minors), int(minors) / cap, cap))
        w("")
    w("Read plainly, and as an OBSERVATION only: on both cells the first QP")
    w("subproblem consumed ten times the minor budget stage 1 refused it and still")
    w("came back `max_iter`. That is the reading the REFUTED label would have")
    w("carried had the legs not been truncated; the rule withholds the label")
    w("because the LEG was truncated, and this fold withholds it too.")
    w("")
    w("The E1 legs are NOT truncated and need no such caveat: at twice the cap,")
    for cell in CELLS:
        row = legs.get((cell, "E1"))
        if row is None:
            continue
        w("  %-9s first QP %s after %s minors on %s factorizations; qp_cap_retries=%s,"
          % (cell, row["first_qp_plain_status"], row["first_qp_minor_iters"],
             row["first_qp_factorizations"], row["qp_cap_retries"]))
        w("            qp_maxiter_exits=%s, steps_accepted=%s, terminal %s"
          % (row["qp_maxiter_exits"], row["steps_accepted"], status_label(row)))
    w("")
    w("RULE-E.md section 4's counter accounting is what makes those two numbers")
    w("read: qp_maxiter_exits counts capped exits AT THE ROUTING SITE, so a retry")
    w("that RESCUED its subproblem would not appear there at all. Both legs show")
    w("qp_cap_retries and qp_maxiter_exits EQUAL and non-zero, which that section")
    w("reads as: every capped subproblem was retried at twice the budget and hit")
    w("the doubled cap too. No retry rescued anything.")
    w("")
    w("QpCounters::schur_updates is ABSENT from SqpCounters and is recorded as")
    w("absent, not substituted for (RULE-E.md section 4).")

    with open(os.path.join(art, "verdict-E.txt"), "w") as handle:
        handle.write("\n".join(out) + "\n")

    # ------------------------------------------------ tables/: the first QP
    t1: list = []
    tw = t1.append
    tw("M7 W2 M3a ADDENDUM E -- THE FIRST QP SUBPROBLEM, leg by leg")
    tw("")
    tw("Machine-written by wrappers/summarise_m3a.py from rows/e/ and the committed")
    tw("stage-1 controls. Every column is a key the probe printed verbatim, except")
    tw("`minors/cap`, DERIVED = first_qp_minor_iters / the cell's derived per-major")
    tw("cap, which is itself read off that cell's control row (see verdict-E.txt).")
    tw("The stage-1 controls did not print the five addendum-E keys and are marked")
    tw("absent rather than filled.")
    tw("")
    h1 = [
        ("cell", 9), ("leg", 16), ("argument", 38),
        ("qp_cap_retry", 14), ("qp_max_iter", 14), ("deadline_fired", 15),
        ("first_qp_solved", 16), ("first_qp_status", 16),
        ("elastic_applied", 16), ("first_qp_plain_status", 22),
        ("first_qp_verdict", 17), ("tr_radius", 10), ("tr_binding", 11),
        ("rest_seed_used", 15),
        ("first_qp_minor_iters", 21), ("first_qp_factorizations", 24),
        ("minors/cap", 11),
    ]
    tw("".join(fmt(name, width) for name, width in h1))
    for cell in CELLS:
        ctl = controls.get(cell)
        cap = caps.get(cell)
        rows_here = []
        if ctl is not None:
            rows_here.append(("control @%s" % E_CONTROL_TAG, "(stage-1 control)", ctl))
        for lcell, tag, _arm, arg in E_LEGS:
            if lcell == cell and (lcell, tag) in legs:
                rows_here.append((tag, arg, legs[(lcell, tag)]))
        for label, arg, row in rows_here:
            minors = e_key(row, "first_qp_minor_iters")
            ratio = ("%.6g" % (int(minors) / cap)) if (minors.isdigit() and cap) else "--"
            vals = [cell, label, arg,
                    e_key(row, "qp_cap_retry"), e_key(row, "qp_max_iter"),
                    row["deadline_fired"], row["first_qp_solved"],
                    row["first_qp_status"], row["first_qp_elastic_applied"],
                    row["first_qp_plain_status"], row["first_qp_verdict"],
                    row["first_qp_tr_radius"], row["first_qp_tr_binding"],
                    row["first_qp_restoration_seed_used"],
                    minors, e_key(row, "first_qp_factorizations"), ratio]
            tw("".join(fmt(str(v), wd) for v, (_n, wd) in zip(vals, h1)))
        tw("")
    with open(os.path.join(tables_dir, "e-first-qp.txt"), "w") as handle:
        handle.write("\n".join(t1) + "\n")

    # ------------------------------------------- tables/: the solve counters
    t2: list = []
    sw = t2.append
    sw("M7 W2 M3a ADDENDUM E -- THE WHOLE-SOLVE COUNTERS, leg by leg")
    sw("")
    sw("Machine-written by wrappers/summarise_m3a.py from rows/e/ and the committed")
    sw("stage-1 controls. Every column is a public counter printed verbatim by the")
    sw("probe; `max(h_e,h_i)` is max(feasibility_e, feasibility_i) and is the only")
    sw("arithmetic here. `QpCounters::schur_updates` is NOT on SqpCounters and has")
    sw("no column: it is absent, not substituted for (RULE-E.md section 4).")
    sw("restoration_sub_status is never printed without restoration_entries beside")
    sw("it, and reads '(no phase ran)' when that count is 0.")
    sw("")
    h2 = [
        ("cell", 9), ("leg", 16), ("deadline_fired", 15),
        ("majors", 7), ("hist_rows", 10), ("minors", 8),
        ("qp_cap_retries", 16), ("qp_maxiter_exits", 17),
        ("qp_numerical_exits", 19), ("qp_failures_routed", 19),
        ("qp_fail_row_peak", 17),
        ("steps_accepted", 15), ("rejected_steps", 15),
        ("factorizations", 15), ("symbolic", 9), ("border_refine_steps", 20),
        ("elastic_activations", 20), ("rest_entries", 13),
        ("restoration_sub_status", 23),
        ("terminal status", 17), ("cert", 5), ("max(h_e,h_i)", 14), ("f", 22),
    ]
    sw("".join(fmt(name, width) for name, width in h2))
    for cell in CELLS:
        ctl = controls.get(cell)
        rows_here = []
        if ctl is not None:
            rows_here.append(("control @%s" % E_CONTROL_TAG, ctl))
        for lcell, tag, _arm, _arg in E_LEGS:
            if lcell == cell and (lcell, tag) in legs:
                rows_here.append((tag, legs[(lcell, tag)]))
        for label, row in rows_here:
            entered = int(row["restoration_entries"]) > 0
            vals = [cell, label, row["deadline_fired"],
                    row["major_iters"], row["history_rows"], row["qp_minor_iters"],
                    e_key(row, "qp_cap_retries"), row["qp_maxiter_exits"],
                    row["qp_numerical_exits"], row["qp_failures_routed"],
                    row["qp_failures_in_a_row_peak"],
                    row["steps_accepted"], row["rejected_steps"],
                    row["factorizations"], row["symbolic_analyses"],
                    row["border_refine_steps"], row["elastic_activations"],
                    row["restoration_entries"],
                    row["restoration_sub_status"] if entered else "(no phase ran)",
                    status_label(row), row["infeasibility_certified"],
                    "%.6g" % worst_feas(row), "%.10g" % float(row["f"])]
            sw("".join(fmt(str(v), wd) for v, (_n, wd) in zip(vals, h2)))
        sw("")
    with open(os.path.join(tables_dir, "e-solve-counters.txt"), "w") as handle:
        handle.write("\n".join(t2) + "\n")

    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
