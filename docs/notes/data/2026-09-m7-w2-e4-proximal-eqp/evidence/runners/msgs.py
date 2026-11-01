import re,sys
log=open(sys.argv[1],errors='replace').read()
want=set(l.strip() for l in open(sys.argv[2]) if l.strip())
# split on gtest RUN markers
parts=re.split(r'\[ RUN      \] ', log)
out={}
for p in parts[1:]:
    name=p.split('\n',1)[0].strip()
    if name not in want or name in out: continue
    body=p.split('[  FAILED  ]')[0]
    msgs=[]
    for m in re.finditer(r'^(.*?):(\d+): Failure\n((?:.*\n){0,6})', body, re.M):
        msgs.append(m.group(1).split('/')[-1]+':'+m.group(2)+' | '+' | '.join(x.strip() for x in m.group(3).strip().split('\n')[:5]))
        if len(msgs)>=2: break
    if msgs: out[name]=msgs
for n in sorted(want):
    print(n)
    for m in out.get(n,['<no message captured>']):
        print('    '+m[:400])
