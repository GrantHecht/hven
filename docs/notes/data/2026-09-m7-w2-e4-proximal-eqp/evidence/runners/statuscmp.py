#!/usr/bin/env python3
"""Status/minor comparison across corpus tags. usage: statuscmp.py ALG TAG1 TAG2 [TAG3...]"""
import re,sys,os
S=os.path.join(os.path.dirname(os.path.abspath(__file__)),'corpus')
N={'0':'opt','1':'cap','2':'inf','3':'num'}
def load(t,alg):
    out={}
    for lst in ('old36','new15'):
        f=f'{S}/{t}-{alg}-{lst}.txt'
        if not os.path.exists(f): continue
        for line in open(f):
            p=line.split()
            if not p: continue
            m=re.search(r'dumps/([^/]+)/(qp\d+)\.txt',p[0])
            if m: k=m.group(1)+'/'+m.group(2)
            else:
                m=re.search(r'(Cs_STEENBRB_qp\d)\.txt',p[0])
                if not m: continue
                k='R1_'+m.group(1)
            out[k]=dict(x.split('=',1) for x in p[1:] if '=' in x)
    return out
alg=sys.argv[1]; tags=sys.argv[2:]
d=[load(t,alg) for t in tags]
keys=sorted(set().union(*[set(x) for x in d]))
def s(dd,k):
    if k not in dd: return "-"
    return f"{N.get(dd[k].get('status'),dd[k].get('status','?'))}/{dd[k].get('minors','?')}"
hdr=f"{'cell':26}"+"".join(f"{t:>16}" for t in tags)
print(hdr); print('-'*len(hdr))
nm=0
for k in keys:
    row=[s(x,k) for x in d]
    if len(set(row))>1:
        nm+=1
        print(f"{k:26}"+"".join(f"{r:>16}" for r in row))
print(f"-- {nm} of {len(keys)} cells differ in status/minors ({alg})")
