# W5 T8.9r-attrib3 -- the EXPERIMENT arms, measured in ONE set of rounds so the
# four are paired within every batch.
#   e1 = 8ae1618  (the PARENT, unpatched -- the baseline both experiments answer to)
#   e2 = 9cebbbe  (the CULPRIT, unpatched)
#   e3 = x1       (the CULPRIT + experiment-1.patch: hot members back at the parent's offsets)
#   e4 = x2       (the PARENT  + experiment-2.patch: hot members at the culprit's offsets)
ARMS_EXP="e1:parent:8ae1618 e2:culprit:9cebbbe e3:E1:x1_____ e4:E2:x2_____"
ARMBIN=/home/ghecht/Projects/hven/.scratch/w5t89ra3/bin
CELLS="f7_n1000_bound_physics,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n20000_bound_neutral"
