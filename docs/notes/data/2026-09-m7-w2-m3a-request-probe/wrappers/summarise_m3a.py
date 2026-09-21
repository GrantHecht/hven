#!/usr/bin/env python3
"""M7 W2 M3a -- THE MACHINE FOLD (RULE.md section 7 item 4).

Reads rows/stage1/<CELL>@<tag>.txt and writes tables/ and verdict.txt.
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


if __name__ == "__main__":
    main()
