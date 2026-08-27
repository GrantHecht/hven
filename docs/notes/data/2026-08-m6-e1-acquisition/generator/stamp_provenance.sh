#!/usr/bin/env bash
# stamp_provenance.sh — writes artifact/PROVENANCE.txt and prepends the same
# stamp, as `#` comment lines, to artifact/e1_cells.csv.
#
# CLAUDE.md section 7: every benchmark artifact carries a provenance stamp
# (toolchain, hardware, date, commit). The `#` comment convention matches the
# archived oracle CSV's own header block.

set -euo pipefail
W="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CSV="$W/artifact/e1_cells.csv"
OUT="$W/artifact/PROVENANCE.txt"
ARCHIVE=/home/ghecht/Projects/tycho_sqp
PIQP_SRC="${PIQP_SRC_DIR:-$HOME/Software/piqp-src}"
PIQP_DIR="${PIQP_DIR:-$HOME/Software/piqp}"
NOTE=/home/ghecht/Projects/hven/docs/notes/2026-08-26-e1-acquisition-experiment.md

{
echo "E1 barrier active-set acquisition experiment -- PROVENANCE STAMP"
echo
echo "date (UTC)                 : $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "host                       : $(hostname)"
echo "os                         : $(cat /etc/fedora-release 2>/dev/null || uname -o)"
echo "kernel                     : $(uname -srm)"
echo "cpu model                  : $(lscpu | sed -n 's/^Model name: *//p')"
echo "cpu topology               : $(lscpu | sed -n 's/^Core(s) per socket: *//p') physical cores, $(lscpu | sed -n 's/^Thread(s) per core: *//p') threads/core"
echo "toolchain                  : $(clang++ --version | head -1)"
echo "                             target $(clang++ --version | sed -n 's/^Target: *//p')"
echo "compile flags (both tools) : -O3 -DNDEBUG -std=c++20 -DFMT_HEADER_ONLY"
echo "                             driver adds -DPIQP_WITH_TEMPLATE_INSTANTIATION"
echo
echo "ORACLE"
echo "piqp version               : v0.6.3 (pinned by the archived build_piqp.sh; see its banner)"
echo "piqp source commit         : $(git -C "$PIQP_SRC" rev-parse HEAD 2>/dev/null || echo UNKNOWN)"
echo "piqp source tag            : $(git -C "$PIQP_SRC" describe --tags 2>/dev/null || echo UNKNOWN)"
echo "piqp install prefix        : $PIQP_DIR  (EXTERNAL -- never vendored into any repository)"
echo "piqp libpiqp.so sha256     : $(sha256sum "$PIQP_DIR/lib64/libpiqp.so" | cut -d' ' -f1)"
echo "piqp OpenMP                : BUILD_WITH_OPENMP defaults OFF and was never enabled;"
echo "                             ldd on the driver links no libgomp, no BLAS, no MKL:"
ldd "$W/bridge/piqp_e1_driver" | sed 's/^/                               /'
echo
echo "BRIDGE ORIGIN"
echo "archive path               : $ARCHIVE/prototypes/piqp_bridge/"
echo "archive git commit         : $(git -C "$ARCHIVE" rev-parse HEAD)"
echo "archive git describe       : $(git -C "$ARCHIVE" describe --tags --always 2>/dev/null || echo UNKNOWN)"
echo "archive working tree       : $( [ -z "$(git -C "$ARCHIVE" status --porcelain)" ] && echo CLEAN || echo DIRTY )"
echo "copied, never edited in place. The workspace's driver (piqp_e1_driver.cpp)"
echo "is DERIVED from that archive's piqp_f7_driver.cpp; the generator"
echo "(e1_generate.cpp) and the dump container (e1_dump.h) are new."
echo "piqp_f7_driver.cpp sha256  : $(sha256sum "$ARCHIVE/prototypes/piqp_bridge/piqp_f7_driver.cpp" | cut -d' ' -f1)"
echo "scale_problems.h sha256    : $(sha256sum "$ARCHIVE/tests/support/scale_problems.h" | cut -d' ' -f1)"
echo
echo "PROTOCOL"
echo "protocol note              : $NOTE"
echo "protocol note sha256       : $(sha256sum "$NOTE" | cut -d' ' -f1)"
echo "hven HEAD when read        : $(git -C /home/ghecht/Projects/hven rev-parse HEAD)"
echo
echo "MEASUREMENT TERMS (declared)"
echo "threads                    : MKL_NUM_THREADS=1, OMP_NUM_THREADS=1 exported for every"
echo "                             cell. Neither is read by PIQP; the ldd evidence above is"
echo "                             what carries the single-thread claim."
echo "scheduling                 : SEQUENTIAL, one process at a time; no cell co-ran with"
echo "                             another. The box was not otherwise serialized, which is"
echo "                             admissible only because NO WALL-CLOCK CLAIM IS MADE from"
echo "                             this artifact: wall_info_s is informational."
echo "per-cell budget            : 120 s hard timeout on the whole run invocation"
echo "                             (timeout -k 5 120), dump parse included. A timeout is a"
echo "                             recorded outcome (status=timeout, counters -1)."
echo "retry policy               : one retry on cell GENERATION failure only. A solve that"
echo "                             fails is recorded as it failed and never retried."
echo "solver tolerance           : eps_abs = 1e-10, eps_rel = 0, duality-gap check OFF,"
echo "                             max_iter = 200 (the archived driver's own operating"
echo "                             point; max_iter deliberately generous so the '< 40"
echo "                             iterations' question is read off info.iter after the"
echo "                             solve, not enforced as a budget)."
echo "start                      : NEUTRAL COLD on every cell. PIQP has no primal/dual/slack"
echo "                             seeding surface at all (solve() takes no arguments), so"
echo "                             every row here runs its own from-scratch barrier"
echo "                             initialization -- cold is not a choice, it is the only"
echo "                             thing the oracle can do."
} > "$OUT"

TMP="$(mktemp)"
sed 's/^/# /' "$OUT" > "$TMP"
{ echo "# artifact/e1_cells.csv -- E1 barrier active-set acquisition, per-cell rows."; echo "#"; } >> /dev/null
cat "$CSV" >> "$TMP"
mv "$TMP" "$CSV"
echo "stamped: $OUT and $CSV"
