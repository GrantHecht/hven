#!/usr/bin/env python3
"""W5 T8.9r-attrib5 -- the single-row interior instruction verdict on 9cebbbe.

Reads every `perf stat` output under art/perf{A,B,C}/<arm>/<row>-r<n>.txt, writes
perf.csv (one row per arm x row x round x event), and prints:

  * the per-row instruction ratio table, culprit/parent and control/parent,
    medians of five rounds, with the per-arm round-to-round spread;
  * the classification per reading.md 11.1/11.2;
  * the pass B and pass C deltas.

THE SCORED SET IS 11's, NOT THIS LEG'S CHOICE. 11 scored eleven rows: four
cells x three treatments minus `f7_n1000_bound_physics/MakeParameter`, which its
pre-declared positional rule excluded as the process's warm-up row. This leg
runs all twelve -- every row is its own process here, so no row is any other
row's warm-up -- and reports the twelfth beside the eleven rather than folding
it in. What it finds on that row is in the reading.
"""
import re, glob, os, statistics, csv, sys

R = '/home/ghecht/Projects/hven/.scratch/w5t89ra5'
ARMS = ['510a4bb', '9cebbbe', 'control']
CELLS = ['f7_n1000_bound_physics', 'f7_n5000_bound_physics',
         'f7_n10000_bound_neutral', 'f7_n20000_bound_neutral']
TREAT = ['MakeParameter', 'MakeConstraint', 'RelaxBounds']
# The eleven rows 11 scored. The twelfth is reported, never folded in.
EXCLUDED = ('f7_n1000_bound_physics', 'MakeParameter')
SCORED = [(c, t) for c in CELLS for t in TREAT if (c, t) != EXCLUDED]

EVENT = re.compile(r'^\s+([\d,]+|<not supported>|<not counted>)\s+([A-Za-z0-9_.:-]+)\s*$')
ELAPSED = re.compile(r'^\s+([\d.]+) seconds time elapsed')

def parse(path):
    out = {}
    for line in open(path):
        m = EVENT.match(line.rstrip('\n'))
        if m:
            v, name = m.group(1), m.group(2)
            out[name] = None if v.startswith('<') else int(v.replace(',', ''))
            continue
        m = ELAPSED.match(line)
        if m:
            out['elapsed_s'] = float(m.group(1))
    return out

recs = []
for p in sorted(glob.glob(f'{R}/art/perf[ABCS]/*/*.txt')):
    parts = p.split('/')
    ppass = parts[-3][-1]
    arm = parts[-2]
    m = re.match(r'(.+)-(MakeParameter|MakeConstraint|RelaxBounds)-r(\d)\.txt$', parts[-1])
    cell, treat, rnd = m.group(1), m.group(2), int(m.group(3))
    for ev, val in parse(p).items():
        recs.append(dict(pass_=ppass, arm=arm, cell=cell, treatment=treat, round=rnd,
                         event=ev, value=('' if val is None else val)))

with open(f'{R}/perf.csv', 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=['pass_', 'arm', 'cell', 'treatment', 'round', 'event', 'value'])
    w.writeheader()
    for r in recs:
        w.writerow(r)
print(f"perf.csv: {len(recs)} records\n")

def med(ppass, arm, cell, treat, ev):
    vs = [r['value'] for r in recs
          if r['pass_'] == ppass and r['arm'] == arm and r['cell'] == cell
          and r['treatment'] == treat and r['event'] == ev and r['value'] != '']
    return statistics.median(vs) if vs else None

def spread(ppass, arm, cell, treat, ev):
    vs = [r['value'] for r in recs
          if r['pass_'] == ppass and r['arm'] == arm and r['cell'] == cell
          and r['treatment'] == treat and r['event'] == ev and r['value'] != '']
    return (max(vs) - min(vs)) / statistics.median(vs) if vs else None

def table(ev, ppass, title, unit=1e9, fmt='9.4f'):
    print(f"=== {title} ({ev}, pass {ppass}, median of five rounds) ===")
    print(f"{'row':44s} {'parent':>10s} {'culprit':>10s} {'control':>10s} "
          f"{'c/p':>9s} {'x/p':>9s} {'p spread':>9s} {'c spread':>9s}")
    out = {}
    for c in CELLS:
        for t in TREAT:
            p = med(ppass, '510a4bb', c, t, ev)
            k = med(ppass, '9cebbbe', c, t, ev)
            x = med(ppass, 'control', c, t, ev)
            if p is None or k is None or x is None or p == 0:
                print(f"{c+'/'+t:44s} {'NOT SUPPORTED ON THIS PMU -- reported absent':>60s}")
                continue
            mark = '  (12th, NOT SCORED)' if (c, t) == EXCLUDED else ''
            print(f"{c+'/'+t:44s} {p/unit:10.4f} {k/unit:10.4f} {x/unit:10.4f} "
                  f"{k/p:9.5f} {x/p:9.5f} {spread(ppass,'510a4bb',c,t,ev):9.5f} "
                  f"{spread(ppass,'9cebbbe',c,t,ev):9.5f}{mark}")
            out[(c, t)] = (p, k, x)
    return out

A = table('instructions:u', 'A', 'INSTRUCTIONS')
print()
Ab = table('branches:u', 'A', 'BRANCHES')
print()
Ac = table('cycles:u', 'A', 'CYCLES -- INFORMATIONAL ONLY (CLAUDE.md 7)')
print()

print("=== THE VERDICT, on the eleven rows 11 scored ===")
cp = [A[k][1] / A[k][0] for k in SCORED]
xp = [A[k][2] / A[k][0] for k in SCORED]
floor = max(abs(v - 1.0) for v in xp)
print(f"control/parent floor (the instrument's own): worst |x/p - 1| = {floor:.6f}")
print(f"culprit/parent: min {min(cp):.5f}  median {statistics.median(cp):.5f}  max {max(cp):.5f}")
print(f"rows whose |c/p - 1| exceeds the floor: {sum(1 for v in cp if abs(v-1) > floor)} of {len(cp)}")
print(f"rows whose |c/p - 1| exceeds 1e-3:      {sum(1 for v in cp if abs(v-1) > 1e-3)} of {len(cp)}")
print(f"rows with FEWER instructions at the culprit: {sum(1 for v in cp if v < 1.0)} of {len(cp)}")
print(f"rows with MORE  instructions at the culprit: {sum(1 for v in cp if v > 1.0)} of {len(cp)}")
worst = max(abs(v - 1) / floor for v in cp)
print(f"largest step, in units of the floor: {worst:.1f}x")
print()
if all(v < 1.0 for v in cp) and min(abs(v-1) for v in cp) > max(floor, 1e-3):
    print("WHOLE-PROCESS READING: instructions are DOWN at the culprit on EVERY scored row,")
    print("reproducibly and far outside the floor. NOT WORK-MOVED on its face -- but this is")
    print("NOT A WORK READING, and the leg says so rather than banking it: 17 % (n20000) to")
    print("96 % (n1000) of each of these counts is MKL's dsecnd() first-call clock")
    print("calibration, a WALL-TIMED busy-wait whose instruction count measures how fast that")
    print("loop's own code runs, not any work the solver does. See the pass S block below and")
    print("record_analyze.out.")
elif all(abs(v-1) <= 1e-4 for v in cp):
    print("WHOLE-PROCESS READING: identical within 1e-4.")
else:
    print("WHOLE-PROCESS READING: see the reading.")
print()
print("THE VERDICT ITSELF IS record_analyze.out's, on the two n20000 rows where the row's own")
print("work is the majority of the profile: with the calibration set aside INSIDE each")
print("profile, the culprit/parent ratio reads 0.99452 and 0.99696 against a control/parent")
print("floor of 0.99863 and 0.99980, and the three rounds' values INTERLEAVE between the two")
print("arms. NOT WORK-MOVED: 9cebbbe's solve does not execute more instructions -- it executes")
print("the same, to within 0.3-0.6 %, and if anything marginally fewer. 11.1's LAYOUT-MOVED")
print("band (identical within 1e-4) is NOT reached either, so no 11.1 label is claimed.")
print()

for ev in ['de_dis_uop_queue_empty_di0:u', 'op_cache_hit_miss.op_cache_miss:u',
           'op_cache_hit_miss.op_cache_hit:u', 'ic_fetch_stall.ic_stall_any:u']:
    table(ev, 'B', f'PASS B {ev}')
    print()
for ev in ['L1-dcache-load-misses:u', 'LLC-load-misses:u', 'dTLB-load-misses:u', 'page-faults:u']:
    table(ev, 'C', f'PASS C {ev}', unit=1e6)
    print()

print("=== PASS S -- THE SUBTRAHEND, AND WHY DIFFERENCING IS REFUSED ===")
print("hs071_x1_fixed is four variables dense and costs 0.00018 s of solve inside a warm leg,")
print("so a single-row hs071 process is very nearly the per-process constant alone.")
sub = {}
for arm in ARMS:
    vs = [r['value'] for r in recs if r['pass_'] == 'S' and r['arm'] == arm
          and r['event'] == 'instructions:u' and r['value'] != '']
    sub[arm] = statistics.median(vs)
    print(f"  {arm:9s} median {sub[arm]/1e9:8.4f} e9   min {min(vs)/1e9:8.4f}   max {max(vs)/1e9:8.4f}"
          f"   spread {(max(vs)-min(vs))/sub[arm]*100:6.3f} %")
print()
print("The naive repair -- subtract it, row by row -- would read:")
print(f"{'row':44s} {'p-sub e9':>9s} {'c-sub e9':>9s} {'(c-sub)/(p-sub)':>16s}")
for k in SCORED:
    p_, c_, x_ = A[k]
    print(f"{k[0]+'/'+k[1]:44s} {(p_-sub['510a4bb'])/1e9:9.4f} {(c_-sub['9cebbbe'])/1e9:9.4f}"
          f" {(c_-sub['9cebbbe'])/(p_-sub['510a4bb']):16.5f}")
print()
print("THAT DIFFERENCE IS NOT TAKEN AS A MEASUREMENT, and the reason is measured, not feared.")
print("The per-process constant is MKL's dsecnd() first-call clock calibration -- a WALL-TIMED")
print("busy-wait (hven calls it deliberately at src/drivers/solver_init.cpp:27, through")
print("InteriorPointSolver::ensure_solver_initialized, src/drivers/interior_point_solver.cpp:798")
print("and :5535). Its INSTRUCTION count is not a constant: the sampled profiles put it at")
print("2.9 e9 of an n20000 process at the same arm whose hs071 process reads 4.6 e9, and one")
print("f7_n1000_bound_physics/MakeParameter sample read 0.9 e9. A term that moves by 5x is not")
print("a term to subtract. reading.md 5 (iv) refused a differencing instrument for a related")
print("reason and this leg refuses this one; what stands in its place is the PER-SYMBOL")
print("measurement in record_analyze.out, which sets the calibration aside INSIDE each")
print("profile and so assumes nothing about its size.")
print()
print("=== WALL, INFORMATIONAL ONLY -- never asserted (CLAUDE.md 7) ===")
table('elapsed_s', 'A', 'elapsed seconds, pass A', unit=1.0)
