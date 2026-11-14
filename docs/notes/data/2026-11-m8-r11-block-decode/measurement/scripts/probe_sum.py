import sys,re,collections
for f in sys.argv[1:]:
    tot=collections.Counter(); n=0; hits=[]
    for l in open(f):
        if not l.startswith('R11 '): continue
        n+=1
        kv=dict(re.findall(r'(\w+)=(-?\d+)', l.split(' cmd=')[0]))
        kv.pop('pid',None)
        for k,v in kv.items(): tot[k]+=int(v)
        cmd=l.split(' cmd=')[1].strip()
        bad={k:v for k,v in kv.items() if ('disagree' in k or 'differ' in k or 'nonfinite' in k or 'wrongsign' in k or 'small_zero' in k or 'mag_diff' in k or 'det_zero' in k) and int(v)}
        if bad: hits.append((cmd[-120:],bad))
    print(f"== {f}: {n} processes")
    for k,v in tot.items(): print(f"  {k}={v}")
    for h in hits[:40]: print("  HIT", h)
