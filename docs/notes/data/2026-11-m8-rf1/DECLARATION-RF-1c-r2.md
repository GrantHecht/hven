# DECLARATION-RF-1c-r2 -- the mutation leg's M2 anchor (successor to DECLARATION-RF-1c; that file and `legs-rf1c/` kept unedited)

Committed before the r2 mutation leg is run; never edited afterwards.

## 1. What failed

Leg M of `rf1c_chain.sh` ended `PREFLIGHT_DOES_NOT_APPLY M2` (`gate-RF-1c/mutations.out`), no mutation having been applied and the tree
restored untouched. `mutate_rf1c.py` anchors M2 on `            phase_stall.new_run();  // a restart ...` with two spaces before the comment; the
committed line (after `clang-format`) has one. The defect is in the declared script's anchor text, not in the test or the code. Every other leg
of the chain (build, ctest Release and Debug, the counter replay, the install smoke) is independent of it and is not re-run.

## 2. The correction

`legs-rf1c/mutate_rf1c_r2.py` and `legs-rf1c/mutations_rf1c_r2.sh` are `mutate_rf1c.py` and `mutations_rf1c.sh` with the M2 anchor's comment
spacing corrected to the committed line (one space) and the table-script name re-pointed; nothing else differs (`diff` of each against its
original is that and the name). Ids, must-kills (M1, M2) and the reported R1 are those of `DECLARATION-RF-1c.md` section 3; bars as declared:
control clean, M1 and M2 killed, R1 reported (`VERDICT MUTATIONS_AS_DECLARED`). The leg is run by
`~/.local/bin/box-lane -- bash legs-rf1c/mutations_rf1c_r2.sh gate-RF-1c/mutation-r2 <worktree> <build-rf1/rel> M1 M2 R1` on the same Release
build the chain made, after the chain has ended (the leg rewrites `src/`; the chain's install smoke reads the tree).
