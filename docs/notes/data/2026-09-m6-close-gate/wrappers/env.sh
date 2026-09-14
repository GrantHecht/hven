# Shared constants for every M6 CLOSE GATE leg.  Sourced, never executed.
R=/home/ghecht/Projects/hven
F=$R/.scratch/m6-close
# The uniform flag regime, as CMakePresets.json's linux-clang-{release,debug}
# presets spell it (CLAUDE.md section 7's one uniform flag regime).  Copied from
# .scratch/w6t7/env.sh unchanged.
UNIFORM=(-G Ninja
         -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE
         -DHVEN_FP_MODE=SAFER_FAST
         -DCMAKE_CXX_COMPILER=/usr/bin/clang++)
# THE BOX AT THIS TASK'S START: the separate pgrep before the first leg matched
# exactly ONE line -- this session's own pgrep Bash call (pid 591774), whose
# command line embeds the pattern.  No foreign build, test or bench process.
# Nothing is excluded by pid.
export DECLARED_SKIP_PIDS=
export DECLARED_NOTE=
