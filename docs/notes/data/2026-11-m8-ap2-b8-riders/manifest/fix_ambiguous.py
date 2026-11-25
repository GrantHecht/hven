import re,sys,collections
log=sys.argv[1]
amb=collections.defaultdict(set)
for line in open(log,errors='replace'):
    m=re.match(r"^(\S+?):\d+:\d+: error: call to '(\w+)' is ambiguous",line)
    if m: amb[re.sub(r'/[^/]+/\.\./','/',m.group(1))].add(m.group(2))
for f,names in amb.items():
    s=open(f).read()
    for n in names:
        s2,k=re.subn(r'^((?:template <[^\n]*>\n)?[A-Za-z][\w:<>,\s*&]*?[\s*&])'+n+r'\((?=[^;]*?\)\s*(?:const\s*)?(?:noexcept\s*)?\{)',lambda m:m.group(1)+'detail::'+n+'(',s,flags=re.M)
        print(f.split('/')[-1],n,k); s=s2
    open(f,'w').write(s)
