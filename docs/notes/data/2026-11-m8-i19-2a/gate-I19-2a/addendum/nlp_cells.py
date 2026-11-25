"""NLP-46 cells named in the review (B1): H0 / P / C1 status, majors, LA, EV, objective; and the counts of moved-row columns over ALL moved rows."""
from common import *
import collections
h0 = load(STORE_H, "H0", LIVE + ["NLP-46"]); p = load(STORE_H, "P", LIVE + ["NLP-46"]); c1 = load(STORE_H, "C1", LIVE + ["NLP-46"])
def line(c):
    return "%s: H0 %s maj %s LA %s EV %s | P %s maj %s LA %s EV %s | C1 %s maj %s LA %s EV %s" % (
        c[2] + "/" + c[3], h0[c]["status"], h0[c]["majors"], h0[c]["la"], h0[c]["ev"], p[c]["status"], p[c]["majors"], p[c]["la"], p[c]["ev"], c1[c]["status"], c1[c]["majors"], c1[c]["la"], c1[c]["ev"])
for prob, base in [("ACOPP118", "default"), ("ACOPR118", "default"), ("BRITGAS", "default"), ("SAWPATH", "sqp-qn"), ("STEENBRD", "default"), ("STEENBRE", "default"), ("MSS2", "sqp-qn")]:
    print("NLP-46 " + line(("NLP-46", "nlp", prob, base)))
for prob, base in [("BT7", "default"), ("CRESC100", "default")]:
    for corp in ("TRAIN-238",):
        c = (corp, "sd", prob, base)
        print("%s %s/%s: H0 %s maj %s LA %s f %s | P %s maj %s LA %s f %s" % (corp, prob, base, h0[c]["status"], h0[c]["majors"], h0[c]["la"], h0[c]["objective"], p[c]["status"], p[c]["majors"], p[c]["la"], p[c]["objective"]))
# moved-row column counts over ALL moved TRAIN rows (not only the bookkeeping-only ones)
mv = [c for c in h0 if c[0] == "TRAIN-238" and i19_rule.moved(h0[c], p[c], params)]
cnt = collections.Counter()
for c in mv:
    for k in set(h0[c]["cols"]) | set(p[c]["cols"]):
        bare = k[5:] if k.startswith("side.") else k
        if bare in params["nondeterministic"] or bare.startswith(tuple(params["new_prefixes"])): continue
        if not i19_rule.columns_agree(bare, h0[c]["cols"].get(k), p[c]["cols"].get(k)): cnt[bare] += 1
print("TRAIN moved rows %d; rows differing in eqp_polish_steps %d, backend_solves_polish %d, border_refine_steps %d" % (len(mv), cnt["eqp_polish_steps"], cnt["backend_solves_polish"], cnt["border_refine_steps"]))
