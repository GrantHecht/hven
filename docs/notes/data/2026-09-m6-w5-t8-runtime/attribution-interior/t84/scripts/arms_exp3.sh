# W5 T8.9r-attrib3 -- experiment 3's arms. Directory names are FIXED WIDTH (seven
# characters) so the measured child's argv byte length is identical at every arm.
#   f1 = 8ae1618  the PARENT, unpatched
#   f2 = 9cebbbe  the CULPRIT, unpatched
#   f3/f4/f5 = the parent with 4096 / 9712 / 16384 bytes of unreachable .text at the
#              head of src/drivers/interior_point_solver.cpp (9712 is the exact amount
#              the culprit grew that object by)
ARMS_EXP3="f1:parent:8ae1618 f2:culprit:9cebbbe f3:pad4096:p4096__ f4:pad9712:p9712__ f5:pad16384:p16384_"
ARMBIN=/home/ghecht/Projects/hven/.scratch/w5t89ra3/bin
CELLS="f7_n1000_bound_physics,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n20000_bound_neutral"
