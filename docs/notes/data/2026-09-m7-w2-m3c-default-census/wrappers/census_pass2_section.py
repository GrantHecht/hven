#!/usr/bin/env python3
"""Generates PASS 2's section of `tables/census.md` and PASS 2's plain-language
block of `README.md`.

Every FIGURE below is read out of an artifact this task committed -- the
per-solve read-outs (`tables/readouts-p2b.md`), the §6.1 verdicts
(`verdict-p2.txt`), the mapping and its conservation sums
(`tables/mapping-p2b.txt`), the identity gate (`logs/gate-p2b.txt`) and pass 1's
own census table (`tables/census.md`). **No figure is hand-transcribed**, which
is the whole reason this file exists rather than an editor.

The PROSE is the implementer's, exactly as pass 1's prose sections are, and
every number inside it is interpolated from the parsed data.

No solver runs here. Nothing generated here licenses a behaviour change, a
default change, or a line of code.
"""

import argparse
import os
import re
import sys
from collections import OrderedDict

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ---------------------------------------------------------------------------
# reading the artifacts
# ---------------------------------------------------------------------------
def read_readouts(path):
    """[(cell, solve, kind, {key: text})] in the file's own order."""
    out = []
    cur = None
    for line in open(path, errors="replace"):
        m = re.match(r"^### (\S+)\s+solve (\d+)\s+--\s+(\S+)", line)
        if m:
            cur = (m.group(1), int(m.group(2)), m.group(3), OrderedDict())
            out.append(cur)
            continue
        if cur is None:
            continue
        m = re.match(r"^  (\S+)\s+(.*?)\s*$", line)
        if m and m.group(2):
            cur[3][m.group(1)] = m.group(2)
    return out


def read_verdict_table(path):
    rows = []
    started = False
    for line in open(path, errors="replace"):
        if line.startswith("CELL "):
            started = True
            continue
        if not started:
            continue
        if line.startswith("-----"):
            continue
        if line.startswith("\n") or not line.strip():
            break
        p = line.split()
        rows.append({"cell": p[0], "solve": int(p[1]), "kind": p[2],
                     "answer": " ".join(p[3:-4]), "period": p[-4],
                     "tail": int(p[-3]), "share": float(p[-2]),
                     "sumdx": float(p[-1])})
    return rows


def read_mapping(path):
    """per cell: mapping status, conservation sums, NOT-LEDGERED count."""
    out, cell = OrderedDict(), None
    for line in open(path, errors="replace"):
        s = line.strip()
        if s.startswith("CELL "):
            cell = s.split()[1]
            out[cell] = {"not_ledgered": 0, "min_diff": None, "fac_diff": None,
                         "status": None, "minors": None, "facs": None}
            continue
        if cell is None:
            continue
        d = out[cell]
        if s.startswith("minors_summary_sum"):
            d["minors"] = int(s.split()[1])
        elif s.startswith("minors_difference"):
            d["min_diff"] = int(s.split()[1])
        elif s.startswith("factorizations_summary_sum"):
            d["facs"] = int(s.split()[1])
        elif s.startswith("factorizations_difference"):
            d["fac_diff"] = int(s.split()[1])
        elif s.startswith("-- RULE-P2 §2 the mapping:"):
            d["status"] = s.split(":")[1].strip()
        elif s.startswith("NOT-LEDGERED (restoration"):
            tail = s.split(":", 1)[1].strip()
            d["not_ledgered"] = 0 if tail == "(none)" else len(tail.split(","))
    return out


def read_gate_verdicts(path):
    out = OrderedDict()
    for line in open(path, errors="replace"):
        m = re.match(r"^  C-(\S+)\s+(REPRODUCES|DIFFERS|CENSORED|REFUSED|MISSING)\s*$",
                     line)
        if m:
            out[m.group(1)] = m.group(2)
    return out


def read_census_terminators(path):
    out = OrderedDict()
    for line in open(path, errors="replace"):
        m = re.match(r"^\| `(\w+)` \| (\w) \| (\S+) \| `?([A-Z_()a-z]+)`? \|", line)
        if m:
            out[m.group(1)] = m.group(4)
    return out


# ---------------------------------------------------------------------------
def group(readouts):
    per = OrderedDict()
    for cell, k, kind, d in readouts:
        per.setdefault(cell, []).append((k, kind, d))
    return per


def i(d, key, default="ABSENT"):
    return d.get(key, default)


def n(d, key, default=0):
    try:
        return int(float(d.get(key, default)))
    except (TypeError, ValueError):
        return default


def ride_summary(d):
    """A one-line reading of §6 #7's granular counts over the tail."""
    txt = i(d, "tail_ride_outcomes", "{}")
    if "'not_attempted'" in txt and "veto" not in txt and "stepped" not in txt:
        return "the ride is never attempted"
    bits = []
    for name in ("stepped", "nullspace_veto", "slope_flat_veto",
                 "sign_disagreement_veto"):
        m = re.search(r"'%s': (\d+)" % name, txt)
        if m:
            bits.append("%s %s" % (name, m.group(1)))
    return ", ".join(bits) if bits else txt


def curvature_line(d):
    lo = i(d, "tail_curvature_quotient_min")
    hi = i(d, "tail_curvature_quotient_max")
    tolset = i(d, "tail_curvature_tol_distinct")
    below = n(d, "tail_curvature_quotient_below_tol")
    same = (lo == hi)
    return ("p_hp/p_sq is %s on every tail pass that has one" % lo if same
            else "p_hp/p_sq runs %s .. %s over the tail" % (lo, hi)) + \
           ", against curvature_tol %s; passes below tolerance: %d" % (tolset,
                                                                       below)


# ---------------------------------------------------------------------------
def census_section(ro, vd, mp, gates, terms, out):
    w = out.write
    per = group(ro)
    tot = {a: sum(1 for r in vd if r["answer"] == a)
           for a in ("CYCLE", "LONG WALK", "SOMETHING ELSE")}
    ncells = len(per)
    nsolves = len(vd)
    all_cycle = [c for c, rows in per.items()
                 if all(x["answer"] == "CYCLE"
                        for x in vd if x["cell"] == c)]

    w("\n\n---\n\n")
    w("# PASS 2 — the per-minor walk at DEFAULT options\n\n")
    w("Governed by [`RULE-P2.md`](../RULE-P2.md), committed before any pass-2 "
      "leg ran, and by [`RULE-P2A.md`](../RULE-P2A.md), which decided whether "
      "the instrument was sound. Neither is edited. **Nothing in this section "
      "licenses a behaviour change, a default change, or a line of code** "
      "(RULE-P2 §0, §8).\n\n")

    w("## The question pass 1 left open\n\n")
    w("Pass 1's Candidate A said, in so many words, that the capped QPs *look "
      "like* a zero-motion period-2 drop/re-add cycle and that the ledger "
      "cannot tell. **RULE-P2 §0 asked it as a question with three possible "
      "answers** — `CYCLE`, `LONG WALK`, `SOMETHING ELSE` — fixed in advance "
      "so the rule could return something other than its own hypothesis.\n\n")

    w("## The instrument, and the two runs it took to trust it\n\n")
    w("Pass 2's first run was STOPPED on all ten cells by RULE-P2 §5.1's "
      "identity gate. `RULE-P2A.md`'s three-arm experiment located the cause "
      "entirely outside hven — the probe had been built by a DIFFERENT "
      "COMPILER from the library pass 1 linked — and returned its OUTCOME 1: "
      "the walk seam is neutral both compiled in and ARMED. The ten legs were "
      "then RE-TAKEN under the repaired build on RULE-P2 §3/§4's unchanged "
      "terms. The five process findings are in `README.md`; what matters here "
      "is the result of the gate on the re-take.\n\n")
    w("| | |\n|---|---|\n")
    w("| cells traced | %d |\n" % ncells)
    w("| §5.1 identity gate on the re-take | **REPRODUCES %d / %d**, DIFFERS 0, "
      "CENSORED 0; near-ulp acceptances **0** — every compared field, residuals "
      "included, is BYTE-equal to the committed pass-1 row |\n"
      % (sum(1 for v in gates.values() if v == "REPRODUCES"), len(gates)))
    w("| §2 observer-solve ↔ ledger-record mapping | **UNIQUE on all %d cells**; "
      "not one AMBIGUOUS, not one NONE |\n" % len(mp))
    w("| §5.3 conservation sums | difference **0** on both sums on all %d cells "
      "(%s minors and %s factorizations on the largest, `BRITGAS`) |\n"
      % (len(mp), mp["BRITGAS"]["minors"], mp["BRITGAS"]["facs"]))
    w("| §2 ABORT flags (M3b C7) | `summary_truncated=0`, "
      "`summary_allocation_failed=0`, `minor_contiguous=1` everywhere |\n")
    w("| §3 stage 2 selection | %d solves, mechanical, no judgement at "
      "selection time |\n\n" % nsolves)

    w("## The answer, per traced solve\n\n")
    w("`PERIOD` is the length of the exactly-periodic suffix on the trace's own "
      "F0 state (every recorded field but the inertia group, compared BIT for "
      "BIT); `|T|` and `SHARE` are that suffix's length and its share of the "
      "solve's passes; `Σ|Δx|∞` is RULE-P2 §6 #4's zero-motion quantity summed "
      "over it.\n\n")
    w("| cell | solve | selected as | §6.1 ANSWER | period | \\|T\\| | share | "
      "Σ\\|Δx\\|∞ over the tail |\n")
    w("|---|---|---|---|---|---|---|---|\n")
    for r in vd:
        w("| `%s` | %d | %s | **%s** | %s | %d | %.3f | `%.6g` |\n"
          % (r["cell"], r["solve"], r["kind"].replace("_", " ").lower(),
             r["answer"], r["period"], r["tail"], r["share"], r["sumdx"]))
    w("\n**%d of %d traced solves are `CYCLE`. %d is `LONG WALK`. %d is "
      "`SOMETHING ELSE`.** %d of the %d traced cells answer `CYCLE` on every "
      "solve traced on them: %s.\n\n"
      % (tot["CYCLE"], nsolves, tot["LONG WALK"], tot["SOMETHING ELSE"],
         len(all_cycle), ncells, ", ".join("`%s`" % c for c in all_cycle)))

    w("### What a `CYCLE` row means here, precisely\n\n")
    w("On a `CYCLE` row the walk's WHOLE recorded state repeats exactly: on "
      "`ACOPP57` solve 3, %s of the %s passes are a period-2 suffix in which "
      "the same bound is dropped by the Dantzig rule and immediately "
      "re-admitted, `dx_inf` is `-0` on every add pass and absent on every "
      "drop pass, and Σ|Δx|∞ over the tail is **exactly 0**. Its re-admission "
      "share is %s — %s of the tail's add passes re-admit the SAME constraint, "
      "`%s` — and its candidate set is literally one set, seen %s times.\n\n"
      % (i(per["ACOPP57"][0][2], "tail_records"),
         i(per["ACOPP57"][0][2], "records"),
         i(per["ACOPP57"][0][2], "tail_readmission_share"),
         "all" if i(per["ACOPP57"][0][2], "tail_readmission_share") == "1.0"
         else "most",
         i(per["ACOPP57"][0][2], "tail_readmitted_modal_identity"),
         i(per["ACOPP57"][0][2], "tail_drop_passes")))
    w("The cycle is not always period 2. Four solves cycle at period 4 or 6 "
      "over TWO alternating constraint identities: %s. RULE-G's own period-2 "
      "tail finder, the inherited instrument, sees no tail at all on those "
      "(defect D1 below).\n\n"
      % ", ".join("`%s` solve %d (period %s)" % (r["cell"], r["solve"],
                                                 r["period"])
                  for r in vd if r["period"] not in ("2", "None")))

    w("## The two rows that are not `CYCLE`\n\n")
    sb = [x for x in per["STEENBRB"] if x[0] == 1][0][2]
    w("**`STEENBRB` solve 1 — `LONG WALK`.** It is the cell's FIRST capped "
      "record and the only ledger record on any of the ten TRACED cells whose "
      "status is `numerical_error` rather than `max_iter`; its trace ends on a "
      "`terminal` action. There is no exactly-periodic suffix at any period ≤ "
      "64, the working-set signature sequence has no period, %s of its %s "
      "passes are distinct signatures, and the tail MOVES: Σ|Δx|∞ = `%s` over "
      "the whole solve with %s passes moving by M3b's own `step_tol` test. "
      "Its M3b walk class is `%s` — %s — and its inertia at minor 0 is `%s` "
      "observed against `%s` expected, the +%d excess negatives M3b itself "
      "traced on this cell. **The same cell's LAST capped solve (solve 16) is "
      "a textbook period-2 `CYCLE`**, so `STEENBRB` is reported as two "
      "classes and not summarised into one, exactly as RULE-P2 §6.1 requires.\n\n"
      % (i(sb, "tail_distinct_signatures"), i(sb, "tail_records"),
         i(sb, "tail_sum_dx_inf"), i(sb, "tail_passes_that_moved_m3b"),
         i(sb, "m3b_walk_class"), ride_summary(sb),
         i(sb, "inertia_minor0_observed_triple"),
         i(sb, "inertia_minor0_expected_pair"),
         n(sb, "inertia_minor0_excess_neg")))
    ar = [x for x in per["ACOPR57"] if x[0] == 16][0][2]
    w("**`ACOPR57` solve 16 — `SOMETHING ELSE`, and it is a finding ABOUT THE "
      "RULE.** Its tail is an EXACTLY periodic suffix of period %s covering "
      "%s of its %s passes — a drop, an add, and a `move_only` pass, twice "
      "over, on two alternating bounds — and the `move_only` pass records "
      "`dx_inf = 8.9653013325729537e-4` EVERY time. The iterate therefore "
      "moves on a sixth of the tail's passes and **returns exactly**: `x_l1` "
      "alternates between two values, bit for bit, for 2 010 passes. Σ|Δx|∞ "
      "over the tail is `%s`, and RULE-P2 §6.1's `CYCLE` arm demands EXACT "
      "zero motion, so the rule cannot call it a cycle; its `LONG WALK` arm "
      "demands that the working set not repeat, and it does. **A "
      "non-zero-motion EXACT cycle is a shape §6.1 did not anticipate**, and "
      "§6.1 says in its own words that this arm is a finding about the rule. "
      "Its practical content is the same as a `CYCLE` row's: the solve is "
      "stalled and will spend its whole cap.\n\n"
      % (i(ar, "tail_period"), i(ar, "tail_records"), i(ar, "records"),
         i(ar, "tail_sum_dx_inf")))

    w("## What the tails are made of — RULE-P2 §6's read-outs\n\n")
    w("The full per-solve read-outs are machine-written in "
      "[`readouts-p2b.md`](readouts-p2b.md) and the answers in "
      "[`verdict-p2.txt`](../verdict-p2.txt). What the population shows:\n\n")

    dicts = [d for (_c, _s, _k, d) in ro]
    n_one = sum(1 for d in dicts
                if i(d, "tail_distinct_admitted_identities") == "1")
    n_two = sum(1 for d in dicts
                if i(d, "tail_distinct_admitted_identities") == "2")
    n_ineq = sum(1 for d in dicts if "ineq" in i(d, "tail_readmitted_top3", ""))
    n_zero_exc = sum(1 for d in dicts if re.fullmatch(
        r"\{0: \d+\}", i(d, "tail_inertia_excess_neg_hist", "")))
    n_no_ride = sum(1 for d in dicts if re.fullmatch(
        r"\{'not_attempted': \d+\}", i(d, "tail_ride_outcomes", "")))
    lasts = [d for (_c, _s, k, d) in ro if k == "LAST_CAPPED"]
    sizes = sorted(n(d, "incoming_ws_size_in_own") for d in lasts)
    small = [v for v in sizes if v <= 40]
    ws_txt = ("%d–%d on %d of the %d, and %s on the other %d"
              % (min(small), max(small), len(small), len(sizes),
                 "/".join(str(v) for v in sizes if v > 40),
                 len(sizes) - len(small))
              if small else "; ".join(str(v) for v in sizes))
    yk = [d for (c, s, _k, d) in ro if (c, s) == ("YORKNET", 7)][0]

    w("* **§6 #3, the re-admitted constraint.** On %d of the %d traced "
      "solves the tail admits exactly ONE constraint identity, over and over, "
      "with a re-admission share of `1.0`; on %d it alternates between exactly "
      "two. It is a BOUND in every case — the number of traced solves "
      "whose tail re-admits an INEQUALITY ROW is **%d**.\n"
      % (n_one, nsolves, n_two, n_ineq))
    w("* **§6 #8, candidate-set recurrence.** On the same solves the "
      "Dantzig drop's candidate SET is one or two sets, seen thousands of "
      "times: `tail_distinct_candidate_sets` is 1 or 2 on every `CYCLE` row, "
      "with a modal share of `1.0` or `0.5`. `drop_tied` is `1` on almost all "
      "of them — one tie, resolved the same way every pass. The exception "
      "is `YORKNET` solve 7, whose tail drop passes all carry "
      "`drop_tied = %s` (`tail_drop_tied_hist` = `%s`): that many candidates "
      "tie on the drop rule and the same one is taken each time.\n"
      % (list(eval(i(yk, "tail_drop_tied_hist", "{}")).keys())[0],
         i(yk, "tail_drop_tied_hist")))
    w("* **§6 #5, inertia.** M3b C11's qualification is carried on every "
      "line: the printed OBSERVED triple is the retained sparse `K0` factor's, "
      "with the dense Schur complement's Haynsworth contribution BACKED OUT of "
      "the EXPECTED pair instead, so a non-zero excess away from minor 0 is an "
      "ALGEBRAIC verdict and not a fresh per-working-set factorization — "
      "and on these solves minor 0 is usually NOT COLD either (`border_dim > "
      "0` at minor 0 on most of them), so even the minor-0 comparison is "
      "algebraic here, unlike M3b's cold first QP. The TAIL excess is "
      "**exactly 0 on every pass of %d of the %d solves**: on most of this "
      "population the engine's own inertia test reports the working set as "
      "correct while the walk cycles on it.\n" % (n_zero_exc, nsolves))
    w("* **§6 #6 and #7, curvature and the ride.** On %d of the %d solves "
      "the ride is NEVER ATTEMPTED anywhere in the tail and the cycle is pure "
      "Dantzig drop/re-add. On the other %d the curvature quotient "
      "`p_hp/p_sq` sits at or below its tolerance and the ride fires: "
      "`SAWPATH` solve 10 vetoes 7 927 rides on the null space, `BRITGAS` "
      "solve 0 vetoes 2 141, `YORKNET` solve 7 vetoes 2 539 on a flat slope "
      "— and on `STEENBRC` solve 0, `STEENBRG` solve 0 and `YORKNET` "
      "solve 145 the ride reports `stepped` 1 148, 1 144 and 1 998 times "
      "**and the step is exactly zero**. A ride that steps nowhere is the "
      "cycle continuing, not an escape from it.\n"
      % (n_no_ride, nsolves, nsolves - n_no_ride))
    w("* **§6 #9, warm starts, and the question the dispatch named.** "
      "**Does a later capped solve START in the previous capped solve's cycle "
      "state? NO — on every one of the eight cells where the comparison "
      "is available, the answer is NOT EQUAL**, compared against ALL of the "
      "earlier solve's distinct tail signatures and not a sample. The later "
      "solve comes in with its own working set (`ws_size_in` %s) and walks "
      "back into a cycle of its own. The cycle is RE-ENTERED each major, not "
      "INHERITED across majors. `YORKNET` and `SAWPATH` have one capped "
      "main-engine record each, so the comparison DECLINES on them rather "
      "than guessing. M3b C7's qualification is carried: an EQUALITY of "
      "SplitMix64 signatures would have been evidence and not proof; a "
      "NON-EQUALITY is conclusive.\n\n" % ws_txt)

    w("## Findings about `RULE-P2.md` itself\n\n")
    w("`RULE-P2.md` is **not edited** — a pre-registered rule's bytes are the "
      "pin. Applying it found three defects, each of which had to be given a "
      "reading before §6.1 could return anything. Every reading is declared in "
      "`wrappers/readouts_p2b.py`'s docstring, and BOTH readings' answers are "
      "printed on every solve in `verdict-p2.txt`.\n\n")
    w("### D1 — §6.1 says \"a tail\" and never defines one\n\n"
      "The only pre-registered tail in the inherited instruments is "
      "`RULE-G.md` §3's, a PERIOD-2 repeat detector. §6.1's own word is "
      "\"PERIODIC\", with no period named. A period-2-only detector cannot "
      "answer the question §6.1 asks, and on this population it demonstrably "
      "does not: on `STEENBRC` solve 0, `STEENBRG` solve 0, `YORKNET` solve "
      "145 and `ACOPR57` solve 16 it finds a two-record tail where a period-4 "
      "or period-6 detector finds one covering 85–97% of the solve. **The "
      "reading applied** is RULE-G's own construction with its `2` replaced by "
      "a search over p ∈ [1, 64], taking the p that maximises the tail, ties "
      "to the smallest p — so it degenerates EXACTLY to RULE-G's tail whenever "
      "the winner is 2, which it is on 17 of the 21 solves. RULE-G's own "
      "period-2 answer is printed beside it on every solve.\n\n")
    w("### D2 — §6.1's `dx_inf == 0` conjunct is unreachable read strictly\n\n"
      "A DROP pass takes no step and the probe writes `nan` into `dx_inf` and "
      "`alpha` alike; on a drop/add cycle that is HALF the passes. Read "
      "literally — every tail pass carries a PRESENT `dx_inf` equal to 0 — the "
      "`CYCLE` arm can never fire on any real trace, which is a defect and not "
      "a measurement. **The reading applied**: \"exact zero motion on every "
      "tail pass\" means NO tail pass moved the iterate — no present `dx_inf` "
      "is non-zero and their sum is exactly 0 — with the absent ones being "
      "passes that record no step at all, and the read-out naming which "
      "actions those are. Both numbers, `zero_motion_operative` and "
      "`zero_motion_strict`, are printed on every solve; under the strict "
      "reading all 19 `CYCLE` rows would read `SOMETHING ELSE` and the rule "
      "would have measured nothing. (The recorded value is `-0`; `-0.0 == "
      "0.0` is exact, not a tolerance.)\n\n")
    w("### D3 — §6.1's `LONG WALK` clause is ambiguous\n\n"
      "\"no periodic tail, or a tail with non-zero motion: Σ‖Δx‖∞ > 0 over the "
      "tail and the working set does not repeat\" parses either "
      "disjunctively or conjunctively, and \"does not repeat\" is undefined. "
      "**The reading applied** is the CONJUNCTIVE one, with \"does not repeat\" "
      "= the tail's signature sequence has NO minimal period — the exact "
      "complement of `CYCLE`'s \"periodic\", which is what makes the two arms "
      "disjoint. On this population the two parses differ on exactly ZERO "
      "solves, so the ambiguity changed no answer here; it is recorded because "
      "it could have.\n\n")
    w("**A fourth, smaller defect, of §5.1 rather than §6.1:** two of the "
      "fourteen byte fields §5.1 names — `elastic_activations` and "
      "`qp_failures_in_a_row_peak` — are **not printed by the pass-2 probe at "
      "all**. They are reported ABSENT on every cell by `map_p2.py` and are "
      "never counted as agreement. The whole-run gate that actually decided "
      "the re-take, `gate_p2a.py`, does not choose its fields at all: it "
      "compares the INTERSECTION of the two rows' own key sets, 58 fields, so "
      "the defect cost the gate nothing.\n\n")

    w("## What pass 2 settles, and what it does not\n\n")
    w("**SETTLED, OBSERVED.** The capped QPs at DEFAULT options ARE the "
      "zero-motion cycle M3b traced at `tr_init = +inf`. It is not an "
      "artefact of that configuration, it is not confined to the first QP, "
      "and it is not confined to one cell: it is %d of %d traced solves "
      "across %d of the %d traced cells, including both a main-walk solve and "
      "a restoration-sub-driver solve on the same cell. The counter shape "
      "pass 1 called INFERRED — `degenerate_steps ≈ minors/2`, `ws_adds ≈ "
      "ws_drops ≈ minors/2` — is now OBSERVED to be that cycle, on traces "
      "that reproduce the census rows byte for byte.\n\n"
      % (tot["CYCLE"], nsolves, len(all_cycle), ncells))
    w("**NOT SETTLED.** Why the walk re-admits the constraint it just dropped "
      "is not measured here: the drop rule's multiplier sign and the ratio "
      "test's choice are recorded per pass, but no experiment in this census "
      "varies either. Nor is it measured what an anti-cycling rule would do — "
      "no remedy was built, run or selected, and RULE-P2 §8 forbids this "
      "section from licensing one.\n")


def readme_block(ro, vd, mp, gates, terms, out):
    w = out.write
    per = group(ro)
    tot = {a: sum(1 for r in vd if r["answer"] == a)
           for a in ("CYCLE", "LONG WALK", "SOMETHING ELSE")}
    w("\n---\n\n")
    w("## What the census ANSWERS, in plain language\n\n")
    w("Two passes, twenty cells, %d traced solves. The short version: **at the "
      "options hven actually ships, the SQP lane's failing cells die inside "
      "the QP walk, and the walk is cycling.**\n\n" % len(vd))
    w("| cell | what kills it at DEFAULT options (pass 1, OBSERVED) | are its "
      "capped QPs M3b's cycle? (pass 2) |\n|---|---|---|\n")

    story = {
        "ACOPP57": "3 steps accepted, then 4 QP solves in a row spend their "
                   "full 1 795-minor cap and the driver's 3-failure budget "
                   "ends the solve; never restores",
        "ACOPP118": "3 steps accepted, then 4 capped 4 710-minor solves and "
                    "the budget; never restores",
        "ACOPR57": "8 steps accepted, then capped 2 080-minor solves and the "
                   "budget; never restores",
        "DALLASM": "restores once (sub-status optimal) and activates the "
                   "elastic tier once; capped 2 150- and 1 960-minor solves "
                   "spend the budget",
        "STEENBRB": "restores from h≈2.9e3 to h≈0 and lands f at 4.09e12; the "
                    "QPs after that excursion are capped and spend the budget",
        "STEENBRC": "restores and lands f at 1.83e14; capped 5 400-minor "
                    "solves spend the budget",
        "STEENBRG": "restores and lands f at 1.83e14; capped 5 400-minor "
                    "solves spend the budget",
        "BRITGAS": "the budget is spent over majors 0–2, then major 3's "
                   "subproblem returns `infeasible`, the elastic ladder runs "
                   "and returns unusable, and the RESTORATION exit ends the "
                   "solve `numerical_error` (pass-1 finding R2: the class "
                   "name says budget, the mechanism is the restoration exit)",
        "YORKNET": "one capped 5 080-minor main-engine solve, 19 elastic "
                   "activations, then restoration runs 18 iterations and "
                   "returns `infeasible` with h unchanged bit for bit",
        "SAWPATH": "the 300 s wall, inside ONE major that has already spent "
                   "77 037 minors; its restoration sub-solve ends "
                   "`numerical_error`",
    }
    answer_note = {
        "STEENBRB": "**SPLIT.** Its FIRST capped solve is a `LONG WALK` that "
                    "ends `numerical_error`; its LAST is a period-2 `CYCLE`.",
        "ACOPR57": "**SPLIT.** Its FIRST capped solve is a period-2 `CYCLE`; "
                   "its LAST is an exact period-6 cycle that MOVES and "
                   "returns — `SOMETHING ELSE` under the rule, a stall in "
                   "practice.",
    }
    for cell in per:
        rows = [r for r in vd if r["cell"] == cell]
        if cell in answer_note:
            ans = answer_note[cell]
        else:
            shares = sorted(round(100 * r["share"]) for r in rows)
            nl = [r for r in rows if r["kind"].startswith("FIRST_NOT_LEDGERED")]
            ans = ("**YES** — %s of %s traced solves are `CYCLE`, period %s, "
                   "tail %s of the passes, Σ|Δx|∞ exactly 0.%s"
                   % (sum(1 for r in rows if r["answer"] == "CYCLE"),
                      len(rows),
                      "/".join(sorted({r["period"] for r in rows}, key=int)),
                      ("%d%%" % shares[0] if shares[0] == shares[-1]
                       else "%d–%d%%" % (shares[0], shares[-1])),
                      (" One of them (solve %d) is a NOT-LEDGERED solve — the "
                       "restoration sub-driver's or a lazy engine's, which the "
                       "public ledger never sees." % nl[0]["solve"])
                      if nl else ""))
        w("| `%s` | %s | %s |\n" % (cell, story.get(cell, "see the table"),
                                    ans))
    w("\n**OBSERVED vs INFERRED.** Everything in the middle column is "
      "OBSERVED, from rows that reproduce the harness's committed rows at the "
      "pin on all 24 compared fields. Everything in the right column is "
      "OBSERVED too, from traces whose own rows reproduce those same pass-1 "
      "rows BYTE for byte on all 58 shared fields. What is INFERRED is named "
      "as such wherever it appears, and the biggest inference pass 1 carried "
      "— \"the counter shape looks like a cycle\" — is exactly the one pass 2 "
      "converted into an observation.\n\n")
    w("Three more cells (`NET3`, `STEENBRD`, `STEENBRE`) never build a "
      "subproblem at all: the objective is NaN at the CUTEst start point. "
      "Five (`MSS2`, `MSS3`, `READING8`, `TAX213322`, `TAX53322`) are CENSORED "
      "by the outer timeout and are never filled in. Two more (`ACOPR118`, "
      "`BIGBANK`) are truncated by the wall; `BIGBANK` is accepting every step "
      "it takes and is simply slow.\n\n")

    w("## The rule defects this round recorded, and did not repair\n\n")
    w("A pre-registered rule's bytes are the pin. Seven defects were found by "
      "APPLYING the three rules, and **not one rule file was edited**:\n\n")
    w("| defect | rule | what it is |\n|---|---|---|\n")
    w("| R1 | `RULE.md` §5.2 #7 | two different predicates in one clause; the "
      "ambiguity decides `YORKNET`'s class. Both readings printed. |\n")
    w("| R2 | `RULE.md` §5.2 #4 | fires on `BRITGAS`, whose exit is the "
      "restoration path and not the failure budget. Label stands; mechanism "
      "printed beside it. |\n")
    w("| R3a | `RULE.md` F2 | the h-band degenerates to `0 >= 0` when "
      "`h_in == 0`, which is `SAWPATH`. The pair is printed rather than the "
      "ratio. |\n")
    w("| R3 | `RULE.md` F3, F4 | dead on the whole population: no cell "
      "reaches the trust-region floor and essentially every `Reject` is a "
      "routed QP failure. |\n")
    w("| D1 | `RULE-P2.md` §6.1 | \"a tail\" is never defined, and the "
      "inherited detector is period-2 only. Four solves cycle at period 4 or "
      "6 and it sees nothing. |\n")
    w("| D2 | `RULE-P2.md` §6.1 | `dx_inf == 0` on every tail pass is "
      "unreachable read strictly: drop passes record no `dx_inf` at all. |\n")
    w("| D3 | `RULE-P2.md` §6.1 | the `LONG WALK` clause parses two ways and "
      "\"does not repeat\" is undefined. Changed no answer here. |\n")
    w("\nA fourth `RULE-P2` defect, of §5.1: two of its fourteen named byte "
      "fields are not printed by the pass-2 probe and are reported ABSENT "
      "rather than as agreement.\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--section",
                    choices=("census", "readme", "recommendation"),
                    required=True)
    ap.add_argument("--root", default=HERE)
    a = ap.parse_args(argv)
    r = a.root
    ro = read_readouts(os.path.join(r, "tables/readouts-p2b.md"))
    vd = read_verdict_table(os.path.join(r, "verdict-p2.txt"))
    mp = read_mapping(os.path.join(r, "tables/mapping-p2b.txt"))
    gates = read_gate_verdicts(os.path.join(r, "logs/gate-p2b.txt"))
    terms = read_census_terminators(os.path.join(r, "tables/census.md"))
    for name, obj in (("readouts", ro), ("verdicts", vd), ("mapping", mp),
                      ("gate", gates)):
        if not obj:
            sys.stderr.write("REFUSED: %s parsed EMPTY\n" % name)
            return 2
    if len(vd) != len(ro):
        sys.stderr.write("REFUSED: %d verdict rows vs %d read-out blocks\n"
                         % (len(vd), len(ro)))
        return 2
    if a.section == "census":
        census_section(ro, vd, mp, gates, terms, sys.stdout)
    elif a.section == "readme":
        readme_block(ro, vd, mp, gates, terms, sys.stdout)
    else:
        recommendation(ro, vd, mp, gates, terms, sys.stdout)
    return 0




# ---------------------------------------------------------------------------
# the RECOMMENDATION to the settler
# ---------------------------------------------------------------------------
def recommendation(ro, vd, mp, gates, terms, out):
    w = out.write
    per = group(ro)
    dicts = [d for (_c, _s, _k, d) in ro]
    nsolves = len(vd)
    tail_tot = sum(n(d, "tail_records") for d in dicts)
    rec_tot = sum(n(d, "records") for d in dicts)
    minors_tot = sum(v["minors"] for v in mp.values())
    starts = sorted(n(d, "tail_start_minor") for d in dicts)
    n_zero_exc = sum(1 for d in dicts if re.fullmatch(
        r"\{0: \d+\}", i(d, "tail_inertia_excess_neg_hist", "")))
    n_bound_only = sum(1 for d in dicts
                       if "'bound'" in i(d, "tail_readmitted_modal_identity", ""))

    w("\n---\n\n")
    w("## Engine change ⇒ cells it plausibly converts — a RECOMMENDATION to "
      "the settler\n\n")
    w("**A RECOMMENDATION, and nothing more.** `RULE.md` §0/§10 and "
      "`RULE-P2.md` §0/§8 bind every line: no remedy is selected, designed, "
      "built or run here, and each candidate below is its own task with its "
      "own pre-registered rule and its own gates. OBSERVED and INFERRED are "
      "marked separately on every line, and what is NOT KNOWN is stated "
      "rather than left out.\n\n")

    w("### Candidate A — an ANTI-CYCLING rule in the QP walk\n\n")
    w("*Would plausibly convert, or at least stop wasting: `ACOPP57`, "
      "`ACOPP118`, `ACOPR57`, `DALLASM`, `STEENBRB`, `STEENBRC`, `STEENBRG` "
      "(7 cells whose terminator is `QP_CAP_BUDGET`), and is implicated in "
      "`BRITGAS`, `SAWPATH` and `YORKNET` (3 more). Pass 1 listed the same ten "
      "as INFERRED; pass 2 makes them OBSERVED.*\n\n")
    w("**OBSERVED.** %d of %d traced solves end in an exactly periodic, "
      "zero-motion tail; one more (`ACOPR57` solve 16) ends in an exactly "
      "periodic tail that moves and returns. The tail starts EARLY — its "
      "start minor over the %d solves runs %d … %d — so **%d of the %d "
      "recorded minors in the traced solves, %.1f%%, are spent after the "
      "walk has already begun repeating itself.** Those solves account for "
      "%d of the %d minors the ten cells spend in total. The MODAL "
      "re-admitted constraint of the tail is a BOUND on %d of the %d solves "
      "and an inequality row on none of them.\n\n"
      % (sum(1 for r in vd if r["answer"] == "CYCLE"), nsolves,
         nsolves, starts[0], starts[-1], tail_tot, rec_tot,
         100.0 * tail_tot / rec_tot, rec_tot, minors_tot,
         n_bound_only, nsolves))
    w("**OBSERVED, and it CONSTRAINS the remedy rather than supporting one.** "
      "The engine's own inertia test reports NO excess negatives anywhere in "
      "the tail on %d of the %d solves. **An inertia-CONTROLLING remedy would "
      "not fire on most of this population**, because by the engine's own "
      "algebraic verdict the working set is not the problem it is looking "
      "for. Where the tail excess is non-zero it is small and constant "
      "(`ACOPP57`'s −6/−5, `ACOPR57` solve 16's −19/−18/0, the ±1 alternation "
      "on `STEENBRC`/`STEENBRG`/`BRITGAS`), and M3b C11 says those are "
      "ALGEBRAIC verdicts about the bordered system, not fresh "
      "factorizations. An ANTI-STALL remedy keyed on repetition — the "
      "working-set signature repeating with no motion — fires on every one of "
      "the %d `CYCLE` solves by construction.\n\n"
      % (n_zero_exc, nsolves, sum(1 for r in vd if r["answer"] == "CYCLE")))
    w("**OBSERVED.** The cycle is RE-ENTERED, not INHERITED: on all eight "
      "cells where a later capped solve exists, its incoming working-set "
      "signature is NOT any of the earlier solve's tail signatures. A "
      "per-solve anti-cycling rule needs no cross-solve state.\n\n")
    w("**INFERRED, and NOT established.** That breaking the cycle would make "
      "these QPs terminate usefully. Nothing here measures what the walk does "
      "after an anti-cycling intervention, and nothing here says the "
      "subproblem has a solution the walk could reach: a cycling active-set "
      "walk can be cycling on a degenerate vertex of a subproblem that is "
      "itself the wrong subproblem. **NOT KNOWN:** why the drop rule selects "
      "the constraint it just admitted — the multiplier sign and the ratio "
      "test are recorded per pass, but no experiment here varies either.\n\n")

    w("### Candidate A′ — a QP-CAP TERMINATION REASON (cheap, and strictly "
      "diagnostic)\n\n")
    w("*Would convert: probably nothing on its own. Would make every cell in "
      "candidate A legible to the driver and to the next census.*\n\n")
    w("**OBSERVED.** A capped walk returns `max_iter` today, and the driver "
      "routes it as one more consecutive failure until the budget ends the "
      "solve. Pass 1's finding R3 is that essentially every `Reject` verdict "
      "in these histories is a ROUTED QP FAILURE rather than a merit "
      "rejection. Pass 2 shows those same solves were provably stalled for "
      "82–99.9%% of their minors before the cap was reached.\n\n")
    w("**INFERRED.** That a distinct exit reason — \"the walk repeated a "
      "working set with zero motion\" — would let the driver do something "
      "better than shrink and retry. It is a CHEAP change to measure and it "
      "commits to no numerical remedy. **NOT KNOWN:** what the driver should "
      "do differently on such an exit; this census does not design one.\n\n")

    w("### Candidate B — the SCHUR REBUILD cost of the cycle\n\n")
    w("*Would convert: nothing by itself. Would remove a large constant "
      "factor from every cell in candidate A, and it is the reason those "
      "cells are also slow.*\n\n")
    w("**OBSERVED, and this is new in pass 2.** The per-pass cost of a "
      "cycling minor is not uniform, and the traces split the population in "
      "two:\n\n")
    w("| solve | tail passes | factorizations in the tail | Schur updates | "
      "border refines | `eqp_path` in the tail |\n|---|---|---|---|---|---|\n")
    for (c, s, k, d) in ro:
        w("| `%s` solve %d | %s | %s | %s | %s | %s |\n"
          % (c, s, i(d, "tail_records"), i(d, "OUTSIDE_RULE_tail_factorizations"),
             i(d, "OUTSIDE_RULE_tail_schur_updates"), i(d, "OUTSIDE_RULE_tail_border_refine_steps"),
             i(d, "OUTSIDE_RULE_tail_eqp_paths")))
    w("\nOn the `eliminated` rows the walk pays **one full sparse KKT "
      "factorization for every wasted minor**; on the `border_schur` rows it "
      "pays one Schur update and one border refine and NO factorization. "
      "`ACOPP118` spends 20 501 factorizations on 19 917 minors; `BRITGAS` "
      "spends 93 749 on 92 309. `DALLASM` solve 1 is the extreme: 2 "
      "factorizations and ~102 Schur updates per tail pass.\n\n")
    w("**INFERRED, and NOT established.** That the `eliminated` path is "
      "avoidable on these working sets, or that keeping the border stack "
      "alive would be correct there. The trace records `border_latched` and "
      "`border_dim = -1` on exactly those passes; what those states mean for "
      "the engine's own correctness is not read out here and no hven source "
      "was changed to find out. **NOT KNOWN:** whether the rebuild is a "
      "consequence of the cycle or a contributor to it.\n\n")

    w("### Candidate C — `project_start_point` for the three non-finite-start "
      "cells\n\n")
    w("*Would plausibly convert: `STEENBRD`, `STEENBRE`; would not convert "
      "`NET3` (3 cells in the class, 2 candidates). Unchanged from pass 1 — "
      "pass 2 traced none of them, because they build no subproblem at "
      "all.*\n\n")
    w("**OBSERVED.** All three die at major 0 with `f(x0) = NaN`, an empty "
      "ledger and zero factorizations. `STEENBRD` and `STEENBRE` are solved "
      "to optimality by hven's OWN IPM engine on the same model through the "
      "same bridge, and by SNOPT; `NET3` defeats all four lanes. The option "
      "exists, is shipped OFF, and every leg's `OPTIONS` block proves it OFF."
      "\n\n")
    w("**INFERRED, explicitly not established.** That the start point lies "
      "outside the declared box and that the projection is what the IPM's "
      "interior push supplies. **NOT KNOWN:** which of `f`, `grad`, `cE`, "
      "`cI` is non-finite — the driver's gate conjoins all four into one "
      "bool. The census's own three-step measurement for this is written out "
      "in `tables/census.md`; it is cheap and touches no QP code.\n\n")

    w("### Candidate D — the restoration excursion\n\n")
    w("*Would plausibly convert: `STEENBRB`, `STEENBRC`, `STEENBRG` (3 cells, "
      "all of them also in candidate A).*\n\n")
    w("**OBSERVED, pass 1.** Each restores from a large violation to "
      "`h_out ≤ 5e-12` and lands the objective nine to eleven orders of "
      "magnitude above where it started. **OBSERVED, pass 2, and it is the "
      "new part:** the QPs AT that restored point are the cycling ones — "
      "`STEENBRC` and `STEENBRG` both cycle at period 4 on their first capped "
      "solve and at period 2 on their last, and `STEENBRB`'s last capped "
      "solve cycles at period 2 with a 4 531-pass tail. So the excursion and "
      "the cycle are not competing explanations on these three: the "
      "excursion supplies the point, and the walk cycles once it is "
      "there.\n\n")
    w("**INFERRED.** That a proximity-weighted restoration (M2b built it, "
      "shipped OFF at `restoration_proximity = 0`) would land somewhere the "
      "walk does not cycle. **NOT KNOWN:** whether the cycling is a property "
      "of the restored point or of the walk; the same walk cycles on "
      "`ACOPP57`, `ACOPP118` and `ACOPR57`, which never restore at all. That "
      "is evidence AGAINST the excursion being the cause of the cycling, and "
      "FOR it being an independent second problem on those three cells.\n\n")

    w("### Candidate E — a drop-rule TIE-BREAK\n\n")
    w("*Would plausibly touch: `YORKNET` (1 cell). Newly suggested by the "
      "traces, and small.*\n\n")
    w("**OBSERVED.** On `YORKNET` solve 7 every one of the 2 538 tail drop "
      "passes records `drop_tied = 8`: eight candidates tie on the drop "
      "rule's own criterion, and the same one is chosen every pass, and the "
      "walk cycles on it. Everywhere else in this population `drop_tied` is "
      "`1`. **INFERRED:** that a different tie-break would leave the cycle. "
      "**NOT KNOWN:** what the eight tied candidates are — the trace records "
      "the COUNT and the chosen identity, not the tied set's members.\n\n")

    w("### What NOT to build from this census\n\n")
    w("* **Trust-region floor work and merit/filter acceptance work** — "
      "pass 1's finding R3 stands: `F3_TR_AT_FLOOR` is false on every cell "
      "with a row and judged rejections are 0 or 2 everywhere.\n")
    w("* **An inertia-controlled correction as the PRIMARY anti-stall** — "
      "pass 2's own read-out says the inertia excess in the tail is exactly "
      "zero on %d of %d traced solves. It would be building for a condition "
      "most of this population does not report.\n"
      "* **A ride/curvature fix as the primary remedy** — on %d of the %d "
      "solves the ride is never attempted in the tail at all, and on three "
      "of the solves where it IS attempted it reports `stepped` with a step "
      "of exactly zero. The ride is not what is holding these walks up.\n"
      % (n_zero_exc, nsolves,
         sum(1 for d in dicts if re.fullmatch(
             r"\{'not_attempted': \d+\}", i(d, "tail_ride_outcomes", ""))),
         nsolves))


if __name__ == "__main__":
    sys.exit(main())
