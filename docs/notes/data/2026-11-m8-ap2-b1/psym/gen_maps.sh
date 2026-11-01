#!/usr/bin/env bash
# Generates the P-SYM symbol map and exceptions file for AP-2 B1 mechanically,
# from the two captures' own object files. No entry is hand-written.
#
# usage: gen_maps.sh <before-snapshot-dir> <after-snapshot-dir> <out-dir>
#
# THE RULE (DECLARATION.txt, B1-c). The engine classes IpmSolver and SqpSolver
# split into a public handle (the same names) and a private implementation
# (IpmSolverImpl / SqpSolverImpl). The code that moved is the class's own member
# functions, so:
#
#   symbol-map : every symbol of the BEFORE arm whose demangled name begins with
#                "hven::solvers::IpmSolver::" or "hven::solvers::SqpSolver::"
#                (defined or merely referenced) maps to the same name with
#                "Impl" appended to the class. Nothing else is mapped: a symbol
#                that only mentions the class in a parameter or template
#                argument is not a member of it and is left alone.
#   exceptions : every symbol DEFINED in the AFTER arm whose demangled name begins
#                with the same two prefixes (the handle's forwarding bodies) is
#                excused as [ONLY-AFTER], qualified by the object that defines
#                it. Those are, by construction, the new symbols.
set -euo pipefail
BEFORE=$1
AFTER=$2
OUT=$3
mkdir -p "${OUT}"

prefix_re='^hven::solvers::(Ipm|Sqp)Solver::'
strip_nm='s/^[0-9a-f ]{16} [A-Za-z] //'

objects_of() { # snapshot dir -> relative paths of the .o files
    (cd "$1" && find . -name '*.o' | sed 's|^\./||' | LC_ALL=C sort)
}

while IFS= read -r obj; do
    nm -C "${BEFORE}/${obj}" 2>/dev/null | sed -E "${strip_nm}" | { grep -E "${prefix_re}" || true; }
done < <(objects_of "${BEFORE}") | LC_ALL=C sort -u |
    awk '{
        old = $0; new = $0
        if (new ~ /^hven::solvers::IpmSolver::/) sub(/^hven::solvers::IpmSolver::/, "hven::solvers::IpmSolverImpl::", new)
        else if (new ~ /^hven::solvers::SqpSolver::/) sub(/^hven::solvers::SqpSolver::/, "hven::solvers::SqpSolverImpl::", new)
        print old " => " new
    }' > "${OUT}/symbol-map.txt"

while IFS= read -r obj; do
    nm -C --defined-only "${AFTER}/${obj}" 2>/dev/null | sed -E "${strip_nm}" |
        { grep -E "${prefix_re}" || true; } | LC_ALL=C sort -u |
        while IFS= read -r sym; do
            echo "${obj}::${sym} [ONLY-AFTER] ## B1 handle forwarder: new public-handle member; the engine body is IpmSolverImpl/SqpSolverImpl"
        done
done < <(objects_of "${AFTER}") > "${OUT}/exceptions.txt"

echo "symbol-map entries: $(wc -l < "${OUT}/symbol-map.txt")"
echo "exceptions entries: $(wc -l < "${OUT}/exceptions.txt")"
