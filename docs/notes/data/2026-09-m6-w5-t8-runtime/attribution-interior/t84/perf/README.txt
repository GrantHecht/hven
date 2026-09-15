W5 T8.9r-attrib3 -- THE PERF ARTIFACTS
======================================
NONE OF THIS ASSERTS WALL CLOCK. perf is a POINTER on this leg: it says where the
cycles sit and what the machine is doing differently. Every number this artifact
quotes as a measurement comes from the wall legs (raw/wall, raw/xwall, raw/ywall,
raw/awall) or from the fault counts, not from here.

  rec/<sha>-r{1,2,3}.data     perf record -e cycles:u -F 997, three rounds each
                              on the parent and the culprit, ALTERNATING, on the
                              wall pin's own four-cell invocation. The comparison
                              in mechanism-tables.txt B2 is computed from the
                              rep-*.txt reports beside them, which are
                                perf report --stdio --no-children -q \
                                  -F overhead,dso,symbol --percent-limit 0
  <sha>.data                  an earlier, single-round record on ONE big cell
                              (f7_n20000_bound_neutral) kept as the first look.
                              Its single-run wall is not a measurement and is not
                              quoted; the batch log is logs/P1-perf.log.
  annotate-mkl-blkl-<sha>.txt perf annotate of mkl_pds_lp64_blkl_ll_real.extracted,
                              the profile's single largest symbol at both arms
                              (10-12 % of cycles). Retained because B2's finding
                              is that two thirds of the extra cycles land in MKL's
                              own unchanged kernels, and this is the biggest of
                              them. hven::solvers::InteriorPointSolver::alg_impl
                              carries too few samples to annotate usefully -- the
                              whole `hven::` group is 2.2-2.7 % of the profile --
                              and no annotation of it is retained rather than a
                              misleading one.
  stat/S-bNN-rN.txt           perf stat across all SIX bisect arms, three rounds
                              (mechanism-tables.txt B1)
  mem/M-gN-rN.txt             the memory-behaviour leg, six rounds, with a
                              byte-identical control pair (B3a)
  screen/S-<mode>-<arm>.txt   the SCREENING probe (logs/S1-screen-malloc.log).
                              Nothing in any table is taken from it; it is kept
                              because it is the reason the fault count was
                              re-taken WITHOUT perf, in the wall leg's own
                              condition, at raw/fwall + logs/F*-wallpf-r*.log.

perf_event_paranoid = 2 on this box, so every hardware event is user-space
(`:u`). dTLB-store-misses and the LLC events read `<not supported>` on this Zen 3
part and are reported as absent rather than as zero.
