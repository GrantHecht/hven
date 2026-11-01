import sys, os, shutil, subprocess
sys.path.insert(0, os.path.dirname(__file__))
import importlib.util
spec = importlib.util.spec_from_file_location("tc", os.path.join(os.path.dirname(__file__), "train_close.py")); tc = importlib.util.module_from_spec(spec); spec.loader.exec_module(tc)
T, bench, labels = sys.argv[1], sys.argv[2], sys.argv[3].split(",")
cells_dir = sys.argv[4] if len(sys.argv) > 4 else "cells"
tree = "/tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/preview"
shutil.rmtree(tree, ignore_errors=True)
sets = {"hs": tc.roster(bench, "hs-27.json"), "cutest": tc.roster(bench, "cutest-nlp.json")}
for i, spec_ in enumerate(labels):
    cd, label, slot = spec_.split(":")
    for s in ("hs", "cutest"):
        try: tc.merge(os.path.join(T, cd), label, s, "A", sets[s], os.path.join(tree, "hven-sqp", slot, s + "-run-A"))
        except SystemExit as e: print("skip", spec_, s, e)
print(subprocess.run(["python3", "-B", os.path.join(bench, "scripts", "w2_a4_decide.py"), tree], capture_output=True, text=True).stdout)
