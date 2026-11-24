"""N2b: H0 under the i19-base lane binary vs H0 under the head (i19-keys) lane binary.  Uses the harness's own comparator
(i19_run.neutrality) on TRAIN-238, both CAP corpora and PILOT-62 (complete), and compare_arm_rows on the NLP-46 cells present on BOTH
sides (the base side ran the co-run tier only; DEVIATION: the exclusive-tier cells are not re-run)."""
import sys
sys.path.insert(0, "/home/ghecht/Projects/hven-bench-i19/tu")
import i19_run
spec = i19_run.load_spec()
SA, SB = sys.argv[1], sys.argv[2]
res = i19_run.neutrality(spec, SA, SB, ["H0"], ["H0"], ["TRAIN-238", "CAP-238-1", "CAP-238-01", "PILOT-62"], "i19-base", "i19-keys") if False else \
    i19_run.neutrality(spec, SA, SB, "H0", "H0", ["TRAIN-238", "CAP-238-1", "CAP-238-01", "PILOT-62"], "i19-base", "i19-keys")
print("N2b complete corpora: %d cells, %d difference(s), only_b %s, undeclared %s" % (res["cells"], len(res["differences"]), res["only_b"], res["undeclared_only_b"]))
for d in res["differences"][:20]:
    print("DIFF", d)
a, b = i19_run.cell_table(SA, "NLP-46", "H0"), i19_run.cell_table(SB, "NLP-46", "H0")
common = sorted(set(a) & set(b))
a2, b2 = {k: a[k] for k in common}, {k: b[k] for k in common}
allowed = set(spec["neutrality"]["N2"]["differ_columns_allowed"])
d, x, y, u = i19_run.compare_arm_rows(a2, b2, allowed, spec["rule"]["new_counter_prefixes"], "i19-base", "i19-keys")
print("N2b NLP-46 common cells: %d (base side %d, keys side %d), %d difference(s), only_a %s only_b %s" % (len(common), len(a), len(b), len(d), x, y))
for t in d[:20]:
    print("DIFF NLP-46", t)
ok = not res["differences"] and not d
print("N2b %s" % ("PASS" if ok else "DIFFERENCES"))
sys.exit(0 if ok else 3)
