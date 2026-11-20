#!/usr/bin/env python3
"""PF-3: one cell -> one JSON.  Run with cwd = the hven-bench pf3 checkout (CUTEst paths are cwd-relative).

usage: pf3_cell.py <cpu> <lane> <problem> <mode> <outdir>      mode = full | trunc30

mode solo_wall: the SOLO WALL LEG (run only alone on the box, one cell at a time, MKL_NUM_THREADS=1): no pass A; perf record
              -e cpu-clock -F 499 (time-sampled), full run (--budget-iters 0, 300 s timeout cut by SIGINT).  Component values are
              SECONDS (samples / 499).  Deadline-class cells are cut at the timeout and flagged truncated.
mode full   : the cell runs to its own end (--budget-iters 0, the engine's own cap).
mode trunc30: DEADLINE-CLASS cell (dnf at the 60 s wall in the frozen baseline).  No pass A; one perf record pass cut at 30 s of
              wall by SIGINT, period fixed at 2e7.  Its component SHARES are reported as informational; its instruction
              total is deadline-dependent (CLAUDE.md section 7) and is never asserted or summed into an asserted figure.

Pass A  perf stat -e instructions:u            -> exact instruction total (deterministic at MKL_NUM_THREADS=1)
Pass B  perf record -e instructions:u -c P      -> P = max(500000, total // 12000); dwarf callchains, user only
Both passes run the SAME argv the serial runner builds, pinned with taskset to <cpu>.
"""
import collections, hashlib, json, os, re, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import components as C

BIN = "build/lanes/hven/hven_bench_cell"


def sha256_file(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def cell_argv(lane, problem, K, dump):
    return [BIN, "--lane", lane, "--problem", problem, "--options", "default", "--budget-iters", str(K),
            "--options-dump", dump, "--assert-reference"]


def parse_cell(out):
    f, inside = {}, False
    for line in out.splitlines():
        if line == "CELL_BEGIN":
            inside = True
        elif line == "CELL_END":
            inside = False
        elif inside and "=" in line:
            k, _, v = line.partition("=")
            f[k] = v
    return f


def main():
    cpu, lane, problem, mode, outdir = sys.argv[1:6]
    K = 0
    trunc = (mode == "trunc30")
    solo = (mode == "solo_wall")
    env = dict(os.environ, MKL_NUM_THREADS="1", OMP_NUM_THREADS="1")
    os.makedirs(outdir, exist_ok=True)
    tag = f"{lane}__{problem}"
    scratch = os.path.join(os.environ.get("TMPDIR", "."), "pf3-" + tag)
    os.makedirs(scratch, exist_ok=True)
    dump = os.path.join(scratch, "opts.txt")
    base = ["taskset", "-c", cpu]
    # pass A
    wallA = None
    if trunc or solo:
        fields, total, period = {}, None, 20_000_000
    else:
        t0 = time.monotonic()
        a = subprocess.run(base + ["timeout", "600", "perf", "stat", "-x,", "-e", "instructions:u", "--"] + cell_argv(lane, problem, K, dump),
                           capture_output=True, text=True, env=env)
        wallA = time.monotonic() - t0
        fields = parse_cell(a.stdout)
        total = None
        for line in a.stderr.splitlines():
            p = line.split(",")
            if len(p) > 2 and p[2].startswith("instructions"):
                total = int(p[0])
        if total is None or not fields:
            json.dump({"cell": tag, "error": "passA", "rc": a.returncode, "stderr": a.stderr[-2000:]}, open(f"{outdir}/{tag}.json", "w"))
            return 1
        period = max(500_000, total // 12000)
    data = os.path.join(scratch, "perf.data")
    evsel = ["-e", "cpu-clock", "-F", "499"] if solo else ["-e", "instructions:u", "-c", str(period)]
    rec = ["perf", "record", "-q", "--per-thread", "-m", "4M"] + evsel + ["--user-callchains",
           "--call-graph", "dwarf,16384", "-o", data, "--"] + cell_argv(lane, problem, K, dump)
    if trunc:
        rec = ["timeout", "-s", "INT", "30"] + rec
    if solo:
        rec = ["timeout", "-s", "INT", "300"] + rec
    if not trunc and not solo:
        rec = ["timeout", "-s", "INT", "900"] + rec
    b = subprocess.run(base + rec, capture_output=True, text=True, env=env)
    fieldsB = parse_cell(b.stdout)
    if (b.returncode not in (0, 3, 124, 130)) or not os.path.exists(data):
        json.dump({"cell": tag, "error": "passB", "rc": b.returncode, "stderr": b.stderr[-2000:]}, open(f"{outdir}/{tag}.json", "w"))
        return 1
    sp = subprocess.Popen(["perf", "script", "-i", data, "-F", "ip,sym,dso", "--no-inline"],
                          stdout=subprocess.PIPE, text=True, errors="replace")
    bcall = collections.defaultdict(collections.Counter); lparent = collections.Counter()
    famcomp = collections.defaultdict(collections.Counter); trunc_fam = collections.Counter()
    comp = collections.Counter(); leafs = collections.Counter(); leaf_comp = {}
    hfr = collections.Counter(); ov = collections.Counter(); n = 0; depth_sum = 0
    frame_re = re.compile(r"^\s*[0-9a-f]+ (.*) \((.*)\)$")
    buf = []

    def flush():
        nonlocal n, depth_sum
        if not buf:
            return
        fr = []
        for l in buf:
            m = frame_re.match(l)
            if m:
                fr.append((m.group(1), os.path.basename(m.group(2))))
        buf.clear()
        if not fr:
            return
        n += 1; depth_sum += len(fr)
        c = C.classify(fr)
        comp[c] += 1
        if c.startswith("unattributed"):
            fam = C.blas_family(fr[0][0])
            if fam:
                trunc_fam[fam] += 1
        else:
            fams = {C.blas_family(s) for s, d in fr} - {None}
            for fam in fams:
                famcomp[fam][c] += 1
        lsym = fr[0][0][:120]
        # who drives the backend: the first hven:: frame above the linear/KKT glue (backend components only)
        if c.startswith("la."):
            i0 = next((i for i, (s, d) in enumerate(fr) if re.search(r"FactorSession::|DenseSession::|DenseSymmetricFactor::", s)), None)
            if i0 is not None:
                for s, d in fr[i0:]:
                    if s.startswith("hven::") and not re.search(r"hven::linear::|solve_vec|KktFactor|factorize_checked|DenseShiftSite|hven::detail::solve_", s):
                        bcall[c][re.sub(r"\(.*", "", s)[:100]] += 1
                        break
        if not fr[0][0].startswith("hven::"):
            h = next((re.sub(r"\(.*", "", s)[:90] for s, d in fr if s.startswith("hven::")), "-")
            lparent[(re.sub(r"(_lp64)?_(def|avx2|avx512\w*)_", "_", fr[0][0])[:70], h)] += 1
        leafs[(lsym, fr[0][1])] += 1
        leaf_comp[(lsym, fr[0][1])] = c
        if C.ALLOC_RE.match(fr[0][0]):
            ov["alloc_leaf"] += 1; ov["alloc_leaf." + c] += 1
        if fr[0][1].startswith("libiomp5"):
            ov["iomp_leaf"] += 1; ov["iomp_leaf." + c] += 1
        if fr[0][1].startswith("libpb") or fr[0][1].startswith("libcutest") or fr[0][1].startswith("libgfortran"):
            ov["fortran_leaf"] += 1
        seen = set()
        for s, d in fr:
            if s.startswith("hven::") or s.startswith("mkl_pds") and False:
                key = re.sub(r"\(.*", "", s)[:110]
                if key not in seen:
                    seen.add(key); hfr[key] += 1
        if c.startswith("unattributed"):
            ov["unattr_leaf:" + lsym[:80]] += 1

    for line in sp.stdout:
        if line.strip() == "":
            flush()
        else:
            buf.append(line)
    flush()
    sp.wait()
    if n == 0:
        json.dump({"cell": tag, "error": "zero samples", "rc": b.returncode, "stderr": b.stderr[-2000:]}, open(f"{outdir}/{tag}.json", "w"))
        return 1
    if solo:
        period = 1.0 / 499.0   # values are seconds
    est = n * period
    if total is None:
        total = est
    out = {
        "cell": tag, "lane": lane, "problem": problem, "mode": mode, "truncated": trunc, "budget_iters_arg": int(K),
        "instructions_total": total, "period": period, "samples": n, "unit": ("seconds" if solo else "instructions"), "sample_est_total": est,
        "est_over_total": est / total, "instructions_asserted": (not trunc and not solo), "solo_wall": solo, "mean_chain_depth": depth_sum / max(n, 1),
        "wall_passA_s_informational": wallA, "cpu": int(cpu),
        "status": fields.get("status"), "outcome": fields.get("outcome"), "iterations": fields.get("iterations"),
        "factorizations": fields.get("factorizations"),
        "passB_status_matches": (trunc or fieldsB.get("status") == fields.get("status") and fieldsB.get("iterations") == fields.get("iterations")
                                 and fieldsB.get("factorizations") == fields.get("factorizations")),
        "fields": fields if fields else fieldsB,
        "components": {k: v * period for k, v in comp.items()},
        "overlays": {k: v * period for k, v in ov.items() if not k.startswith("unattr_leaf:")},
        "unattributed_leafs": {k[len("unattr_leaf:"):]: v * period for k, v in ov.items() if k.startswith("unattr_leaf:")},
        "blas_family_attributed": {f: {k: v * period for k, v in cc.items()} for f, cc in famcomp.items()},
        "blas_family_truncated": {f: v * period for f, v in trunc_fam.items()},
        "backend_callers": {k: {a: v * period for a, v in cc.most_common(30)} for k, cc in bcall.items()},
        "leaf_by_hven_caller": [[a, b, v * period] for (a, b), v in lparent.most_common(80)],
        "top_leaf": [[s, d, leaf_comp[(s, d)], v * period] for (s, d), v in leafs.most_common(60)],
        "hven_frames_inclusive": [[k, v * period] for k, v in hfr.most_common(80)],
        "components_sha256": sha256_file(os.path.join(os.path.dirname(os.path.abspath(__file__)), "components.py")),
    }
    json.dump(out, open(f"{outdir}/{tag}.json", "w"))
    for p in (data, dump):
        try: os.remove(p)
        except OSError: pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
