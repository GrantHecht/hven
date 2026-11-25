import csv,glob,os,re,statistics as st,sys
D=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R='/home/ghecht/Projects/hven/.claude/worktrees/pf2/'
HDR=[l for l in open(R+'bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv') if l.startswith('cell_id,')][0]
def rows(p):
    ls=[l for l in open(p) if not l.startswith('#')]
    if not ls[0].startswith('cell_id,'): ls=[HDR]+ls
    return {x['cell_id']:x for x in csv.DictReader(ls)}
U0=rows(R+'bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv')
CL=rows(R+'bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv')
def perf(p):
    d={}
    for l in open(p):
        m=re.match(r'\s*([\d,]+)\s+(\S+)',l)
        if m: d[m.group(2).split(':')[0]]=int(m.group(1).replace(',',''))
    return d
CNT=['status','factorizations','qp_minors','escapes','qp_subproblems','qp_fact_per_qp']
cells=sorted({re.match(r'(?:u0|m8)-r\d-(.*)\.csv',os.path.basename(f)).group(1) for f in glob.glob(D+'/raw/*.csv')})
print('cell | arm | status fact minors | instr/minor (median) | spread | rounds identical | matches baseline')
res={}
for c in cells:
    for arm in('u0','m8'):
        rs=[];pf=[]
        for r in (1,2,3):
            f=f'{D}/raw/{arm}-r{r}-{c}.csv'
            if os.path.exists(f) and os.path.getsize(f):
                x=rows(f)[c]; rs.append(x); pf.append(perf(f'{D}/perf/{arm}-r{r}-{c}.txt'))
        if not rs: continue
        ident=all(all(rs[0][k]==y[k] for k in CNT) for y in rs)
        ipm=[p['instructions']/int(x['qp_minors']) for p,x in zip(pf,rs)]
        base=(U0 if arm=='u0' else CL).get(c)
        mb=all(base[k]==rs[0][k] for k in CNT) if base else None
        med=st.median(ipm); res[(c,arm)]=(med,rs[0],st.median(p['instructions'] for p in pf))
        print(f"{c} | {arm} | {rs[0]['status']} {rs[0]['factorizations']} {rs[0]['qp_minors']} | {med:.1f} | {(max(ipm)-min(ipm))/med*100:.3f}% | {ident}({len(rs)}) | {mb}")
print('\ncell | U0 instr/minor | m8 instr/minor | ratio | total instr ratio | minors ratio')
for c in cells:
    if (c,'u0') in res and (c,'m8') in res:
        a,b=res[(c,'u0')],res[(c,'m8')]
        print(f"{c} | {a[0]:.1f} | {b[0]:.1f} | {b[0]/a[0]:.3f} | {b[2]/a[2]:.3f} | {int(b[1]['qp_minors'])/int(a[1]['qp_minors']):.3f}")
