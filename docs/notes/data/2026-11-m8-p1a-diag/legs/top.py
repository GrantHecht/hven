import sys, glob, os
def load(d):
    out={}
    for f in glob.glob(d+"/*.out"):
        r=dict(l.rstrip("\n").split("=",1) for l in open(f) if "=" in l)
        out[os.path.basename(f)[9:-4]]=r
    return out
root=sys.argv[1]; b=load(root+"/base")
for arm in sys.argv[2:]:
    a=load(root+"/"+arm); d=[]
    for k in b:
        try: d.append((int(a[k]["side.qp_minor_iters"])-int(b[k]["side.qp_minor_iters"]),k,b[k]["status"],a[k]["status"],b[k]["side.qp_minor_iters"],a[k]["side.qp_minor_iters"],b[k]["iterations"],a[k]["iterations"]))
        except Exception: pass
    d.sort()
    print("==",arm); [print("  ",x) for x in d[:5]+d[-8:]]
