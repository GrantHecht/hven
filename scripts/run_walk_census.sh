#!/bin/bash
# Walk-corpus census re-sweep — parallel-tiered protocol v1.
#
# Replays the committed 57-cell walk corpus on a freshly built corpus binary and
# compares the 13 asserted counter/status columns against a frozen baseline CSV.
# This is a COUNTER-asserting replay, not a wall-clock measurement, which is what
# licenses it to run cells concurrently at all — see "Measurement discipline"
# below.
#
# ---------------------------------------------------------------- protocol ---
#
# Tiers are assigned from the FROZEN baseline CSV, not from this run's outcomes,
# so the whole schedule is fixed before a single solve starts and cannot be
# tuned by what the fresh run happens to do:
#
#   T2  wall-sensitive, SOLO: every terminating (non-dnf) baseline cell whose
#       baseline wall_s >= --wall-sensitive-s, plus --physics-cell regardless
#       (baseline dnf, known near-deadline; an Optimal outcome from it is only
#       meaningful if the cell was alone on the machine).
#   T3  deep-DNF: every other baseline dnf_* cell.  Run --t3-width wide, each
#       with its full per-phase budget untouched.
#   T1  everything else: solved and comfortably sub-deadline.  --t1-width wide.
#
# Order is T1, then T2 (machine otherwise idle — tier 2 starts only after every
# tier-1 worker is reaped, and tier 3 has not started), then T3.  Every cell
# process runs MKL_NUM_THREADS=1 and is taskset-pinned to ONE logical CPU of a
# DISTINCT PHYSICAL core; SMT siblings are never both used.  One process per
# pinned physical core is the whole of the concurrency claim.
#
# ------------------------------------------------- measurement discipline ---
#
# CLAUDE.md §7 requires serialization for any WALL-CLOCK-asserted measurement,
# and permits counter-asserting replays such as this one to co-run under exactly
# the conditions above.  The three obligations that come with the permission are
# implemented here and must not be dropped:
#
#   1. Counters only.  wall_s (column 14) is informational and is excluded from
#      the comparison by the comparator.  Nothing in this script's output may be
#      quoted as a timing measurement.
#   2. Serial confirm on deviation.  Any cell whose 13 asserted columns disagree
#      with the baseline is written to serial_confirm_list.txt and is NOT a
#      regression until it has been re-run ALONE and still disagrees.  Co-running
#      can only bias a cell toward dnf (budget pressure); it cannot manufacture a
#      false Optimal or a false counter value at MKL_NUM_THREADS=1.
#   3. Declared protocol.  The tier assignment, the widths, the pinning and the
#      topology are stamped into the merged CSV's provenance header, so the
#      evidence artifact carries the protocol it was produced under.
#
# --------------------------------------------------------------- provenance --
#
# The corpus binary must come from a CLEAN configure: its compiled-in
# git-describe stamp is resolved at configure time and lands in every artifact
# it writes.  A stamp carrying `-dirty` does not name a real commit and fails the
# gate's provenance requirement.  Check before starting a multi-hour sweep:
#
#     "$BIN" --from-csv <any committed baseline> --csv /dev/null   # header only
#
# Resumability: a cell whose row file already exists is skipped, so a crash or a
# kill resumes rather than restarting the sweep.
#
# Usage:
#   scripts/run_walk_census.sh --binary PATH --out DIR [options]
#
#   --binary PATH        corpus binary to replay with (REQUIRED; deliberately
#                        not defaulted — each gate uses its own clean build)
#   --out DIR            run directory: rows/, logs/, merged CSV, compare output
#                        (REQUIRED)
#   --baseline PATH      frozen baseline CSV
#                        (default: bench/baselines/2026-08-06-corpus/walk_baseline.csv)
#   --compare PATH       comparator script; the compare step runs only when this
#                        is given (the comparator may live evidence-side)
#   --expect-cells N     baseline cell count to require (default 57)
#   --t1-width N         tier-1 worker count (default 6)
#   --t3-width N         tier-3 worker count (default 5)
#   --wall-sensitive-s S baseline wall_s at or above which a terminating cell is
#                        promoted to solo tier 2 (default 1800)
#   --physics-cell ID    always-solo cell id (default f7_n10000_path_physics)
#   --dry-run            print the schedule and exit; no directories, no solves
#
# ------------------------------------------------------------- exit status ---
#
# A MISMATCH is a report, not an error: this sweep's whole purpose is to find
# and list disagreeing cells, and the serial-confirm list is the finding.  An
# INFRASTRUCTURE failure is an error: it means the sweep did not produce the
# evidence it claims to.  The two must not share an exit code, or a broken run
# reads as a clean one.
#
#   0   the sweep completed and its evidence is whole.  Either every cell
#       matched, or some cells mismatched -- in which case a WARN line names
#       them and serial_confirm_list.txt carries the list.
#   1   INFRASTRUCTURE failure: a row file is missing (completed rows !=
#       --expect-cells), the merge/score step failed, the comparator itself
#       failed, or the comparator reported a cell-roster problem (MISSING /
#       EXTRA cell) rather than a value disagreement.  Nothing here is a
#       finding about the engine; all of it means re-run, not re-adjudicate.
#   2   usage / preflight failure (bad arguments, unreadable inputs, too few
#       physical cores, lock held).
#   3   fatal mid-sweep condition (binary's cell set disagrees with the
#       baseline; no row files at all).
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd)

BIN=""
RUN=""
BASELINE="$REPO_ROOT/bench/baselines/2026-08-06-corpus/walk_baseline.csv"
CMP=""
EXPECT_CELLS=57
T1_WIDTH=6
T3_WIDTH=5
WALL_SENSITIVE_S=1800
PHYS_CELL=f7_n10000_path_physics
DRY_RUN=0
PROTOCOL_VERSION="parallel-tiered v1"

usage() {
    sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//; $d' >&2
    exit 2
}

# ---------------------------------------------------------------- arguments --
while [ "$#" -gt 0 ]; do
    case "$1" in
        --binary)           BIN="${2:-}"; shift 2 ;;
        --out)              RUN="${2:-}"; shift 2 ;;
        --baseline)         BASELINE="${2:-}"; shift 2 ;;
        --compare)          CMP="${2:-}"; shift 2 ;;
        --expect-cells)     EXPECT_CELLS="${2:-}"; shift 2 ;;
        --t1-width)         T1_WIDTH="${2:-}"; shift 2 ;;
        --t3-width)         T3_WIDTH="${2:-}"; shift 2 ;;
        --wall-sensitive-s) WALL_SENSITIVE_S="${2:-}"; shift 2 ;;
        --physics-cell)     PHYS_CELL="${2:-}"; shift 2 ;;
        --dry-run)          DRY_RUN=1; shift ;;
        -h|--help)          usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

[ -n "$BIN" ] || { echo "--binary is required" >&2; usage; }
[ -n "$RUN" ] || { echo "--out is required" >&2; usage; }
if [ "$DRY_RUN" -eq 0 ] && [ ! -x "$BIN" ]; then
    echo "corpus binary not executable: $BIN" >&2; exit 2
fi
[ -r "$BASELINE" ] || { echo "baseline not readable: $BASELINE" >&2; exit 2; }
if [ -n "$CMP" ] && [ ! -r "$CMP" ]; then
    echo "comparator not readable: $CMP" >&2; exit 2
fi
command -v taskset >/dev/null || { echo "taskset not found" >&2; exit 2; }

ROWS="$RUN/rows"
LOCKS="$RUN/locks"
FAILED="$RUN/failed"
LOG="$RUN/run.log"
MERGED="$RUN/walk_census.csv"
PROTOCOL="$PROTOCOL_VERSION (T1 ${T1_WIDTH}-wide pinned / T2 solo / T3 ${T3_WIDTH}-wide full-budget)"

# ------------------------------------------------------------------ topology --
# One logical CPU per PHYSICAL core, ordered by core id: the first thread sibling
# of each core.  The siblings stay idle, so no two solves ever share a core.
PCORES=()
if command -v lscpu >/dev/null; then
    mapfile -t PCORES < <(lscpu -p=CPU,CORE 2>/dev/null | grep -v '^#' |
        sort -t, -k2,2n -k1,1n | awk -F, '!seen[$2]++ { print $1 }')
fi
if [ "${#PCORES[@]}" -eq 0 ]; then
    echo "WARNING: lscpu topology unavailable; falling back to CPUs 0..n-1" >&2
    mapfile -t PCORES < <(seq 0 $(($(nproc) - 1)))
fi
if [ "${#PCORES[@]}" -lt "$T1_WIDTH" ]; then
    echo "need >= $T1_WIDTH physical cores, detected ${#PCORES[@]}" >&2; exit 2
fi
TOPOLOGY="${#PCORES[@]} physical cores; representatives (one sibling each): $(IFS=,; echo "${PCORES[*]}")"

# ------------------------------------------------------------------- tiering --
# Assigned from the frozen baseline: column 7 status, column 14 wall_s.
TIERTAB=$(awk -F, -v phys="$PHYS_CELL" -v lim="$WALL_SENSITIVE_S" '
    { sub(/\r$/, "") }
    /^#/ { next }
    !hdr { hdr = 1; next }
    NF < 14 { next }
    {
        cell = $1; st = $7; w = $14 + 0
        if (cell == phys)          t = 2
        else if (st ~ /^dnf_/)     t = 3
        else if (w >= lim)         t = 2
        else                       t = 1
        printf "%d\t%s\t%s\t%.3f\n", t, cell, st, w
    }' "$BASELINE" | sort -k1,1n -k4,4gr)

mapfile -t T1 < <(awk -F'\t' '$1==1 {print $2}' <<<"$TIERTAB")
mapfile -t T2 < <(awk -F'\t' '$1==2 {print $2}' <<<"$TIERTAB")
mapfile -t T3 < <(awk -F'\t' '$1==3 {print $2}' <<<"$TIERTAB")
N_ALL=$(( ${#T1[@]} + ${#T2[@]} + ${#T3[@]} ))
[ "$N_ALL" -eq "$EXPECT_CELLS" ] || {
    echo "expected $EXPECT_CELLS baseline cells, tiered $N_ALL" >&2; exit 2; }

print_tier_table() {
    local pfx="${1:-}"
    printf '%stier assignment (from frozen baseline %s)\n' "$pfx" "$BASELINE"
    printf '%s  T2 wall-sensitive SOLO : non-dnf cells with wall_s >= %ss, plus %s\n' \
        "$pfx" "$WALL_SENSITIVE_S" "$PHYS_CELL"
    printf '%s  T3 deep-DNF %s-wide      : baseline status dnf_*, full budget each\n' \
        "$pfx" "$T3_WIDTH"
    printf '%s  T1 remainder %s-wide     : everything else\n' "$pfx" "$T1_WIDTH"
    printf '%s  counts: T1=%d  T2=%d  T3=%d  (total %d)\n' \
        "$pfx" "${#T1[@]}" "${#T2[@]}" "${#T3[@]}" "$N_ALL"
    printf '%s  %-4s %-32s %-14s %s\n' "$pfx" tier cell base_status base_wall_s
    while IFS=$'\t' read -r t c s w; do
        printf '%s  T%-3s %-32s %-14s %s\n' "$pfx" "$t" "$c" "$s" "$w"
    done <<<"$TIERTAB"
}

# ------------------------------------------------------------------- dry run --
cell_cmdline() { # cell core -> the exact command line a worker would exec
    printf 'taskset -c %s env MKL_NUM_THREADS=1 %s --engine walk --cells %s --csv %s\n' \
        "$2" "$BIN" "$1" "$ROWS/$1.csv.part"
}
if [ "$DRY_RUN" -eq 1 ]; then
    echo "=== DRY RUN — no solves, no directories created ==="
    echo "binary   : $BIN"
    echo "protocol : $PROTOCOL"
    echo "topology : $TOPOLOGY"
    echo "rows dir : $ROWS"
    echo
    print_tier_table ""
    echo
    for tier in 1 2 3; do
        case "$tier" in
            1) cells=("${T1[@]}"); width=$T1_WIDTH ;;
            2) cells=("${T2[@]}"); width=1 ;;
            3) cells=("${T3[@]}"); width=$T3_WIDTH ;;
            *) continue ;;
        esac
        slots=()
        for ((i = 0; i < width; i++)); do slots+=("slot$i->cpu${PCORES[$i]}"); done
        printf 'tier %s: %d cells, %d worker(s) [%s]\n' \
            "$tier" "${#cells[@]}" "$width" "$(IFS=' '; echo "${slots[*]}")"
        n=0
        for c in "${cells[@]}"; do
            [ "$n" -ge 3 ] && break
            printf '  %s\n' "$(cell_cmdline "$c" "${PCORES[$(( n % width ))]}")"
            n=$((n + 1))
        done
        [ "${#cells[@]}" -gt 3 ] && printf '  ... (%d more, pulled by whichever worker frees first)\n' \
            $(( ${#cells[@]} - 3 ))
        echo
    done
    echo "post-pass: merge -> $MERGED ;"
    if [ -n "$CMP" ]; then
        echo "           compare ($CMP) -> $RUN/compare.txt ;"
    else
        echo "           compare SKIPPED (no --compare given) ;"
    fi
    echo "           serial re-run list -> $RUN/serial_confirm_list.txt"
    exit 0
fi

# ------------------------------------------------------------------ run dirs --
mkdir -p "$ROWS" "$FAILED" "$RUN/logs"
# A single invocation owns the run: guard against two schedulers racing.
if ! mkdir "$RUN/.runlock" 2>/dev/null; then
    echo "another run holds $RUN/.runlock (remove it if that run is dead)" >&2; exit 2
fi
# shellcheck disable=SC2329  # invoked indirectly by the EXIT trap below
cleanup() { rmdir "$RUN/.runlock" 2>/dev/null || true; }
trap cleanup EXIT
# Cell claims are per-invocation only; completion is recorded by the row file,
# so clearing stale claims from a killed run cannot lose work.
rm -rf "${LOCKS:?}"; mkdir -p "$LOCKS"

log() { printf '%s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$*" >>"$LOG"; }
# Row files lead with '#' provenance lines then the header; the cell's status is
# column 7 of the first data line.
cell_status() { awk -F, '!/^#/ && NF >= 14 && $1 != "cell_id" { print $7; exit }' "$1"; }
say() { printf '%s\n' "$*"; log "$*"; }

say "=== walk census: $PROTOCOL ==="
say "binary   : $BIN"
say "baseline : $BASELINE"
say "topology : $TOPOLOGY"
print_tier_table "" | tee -a "$LOG"

# Cross-check the binary's own cell set against the baseline: a mismatch would
# surface later as MISSING/EXTRA cells, so fail loudly now.
mapfile -t BIN_CELLS < <("$BIN" --list | awk 'NF {print $1}' | sort)
mapfile -t BASE_CELLS < <(awk -F'\t' '{print $2}' <<<"$TIERTAB" | sort)
if [ "$(IFS=$'\n'; echo "${BIN_CELLS[*]}")" != "$(IFS=$'\n'; echo "${BASE_CELLS[*]}")" ]; then
    say "FATAL: binary --list cell set differs from the baseline's $EXPECT_CELLS cells"
    diff <(printf '%s\n' "${BASE_CELLS[@]}") <(printf '%s\n' "${BIN_CELLS[@]}") || true
    exit 3
fi

# -------------------------------------------------------------------- worker --
# Pulls unclaimed cells until the tier's list is exhausted.  A cell's nonzero
# exit (deadline SIGKILL, crash) is recorded and the pool continues; set -e is
# suspended only around the cell invocation itself.
worker() {
    local tier="$1" core="$2" slot="$3"
    shift 3
    local cell f rc t0 t1
    for cell in "$@"; do
        f="$ROWS/$cell.csv"
        if [ -s "$f" ]; then log "[skip] T$tier slot$slot cpu$core $cell (row exists)"; continue; fi
        mkdir "$LOCKS/$cell.lock" 2>/dev/null || continue
        if [ -s "$f" ]; then log "[skip] T$tier slot$slot cpu$core $cell (row exists)"; continue; fi
        t0=$(date +%s)
        log "[strt] T$tier slot$slot cpu$core $cell"
        set +e
        taskset -c "$core" env MKL_NUM_THREADS=1 \
            "$BIN" --engine walk --cells "$cell" --csv "$f.part" \
            >"$RUN/logs/$cell.out" 2>&1
        rc=$?
        set -e
        t1=$(date +%s)
        if [ "$rc" -eq 0 ] && [ -s "$f.part" ]; then
            mv "$f.part" "$f"
            log "[ ok ] T$tier slot$slot cpu$core $cell rc=0 wall=$((t1 - t0))s status=$(cell_status "$f")"
        else
            [ -e "$f.part" ] && mv "$f.part" "$FAILED/$cell.rc$rc.part"
            log "[FAIL] T$tier slot$slot cpu$core $cell rc=$rc wall=$((t1 - t0))s (no row; needs serial re-run)"
        fi
    done
    log "[exit] T$tier slot$slot cpu$core drained"
}

run_tier() {
    local tier="$1" width="$2"
    shift 2
    local cells=("$@") pids=() i pid
    if [ "${#cells[@]}" -eq 0 ]; then say "--- tier $tier: no cells ---"; return 0; fi
    say "--- tier $tier: ${#cells[@]} cells, $width worker(s), started $(date -u +%H:%M:%SZ) ---"
    for ((i = 0; i < width; i++)); do
        worker "$tier" "${PCORES[$i]}" "$i" "${cells[@]}" &
        pids+=("$!")
    done
    for pid in "${pids[@]}"; do wait "$pid" || say "worker pid $pid exited nonzero (pool continues)"; done
    say "--- tier $tier: complete $(date -u +%H:%M:%SZ) ---"
}

run_tier 1 "$T1_WIDTH" "${T1[@]}"
# Tier 2 is SOLO by construction: one worker, and it runs only after every
# tier-1 worker has been reaped, with tier 3 not yet started.
run_tier 2 1 "${T2[@]}"
run_tier 3 "$T3_WIDTH" "${T3[@]}"

# --------------------------------------------------------------- merge/score --
mapfile -t ROWFILES < <(find "$ROWS" -maxdepth 1 -name '*.csv' | sort)
if [ "${#ROWFILES[@]}" -eq 0 ]; then say "FATAL: no row files to merge"; exit 3; fi

# Hard completeness check BEFORE the merge.  A merge over 55 of 57 rows produces
# a CSV that looks well-formed and silently under-reports the sweep, so the
# count is checked against --expect-cells here rather than inferred later from
# the comparator's MISSING CELL lines.
INFRA_FAIL=0
if [ "${#ROWFILES[@]}" -ne "$EXPECT_CELLS" ]; then
    INFRA_FAIL=1
    say "FATAL: $EXPECT_CELLS cells expected, ${#ROWFILES[@]} row files completed --"
    say "       $(( EXPECT_CELLS - ${#ROWFILES[@]} )) cell(s) produced no row; see $FAILED/ and $RUN/logs/."
    say "       Merging anyway so the partial evidence is inspectable, but this run FAILS."
fi

say "merging ${#ROWFILES[@]} row files"
FILES=$(IFS=,; echo "${ROWFILES[*]}")
set +e
MKL_NUM_THREADS=1 "$BIN" --from-csv "$FILES" --csv "$MERGED" --score-gates \
    >"$RUN/score_gates.txt" 2>&1
merge_rc=$?
set -e
say "merge+score exit: $merge_rc"
if [ "$merge_rc" -ne 0 ]; then
    INFRA_FAIL=1
    say "FATAL: merge/score step failed (rc=$merge_rc); see $RUN/score_gates.txt"
fi

# Provenance header, prepended to the merged CSV.  A comparator that ignores
# '#' lines cannot be perturbed by it; the protocol declaration required by
# CLAUDE.md §7 lives here.
{
    printf '# census.protocol: %s\n' "$PROTOCOL"
    printf '# census.binary: %s\n' "$BIN"
    if [ -s "$MERGED" ]; then
        grep -m5 -E '^# (binary|schema|budget_table_hash|[A-Za-z_]+ provenance)' "$MERGED" |
            sed 's/^# /# census.binary_stamp: /' || true
    fi
    printf '# census.generated: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf '# census.host: %s\n' "$(hostname)"
    printf '# census.topology: %s\n' "$TOPOLOGY"
    printf '# census.baseline: %s\n' "$BASELINE"
    print_tier_table '# census.'
} >"$RUN/provenance_header.txt"
if [ -s "$MERGED" ]; then
    cat "$RUN/provenance_header.txt" "$MERGED" >"$MERGED.tmp" && mv "$MERGED.tmp" "$MERGED"
else
    INFRA_FAIL=1
    say "FATAL: merged CSV missing or empty; provenance header left at $RUN/provenance_header.txt"
fi

# ------------------------------------------------------------------- compare --
cmp_rc=0
if [ -n "$CMP" ]; then
    set +e
    python3 "$CMP" "$BASELINE" "$MERGED" >"$RUN/compare.txt" 2>&1
    cmp_rc=$?
    set -e
    say "compare exit: $cmp_rc"
    cat "$RUN/compare.txt"
    # Classify the comparator's verdict.  Its contract: 0 = every cell matched;
    # 1 = it ran and found disagreements, which it lists; anything else = the
    # comparator itself failed (bad header, unreadable input, traceback).  A
    # value disagreement is a FINDING and leaves the exit code alone; a roster
    # problem (MISSING / EXTRA cell) means the evidence is incomplete, which is
    # infrastructure.
    N_MISMATCH=$(grep -c '^MISMATCH ' "$RUN/compare.txt" || true)
    N_ROSTER=$(grep -cE '^(MISSING|EXTRA) CELL ' "$RUN/compare.txt" || true)
    if [ "$cmp_rc" -ne 0 ] && [ "$cmp_rc" -ne 1 ]; then
        INFRA_FAIL=1
        say "FATAL: comparator failed (rc=$cmp_rc) -- this is not a census result; see $RUN/compare.txt"
    elif [ "$cmp_rc" -eq 1 ] && [ "$N_MISMATCH" -eq 0 ] && [ "$N_ROSTER" -eq 0 ]; then
        INFRA_FAIL=1
        say "FATAL: comparator reported failure (rc=1) with no MISMATCH/MISSING/EXTRA line to explain it"
    fi
    if [ "$N_ROSTER" -gt 0 ]; then
        INFRA_FAIL=1
        say "FATAL: comparator reports $N_ROSTER cell-roster problem(s) (MISSING/EXTRA CELL) -- evidence incomplete"
    fi
    if [ "$N_MISMATCH" -gt 0 ]; then
        say "WARN: $N_MISMATCH cell(s) disagree with the baseline on the 13 asserted columns."
        say "WARN: that is a REPORT, not a runner failure -- each one is listed in"
        say "WARN: serial_confirm_list.txt and is NOT a regression until it has been"
        say "WARN: re-run ALONE and still disagrees.  Exit status stays 0."
    fi
else
    : >"$RUN/compare.txt"
    say "compare SKIPPED (no --compare given); merged CSV at $MERGED"
fi

# Cells whose 13 asserted columns disagree with the baseline are NOT regressions
# until they have been re-run serially: this sweep ran them under contention.
{
    printf '# Cells requiring a SERIAL re-run before any regression claim.\n'
    printf '# Produced under %s; contention is a live confound for every line here.\n' "$PROTOCOL"
    printf '# Re-run each alone: taskset -c %s env MKL_NUM_THREADS=1 %s --engine walk --cells <cell> --csv <row>\n' \
        "${PCORES[0]}" "$BIN"
    if [ -n "$CMP" ]; then
        awk -v phys="$PHYS_CELL" '
            /^MISMATCH /   { c = $2; k = "13-column mismatch" }
            /^MISSING CELL/{ c = $3; k = "no row produced (cell run failed or was never claimed)" }
            /^EXTRA CELL/  { c = $3; k = "cell not in baseline" }
            c != "" {
                note = (c == phys) ? "  [pre-adjudicated — expected flip, not a regression]" : ""
                printf "%s\t%s%s\n", c, k, note
                c = ""
            }' "$RUN/compare.txt"
    else
        printf '# (no --compare given: run the comparator, then re-generate this list)\n'
    fi
} >"$RUN/serial_confirm_list.txt"
n_confirm=$(grep -vc '^#' "$RUN/serial_confirm_list.txt" || true)
say "serial-confirm list: $n_confirm cell(s) -> $RUN/serial_confirm_list.txt"
[ "$n_confirm" -gt 0 ] && grep -v '^#' "$RUN/serial_confirm_list.txt"

say "=== census done (merge_rc=$merge_rc compare_rc=$cmp_rc rows=${#ROWFILES[@]}/$EXPECT_CELLS) ==="
if [ "$INFRA_FAIL" -ne 0 ]; then
    say "=== RUN FAILED: the sweep did not produce whole evidence (see the FATAL lines above) ==="
    exit 1
fi
exit 0
