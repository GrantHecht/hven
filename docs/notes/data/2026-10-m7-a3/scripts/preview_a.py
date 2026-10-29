"""Preview (NOT the close): lay out run A only and run the harness decide script on it."""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
import train_close as tc  # noqa: E402

S = os.path.dirname(os.path.abspath(__file__))
bench = f"{S}/bench"
cells = f"{S}/train/cells"
out = f"{S}/train/preview-A"
sets = {"hs": tc.roster(bench, "hs-27.json"), "cutest": tc.roster(bench, "cutest-nlp.json")}
for label in tc.LABELS:
    slot = "default" if label == "sqp-ipm" else label
    for s in tc.SETS:
        tc.merge(cells, label, s, "A", sets[s], f"{out}/tree/hven-sqp/{slot}/{s}-run-A")
r = subprocess.run(["python3", "-B", f"{bench}/scripts/w2_a4_decide.py", f"{out}/tree"],
                   capture_output=True, text=True)
print(r.stdout[-6000:], r.stderr[-2000:])
