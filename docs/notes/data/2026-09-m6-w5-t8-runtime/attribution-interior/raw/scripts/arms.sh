# The eleven arms, in group-1 order. NN task sha7.
ARMS="01:base:102f729 02:T8.1:b3915ff 03:T8.2:b43580f 04:T8.3:510a4bb 05:T8.4:3c8e43b 06:T8.5:8f95655 07:T8.6:8cbaa39 08:T8.7:56042be 09:T8.7b:ddac2cf 10:T8.8:b9848bf 11:T8.9:e51a7e0"
ARMBIN=/home/ghecht/Projects/hven/.scratch/w5t89ra/bin
# THE ARGV LOCK (attribution.md section 2 / section 6: instructions:u on these
# cells is bimodal in the measured child's argv+environ byte footprint). Every
# path component that varies between arms here is FIXED WIDTH -- the sha is
# seven characters at all eleven arms, the arm number two digits, the round one
# digit, and the pass letter one -- so the child's argv byte length is
# identical at every arm by construction, and the leg PRINTS it to prove it.
CELLS="f7_n1000_bound_physics,f7_n5000_bound_physics,f7_n10000_bound_neutral,f7_n20000_bound_neutral"
