# W5 T8.9r-attrib4 -- THE FOUR MEASURED ARMS, paired inside every batch.
#
#   a1 = 102f729  the interior leg's BASE (reading.md section 5's arm102)
#   a2 = e51a7e0  the group-1 HEAD
#   a3 = E1       e51a7e0 + experiment-E1.patch (persistent result storage)
#   a4 = E2       e51a7e0 + experiment-E2.patch (E1 + persistent iterate storage)
#
# THE ARGV LOCK (attribution.md section 6, arms.txt): every binary directory is
# SEVEN characters, so the measured child's argv is the same length at every
# arm. Every batch log prints ARGV_LEN rather than asserting it.
ARMS="a1:base:base___ a2:head:head___ a3:E1:e1_____ a4:E2:e2_____"
ARMBIN=/home/ghecht/Projects/hven/.scratch/w5t89ra4/bin
CELLS="f7_n1000_bound_physics,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n20000_bound_neutral"
