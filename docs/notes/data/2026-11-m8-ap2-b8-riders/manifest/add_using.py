import re,sys,collections
log=sys.argv[1]; wt=sys.argv[2]
files=collections.Counter()
for line in open(log,errors='replace'):
    m=re.match(r'^(\S+?):\d+:\d+: error: .*(did you mean \'(hven::)?detail::|unknown type name|undeclared identifier|no template named|expected class name|only virtual member|no type named \'\w+\' in namespace \'hven\'|no member named \'\w+\' in namespace \'hven\')',line)
    if m: files[m.group(1).replace('/tests/../','/').replace('/bench/../','/')]+=1
for f in files:
    f=re.sub(r'/[^/]+/\.\./','/',f)
    if '/include/hven/' in f: print('HEADER', f); continue
    s=open(f).read()
    if 'using namespace hven::detail;' in s: print('ALREADY',f); continue
    if re.search(r'^using namespace hven;$',s,re.M):
        s=re.sub(r'^using namespace hven;$','using namespace hven;\nusing namespace hven::detail;',s,count=1,flags=re.M)
    else:
        incs=list(re.finditer(r'^#include .*$',s,re.M))
        i=incs[-1].end()
        # the last include may sit inside an #if block: move past a following #endif
        rest=s[i:]
        mm=re.match(r'(\n(?:#endif[^\n]*|\s*)\n)*',rest)
        s=s[:i]+'\n\nusing namespace hven::detail;'+s[i:]
    open(f,'w').write(s); print('ADDED',f.replace(wt+'/',''))
