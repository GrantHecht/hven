#!/usr/bin/env python3
"""M7 W2 M3a -- THE MACHINE FOLD (RULE.md section 7 item 4).

Reads rows/stage1/<CELL>@<tag>.txt and writes tables/ and verdict.txt, and --
when rows/e/ exists -- applies RULE-E.md section 5 to the addendum-E legs and
writes verdict-E.txt, tables/e-first-qp.txt and tables/e-solve-counters.txt.
ADDENDUM F appends a THIRD, independent fold: it writes verdict-F.txt and
tables/f-*.txt. Re-running must leave verdict.txt, verdict-E.txt and the
stage-1 and E tables byte-identical (RULE-F.md section 6).
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


def main() -> int:
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

    # RULE-F.md section 6 item 4. Writes only NEW files, on the same terms.
    #
    # AND ITS RETURN CODE IS THIS SCRIPT'S (M7 W2 M3a fix1, R2). The F fold
    # re-runs RULE-F.md section 3's identity and neutrality checks on the
    # committed rows; until fix1 it PRINTED a STOP on a failure and then went on
    # to compute an H-DEG verdict anyway, and this script exited 0 whatever it
    # found. A check whose failure changes nothing is not a check. The fold now
    # stops where the STOP is -- no verdict table is computed, no verdict is
    # written -- and hands the failure out through the process's exit status.
    return fold_f(art, tables_dir)


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



# ===========================================================================
# ADDENDUM F -- THE DEGENERATE-STALL PROBE.
#
# RULE-F.md was committed at 229d63de, BEFORE any F leg ran (the identity leg
# included). Everything below applies it AS WRITTEN. Nothing in this block
# touches the stage-1 or E folds: verdict.txt, verdict-E.txt and the
# stage1-*/e-* tables are not re-opened for writing, and re-running this script
# must leave all five byte-identical.
#
# It writes verdict-F.txt, tables/f-first-qp.txt, tables/f-records.txt and
# tables/f-neutrality.txt. No number in any of them is transcribed by hand.
# ===========================================================================

# RULE-F.md section 2's table: leg tag -> (cell, the committed stage-1 twin).
F_LEGS = [
    ("STEENBRB@identity-F", "STEENBRB", "STEENBRB@trinf.txt", "identity"),
    ("STEENBRB@F-trinf", "STEENBRB", "STEENBRB@trinf.txt", "measured"),
    ("STEENBRB@F-tr1", "STEENBRB", "STEENBRB@tr1.txt", "measured"),
    ("YORKNET@F-trinf", "YORKNET", "YORKNET@trinf.txt", "measured"),
    ("YORKNET@F-tr1", "YORKNET", "YORKNET@tr1.txt", "measured"),
]
# RULE-F.md section 4: the verdict is taken on record 0 of these legs alone.
F_VERDICT_LEGS = [("STEENBRB", "STEENBRB@F-trinf"), ("YORKNET", "YORKNET@F-trinf")]
F_STALL_RATIO = 0.5
F_STALL_RUN = 1000
F_WALK_RATIO = 0.1
# RULE-F.md section 3.1's exclusion list, BY NAME, plus RULE.md section 4's
# time-valued list carried over. Nothing else is excluded from a comparison.
F_NEW_KEYS = {"ledger"}
F_TIME_KEYS = {"wall_seconds", "elapsed", "elapsed_seconds"}
F_RESIDUAL_KEYS = {
    "f", "stationarity", "feasibility_e", "feasibility_i", "complementarity",
    "restoration_h_in", "restoration_h_out", "restoration_f_in",
    "restoration_f_out", "requesting_h", "requesting_f",
}
F_NEAR_ULP_REL = 1.0e-12
# QpCounters' twenty fields in DECLARATION ORDER, the probe's own table.
F_FIELDS = ["factorizations", "schur_updates", "minor_iters", "eqp_refine_steps",
            "border_refine_steps", "verdict_refine_steps", "suspect_escalations",
            "k0_reused", "symbolic_analyses", "ws_adds", "ws_drops", "shift_adds",
            "degenerate_steps", "degenerate_run_max", "ws_adds_bound",
            "ws_drops_bound", "distinct_ineq_added", "distinct_bound_added",
            "drop_ties", "ratio_ties"]


def read_f_row(path: str) -> dict:
    """read_row() for an F row, plus the LEDGER block.

    The key block STOPS at LADDER_BEGIN, so the LEDGER block's own `=` lines
    are never folded into the key set -- RULE-F.md section 3.1 says that block
    is not part of the key block. The ledger is returned under '_first',
    '_agg' and '_records'.
    """
    keys: dict = {}
    ladder: list = []
    first: dict = {}
    agg: dict = {}
    records: list = []
    state = "keys"
    header: list = []
    with open(path, "r") as handle:
        for line in handle:
            line = line.rstrip("\n")
            if line == "LADDER_BEGIN":
                state = "ladder"
                continue
            if line == "LADDER_END":
                state = "after"
                continue
            if line == "LEDGER_FIRST_BEGIN":
                state = "first"
                continue
            if line == "LEDGER_AGG_BEGIN":
                state = "agg"
                continue
            if line == "LEDGER_RECORDS_BEGIN":
                state = "records"
                header = []
                continue
            if line in ("LEDGER_FIRST_END", "LEDGER_AGG_END", "LEDGER_RECORDS_END"):
                state = "after"
                continue
            if state == "ladder":
                ladder.append(line)
                continue
            if state == "records":
                if not header:
                    header = line.split(",")
                else:
                    records.append(dict(zip(header, line.split(","))))
                continue
            if "=" not in line or line.startswith("PROBE_"):
                continue
            key, value = line.split("=", 1)
            if state == "keys":
                keys[key] = value
            elif state == "first":
                first[key[len("first_"):] if key.startswith("first_") else key] = value
            elif state == "agg":
                agg[key[len("agg_"):] if key.startswith("agg_") else key] = value
    keys["_ladder"] = ladder
    keys["_first"] = first
    keys["_agg"] = agg
    keys["_records"] = records
    return keys


def f_near_ulp(a: str, b: str):
    """RULE.md section 4's declared fallback, carried over unchanged."""
    try:
        x, y = float(a), float(b)
    except (TypeError, ValueError):
        return False, None
    if x != x or y != y:
        return False, None
    if x in (float("inf"), float("-inf")) or y in (float("inf"), float("-inf")):
        return False, None
    gap = abs(x - y)
    return gap <= F_NEAR_ULP_REL * max(1.0, abs(x), abs(y)), gap


def f_compare(new: dict, old: dict):
    """One leg against its committed twin. Returns (n_shared, diffs, tolerated,
    ladder_diffs, only_new). The compared set is every shared key but the ones
    RULE-F.md section 3.1 excludes BY NAME, plus the whole ladder block."""
    nk = {k: v for k, v in new.items() if not k.startswith("_")}
    ok_ = {k: v for k, v in old.items() if not k.startswith("_")}
    shared = (set(nk) & set(ok_)) - F_NEW_KEYS - F_TIME_KEYS
    diffs, tolerated = [], 0
    for key in sorted(shared):
        a, b = nk[key], ok_[key]
        if a == b:
            continue
        if key in F_RESIDUAL_KEYS:
            accepted, gap = f_near_ulp(a, b)
            if accepted:
                tolerated += 1
                continue
        diffs.append((key, a, b))
    la, lb = new.get("_ladder", []), old.get("_ladder", [])
    ladder_diffs = []
    if len(la) != len(lb):
        ladder_diffs.append(("<ladder line count>", str(len(la)), str(len(lb))))
    else:
        for i, (x, y) in enumerate(zip(la, lb)):
            if x != y:
                ladder_diffs.append(("ladder line %d" % i, x, y))
    return len(shared), diffs, tolerated, ladder_diffs, sorted(set(nk) - set(ok_))


def f_i(row: dict, name: str) -> int:
    return int(row[name])


def f_ratio(num: int, den: int) -> str:
    return "n/a (minor_iters = 0)" if den == 0 else "%.6f" % (float(num) / float(den))


def f_verdict(first: dict):
    """RULE-F.md section 4, applied as written and in its stated order."""
    if not first or "minor_iters" not in first:
        return "UNDETERMINED (no ledger record)", None, None
    minors = f_i(first, "minor_iters")
    degen = f_i(first, "degenerate_steps")
    run = f_i(first, "degenerate_run_max")
    if minors == 0:
        return "UNDETERMINED (the first ledger record has no minor iterations)", None, run
    ratio = float(degen) / float(minors)
    if ratio >= F_STALL_RATIO and run >= F_STALL_RUN:
        return "DEGENERATE_STALL", ratio, run
    if ratio < F_WALK_RATIO:
        return "GENUINE_WALK", ratio, run
    return "MIXED", ratio, run


def fold_f(art: str, tables_dir: str) -> int:
    rows_dir = os.path.join(art, "rows", "f")
    if not os.path.isdir(rows_dir):
        return 0

    legs: dict = {}
    missing: list = []
    for tag, _cell, _twin, _kind in F_LEGS:
        path = os.path.join(rows_dir, "%s.txt" % tag)
        if not os.path.exists(path):
            missing.append(tag)
            continue
        legs[tag] = read_f_row(path)
    twins: dict = {}
    for tag, _cell, twin, _kind in F_LEGS:
        path = os.path.join(art, "rows", "stage1", twin)
        if os.path.exists(path):
            twins[tag] = read_f_row(path)

    out: list = []
    w = out.append
    w("M7 W2 M3a ADDENDUM F -- THE VERDICT: RULE-F.md section 4, applied to the")
    w("four measured F legs.")
    w("")
    w("Machine-written by wrappers/summarise_m3a.py from rows/f/ and from the four")
    w("COMMITTED stage-1 twins in rows/stage1/. No number below is transcribed by")
    w("hand. RULE-F.md was committed at 229d63de, BEFORE any F leg ran, and is")
    w("applied here AS WRITTEN -- no predicate is relaxed, widened, re-labelled or")
    w("re-read in the light of what the legs said.")
    w("")
    w("THIS RE-TESTS NEITHER EARLIER HYPOTHESIS (RULE-F.md section 0). The radius")
    w("hypothesis stays CLOSED by RULE.md section 5.5 and H-QP stays closed by")
    w("RULE-E.md section 5; verdict.txt and verdict-E.txt stand exactly as")
    w("committed. The hypothesis under test here is:")
    w("")
    w("  H-DEG  The first QP's tens of thousands of minor iterations on STEENBRB")
    w("         and YORKNET at tr_init = inf are a DEGENERATE STALL of the")
    w("         active-set walk, not a long genuine walk.")
    w("")
    w("THE PREDICATES, as the rule states them, on the FIRST QP-LEDGER RECORD of")
    w("each cell's tr_init = inf leg and on nothing else:")
    w("  DEGENERATE_STALL iff degenerate_steps / minor_iters >= %.1f" % F_STALL_RATIO)
    w("                   AND degenerate_run_max >= %d" % F_STALL_RUN)
    w("  GENUINE_WALK     iff degenerate_steps / minor_iters <  %.1f" % F_WALK_RATIO)
    w("  MIXED            anything else -- reported WITH the numbers")
    w("  UNDETERMINED     minor_iters == 0, or no record at all")
    w("")
    if missing:
        w("CENSORED LEGS (no row, never filled): %s" % ", ".join(missing))
    else:
        w("CENSORED LEGS: none. All %d legs produced a row." % len(F_LEGS))
    trunc = [t for t in legs if legs[t].get("deadline_fired") == "1"]
    w("TRUNCATED LEGS (deadline_fired=1): %s" % (", ".join(trunc) if trunc else "none"))
    w("")

    # --- the two checks, RE-RUN here on the COMMITTED files -----------------
    w("THE TWO CHECKS ON THE REBUILT PROBE (RULE-F.md section 3), re-run by this")
    w("fold on the COMMITTED rows -- an independent re-check of the same files the")
    w("chain's own reports (identity-f.txt, neutral-first-f.txt, neutrality-f.txt)")
    w("cover. Compared: every shared key but `ledger` and the time-valued names,")
    w("and the whole ladder block, as STRINGS.")
    ntab: list = []
    nw = ntab.append
    nw("M7 W2 M3a ADDENDUM F -- tables/f-neutrality.txt")
    nw("")
    nw("RULE-F.md section 3: the IDENTITY leg proves --ledger is inert at its")
    nw("default; each NEUTRALITY leg proves the ATTACHED ledger moved no solver")
    nw("counter, status, f or feasibility on that leg's own committed twin.")
    nw("")
    nhdr = [("leg", 22), ("kind", 10), ("twin", 22), ("keys", 6), ("differing", 11),
            ("ladder", 8), ("near-ulp", 10), ("verdict", 18)]
    nw("".join(fmt(n, wd) for n, wd in nhdr))
    nw("-" * sum(wd for _n, wd in nhdr))
    check_worst = 0
    total_tol = 0
    for tag, _cell, twin, kind in F_LEGS:
        if tag not in legs or tag not in twins:
            nw("".join(fmt(v, wd) for v, (_n, wd) in
                       zip([tag, kind, twin, "-", "-", "-", "-", "MISSING"], nhdr)))
            check_worst = 1
            continue
        if legs[tag].get("deadline_fired") == "1":
            nw("".join(fmt(v, wd) for v, (_n, wd) in
                       zip([tag, kind, twin, "-", "-", "-", "-", "WITHHELD (TRUNC)"], nhdr)))
            continue
        nshared, diffs, tol, ldiffs, only_new = f_compare(legs[tag], twins[tag])
        total_tol += tol
        bad = bool(diffs or ldiffs)
        check_worst = max(check_worst, 1 if bad else 0)
        nw("".join(fmt(str(v), wd) for v, (_n, wd) in
                   zip([tag, kind, twin, nshared, len(diffs), len(ldiffs), tol,
                        "DIFFERS -- STOP" if bad else "IDENTICAL"], nhdr)))
        for key, a, b in diffs + ldiffs:
            nw("    DIFF %s: f=%s  stage1=%s" % (key, a, b))
        if only_new:
            nw("    keys only in the F probe (not in the shared set): %s"
               % ", ".join(only_new))
        w("  %-22s vs %-22s %3d keys, %d differing; ladder %d differing   %s"
          % (tag, twin, nshared, len(diffs), len(ldiffs),
             "DIFFERS -- STOP" if bad else "IDENTICAL"))
    nw("")
    nw("NEAR_ULP_TOLERATED=%d" % total_tol)
    nw("F_CHECK_WORST=%d" % check_worst)
    with open(os.path.join(tables_dir, "f-neutrality.txt"), "w") as handle:
        handle.write("\n".join(ntab) + "\n")
    w("  NEAR_ULP_TOLERATED=%d   F_CHECK_WORST=%d" % (total_tol, check_worst))
    if check_worst == 0:
        w("  NO SOLVER COUNTER MOVED WITH THE LEDGER ATTACHED. RULE-F.md section")
        w("  3.2's STOP does not fire, and the verdicts below may be taken.")
    else:
        w("  A SOLVER COUNTER MOVED WITH THE LEDGER ATTACHED -- RULE-F.md section")
        w("  3.2's STOP. That is a FINDING ABOUT hven; no H-DEG verdict is taken.")
    w("")

    # THE STOP IS A STOP (M7 W2 M3a fix1, R2). Until fix1 the two branches above
    # differed only in what they PRINTED: the fold went on to compute the verdict
    # table either way, wrote verdict-F.txt either way, and exited 0 either way.
    # A reader could therefore be handed an H-DEG verdict taken from legs whose
    # identity or neutrality check had failed -- the exact reading RULE-F.md
    # section 3 forbids. Now the fold RETURNS HERE: no verdict is computed, the
    # transcript ends at the STOP, and the non-zero return reaches the process's
    # exit status. f-neutrality.txt is written above and stands as the evidence
    # of WHICH pair failed and on which keys.
    if check_worst != 0:
        w("=" * 78)
        w("THE FOLD STOPS HERE -- NO H-DEG VERDICT IS COMPUTED")
        w("=" * 78)
        w("")
        w("RULE-F.md section 3 makes the identity and neutrality checks a")
        w("PRECONDITION of the verdict, not a report beside it. One of them")
        w("failed above, so this fold computes no verdict table, writes no")
        w("verdict, and exits non-zero. The failing pair and its differing keys")
        w("are in the table above and in tables/f-neutrality.txt.")
        w("")
        w("Nothing here is a judgement about H-DEG. It is the refusal to make")
        w("one on evidence whose own precondition did not hold.")
        with open(os.path.join(art, "verdict-F.txt"), "w") as handle:
            handle.write("\n".join(out) + "\n")
        sys.stdout.write("\n".join(out) + "\n")
        return 3

    # --- the verdict table --------------------------------------------------
    w("THE VERDICT TABLE (RULE-F.md section 4), on record 0 of each cell's")
    w("tr_init = inf leg. `ratio` is degenerate_steps / minor_iters.")
    w("-" * 78)
    vhdr = [("cell", 10), ("leg", 18), ("record", 9), ("QpStatus", 10),
            ("minor_iters", 12), ("degen_steps", 12), ("ratio", 10),
            ("run_max", 9), ("VERDICT", 16)]
    w("".join(fmt(n, wd) for n, wd in vhdr))
    verdicts: dict = {}
    for cell, tag in F_VERDICT_LEGS:
        if tag not in legs:
            verdicts[cell] = "UNDETERMINED (CENSORED -- no row)"
            w("".join(fmt(str(v), wd) for v, (_n, wd) in
                      zip([cell, tag, "-", "-", "-", "-", "-", "-",
                           "UNDETERMINED"], vhdr)))
            continue
        if legs[tag].get("deadline_fired") == "1":
            verdicts[cell] = "NO VERDICT (TRUNCATED)"
            w("".join(fmt(str(v), wd) for v, (_n, wd) in
                      zip([cell, tag, "0", "-", "-", "-", "-", "-",
                           "NO VERDICT (TRUNC)"], vhdr)))
            continue
        first = legs[tag]["_first"]
        verdict, ratio, run = f_verdict(first)
        verdicts[cell] = verdict
        w("".join(fmt(str(v), wd) for v, (_n, wd) in
                  zip([cell, tag, first.get("label", "-"),
                       first.get("qp_record_status", "-"),
                       first.get("minor_iters", "-"),
                       first.get("degenerate_steps", "-"),
                       "%.6f" % ratio if ratio is not None else "-",
                       str(run) if run is not None else "-", verdict], vhdr)))
    w("")
    w("WHAT THE RULE RETURNS")
    w("-" * 78)
    for cell, _tag in F_VERDICT_LEGS:
        w("  %-10s %s" % (cell, verdicts.get(cell, "UNDETERMINED (no leg)")))
    w("")
    labels = sorted({verdicts.get(c, "") for c, _t in F_VERDICT_LEGS})
    if labels == ["MIXED"]:
        w("VERDICT: MIXED ON BOTH CELLS.")
        w("")
        w("H-DEG is NEITHER SUPPORTED NOR REFUTED as the rule states it. On both")
        w("cells the degeneracy ratio sits just BELOW the 0.5 bar while")
        w("degenerate_run_max sits far ABOVE the 1000 bar, so the conjunction")
        w("DEGENERATE_STALL asks for is false and the GENUINE_WALK threshold of")
        w("0.1 is nowhere near met. RULE-F.md section 4's third row governs:")
        w("'MIXED -- reported WITH the numbers'. The numbers are above and in")
        w("tables/f-first-qp.txt; nothing is relabelled to reach a cleaner word.")
    else:
        w("VERDICT: %s" % " / ".join("%s = %s" % (c, verdicts.get(c, "-"))
                                     for c, _t in F_VERDICT_LEGS))
    w("")
    w("NO DEFAULT IS CHANGED BY THIS ADDENDUM UNDER ANY OUTCOME (RULE-F.md")
    w("section 0), and no verdict licenses one. No file under src/ or include/ is")
    w("touched by it at all.")
    w("")

    # --- the first-QP table -------------------------------------------------
    t1: list = []
    tw = t1.append
    tw("M7 W2 M3a ADDENDUM F -- tables/f-first-qp.txt")
    tw("")
    tw("Record 0 of every measured leg: all TWENTY QpCounters fields in")
    tw("declaration order, the record's own label/warm/QpStatus, and RULE-F.md")
    tw("section 1.4's three DERIVED quantities. Nothing here is selected; the")
    tw("probe prints the struct and this table prints what it printed.")
    tw("")
    tw("ws_net_change = ws_adds - ws_drops + shift_adds IS `W - S`, the working-")
    tw("set size at exit minus at entry, BY THE WALK COUNTERS' DOCUMENTED NET")
    tw("IDENTITY -- SolveRecord carries no size field. It carries that identity's")
    tw("one documented exception: the temporary-vertex start repair writes")
    tw("ws.bound_state() directly without touching QpCounters, so on a solve that")
    tw("ran it the identity is off by the pins it left standing, and no counter")
    tw("says whether it ran.")
    tw("")
    fhdr = [("field", 24)] + [(t, 20) for t, _c, _w, k in F_LEGS if k == "measured"]
    tw("".join(fmt(n, wd) for n, wd in fhdr))
    tw("-" * sum(wd for _n, wd in fhdr))
    measured = [t for t, _c, _w, k in F_LEGS if k == "measured" and t in legs]
    for name in ["label", "warm", "qp_record_status"] + F_FIELDS:
        vals = [name] + [legs[t]["_first"].get(name, "-") for t in measured]
        tw("".join(fmt(str(v), wd) for v, (_n, wd) in zip(vals, fhdr)))
    tw("-" * sum(wd for _n, wd in fhdr))
    for name in ["degenerate_fraction", "ws_adds_minus_drops", "ws_net_change"]:
        vals = ["(derived) " + name] + [legs[t]["_first"].get(name, "-") for t in measured]
        tw("".join(fmt(str(v), wd) for v, (_n, wd) in zip(vals, fhdr)))
    # Two further derived readings RULE-F.md section 4.1 asks for, computed here
    # rather than in the probe because they divide one printed field by another.
    for label, num, den in [("degen/ws_adds", "degenerate_steps", "ws_adds"),
                            ("run_max/degen_steps", "degenerate_run_max", "degenerate_steps"),
                            ("ws_adds/distinct_bound", "ws_adds", "distinct_bound_added"),
                            ("adds+drops vs minors", None, None)]:
        vals = ["(derived) " + label]
        for t in measured:
            first = legs[t]["_first"]
            if num is None:
                vals.append("%s vs %s" % (f_i(first, "ws_adds") + f_i(first, "ws_drops"),
                                          f_i(first, "minor_iters")))
            else:
                d = f_i(first, den)
                vals.append("n/a (0)" if d == 0 else "%.4f" % (float(f_i(first, num)) / d))
        tw("".join(fmt(str(v), wd) for v, (_n, wd) in zip(vals, fhdr)))
    with open(os.path.join(tables_dir, "f-first-qp.txt"), "w") as handle:
        handle.write("\n".join(t1) + "\n")

    # --- the all-records table ---------------------------------------------
    t2: list = []
    rw = t2.append
    rw("M7 W2 M3a ADDENDUM F -- tables/f-records.txt")
    rw("")
    rw("EVERY QP-ledger record of every measured leg, not record 0 alone, so a")
    rw("reader can see whether a later major repeats the same stall from the same")
    rw("start. The ledger's SCOPE (RULE-F.md section 1.2): attach_ledger attaches")
    rw("to the driver's MAIN WALK ENGINE only -- the restoration sub-driver and")
    rw("the four lazy engines write nothing -- so a record count is a LOWER BOUND")
    rw("on the solve's QP solves, and that matters on the tr_init = 1 legs where a")
    rw("restoration phase runs.")
    rw("")
    cols = ["index", "label", "warm", "status"] + F_FIELDS + \
           ["ws_adds_minus_drops", "ws_net_change", "degenerate_fraction"]
    for tag in measured:
        rows = legs[tag]["_records"]
        rw("=== %s   (%s records; ledger_qp_records=%s, print-truncated=%s)"
           % (tag, len(rows), legs[tag].get("ledger_qp_records", "?"),
              legs[tag].get("ledger_records_print_truncated", "?")))
        rw(",".join(cols))
        for rec in rows:
            rw(",".join(rec.get(c, "") for c in cols))
        rw("")
    with open(os.path.join(tables_dir, "f-records.txt"), "w") as handle:
        handle.write("\n".join(t2) + "\n")

    # --- OUTSIDE THE VERDICT ------------------------------------------------
    w("=" * 78)
    w("OUTSIDE THE VERDICT -- AND NOT ONE")
    w("=" * 78)
    w("")
    w("Everything above this line is RULE-F.md applied as written. Everything")
    w("below it is DESCRIPTIVE. It licenses NO verdict, NO default change and NO")
    w("re-labelling of the two MIXED cells; RULE-F.md section 4.1 pre-registered")
    w("these read-outs as non-verdict read-outs precisely so they could be")
    w("reported without becoming one.")
    w("")
    w("1. THE SETTLER'S OWN DECLARED ERROR IN THE RULE'S DESIGN.")
    w("")
    w("   The 0.5 bar was set on a denominator that cannot reach it. Checked")
    w("   against the headers rather than assumed:")
    w("")
    w("   * `minor_iters` increments ONCE PER PASS of the walk loop --")
    w("     `++counters.minor_iters` is the first statement inside")
    w("     `for (Index iter = 0; ...; ++iter)` in src/qp/qp_engine.cpp:613, and")
    w("     detail/qp/qp_engine.h's COUNTER SEMANTICS note fixes it as a relied-on")
    w("     contract. A DROP iteration is a pass and counts.")
    w("   * `degenerate_steps` counts 'minor iterations that ADDED A BLOCKING")
    w("     CONSTRAINT while moving the iterate by no more than the loop's own")
    w("     step tolerance' (solver_counters.h). A pass that only DROPS adds")
    w("     nothing and is therefore never a degenerate step, however stuck the")
    w("     iterate is.")
    w("   * `degenerate_run_max` is 'the longest consecutive run of degenerate")
    w("     steps... A run is broken by a minor on which the iterate MOVED, and by")
    w("     nothing else: ... a drop iteration and a zero-multiplier probe do not'")
    w("     break it (solver_counters.h). So the RUN counter spans the drops the")
    w("     RATIO's denominator charges for.")
    w("")
    w("   CONSEQUENCE: in a STEADY add/drop alternation at a stuck iterate, half")
    w("   the passes are drops, so degenerate_steps / minor_iters sits NEAR 0.5 --")
    w("   the bar was set at the value the quantity it measures converges to, and")
    w("   a bar at a near-asymptote decides nothing. That is a defect in the rule,")
    w("   declared here by the settler; it is NOT a reason to move the bar after")
    w("   the fact, and the MIXED verdicts above stand.")
    w("")
    w("   AND 0.5 IS NOT A CEILING. An earlier draft of this item said the ratio")
    w("   'cannot exceed about 0.5 BY CONSTRUCTION'; that was WRONG and is")
    w("   withdrawn here (M7 W2 M3a fix1, R4). Nothing forces adds and drops to")
    w("   alternate one for one: a run with NET ADDITIONS spends more than half")
    w("   its passes adding, and every such pass at a stuck iterate is a")
    w("   degenerate step. The rows themselves show the adds share already ABOVE")
    w("   0.5 on EVERY measured leg, the two verdict legs included --")
    for tag in measured:
        first = legs[tag]["_first"]
        a, m = f_i(first, "ws_adds"), f_i(first, "minor_iters")
        w("     %-20s ws_adds / minor_iters = %6d / %6d = %s"
          % (tag, a, m, ("%.5f" % (float(a) / m)) if m else "n/a (0)"))
    w("   -- so a walk that added without dropping would carry the ratio past the")
    w("   bar. What the counters DO make unreachable is nothing; what they make")
    w("   UNLIKELY, on a churning walk, is a ratio far above 0.5. The correct")
    w("   reading of the two MIXED cells is therefore that the bar was badly")
    w("   chosen, not that the cells fell short of a ceiling.")
    w("")
    w("   THE ARITHMETIC, from the rows: on record 0, ws_adds + ws_drops against")
    w("   minor_iters --")
    for tag in measured:
        first = legs[tag]["_first"]
        a, d, m = f_i(first, "ws_adds"), f_i(first, "ws_drops"), f_i(first, "minor_iters")
        w("     %-20s %6d + %6d = %6d   vs minor_iters %6d   (%d passes were neither)"
          % (tag, a, d, a + d, m, m - a - d))
    w("")
    w("2. THE DESCRIPTIVE READ-OUTS (RULE-F.md section 4.1), record 0 of each leg.")
    w("")
    dhdr = [("leg", 20), ("ws_adds", 9), ("ws_drops", 10), ("net", 6),
            ("degen/adds", 12), ("run_max", 9), ("drop_ties", 10),
            ("ratio_ties", 11), ("distinct_ineq", 14), ("distinct_bound", 15),
            ("adds/distinct", 14)]
    w("".join(fmt(n, wd) for n, wd in dhdr))
    w("-" * sum(wd for _n, wd in dhdr))
    for tag in measured:
        first = legs[tag]["_first"]
        a, d = f_i(first, "ws_adds"), f_i(first, "ws_drops")
        db = f_i(first, "distinct_bound_added")
        w("".join(fmt(str(v), wd) for v, (_n, wd) in zip(
            [tag, a, d, a - d,
             "%.4f" % (float(f_i(first, "degenerate_steps")) / a) if a else "n/a (0)",
             first["degenerate_run_max"], first["drop_ties"], first["ratio_ties"],
             first["distinct_ineq_added"], db,
             "%.1f" % (float(a) / db) if db else "n/a (0)"], dhdr)))
    w("")
    bound_only = all(rec["ws_adds"] == rec["ws_adds_bound"]
                     for t in measured for rec in legs[t]["_records"])
    nrec = sum(len(legs[t]["_records"]) for t in measured)
    w("")
    w("   CHECKED, not asserted: ws_adds == ws_adds_bound on %s of the %d records"
      % ("ALL" if bound_only else "NOT all", nrec))
    w("   above, and every leg prints mi = %s."
      % ", ".join(sorted({legs[t]["mi"] for t in measured})))
    w("")
    w("   `distinct_ineq_added` is 0 on every leg because BOTH cells have mi = 0")
    w("   (the probe prints mi; STEENBRB and YORKNET are equality-constrained with")
    w("   bounds), so every admission is a VARIABLE-BOUND pin -- ws_adds ==")
    w("   ws_adds_bound on every record above, which is that fact checked.")
    w("   `adds/distinct` is then ADMISSION MULTIPLICITY on the bound side, the")
    w("   quantity solver_counters.h defines as ws_adds_bound / distinct_bound_added:")
    w("   the mean number of times a touched variable was admitted. RE-admissions")
    w("   are that number minus one.")
    w("")
    w("   A few distinct constraints re-admitted many times is what cycling looks")
    w("   like. It is NOT proof of cycling: admission multiplicity says a variable")
    w("   came back, not that the WHOLE WORKING SET came back. See item 4.")
    w("")
    w("2b. DO THE LATER MAJORS REPEAT THE SAME STALL? Every record of the two")
    w("    tr_init = inf legs, not record 0 alone (full table: tables/f-records.txt).")
    w("")
    rhdr = [("leg", 20), ("record", 9), ("warm", 6), ("status", 10), ("minors", 8),
            ("degen", 8), ("run_max", 9), ("adds", 7), ("drops", 7), ("net", 6),
            ("distinct_bound", 15), ("adds/distinct", 14)]
    w("".join(fmt(n, wd) for n, wd in rhdr))
    w("-" * sum(wd for _n, wd in rhdr))
    for _cell, tag in F_VERDICT_LEGS:
        if tag not in legs:
            continue
        for rec in legs[tag]["_records"]:
            db = int(rec["distinct_bound_added"])
            w("".join(fmt(str(v), wd) for v, (_n, wd) in zip(
                [tag, rec["label"], rec["warm"], rec["status"], rec["minor_iters"],
                 rec["degenerate_steps"], rec["degenerate_run_max"], rec["ws_adds"],
                 rec["ws_drops"], rec["ws_net_change"], db,
                 "%.1f" % (float(rec["ws_adds"]) / db) if db else "n/a (0)"], rhdr)))
        # ALL TWENTY FIELDS, symbolic_analyses INCLUDED (M7 W2 M3a fix1, R4).
        # The first draft excluded it by name, on the assumption that a re-entered
        # walk would re-analyse and that the exclusion was therefore needed to
        # make the comparison meaningful. The review checked the rows instead:
        # YORKNET's records 1..3 agree on that field too, so the exclusion
        # UNDERSTATED the agreement it was there to report. The comparison is now
        # over the whole struct, and the count below is 20 rather than 19.
        tail = legs[tag]["_records"][1:]
        cols = list(F_FIELDS)
        same = bool(tail) and all(
            all(r[c] == tail[0][c] for c in cols) for r in tail)
        w("    records 1..%d on this leg are %sBYTE-IDENTICAL to one another on ALL"
          % (len(legs[tag]["_records"]) - 1, "" if same else "NOT "))
        w("    %d QpCounters fields, symbolic_analyses INCLUDED." % len(cols))
        if not same:
            differing = sorted({c for r in tail for c in cols if r[c] != tail[0][c]})
            w("    The fields they differ on: %s." % ", ".join(differing))
        w("")
    w("    Every record on both legs ends max_iter at exactly the same minor count,")
    w("    which is the per-major cap: the walk is re-entered each major and stalls")
    w("    again. The ledger records the MAIN WALK ENGINE only, so a tr_init = 1")
    w("    leg's restoration subproblems are absent by construction (RULE-F.md")
    w("    section 1.2) -- that is a scope fact, not a gap in this table.")
    w("")
    w("3. THE tr_init = 1 CONTRAST, same cells, same binary, same terms.")
    w("")
    for cell, inf_tag in F_VERDICT_LEGS:
        one_tag = "%s@F-tr1" % cell
        if inf_tag not in legs or one_tag not in legs:
            continue
        fi, fo = legs[inf_tag]["_first"], legs[one_tag]["_first"]
        w("   %-9s tr_init = 1  : first QP %-16s %5s minors, %5s degenerate, run max %-5s"
          % (cell, fo["qp_record_status"], fo["minor_iters"],
             fo["degenerate_steps"], fo["degenerate_run_max"]))
        w("   %-9s tr_init = inf: first QP %-16s %5s minors, %5s degenerate, run max %-5s"
          % ("", fi["qp_record_status"], fi["minor_iters"],
             fi["degenerate_steps"], fi["degenerate_run_max"]))
    w("")
    w("   The same cell, the same start point, one option apart: with the trust")
    w("   window the first QP takes a short walk and ends with a verdict; without")
    w("   it the walk does not move and ends on the cap.")
    w("")
    w("   WHAT THIS CONTRAST IS NOT (M7 W2 M3a fix1, R4). It is NOT evidence about")
    w("   the START of the walk. `tr_init` enters the subproblem as the l-infinity")
    w("   window `min(st.delta, qp.tr_radius)` -- it changes the QP's own BOUNDS,")
    w("   and therefore its feasible set, its blocking constraints and its whole")
    w("   trajectory, not merely the working set the walk begins from. The two")
    w("   rows above are two DIFFERENT subproblems, so the difference between them")
    w("   cannot be attributed to start/crash quality rather than to the geometry")
    w("   the window imposes. An earlier draft read it as 'a fact about where the")
    w("   walk begins'; that reading is withdrawn.")
    w("")
    w("4. WHAT WOULD CONFIRM CYCLING PROPER, AND WHAT IS NOT REACHABLE TODAY.")
    w("")
    w("   THE WHOLE OF THIS ITEM IS SUBORDINATE TO THE MIXED VERDICT ABOVE. The")
    w("   most the rows support as a DESCRIPTION is: LONG DEGENERATE ACTIVE-SET")
    w("   CHURN -- thousands of near-zero-length admissions interleaved with drops")
    w("   while the iterate does not move. CYCLING PROPER IS NOT SHOWN, and no")
    w("   word below upgrades the verdict.")
    w("")
    w("   Cycling proper is THE SAME WORKING SET REVISITED. Nothing above")
    w("   establishes that: `distinct_bound_added` counts distinct VARIABLES ever")
    w("   admitted, so a multiplicity of k says each touched variable came back")
    w("   about k times, which is equally consistent with a long degenerate CRAWL")
    w("   over a combinatorially large family of sets drawn from the same few")
    w("   variables.")
    w("")
    w("   SIX MECHANISMS THESE ROWS CANNOT EXCLUDE (M7 W2 M3a fix1, R4; the")
    w("   review's own list, kept because each one would produce these same")
    w("   counters and each calls for a different remedy):")
    w("")
    w("     (1) ZERO-MULTIPLIER PROBES. `ws_drops` merges a probe-driven drop with")
    w("         a Dantzig drop, so a walk spending its passes probing looks")
    w("         exactly like one dropping on a price test.")
    w("     (2) IMMEDIATE DANTZIG DROP AND RE-ADD of the bound just admitted --")
    w("         a two-cycle, which no counter here can tell from a long orbit.")
    w("     (3) LOWER/UPPER TOLERANCE PING-PONG on one fixed or near-fixed")
    w("         variable: the side is not counted, so both sides read as ADDS of")
    w("         the same variable.")
    w("     (4) A LONG CRAWL through DIFFERENT working sets composed from the same")
    w("         few variables -- high admission multiplicity, no repeat.")
    w("     (5) NEGATIVE-CURVATURE RIDE BLOCKERS, which are folded into `ws_adds`")
    w("         and are not degenerate stalling at all.")
    w("     (6) WARM WORKING-SET HAND-OFF between majors replaying the same")
    w("         stalled walk -- item 2b is consistent with this and does not")
    w("         establish it.")
    w("")
    w("   The distinguishing observation is a per-minor identity of the working")
    w("   SET -- a hash or a signature per pass, and a repeat detector over it.")
    w("   NOT REACHABLE FROM ANY PUBLIC SURFACE TODAY, checked rather than assumed:")
    w("   QpEngine has NO trace sink at all (no attach_trace, no emit site in")
    w("   src/qp/qp_engine.cpp), and its ONLY instrument is the per-SOLVE Ledger")
    w("   record this addendum read. The finest public QP granularity is the")
    w("   subproblem, never the minor.")
    w("")
    w("   So confirming it needs a REGISTERED INSTRUMENT, not another leg: an")
    w("   HVEN_TESTING-gated per-minor working-set signature on CLAUDE.md section")
    w("   8's test-seam convention, or an offline replay of one subproblem. Either")
    w("   is its own task with its own pre-registered rule. THIS ADDENDUM RUNS NO")
    w("   MORE LEGS.")
    w("")
    w("5. WHAT THIS RANKS, FOR THE SETTLER -- AND IT RANKS, IT DOES NOT DECIDE.")
    w("")
    w("   RULE-F.md section 0's three candidates, with what the numbers say about")
    w("   each and nothing more:")
    w("")
    w("   * AN ANTI-CYCLING RULE FOR THE ACTIVE-SET WALK -- the LEAD candidate.")
    w("     hven states it has none ('No anti-cycling rule is implemented; a")
    w("     degenerate stall is bounded by max_iter', detail/qp/qp_engine.h), and")
    w("     the first QP at tr_init = inf spends essentially every pass adding or")
    w("     dropping a bound without moving. OPEN LITERATURE ONLY: a documented")
    w("     textbook rule (Bland's) or a perturbation / EXPAND-style scheme. NEVER")
    w("     FROM SNOPT SOURCE -- CLAUDE.md section 6's firewall is absolute and")
    w("     applies to this line as to every other.")
    w("   * START / CRASH QUALITY -- a live alternative, and NOT ruled out. Note")
    w("     what does NOT support it either: item 3's tr_init = 1 contrast is not")
    w("     evidence about the walk's START, because changing tr_init changes the")
    w("     QP's bounds and its whole trajectory and not only its initial working")
    w("     set (item 3's own withdrawal, M7 W2 M3a fix1). This candidate is")
    w("     ranked here on the general argument that a better initial working set")
    w("     could shorten a degenerate walk, not on a measurement in this")
    w("     artifact.")
    w("   * CAP POLICY -- ranked LAST, on two readings and their limits. Addendum")
    w("     E measured that AT 10x THE CAP each completed FIRST QP still returned")
    w("     max_iter; its legs were TRUNCATED and carry no whole-leg E2 verdict")
    w("     (verdict-E.txt), so 'a 10x cap changes nothing' is a statement about")
    w("     the first QP and about nothing else (M7 W2 M3a fix1, R3). Item 1 adds")
    w("     that the ratio and run readings do not look like a budget shortage.")
    w("")
    w("   Which of the three to fund is the settler's call, not this fold's.")
    w("")

    with open(os.path.join(art, "verdict-F.txt"), "w") as handle:
        handle.write("\n".join(out) + "\n")

    sys.stdout.write("\n".join(out) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
