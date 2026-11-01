#!/usr/bin/env bash
# Generates the P-SYM object map, symbol map and exceptions file for AP-2 B1
# mechanically, from the two captures' own object files. No entry is hand-written.
#
# usage: gen_maps.sh <before-snapshot-dir> <after-snapshot-dir> <out-dir>
#
# THE RULE (DECLARATION.txt, B1-c). The engine classes IpmSolver and SqpSolver
# split into a public handle (the same names) and a private implementation
# (IpmSolverImpl / SqpSolverImpl). The code that moved is the class's own member
# functions and nested types, so:
#
#   symbol-map : every symbol of the BEFORE arm (defined or merely referenced)
#                whose demangled name contains "hven::solvers::IpmSolver::" or
#                "hven::solvers::SqpSolver::" -- as the symbol's own qualifier OR as
#                the qualifier of a nested type in its parameter list -- maps to
#                the same name with every such qualifier rewritten to
#                "hven::solvers::IpmSolverImpl::" / "hven::solvers::SqpSolverImpl::".
#                That is the substitution DECLARATION.txt B1-c states
#                (s/hven::solvers::(Ipm|Sqp)Solver::/hven::solvers::\1SolverImpl::/),
#                applied to the pin's own symbol names. A name that spells the bare
#                class (no trailing "::", e.g. a `SqpSolver&` parameter) is the
#                public handle, which keeps its name, and is not mapped.
#   object-map : the handle's forwarding bodies live in the new translation units
#                drivers/{ipm,sqp}_solver_handle.cpp. The engine object they were
#                carved beside is declared a SPLIT: the old object's symbols are
#                matched against the union of {the engine object, its handle
#                object}.
#   exceptions : every symbol DEFINED in a handle object of the AFTER arm (the
#                handle's members, their private helper and the instantiations they
#                use) is excused as [ONLY-AFTER], qualified by the BEFORE-arm object
#                of the split that holds it. The handle translation unit is new
#                code, so its symbols are new by construction.
set -euo pipefail
BEFORE=$1
AFTER=$2
OUT=$3
mkdir -p "${OUT}"

prefix_re='^hven::solvers::(Ipm|Sqp)Solver::'
contains_re='hven::solvers::(Ipm|Sqp)Solver::'
strip_nm='s/^[0-9a-f ]{16} [A-Za-z] //'

objects_of() { # snapshot dir -> relative paths of the .o files
    (cd "$1" && find . -name '*.o' | sed 's|^\./||' | LC_ALL=C sort)
}

while IFS= read -r obj; do
    nm -C "${BEFORE}/${obj}" 2>/dev/null | sed -E "${strip_nm}" | { grep -E "${contains_re}" || true; }
done < <(objects_of "${BEFORE}") | LC_ALL=C sort -u |
    awk '{
        old = $0; new = $0
        gsub(/hven::solvers::IpmSolver::/, "hven::solvers::IpmSolverImpl::", new)
        gsub(/hven::solvers::SqpSolver::/, "hven::solvers::SqpSolverImpl::", new)
        print old " => " new
    }' > "${OUT}/symbol-map.txt"

: > "${OUT}/object-map.txt"
: > "${OUT}/exceptions.txt"
while IFS= read -r obj; do
    case "${obj}" in
        *_solver_handle.cpp.o)
            engine_obj="${obj/_handle.cpp.o/.cpp.o}"
            [ -f "${BEFORE}/${engine_obj}" ] || { echo "no before-arm object ${engine_obj}" >&2; exit 1; }
            echo "${engine_obj} => ${engine_obj}" >> "${OUT}/object-map.txt"
            echo "${engine_obj} => ${obj}" >> "${OUT}/object-map.txt"
            nm -C --defined-only "${AFTER}/${obj}" 2>/dev/null | sed -E "${strip_nm}" |
                LC_ALL=C sort -u |
                while IFS= read -r sym; do
                    echo "${engine_obj}::${sym} [ONLY-AFTER] ## B1 handle TU: a symbol defined in the new handle translation unit (public-handle member, its helper, or an instantiation they use); the engine body is IpmSolverImpl/SqpSolverImpl"
                done >> "${OUT}/exceptions.txt"
            ;;
    esac
done < <(objects_of "${AFTER}")
sort -u "${OUT}/object-map.txt" -o "${OUT}/object-map.txt"

echo "symbol-map entries: $(wc -l < "${OUT}/symbol-map.txt")"
echo "object-map entries: $(wc -l < "${OUT}/object-map.txt")"
echo "exceptions entries: $(wc -l < "${OUT}/exceptions.txt")"
