import re, sys, csv
D='docs/notes/data/2026-08-m6-e1-acquisition/'
ref={}
for log in ('KKT-VERIFICATION-full-size.log','VARIANT-CONTIGUOUS-VERIFICATION.log'):
    cur=None
    for line in open(D+log):
        m=re.match(r'VERIFY (\S+)\s+\(',line)
        if m: cur=m.group(1); ref.setdefault(cur,{}); continue
        if cur is None: continue
        if 'stationarity' in line:
            a=re.search(r'= (\S+) \(scale (\S+)\)',line); ref[cur]['stat_inf']=a.group(1); ref[cur]['stat_scale']=a.group(2)
        elif 'min rel slack' in line:
            ref[cur]['min_inactive_slack_rel']=re.search(r'min rel slack = (\S+)',line).group(1)
        elif 'active multipliers STRICTLY positive' in line:
            ref[cur]['min_active_multiplier']=re.search(r'min = (\S+)',line).group(1)
        elif 'decile =' in line:
            ref[cur]['decile']=re.search(r'decile = (\S+) vs',line).group(1)
        elif 'min box slack' in line:
            ref[cur]['min_box_slack']=re.search(r'min box slack = (\S+)',line).group(1)
        elif 'min|D|' in line and 'LICQ (cheap' in line:
            a=re.search(r'min\|D\| = (\S+), max\|D\| = (\S+) over',line); ref[cur]['licq_d_min']=a.group(1); ref[cur]['licq_d_max']=a.group(2)
lay={}
for r in csv.DictReader(l for l in open(D+'variant_contiguous_layout.csv') if not l.startswith('#')):
    lay[r['id']]=r['active_offset']
rows=[l for l in open(sys.argv[1]) if not l.startswith('#')]
cols=['stat_inf','stat_scale','min_inactive_slack_rel','min_active_multiplier','decile','min_box_slack','licq_d_min','licq_d_max']
nd=0; nc=0; miss=[]
for r in csv.DictReader(rows):
    i=r['id']
    if i not in ref: miss.append(i); continue
    for c in cols:
        nc+=1
        if ref[i].get(c)!=r[c]:
            nd+=1; print(f'DIFF {i}.{c}: artifact={ref[i].get(c)} arm={r[c]}')
    if i in lay:
        nc+=1
        if lay[i]!=r['active_offset']:
            nd+=1; print(f'DIFF {i}.active_offset: artifact={lay[i]} arm={r["active_offset"]}')
print(f'cells compared: {len(rows)-1}  reference cells found: {len(rows)-1-len(miss)}  values compared: {nc}  differences: {nd}')
if miss: print('NO REFERENCE:',miss)
