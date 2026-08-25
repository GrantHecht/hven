#!/bin/bash
# Machine-quiet check for the ARM-H leg. Verbatim convention from
# docs/notes/data/2026-08-21-m4-task5-wall/quiet_check.sh: written as a FILE
# so the search pattern never appears on the invoking shell's own command
# line (an inline pgrep matches itself and reads as a false positive).
pat='clangd|ninja|clang\+\+|(^| )clang( |$)|cc1plus'
echo "# quiet check $(date -u +%Y-%m-%dT%H:%M:%SZ)"
hits=$(pgrep -af "$pat" | grep -v -e 'quiet_check' -e 'shell-snapshots' -e 'run_arm_h' -e 'bit_identity_check')
if [ -z "$hits" ]; then
    echo "(no build/index processes running)"
else
    echo "$hits"
fi
echo "# loadavg: $(cut -d' ' -f1-3 /proc/loadavg)"
