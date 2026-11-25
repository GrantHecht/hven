# probe summary for v1 (one exit line per process) and v2 (tick/hit/exit lines; the last line per pid is cumulative)
import sys,re,collections
for f in sys.argv[1:]:
    last={}; cmds={}; tags=collections.defaultdict(set)
    for l in open(f):
        if not l.startswith('R11 '): continue
        head,_,cmd=l.partition(' cmd=')
        kv=dict(re.findall(r'(\w+)=(\S+)', head))
        pid=kv.pop('pid'); tag=kv.pop('tag','exit')
        tags[pid].add(tag.split(':')[0])
        last[pid]={k:int(v) for k,v in kv.items()}; cmds[pid]=cmd.strip()
    tot=collections.Counter()
    for d in last.values(): tot.update(d)
    print(f"== {f}: {len(last)} processes; exited normally {sum('exit' in t for t in tags.values())}")
    print('  '+' '.join(f"{k}={v}" for k,v in tot.items()))
    for pid,d in last.items():
        bad={k:v for k,v in d.items() if v and any(s in k for s in ('disagree','differ','nonfinite','wrongsign','small_zero','mag_diff','det_zero','blocks2'))}
        if bad:
            m=re.search(r'(--internal-run-one|--cells|--problem) (\S+)', cmds[pid]); print('  CELL', m.group(2) if m else cmds[pid][-80:], bad)
