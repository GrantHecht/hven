#!/usr/bin/env python3
"""W5 T8.9r-attrib3 -- every mechanism table in section B of mechanism.md, from this
directory's own raw/ and perf/.

Run from the artifact directory:  python3 mechanism_tables.py [-|out.txt]

  B1  whole-process perf stat across all six bisect arms, with the TWO ZERO CONTROLS
      that measure this instrument's floor
  B2  the cycle profile, parent against culprit, by symbol and by group
  B3  minor page faults and kernel time -- under perf (the mem leg) and, separately,
      in the WALL LEG'S OWN CONDITION (the wallpf leg)
  B4  experiment 4: the allocator intervention, applied identically to both arms
"""
import collections, csv, math, os, re, statistics as st, sys

HERE = os.path.dirname(os.path.abspath(__file__))
def rel(*p): return os.path.join(HERE, *p)

def counts(p):
    out = {}
    for line in open(p):
        m = re.match(r"\s*([\d,]+)\s+(\S+)", line)
        if m and ":" in m.group(2):
            out[m.group(2)] = int(m.group(1).replace(",", ""))
    return out

def timesplit(p):
    t = open(p).read()
    g = lambda pat: float(re.search(pat, t).group(1))
    return g(r"([\d.]+) seconds time elapsed"), g(r"([\d.]+) seconds user"), g(r"([\d.]+) seconds sys")

def wall_rows(p):
    ls = [l for l in open(p) if not l.startswith("#")]
    return [(r["cell_id"], float(r["wall_s"])) for r in csv.DictReader(ls)]

def main():
    out = sys.stdout if len(sys.argv) < 2 or sys.argv[1] == "-" else open(sys.argv[1], "w")
    w = out.write

    # ------------------------------------------------------------------ B0
    w("="*104+"\nB0. THE STEP ON EVERY ROW BOTH ARMS WRITE, INCLUDING THE SEVEN UNCONDITIONAL ONES -- is the\n")
    w("    extra time a FIXED per-call cost, or does it scale with what the row does?\n"+"="*104+"\n\n")
    med0 = {}
    order0 = [k for k,_ in wall_rows(rel("raw","wall","w-b01-r1.csv"))]
    for a in ("02","03"):
        dd = {}
        for r in (1,2,3,4,5):
            for k,s_ in wall_rows(rel("raw","wall","w-b%s-r%d.csv" % (a,r))): dd.setdefault(k,[]).append(s_)
        for k in order0: med0[(a,k)] = st.median(dd[k])
    w("  %-44s%16s%16s%9s%14s\n" % ("row","parent wall_s","culprit wall_s","step","delta (ms)"))
    for k in order0:
        pa, cu = med0[("02",k)], med0[("03",k)]
        w("  %-44s%16.6f%16.6f%9.4f%14.2f\n" % (k,pa,cu,cu/pa,(cu-pa)*1000))
    tp = sum(med0[("02",k)] for k in order0); tc = sum(med0[("03",k)] for k in order0)
    w("  %-44s%16.6f%16.6f%9.4f%14.2f\n" % ("TOTAL over all nineteen rows",tp,tc,tc/tp,(tc-tp)*1000))
    w("\n  The step is between 1.02 and 1.04 on the eleven big rows and 1.05-1.11 on the four tiny\n")
    w("  ones, but the ABSOLUTE delta runs from 0.01 ms on a 0.13 ms row to 36.5 ms on a 1101 ms\n")
    w("  row. It scales with what the row does. A fixed per-call cost would look like the opposite.\n\n")

    # ------------------------------------------------------------------ B1
    ARMS = [("01","T8.3 510a4bb"),("02","8ae1618"),("03","9cebbbe"),
            ("04","9ce9bb2"),("05","fix1 5124aa1"),("06","T8.4 3c8e43b")]
    EV = ["instructions:u","cycles:u","branches:u","branch-misses:u","L1-icache-load-misses:u"]
    d = {}
    for a,_ in ARMS:
        for r in (1,2,3):
            for k,v in counts(rel("perf","stat","S-b%s-r%d.txt" % (a,r))).items():
                d.setdefault((a,k),[]).append(v)
    w("="*104+"\nB1. WHOLE-PROCESS perf stat ACROSS THE SIX BISECT ARMS -- three rounds, the wall pin's\n")
    w("    invocation with perf attached. b01->b02 and b05->b06 are BYTE-IDENTICAL executables,\n")
    w("    so their steps are this instrument's floor and can be nothing else.\n"+"="*104+"\n\n")
    w("  arm  commit          " + "".join(e.replace(":u","").rjust(22) for e in EV) + "      IPC\n")
    for a,l in ARMS:
        m = {e: st.median(d[(a,e)]) for e in EV}
        w("  b%s   %-14s" % (a,l) + "".join("{:>22,}".format(int(m[e])) for e in EV)
          + "   %.4f\n" % (m["instructions:u"]/m["cycles:u"]))
    w("\n  consecutive steps (median of three):\n")
    prev = None
    for a,l in ARMS:
        m = {e: st.median(d[(a,e)]) for e in EV}
        if prev:
            tag = "  <-- ZERO CONTROL (identical bytes)" if a in ("02","06") else ""
            w("    %-20s" % l + "".join("%22.6f" % (m[e]/prev[e]) for e in EV) + tag + "\n")
        prev = m
    w("\n  round-to-round spread per arm (max/min - 1):\n")
    for a,l in ARMS:
        w("    b%s %-14s instructions %.2e   cycles %.2e\n"
          % (a,l,max(d[(a,"instructions:u")])/min(d[(a,"instructions:u")])-1,
             max(d[(a,"cycles:u")])/min(d[(a,"cycles:u")])-1))
    ci = st.median(d[("02","instructions:u")]) / st.median(d[("01","instructions:u")]) - 1
    cc = st.median(d[("02","cycles:u")]) / st.median(d[("01","cycles:u")]) - 1
    ti = st.median(d[("03","instructions:u")]) / st.median(d[("02","instructions:u")]) - 1
    tc = st.median(d[("03","cycles:u")]) / st.median(d[("02","cycles:u")]) - 1
    w("\n  VERDICT: the b01->b02 zero control moves instructions by %+.2f %% and cycles by %+.2f %%\n" % (ci*100, cc*100))
    w("  with a true step of EXACTLY ZERO, while the pair under test moves them %+.2f %% and\n" % (ti*100))
    w("  %+.2f %%; every arm's own round-to-round spread is 0.1-6 %%. The whole-process instruction\n" % (tc*100))
    w("  and cycle counts RESOLVE NOTHING on this leg -- the floor is larger than the step. This\n")
    w("  reproduces reading.md section 11's finding on a control stronger than T8.1's: not merely\n")
    w("  a byte-identical libhven.a, a byte-identical BINARY.\n")

    # ------------------------------------------------------------------ B2
    TIMING = {"difftime","difftime@plt","mkl_serv_get_clocks_frequency","__vdso_time",
              "mkl_serv_cpuisitbarcelona","mkl_serv_cpuiszen","time@plt"}
    def load(s,r):
        o = {}
        for line in open(rel("perf","rec","rep-%s-r%d.txt" % (s,r))):
            m = re.match(r"\s*([\d.]+)%\s+(\S+)\s+\[.\]\s+(.*)$", line.rstrip())
            if m: o[(m.group(2), m.group(3).strip())] = o.get((m.group(2), m.group(3).strip()),0.0)+float(m.group(1))
        return o
    A = {(s,r): load(s,r) for s in ("8ae1618","9cebbbe") for r in (1,2,3)}
    w("\n"+"="*104+"\nB2. THE CYCLE PROFILE, PARENT AGAINST CULPRIT -- perf record -e cycles:u -F 997, three\n")
    w("    rounds each, alternating, the wall pin's four-cell invocation.\n"+"="*104+"\n\n")
    syms = set()
    for k in A: syms |= set(A[k])
    rows = sorted(((st.median([A[("9cebbbe",r)].get(k,0.0) for r in (1,2,3)])
                    - st.median([A[("8ae1618",r)].get(k,0.0) for r in (1,2,3)]),
                    st.median([A[("8ae1618",r)].get(k,0.0) for r in (1,2,3)]),
                    st.median([A[("9cebbbe",r)].get(k,0.0) for r in (1,2,3)]), k) for k in sorted(syms)),
                  key=lambda x: (-abs(x[0]), x[3]))
    w("  the fifteen largest share deltas:\n")
    w("  %9s%9s%10s  symbol\n" % ("d(share)","parent%","culprit%"))
    for dd,a,b,k in rows[:15]:
        w("  %9.3f%9.3f%10.3f  [%s] %s\n" % (dd,a,b,k[0],k[1][:78]))
    def grp(k, excl):
        dso,sym = k
        if sym in TIMING: return "MKL clock calibration"
        if dso != "hven_sqp_corpus": return dso
        if sym.startswith(("mkl_","piv_","L_not_one","ldindx")) or "pardiso" in sym: return "MKL kernels (in-binary)"
        if "Eigen::" in sym: return "Eigen"
        if "hven::" in sym: return "hven::"
        return "other-in-binary"
    for label, excl in (("RAW SHARES", False),
                        ("RENORMALISED with the clock-calibration family removed", True)):
        w("\n  %s\n" % label)
        g = collections.defaultdict(lambda: [[],[]])
        for i,s in enumerate(("8ae1618","9cebbbe")):
            for r in (1,2,3):
                dd = A[(s,r)]
                tot = sum(v for k,v in dd.items() if not (excl and k[1] in TIMING))
                sub = collections.defaultdict(float)
                for k,v in dd.items():
                    if excl and k[1] in TIMING: continue
                    sub[grp(k,excl)] += v/tot*100
                for k,v in sub.items(): g[k][i].append(v)
        w("    %-28s%9s%10s%8s\n" % ("group","parent%","culprit%","delta"))
        for k,(a,b) in sorted(g.items(), key=lambda x: -(st.median(x[1][1]) if x[1][1] else 0)):
            ma = st.median(a) if a else 0.0; mb = st.median(b) if b else 0.0
            w("    %-28s%9.2f%10.2f%8.2f\n" % (k,ma,mb,mb-ma))

    # ------------------------------------------------------------------ B3
    w("\n"+"="*104+"\nB3. MINOR PAGE FAULTS AND KERNEL TIME\n"+"="*104+"\n\n")
    w("  (a) the mem leg -- under `perf stat`, six rounds, with a BYTE-IDENTICAL control pair:\n\n")
    w("    %-20s%14s%14s%12s\n" % ("arm","minor faults","median","sys (s)"))
    memmed = {}
    for a,l in (("g1","ctrlA 510a4bb"),("g2","ctrlB 8ae1618"),("g3","culprit 9cebbbe")):
        pf = [counts(rel("perf","mem","M-%s-r%d.txt" % (a,r)))["page-faults:u"] for r in range(1,7)]
        sy = [timesplit(rel("perf","mem","M-%s-r%d.txt" % (a,r)))[2] for r in range(1,7)]
        memmed[a] = (st.median(pf), st.median(sy))
        w("    %-20s%14s%14s%12.4f\n" % (l, "%d..%d" % (min(pf),max(pf)), "{:,}".format(int(st.median(pf))), st.median(sy)))
    w("\n    control step (identical bytes): faults x%.6f, sys x%.4f\n"
      % (memmed["g2"][0]/memmed["g1"][0], memmed["g2"][1]/memmed["g1"][1]))
    w("    CULPRIT step:                   faults x%.6f (%+d), sys x%.4f (%+.4f s)\n"
      % (memmed["g3"][0]/memmed["g2"][0], memmed["g3"][0]-memmed["g2"][0],
         memmed["g3"][1]/memmed["g2"][1], memmed["g3"][1]-memmed["g2"][1]))
    w("\n  (b) the wallpf leg -- NO perf, the wall pin's invocation byte for byte, the fault count\n")
    w("      read from /usr/bin/time's %R (the format string is /usr/bin/time's argv, not the\n")
    w("      child's, so the measured process sees exactly what it sees in a wall batch):\n\n")
    pf = {}; sy = {}; ur = {}
    for a,l in (("h1","parent 8ae1618"),("h2","culprit 9cebbbe")):
        v = []; s = []; u = []
        for r in (1,2,3,4,5):
            t = open(rel("logs","F%d-wallpf-r%d.log" % (r,r))).read()
            m = re.search(r"tag=wallpf-r%d/%s user=([\d.]+) sys=([\d.]+) real=([\d.]+) minf=(\d+)" % (r,a), t)
            u.append(float(m.group(1))); s.append(float(m.group(2))); v.append(int(m.group(4)))
        pf[a] = v; sy[a] = s; ur[a] = u
        w("    %-20s faults %s  median %s   sys median %.3f   user median %.3f\n"
          % (l, v, "{:,}".format(int(st.median(v))), st.median(s), st.median(u)))
    w("\n    CULPRIT step in the WALL condition: faults x%.4f (%+d), sys %+.3f s, user %+.3f s\n"
      % (st.median(pf["h2"])/st.median(pf["h1"]), st.median(pf["h2"])-st.median(pf["h1"]),
         st.median(sy["h2"])-st.median(sy["h1"]), st.median(ur["h2"])-st.median(ur["h1"])))
    w("    %d extra first-touched 4 KiB pages = %.1f MiB per process.\n"
      % (st.median(pf["h2"])-st.median(pf["h1"]), (st.median(pf["h2"])-st.median(pf["h1"]))*4096/2**20))

    # ------------------------------------------------------------------ B4
    w("\n"+"="*104+"\nB4. EXPERIMENT 4 -- the allocator intervention, applied IDENTICALLY to both arms\n")
    w("="*104+"\n\n")
    AR = [("d1","default / parent"),("d2","default / culprit"),
          ("b1","bigthresh / parent"),("b2","bigthresh / culprit")]
    med = {}; order = None
    for a,_ in AR:
        dd = {}
        for r in (1,2,3,4,5):
            rr = wall_rows(rel("raw","awall","a-%s-r%d.csv" % (a,r)))
            if order is None: order = [k for k,_ in rr][:12]
            for k,s in rr: dd.setdefault(k,[]).append(s)
        for k in order: med[(a,k)] = st.median(dd[k])
    scored = order[1:]
    tot = lambda a: sum(med[(a,k)] for k in scored)
    fault = {}
    for a,_ in AR:
        v = []
        for r in (1,2,3,4,5):
            t = open(rel("logs","A%d-alloc-r%d.log" % (r,r))).read()
            m = re.search(r"tag=alloc-r%d/%s user=([\d.]+) sys=([\d.]+) real=([\d.]+) minf=(\d+)" % (r,a), t)
            v.append((int(m.group(4)), float(m.group(2))))
        fault[a] = (st.median([x[0] for x in v]), st.median([x[1] for x in v]))
    w("  %-22s%18s%16s%14s\n" % ("arm","scored corpus s","minor faults","sys (s)"))
    for a,l in AR:
        w("  %-22s%18.6f%16s%14.3f\n" % (l, tot(a), "{:,}".format(int(fault[a][0])), fault[a][1]))
    sd = tot("d2")/tot("d1"); sb = tot("b2")/tot("b1")
    w("\n  step under the PIN's environment      %.4f  (%+.3f %%)   faults %+d\n"
      % (sd,(sd-1)*100, fault["d2"][0]-fault["d1"][0]))
    w("  step with the intervention            %.4f  (%+.3f %%)   faults %+d\n"
      % (sb,(sb-1)*100, fault["b2"][0]-fault["b1"][0]))
    w("  FRACTION OF THE STEP THE INTERVENTION REMOVES: %.1f %%\n" % ((1-math.log(sb)/math.log(sd))*100)) 
    w("  fraction of the extra faults it removes:       %.1f %%\n"
      % ((1-(fault["b2"][0]-fault["b1"][0])/(fault["d2"][0]-fault["d1"][0]))*100))
    w("\n  and what it does to each arm on its own (bigthresh/default): parent %.4f, culprit %.4f\n"
      % (tot("b1")/tot("d1"), tot("b2")/tot("d2")))
    w("\n  per-row, both modes:\n")
    w("  %-44s%16s%18s\n" % ("row","default step","bigthresh step"))
    for k in scored:
        w("  %-44s%16.4f%18.4f\n" % (k, med[("d2",k)]/med[("d1",k)], med[("b2",k)]/med[("b1",k)]))
    w("\nAPPLE / ACCELERATE: UNOBSERVED.   WINDOWS: UNOBSERVED.\n")
    if out is not sys.stdout: out.close()
    return 0

if __name__ == "__main__":
    sys.exit(main())
