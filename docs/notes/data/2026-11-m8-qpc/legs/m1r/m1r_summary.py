#!/usr/bin/env python3
"""QPC leg M1R summary (DECLARATION-QPC section 6, leg M1R; settler C581) over the [m1r] lines of the M1R diagnostic build.

  m1r_summary.py <M1R out dir>

Reads <out>/train/*.err (TRAIN-238 member 0, one file per cell) and <out>/sensq/<algebra>/*.err (SENS-Q, one file per
dump and algebra). One [m1r] line = one data-half verdict at the walk's would-be certified exit:
  holds=0|1 fail=FRPSLO (finite, rows, product, stationarity beyond rounding, slack, off-working: 1 = that disjunct
  fails, certificate_data_half_holds's own comparisons), the readings in tolerance units, and colstat/otol = the
  COLUMN-LOCAL stationarity beyond rounding (per free column i, max(0, |r_i| - c_i u m_i) / t_i, t_i the column's own
  largest stationarity term, u = eps/2; +inf where the excess is nonzero over t_i = 0), with its column.
Prints, per corpus:
  - verdict counts (certified / refused);
  - THE PER-DISJUNCT REFUSAL HISTOGRAM: for refused verdicts, each disjunct's failure count, the count where it is the
    ONLY failing disjunct, and the count per full failure pattern; and the refusals per cell;
  - THE COLUMN-LOCAL STATIONARITY reading of CERTIFIED verdicts (every certified verdict passed the global test, by
    definition): the number past 10 opt_tol (the gate's own stationarity bound), by decade, the cells, and every such
    verdict's value, column, t_i and r_i (worst 40 printed in full, all counted);
  - the same restricted to HIDDEN columns (hidstat/otol: columns past 10 opt_tol column-locally whose ABSOLUTE excess
    also exceeds 10 opt_tol, i.e. past the global test even at its floor scale 1 -- what a global scale above 1 lets
    through, limitation (iii) proper), with the per-verdict counts colfail / colhidden and the global scale gscale;
    the remainder are columns whose every term is below the gate's absolute resolution (TINY columns);
  - the C581 STOP RULE: STOP iff any certified verdict on TRAIN-238 m0 or SENS-Q has colstat/otol > 10 (or NaN);
    otherwise PASS. The line `M1R STOP-RULE` is the verdict.
rc 0 always (a reading; the STOP is the settler's to act on)."""
import collections
import glob
import math
import os
import re
import sys

DISJ = ("finite", "rows", "product", "stationarity", "slack", "off_working")
LINE = re.compile(r"(\S+?)=(\S+)")


def read(pattern, cell_of):
    out = []
    for f in sorted(glob.glob(pattern)):
        cell = cell_of(f)
        for ln in open(f, errors="replace"):
            if ln.startswith("[m1r]"):
                out.append((cell, dict(m.groups() for m in LINE.finditer(ln))))
    return out


def fnum(s):
    try:
        return float(s)
    except ValueError:
        return float("nan")


def summarize(name, recs):
    print(f"=== {name}")
    cert = [(c, d) for c, d in recs if d["holds"] == "1"]
    ref = [(c, d) for c, d in recs if d["holds"] == "0"]
    cells = sorted({c for c, _ in recs})
    print(f"{name} verdicts={len(recs)} certified={len(cert)} refused={len(ref)} cells_with_a_verdict={len(cells)}")
    # Consistency: holds == no disjunct fails.
    bad = [(c, d) for c, d in recs if (d["holds"] == "1") != (d["fail"] == "000000")]
    if bad:
        print(f"{name} INCONSISTENT holds/fail flags: {len(bad)} (first: {bad[0]})")
    print(f"--- {name} per-disjunct refusal histogram (refused verdicts: {len(ref)})")
    any_fail = collections.Counter()
    only = collections.Counter()
    pattern = collections.Counter()
    for _, d in ref:
        flags = d["fail"]
        pattern[flags] += 1
        failing = [DISJ[k] for k in range(6) if flags[k] == "1"]
        for x in failing:
            any_fail[x] += 1
        if len(failing) == 1:
            only[failing[0]] += 1
    print(f"{'disjunct':<14}{'fails':>8}{'sole':>8}")
    for x in DISJ:
        print(f"{x:<14}{any_fail[x]:>8}{only[x]:>8}")
    print("patterns (finite rows product stationarity slack off_working; 1 = fails):")
    for p, k in sorted(pattern.items(), key=lambda t: (-t[1], t[0])):
        print(f"  {p} {k}  [{' + '.join(DISJ[i] for i in range(6) if p[i] == '1')}]")
    by_cell = collections.Counter(c for c, _ in ref)
    print("refusals by cell:")
    for c, k in sorted(by_cell.items()):
        print(f"  {c}: {k}")
    print(f"--- {name} column-local stationarity on CERTIFIED verdicts ({len(cert)})")
    over = [(c, d) for c, d in cert if not (fnum(d["colstat/otol"]) <= 10.0)]
    dec = collections.Counter()
    for _, d in over:
        v = fnum(d["colstat/otol"])
        if math.isnan(v):
            dec["nan"] += 1
        elif math.isinf(v):
            dec["inf (excess over t_i = 0)"] += 1
        else:
            dec[f"1e{int(math.floor(math.log10(v)))}"] += 1
    print(f"{name} certified with colstat > 10 opt_tol: {len(over)} in {len({c for c, _ in over})} cells")
    for k in sorted(dec, key=lambda s: (s[0] != "1", float(s[2:]) if s.startswith("1e") else 1e99)):
        print(f"  colstat/otol in [{k}, x10): {dec[k]}" if k.startswith("1e") else f"  {k}: {dec[k]}")
    per_cell = collections.Counter(c for c, _ in over)
    print("by cell (count, worst colstat/otol):")
    for c in sorted(per_cell):
        w = max((fnum(d["colstat/otol"]) for cc, d in over if cc == c), key=lambda v: (not math.isnan(v), v))
        print(f"  {c}: {per_cell[c]} worst={w:.3e}")
    hid = [(c, d) for c, d in cert if "hidstat/otol" in d and not (fnum(d["hidstat/otol"]) <= 10.0)]
    print(f"{name} certified with a HIDDEN column (colstat > 10 opt_tol AND absolute excess > 10 opt_tol): {len(hid)} "
          f"in {len({c for c, _ in hid})} cells; the other {len(over) - len(hid)} fail only on TINY columns")
    hcell = collections.Counter(c for c, _ in hid)
    for c in sorted(hcell):
        w = max((fnum(d["hidstat/otol"]) for cc, d in hid if cc == c), key=lambda v: (not math.isnan(v), v))
        print(f"  {c}: {hcell[c]} worst hidstat/otol={w:.3e}")
    hkey = lambda cd: (math.isnan(fnum(cd[1]["hidstat/otol"])), fnum(cd[1]["hidstat/otol"]))
    print("worst 40 HIDDEN (cell hidstat/otol col t_i r_i gscale colhidden/colfail/nfree | stat/otol):")
    for c, d in sorted(hid, key=hkey, reverse=True)[:40]:
        print(f"  {c} hidstat/otol={d['hidstat/otol']} col={d['hid_col']} t={d['hid_t']} r={d['hid_r']} "
              f"gscale={d['gscale']} cols={d['colhidden']}/{d['colfail']}/{d['nfree']} | stat/otol={d['stat/otol']}")
    key = lambda cd: (math.isnan(fnum(cd[1]["colstat/otol"])), fnum(cd[1]["colstat/otol"]))
    print("worst 40 (cell colstat/otol col t_i r_i | global stat/otol normstat/otol n nfree):")
    for c, d in sorted(over, key=key, reverse=True)[:40]:
        print(f"  {c} colstat/otol={d['colstat/otol']} col={d['colstat_col']} t={d['colstat_t']} r={d['colstat_r']} | "
              f"stat/otol={d['stat/otol']} normstat/otol={d['normstat/otol']} n={d['n']} nfree={d['nfree']}")
    return len(over), len(hid)


def main():
    out = os.path.abspath(sys.argv[1])
    train = read(os.path.join(out, "train", "*.err"), lambda f: os.path.basename(f)[:-4])
    sensq = []
    for alg in ("refactorize", "border"):
        sensq += read(os.path.join(out, "sensq", alg, "*.err"), lambda f, a=alg: f"{os.path.basename(f)[:-4]}:{a}")
    n_train, h_train = summarize("TRAIN-238 m0", train)
    n_sensq, h_sensq = summarize("SENS-Q", sensq)
    stop = n_train + n_sensq
    print(f"M1R STOP-RULE {'STOP' if stop else 'PASS'}: certified verdicts failing column-local stationarity at "
          f"10 opt_tol while the global test passes: TRAIN-238 m0 {n_train}, SENS-Q {n_sensq} "
          f"(of which with a HIDDEN column: TRAIN-238 m0 {h_train}, SENS-Q {h_sensq})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
