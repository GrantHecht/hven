#!/usr/bin/env python3
import sys, re, collections, math, os
O=sys.argv[1]
cells=sys.argv[2:]
KT=1e-6
def parse(path):
    M=0; D=[]; U=[]; P=[]; per_minor=[]; cur=None
    cur=0; first=True
    for line in open(path):
        t=line.split()
        if not t: continue
        if t[0]=='M':
            if not first: per_minor.append(cur)
            first=False; cur=0
        elif t[0]=='D':
            d={k:float(v) for k,v in (x.split('=') for x in t[1:])}
            D.append(d)
            if d['tgt0']==0: cur+=1
        elif t[0]=='U':
            U.append((t[1],int(t[2])));
            if int(t[2])==0: cur+=1
        elif t[0]=='P':
            d={'kind':t[1]}
            for x in t[2:]:
                k,v=x.split('=')
                d[k]=v if k=='out' else float(v)
            P.append(d)
    if not first: per_minor.append(cur)
    return D,U,P,per_minor
def tally(c):
    l=open(f'{O}/{c}.csv.tally').read().split('\n')[1].split(',')
    return [int(x) for x in l]
def qpminors(c):
    r=open(f'{O}/{c}.csv').readline().split(',')
    return int(r[8]), int(r[7]), r[6]  # guess: factorizations, qp_minors, status
for c in cells:
    D,U,P,pm=parse(f'{O}/{c}.log')
    tl=tally(c)
    minors=len(pm)
    print('=====',c)
    row=open(f'{O}/{c}.csv').readline().split(',')
    print(' status',row[6],'cols7-8',row[7],row[8],' log minors',minors,' tally detector solves',tl[1],'per minor %.3f'%(tl[1]/max(1,minors)),'polish solves',tl[2],'ord',tl[0],'border',tl[3])
    Dt=[d for d in D if d['tgt0']==0]; Dc=[d for d in D if d['tgt0']==1]
    Ut=[u for u in U if u[1]==0]
    sd=sum(d['solves'] for d in Dt); sc=sum(d['solves'] for d in Dc)
    print(' D tests(solved)',len(Dt),'solves',int(sd),' unrun/nofree tests',len(Ut),' cert D calls',len(Dc),'solves',int(sc),' other detector solves (entry/probe/repair/...)',tl[1]-int(sd)-int(sc))
    print(' tests/minor %.3f (solved+unrun) ; cert/minor %.3f'%((len(Dt)+len(Ut))/minors, len(Dc)/minors))
    # per-minor distribution of tests
    h=collections.Counter(pm)
    print(' tests-per-minor histogram:',' '.join(f'{k}:{h[k]}' for k in sorted(h)[:10]), ' max',max(pm) if pm else 0,' minors with >=2: %.1f%%'%(100*sum(v for k,v in h.items() if k>=2)/max(1,minors)))
    # solves per test
    hs=collections.Counter(int(d['solves']) for d in Dt)
    print(' solves-per-test histogram:',dict(sorted(hs.items())))
    # decided after first solve?
    conv1=[d for d in Dt if d['res1']<=d['target']]
    refined=[d for d in Dt if d['res1']>d['target']]
    print(' first solve already within refinement target:',len(conv1),'  needed refinement:',len(refined),' refinement solves (extra):',int(sum(d['solves']-1 for d in refined)))
    # verdict from rho1: dependent if screen or rho1<=KT else independent. verdict codes 0 indep 1 dep 2 undec
    def v1(d): return 1 if (d['screen'] or d['rho1']<=KT) else 0
    dis=[d for d in Dt if (v1(d)!=int(d['verdict']))]
    print(' verdict(from rho1) != final verdict:',len(dis),'of',len(Dt),'; final undecidable:',sum(1 for d in Dt if d['verdict']==2))
    # rho1 histogram by decade
    def dec(x):
        if x<=0 or math.isnan(x): return 'nonpos/nan'
        return int(math.floor(math.log10(x)))
    hb=collections.Counter(dec(d['rho1']) for d in Dt)
    print(' rho1 decade histogram (all solved tests):',' '.join(f'1e{k}:{hb[k]}' for k in sorted([k for k in hb if k!='nonpos/nan']))+ (f' nonpos/nan:{hb["nonpos/nan"]}' if 'nonpos/nan' in hb else ''))
    hb2=collections.Counter(dec(d['rho1']) for d in refined)
    print(' rho1 decade histogram (tests needing refinement):',' '.join(f'1e{k}:{hb2[k]}' for k in sorted([k for k in hb2 if k!='nonpos/nan'])))
    # rho relative change
    ch=[abs(d['rho']-d['rho1'])/max(d['rho'],1e-300) for d in refined if d['rho']>1e-12]
    if ch:
        ch.sort(); print(' refined tests rel |rho-rho1|/rho: median %.2e p90 %.2e max %.2e'%(ch[len(ch)//2],ch[int(.9*len(ch))],ch[-1]))
    # band sweep
    for (lo,hi) in [(1e-9,1e-3),(1e-8,1e-4),(1e-7,1e-5),(5e-7,2e-6),(3e-7,3e-6),(1e-7,1e-4),(1e-8,1e-3),(1e-8,1e-2)]:
        inside=[d for d in Dt if lo<d['rho1']<hi]
        out_ref=[d for d in refined if not (lo<d['rho1']<hi)]
        bad=[d for d in out_ref if v1(d)!=int(d['verdict'])]
        saved=sum(d['solves']-1 for d in out_ref)
        print('  band (%.0e,%.0e): inside %d (%.1f%%), refined-outside %d, saved refinement solves %d, verdict flips outside %d'%(lo,hi,len(inside),100*len(inside)/max(1,len(Dt)),len(out_ref),saved,len(bad)))
    # grey band tests: converged independent with rho in (1e-6,1e-3) -> cert reread
    grey=[d for d in Dt if d['verdict']==0 and d['conv']==1 and d['rho']<1e-3]
    print(' grey-band converged-independent (rho<1e-3, triggers cert re-read):',len(grey))
    # polish
    calls=[]; 
    for p in P:
        if p['step']==0: calls.append([p])
        else: calls[-1].append(p)
    ps=len(P)
    kept=sum(1 for p in P if p['out']=='kept')
    first=[c_[0] for c_ in calls]
    fk=sum(1 for p in first if p['out']=='kept'); fr=sum(1 for p in first if p['out']=='reject'); fc=sum(1 for p in first if p['out']=='cap')
    print(' polish calls',len(calls),'(E %d B %d) solves %d (%.3f/minor; tally %d) kept steps %d (kept rate %.1f%% of solves); first step: kept %d reject %d cap %d'%(sum(1 for p in first if p['kind']=='E'),sum(1 for p in first if p['kind']=='B'),ps,ps/minors,tl[2],kept,100*kept/max(1,ps),fk,fr,fc))
    second=[c_[1] for c_ in calls if len(c_)>1]
    print('   second step solves',len(second),'kept',sum(1 for p in second if p['out']=='kept'))
    wasted=fr+fc+sum(1 for p in second if p['out']!='kept')
    print('   non-kept polish solves (wasted)',wasted,'= %.1f%% of polish solves; %.3f per minor'%(100*wasted/max(1,ps),wasted/minors))
    for tau in [1e-16,1e-15,1e-14,1e-13,1e-12,1e-11,1e-10,1e-8]:
        sel=[p for p in first if p['res0']/max(1.0,p['rhs'])<=tau]
        lost=[p for p in sel if p['out']=='kept']
        print('   skip first solve if res0/max(1,|rhs|)<=%.0e: %d calls (%.1f%%), of which first step would have been kept %d'%(tau,len(sel),100*len(sel)/max(1,len(first)),len(lost)))
    # res0 relative histogram for first-step by outcome
    def decr(p):
        r=p['res0']/max(1.0,p['rhs']); return int(math.floor(math.log10(r))) if r>0 else -99
    for out in ('kept','reject','cap'):
        hh=collections.Counter(decr(p) for p in first if p['out']==out)
        if hh: print('   res0-rel decade hist, first step',out,':',' '.join(f'1e{k}:{hh[k]}' for k in sorted(hh)))
