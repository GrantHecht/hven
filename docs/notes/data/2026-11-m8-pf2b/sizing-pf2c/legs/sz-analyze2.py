import sys, math, collections
O=sys.argv[1]
def dl(t): return {k:float(v) for k,v in (x.split('=') for x in t[1:])}
for c in sys.argv[2:]:
    print('=====',c)
    seq=[]  # events
    P=[]
    for line in open(f'{O}/{c}.log'):
        t=line.split()
        if not t: continue
        if t[0]=='D': seq.append(('D',dl(t)))
        elif t[0]=='M': seq.append(('M',None))
        elif t[0]=='P':
            d={'kind':t[1]}
            for x in t[2:]:
                k,v=x.split('='); d[k]=v if k=='out' else float(v)
            P.append(d)
    # pair test -> cert
    pairs=[]; orphan_cert=0
    for i,(k,d) in enumerate(seq):
        if k=='D' and d['tgt0']==1:
            if i>0 and seq[i-1][0]=='D' and seq[i-1][1]['tgt0']==0: pairs.append((seq[i-1][1],d))
            else: orphan_cert+=1
    print(' cert calls paired with a test:',len(pairs),' unpaired cert calls (from entry detector/other):',orphan_cert)
    flips=[(t,cc) for t,cc in pairs if cc['rho']<=1e-6]
    print(' cert re-read flips to dependent (rho_cert<=1e-6, before rounding-floor allowance):',len(flips),' of',len(pairs),' (%.1f%%)'%(100*len(flips)/max(1,len(pairs))))
    # by rho1 half-decade
    def hd(x): return round(math.floor(2*math.log10(x))/2,1) if x>0 else -99
    b=collections.defaultdict(lambda:[0,0])
    for t,cc in pairs:
        b[hd(t['rho1'])][0]+=1
        if cc['rho']<=1e-6: b[hd(t['rho1'])][1]+=1
    print(' pairs/flips by rho1 half-decade:',' '.join(f'1e{k}:{v[0]}/{v[1]}' for k,v in sorted(b.items())))
    # cert vs first-solve: the cert's own rho1 (first solve of cert) predicts the flip?
    b2=collections.defaultdict(lambda:[0,0])
    for t,cc in pairs:
        b2[hd(cc['rho1'])][0]+=1
        if cc['rho']<=1e-6: b2[hd(cc['rho1'])][1]+=1
    # test rho (post-refine) bins
    b3=collections.defaultdict(lambda:[0,0])
    for t,cc in pairs:
        b3[hd(t['rho'])][0]+=1
        if cc['rho']<=1e-6: b3[hd(t['rho'])][1]+=1
    print(' pairs/flips by test final rho half-decade:',' '.join(f'1e{k}:{v[0]}/{v[1]}' for k,v in sorted(b3.items())))
    print(' cert rho1 half-decade pairs/flips:',' '.join(f'1e{k}:{v[0]}/{v[1]}' for k,v in sorted(b2.items())))
    # converged status of the test among pairs
    print(' test converged among pairs:',sum(1 for t,cc in pairs if t['conv']==1),'of',len(pairs),' test solves in pairs',int(sum(t['solves'] for t,cc in pairs)),' cert solves',int(sum(cc['solves'] for t,cc in pairs)))
    # polish predictors
    first=[]; prevout=None; trans=collections.Counter()
    for p in P:
        if p['step']==0:
            first.append(p)
    for a,b_ in zip(first,first[1:]):
        trans[(a['out'],b_['out'])]+=1
    print(' first-step outcome transitions (prev->next):',dict(trans))
    kept=[p for p in first if p['out']=='kept']; cap=[p for p in first if p['out']=='cap']
    def capabs(p): return 1e-6*max(1.0,p['y'])
    for T in (1,0.1,0.01,1e-3):
        print('  skip if res0 > %g*cap_abs: caps skipped %d of %d (%.1f%%), kept lost %d of %d'%(T,sum(1 for p in cap if p['res0']>T*capabs(p)),len(cap),100*sum(1 for p in cap if p['res0']>T*capabs(p))/max(1,len(cap)),sum(1 for p in kept if p['res0']>T*capabs(p)),len(kept)))
    ra=sorted(p['delta']/p['res0'] for p in kept if p['res0']>0)
    rc=sorted(p['delta']/p['res0'] for p in cap if p['res0']>0)
    if ra and rc:
        q=lambda a,f:a[int(f*(len(a)-1))]
        print('  amplification delta/res0: kept median %.2e p90 %.2e max %.2e | cap min %.2e p10 %.2e median %.2e'%(q(ra,.5),q(ra,.9),ra[-1],rc[0],q(rc,.1),q(rc,.5)))
    # backoff simulation: after K consecutive caps, skip next S polishes (assume skipped ones would be same outcome as sequence), measure lost kept
    for K,S in [(3,5),(5,10),(10,20),(20,50)]:
        skip=0; run=0; saved=0; lost=0
        for p in first:
            if skip>0:
                skip-=1; saved+=1
                if p['out']=='kept': lost+=1
                continue
            if p['out']=='cap': run+=1
            else: run=0
            if run>=K: skip=S; run=0
        print('  backoff K=%d S=%d: skipped %d first-solves (%.1f%%), kept steps lost %d of %d'%(K,S,saved,100*saved/max(1,len(first)),lost,len(kept)))
