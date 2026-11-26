#!/usr/bin/env python3
"""QPC (runner glue ported from rstb_pilots.py at hven-bench rstb-harness 6ff0dd4): census_pilots.py for declared binary arms.  census_pilots.py plans arms with i19_run.plan_pilot, which knows only the
spec arms; a declared binary arm (F, F+B1+B2, C', ...) must first be registered from the declared-binaries file
(`i19_cells.register_binary_arms`, as i19_run.py arm and scripts/rstb_cells.py do).  This wrapper registers the arms named by
--binaries and then runs census_pilots.main unchanged (its re-exec of itself through box-lane is pointed at this file so the
inner process registers too).  Additive; no row-producing path touched.   usage: as census_pilots.py"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "tu"))
sys.path.insert(0, HERE)
import census_pilots  # noqa: E402
import i19_cells  # noqa: E402
import i19_run  # noqa: E402


def main(argv):
    if "--binaries" in argv:
        path = argv[argv.index("--binaries") + 1]
        i19_cells.register_binary_arms(i19_run.load_spec(), i19_cells.read_declared_binaries(path))
    census_pilots.__file__ = os.path.abspath(__file__)
    return census_pilots.main(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
