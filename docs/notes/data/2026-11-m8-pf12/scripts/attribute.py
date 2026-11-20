# Flat (self-instruction) attribution: perf record -e instructions:u -c 1e8, one sample = 1e8 instructions.
import sys,re,collections
R='/home/ghecht/Projects/hven/.claude/worktrees/pf2/.scratch/'
MIN={'f7_n1000_path_neutral':{'u0':11043,'m8':12593},'f7_n825_path_neutral_control':{'u0':9561,'m8':10589}}
BINS=[('E4_polish (polish_*, adopt_polished, refine_terminal_mu_free)',r'polish_|adopt_polished|refine_terminal_mu_free'),
      ('E2_detector_screen (detect_dependency, dependency_test*, certified_negatives, factor_row_dependence ...)',r'detect_dependency|dependency_test|dependency_rounding|certified_negatives|factor_row_dependence|remove_entry_dependency|dep_factor_view|RowDependence|probe_zero_multiplier'),
      ('BPS_border (border_*, bordered_*, BorderedSystem)',r'border_|bordered_|BorderedSystem|sync_borders|has_border'),
      ('refine_face (refine_*face*, refine_on_face)',r'refine_face_for_verdict|refine_eliminated_face|refine_on_face'),
      ('MKL/Pardiso/BLAS (libmkl, mkl_*, pardiso*)',r'^mkl_|pardiso|^dgemm|^dtrsm|^dpotrf|^dsyr|^ddot|^dgemv|mkl'),
      ('Eigen/other hven','.*')]
def load(arm,cell):
    c=collections.Counter()
    for l in open(R+f'rec-{arm}-{cell}.script'):
        s=l.strip()
        if s: c[s]+=1
    return c
for cell in MIN:
    d={a:load(a,cell) for a in ('u0','m8')}
    print(cell)
    per={}
    for a in d:
        b=collections.Counter()
        for sym,n in d[a].items():
            for name,rx in BINS:
                if re.search(rx,sym): b[name]+=n; break
        per[a]=b
        print(f'  {a}: total {sum(d[a].values())*1e8:.4g} instr, {sum(d[a].values())*1e8/MIN[cell][a]:.4g} instr/minor')
    print('  bin | u0 instr/minor | m8 instr/minor | delta | share of excess')
    ex=sum(per['m8'].values())/MIN[cell]['m8']-sum(per['u0'].values())/MIN[cell]['u0']
    for name,_ in BINS:
        u=per['u0'][name]*1e8/MIN[cell]['u0']; m=per['m8'][name]*1e8/MIN[cell]['m8']
        print(f'  {name[:60]} | {u:.4g} | {m:.4g} | {m-u:+.4g} | {(m-u)/(ex*1e8)*100:.1f}%')
    print('  top m8 symbols by per-minor delta vs u0:')
    allsym=set(d['u0'])|set(d['m8'])
    dl=sorted(((d['m8'][s]*1e8/MIN[cell]['m8']-d['u0'][s]*1e8/MIN[cell]['u0'],s) for s in allsym),reverse=True)[:12]
    for v,s in dl: print(f'    {v:+.4g}  {s[:110]}')
