# RS-3 -- DECLARATION r2 (the P-SYM capture tool carries its own sha stamp)
Committed before the re-run of the P-SYM step; DECLARATION-RS3.md is unchanged. Gate run 1 (build, ctest Release 3869/3869, lists, smoke are unaffected) hit one instrument rule:
scripts/psym_compare.sh stamps every capture with its own sha256 and `compare` refuses to mix stamps. RS-3 edits exactly one COMMENT line of that script
(`tests/drivers/CMakeLists.txt` -> `tests/solver/CMakeLists.txt`, the path-comment fix this stage exists to make), so the before arm (aa1330cad's script) and the after arm
(the RS-3 head's script) carried different stamps and `compare` stopped, as designed.
Amendment: both arms are captured with the AFTER-version script (legs/psym_arms.sh overlays `git show <after>:scripts/psym_compare.sh` while the before arm is checked out,
restores it before the next checkout). The two versions differ in one comment line, so the logic is identical; no tolerance is widened, nothing else about G2 changes.
