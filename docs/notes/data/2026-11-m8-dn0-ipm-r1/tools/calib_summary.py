#!/usr/bin/env python3
"""M8 DN-0 IPM r1: summarize one calibrate.sh output dir. Per cell x period, per run:
L=LOST records, T=THROTTLE records, S=samples, pk=peak samples in any 1 ms window (throttle limit 79),
main=share of chains reaching main, unm=unmatched share of solver, sum=sampled instructions / perf stat.
Usage: calib_summary.py CALIB_DIR TOOLS_DIR"""
import glob,re,collections,sys,json,subprocess,os
d=sys.argv[1]; tools=sys.argv[2]
rows=collections.defaultdict(list)
for f in sorted(glob.glob(d+"/*.stats")):
    c,p,k=re.match(r".*/(.*)\.p(\d+)\.k(\d)\.stats",f).groups()
    s=open(f).read()
    def g(key):
        m=re.search(rf"^\s*{key} events:\s+(\d+)",s,re.M); return int(m.group(1)) if m else 0
    ts=[float(x.split()[0].rstrip(':')) for x in open(f[:-6]+".times")]
    pk=0;j=0
    for i in range(len(ts)):
        while ts[i]-ts[j]>0.001: j+=1
        pk=max(pk,i-j+1)
    extra=""
    sg=f[:-6]+".script.gz"
    if os.path.exists(sg):
        a=json.loads(subprocess.run(["python3",tools+"/attribute.py",tools+"/bucket_map.tsv",sg],capture_output=True,text=True).stdout)
        st=int(open(f"{d}/{c}.stat").read().strip().split("\n")[-1].split(",")[0])
        extra=f"/main{a['chains_reaching_main']:.2f}/unm{a['unmatched_share_of_solver']:.3f}/sum{a['instr_sampled_all']/st:.3f}"
    rows[(c,int(p))].append(f"L{g('LOST')}/T{g('THROTTLE')}/S{g('SAMPLE')}/pk{pk}{extra}")
for (c,p),v in sorted(rows.items()):
    print(c,p," ".join(v))
