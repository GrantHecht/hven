import sys, math, collections, json
O=sys.argv[1]
def dl(t): return {k:float(v) for k,v in (x.split('=') for x in t[1:])}
out={}
for c in sys.argv[2:]:
    seq=[];P=[]
    for line in open(f'{O}/{c}.log'):
        t=line.split()
        if not t: continue
        if t[0]=='D': seq.append(dl(t))
        elif t[0]=='P':
            d={'kind':t[1]}
            for x in t[2:]:
                k,v=x.split('='); d[k]=v if k=='out' else float(v)
            P.append(d)
    csv=open(f'{O}/{c}.csv').readline().split(',')
    dmin=int(csv[8]); tl=[int(x) for x in open(f'{O}/{c}.csv.tally').read().split('\n')[1].split(',')]
    lmin=sum(1 for l in open(f'{O}/{c}.log') if l.startswith('M'))
    pairs=[];tests=[];cert_all=[]
    for i,d in enumerate(seq):
        if d['tgt0']==0: tests.append(d)
        else:
            cert_all.append(d)
            if i>0 and seq[i-1]['tgt0']==0: pairs.append((seq[i-1],d))
    r={}
    r['cell']=c; r['designated_minors']=dmin; r['log_minors']=lmin
    r['tally_detector']=tl[1]; r['D_test_solves']=int(sum(d['solves'] for d in tests)); r['D_cert_solves']=int(sum(d['solves'] for d in cert_all))
    r['tests']=len(tests); r['certs']=len(cert_all); r['paired']=len(pairs)
    # first-solve reuse check
    same=sum(1 for t,cc in pairs if t['rho1']==cc['rho1'] and t['res1']==cc['res1'])
    r['cert_first_solve_identical']=f'{same}/{len(pairs)}'
    flips=[(t,cc) for t,cc in pairs if cc['rho']<=1e-6]
    r['flips']=len(flips); r['flip_max_test_rho']=max([t['rho'] for t,cc in flips],default=float('nan'))
    r['flip_max_test_rho1']=max([t['rho1'] for t,cc in flips],default=float('nan'))
    # Lever L1: reuse first solve in cert: saves 1 per cert (paired)
    L1=len(cert_all)
    # L2: reread only when test rho <= UB; saves 4 (or 3 after L1) per skipped
    res={}
    for UB in (3e-5,1e-4,3e-4):
        skipped=[(t,cc) for t,cc in pairs if t['rho']>UB]
        kept_=[(t,cc) for t,cc in pairs if t['rho']<=UB]
        lostflips=sum(1 for t,cc in skipped if cc['rho']<=1e-6)
        s2=sum(cc['solves'] for t,cc in skipped)
        s12=s2+len(kept_)   # skipped fully + 1 per remaining reread (first solve reuse)
        res[UB]=dict(rereads_left=len(kept_),skipped=len(skipped),flips_lost=lostflips,saved_L2=int(s2),saved_L1_plus_L2=int(s12))
    r['L2']=res
    # L3: skip test refinement when rho1 >= LB (clearly independent)
    for LB in (1e-2,3e-3):
        refined=[d for d in tests if d['res1']>d['target'] and d['rho1']>=LB and d['screen']==0]
        conv=sum(1 for d in refined if d['conv']==1)
        flipv=sum(1 for d in refined if int(d['verdict'])!=0)
        r[f'L3_LB{LB}']=dict(tests=len(refined),saved=int(sum(d['solves']-1 for d in refined)),converged_after=conv,verdict_flips=flipv)
    # polish
    first=[p for p in P if p['step']==0]
    pe=len(P)
    r['polish_logged_solves']=pe; r['polish_tally']=tl[2]; r['polish_calls']=len(first); r['polish_B_calls']=sum(1 for p in first if p['kind']=='B')
    kept_first=[p for p in first if p['out']=='kept']
    r['polish_first_kept']=len(kept_first); r['polish_first_cap']=sum(1 for p in first if p['out']=='cap'); r['polish_first_reject']=sum(1 for p in first if p['out']=='reject')
    r['polish_kept_steps']=sum(1 for p in P if p['out']=='kept')
    # predictor from last observed amplification
    for m in (1,10,100):
        last=None; skipped=0; lost=0
        for p in first:
            capabs=1e-6*max(1.0,p['y'])
            if last is not None and last*p['res0']>m*capabs:
                skipped+=1
                if p['out']=='kept': lost+=1
                continue
            if p['res0']>0: last=p['delta']/p['res0']
        r[f'polish_amp_pred_m{m}']=dict(skipped=skipped,kept_lost=lost)
    # backoff K=20,S=50 and also K=10,S=20
    for K,S in [(5,10),(20,50)]:
        skip=0;run=0;sv=0;lo=0
        for p in first:
            if skip>0:
                skip-=1;sv+=1;lo+=(p['out']=='kept');continue
            run=run+1 if p['out']=='cap' else 0
            if run>=K: skip=S;run=0
        r[f'polish_backoff_K{K}_S{S}']=dict(skipped=sv,kept_lost=lo)
    # P2: skip when res0>0.01*cap
    r['polish_skip_res0_gt_0.01cap']=dict(skipped=sum(1 for p in first if p['res0']>0.01*1e-6*max(1.0,p['y'])),kept_lost=sum(1 for p in kept_first if p['res0']>0.01*1e-6*max(1.0,p['y'])))
    out[c]=r
json.dump(out,open(f'{O}/summary.json','w'),indent=1)
for c,r in out.items():
    print(json.dumps(r))
