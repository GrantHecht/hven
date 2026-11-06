import re,sys,subprocess,collections
log=sys.argv[1]
names=set()
for line in open(log,errors='replace'):
    m=re.search(r"undefined reference to `hven::detail::(\w+)\(",line)
    if m: names.add(m.group(1))
names.discard('to_string')
srcs=subprocess.run("git ls-files src | grep '\\.cpp$'",shell=True,capture_output=True,text=True).stdout.split()
for n in sorted(names):
    hits=[]
    for f in srcs:
        s=open(f).read()
        rx=re.compile(r'^((?:template <[^\n]*>\n)?(?:\[\[[\w:]+\]\]\s*)?[A-Za-z][\w:<>,\s*&]*?[\s*&])'+n+r'\((?=[^;]*?\)\s*(?:const\s*)?(?:noexcept\s*)?\{)',re.M)
        s2,k=rx.subn(lambda m:m.group(1)+'detail::'+n+'(',s)
        if k: open(f,'w').write(s2); hits.append(f"{f}:{k}")
    print(n,hits)
