import json
r=json.load(open('sz/summary.json'))
# PF-2b measure/analysis.txt: instr/minor U0 and new; solves/minor total new (tally); implied price (new-off)/extra solves
U0={'f7_n1000_path_neutral':1.088e8,'f7_n800_path_neutral':8.610e7,'f7_n825_path_neutral_control':9.548e7,'f7_n750_path_neutral_control':7.607e7,'f7_n1000_path_corrupted':3.730e8,'f7_n800_path_corrupted':4.514e8,'f7_n2000_path_activity':1.865e8,'f7_n2000_path_physics':1.993e8}
NEW={'f7_n1000_path_neutral':1.394e8,'f7_n800_path_neutral':1.122e8,'f7_n825_path_neutral_control':1.110e8,'f7_n750_path_neutral_control':1.043e8,'f7_n1000_path_corrupted':4.877e8,'f7_n800_path_corrupted':5.878e8,'f7_n2000_path_activity':3.785e8,'f7_n2000_path_physics':4.790e8}
SPM={'f7_n1000_path_neutral':4.836,'f7_n800_path_neutral':5.078,'f7_n825_path_neutral_control':4.379,'f7_n750_path_neutral_control':5.195,'f7_n1000_path_corrupted':5.206,'f7_n800_path_corrupted':9.828,'f7_n2000_path_activity':None,'f7_n2000_path_physics':None}
IMPL={'f7_n1000_path_neutral':1.662e7,'f7_n800_path_neutral':8.045e6,'f7_n825_path_neutral_control':2.657e7,'f7_n750_path_neutral_control':1.292e7}
rows=[]
print('cell | ratio | det/min | dL1 | dL2 | dL3 | dPol(res0) | dPol(amp100) | est A(L2) | B(L1+L2) | C(L1+L2+L3) | D(C+Pres0) | E(C+Pamp100) [price range lo-hi]')
for c,v in r.items():
    m=v['designated_minors']
    L1=v['certs']          # first-solve reuse, per re-read
    l2=v['L2']['0.0001']; L2=l2['saved_L2']; L12=l2['saved_L1_plus_L2']
    L3=v['L3_LB0.01']['saved']
    # reread-with-L2 and L1 are not additive naively: L12 already includes L1 on remaining rereads
    pf=v['polish_tally']/v['polish_logged_solves'] if 'corrupted' not in c and v['polish_logged_solves'] else (1.095 if '1000' in c else 1.1)
    P0=v['polish_skip_res0_gt_0.01cap']['skipped']*pf
    PA=v['polish_amp_pred_m100']['skipped']*pf
    ratio=NEW[c]/U0[c]
    if c in IMPL:
        avg=NEW[c]/SPM[c]
        lo=IMPL[c] if c in ('f7_n1000_path_neutral','f7_n750_path_neutral_control') else 1.292e7   # n800/n825: off arm took a different path, implied price not clean
        pr=[lo,avg]
    elif 'n1000' in c: pr=[1.662e7,NEW['f7_n1000_path_neutral']/4.836]
    else: pr=None
    def est(saved):
        if pr is None: return None
        return tuple(ratio-saved/m*p/U0[c] for p in pr)
    out=dict(cell=c,ratio=ratio,det_per_min=v['tally_detector']/m,L1=L1/m,L2=L2/m,L12=L12/m,L3=L3/m,P0=P0/m,PA=PA/m)
    A=est(L2);B=est(L12);C=est(L12+L3);D=est(L12+L3+P0);E=est(L12+L3+PA)
    out.update(A=A,B=B,C=C,D=D,E=E,price=pr)
    rows.append(out)
    f=lambda t:'n/a' if t is None else '%.3f-%.3f'%(t[1],t[0]) if t[1]<t[0] else '%.3f-%.3f'%(t[0],t[1])
    print('%s | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %s | %s | %s | %s | %s'%(c,ratio,out['det_per_min'],out['L1'],out['L2'],out['L3'],out['P0'],out['PA'],f(A),f(B),f(C),f(D),f(E)))
json.dump(rows,open('sz/model.json','w'),indent=1,default=str)
