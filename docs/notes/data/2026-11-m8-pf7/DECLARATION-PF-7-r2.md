# DECLARATION-PF-7-r2 -- leg L1 re-taken (instrument defect)

`DECLARATION-PF-7.md` stands unchanged (criteria, code commit `dc0aacc30`). Its L1 Release leg (`gate/ctest-rel-r1-FAILED-no-git-ceiling.log`) read 3957 run, 1 failed:
`BenchStampGuard.SelfTest` ("no git and stamp 'unknown' -> OK (rc 1, want 0)"). Cause: `legs/ctest.sh` was written from the I17 gate's without its `GIT_CEILING_DIRECTORIES=<worktree>` export, so the
self-test's "no git" case found the parent repository. The test is unrelated to `get_core_count`. Change: `legs/ctest.sh` exports the ceiling as every earlier gate's did. L1 (Release and Debug) is taken again with it;
the criterion is unchanged. L2 (replay), L3/L4 (TRAIN, instruction table) and their outputs are unaffected (no ctest involved, same binaries) and stand.
