#!/usr/bin/env bash
# THE SUBSTANTIVE CRITERION, RE-TAKEN BY HAND FROM THE SAVED OBJECTS.
# ============================================================================
# M7 W2 M4's G5 failed against its own classifier (see `psym_posthoc_m4.sh`'s
# header). The claim G5 exists to carry -- that the PRODUCTION LIBRARY is
# unmoved by this round -- is therefore re-taken here from the objects
# themselves, by a script that shares no code with the three tools that already
# reported on them (`psym_compare.sh`, `check_sections_fix2.sh`, the
# classifier). Agreement between independent readings is worth more than a
# second run of the same reading.
#
# It asserts, per configuration:
#   1. the P-SYM comparison carries ZERO `DIFFERS` lines;
#   2. the before and after manifests name the same objects;
#   3. every one of the 42 library objects is byte-identical, EXCEPT exactly
#      the objects this round DECLARED may differ (Release none, Debug
#      `src/qp/qp_engine.cpp.o`);
#   4. each declared exception differs ONLY in `.debug*`/`.rela.debug*`
#      sections, per `section_digest_fix2.py`'s section-header-order digest;
#   5. the EIGHT library objects whose Ninja dependency list names
#      `qp_walk_trace.h` -- the negative control -- are all accounted for, and
#      the seven that are not the declared exception are byte-identical.
#
# Usage: verify_bytes_m4.sh [<psym-dir>]
set -uo pipefail

ROOT=/home/ghecht/Projects/hven
P=${1:-$ROOT/.scratch/m7w2m4gates/gates/psym}
DIGEST=$ROOT/.scratch/m7w2m4gates/section_digest_fix2.py
INCLUDERS="assembly_eval_seam console_trace_sink sqp_print sqp_solver \
soc_elastic_restoration ipqp_engine qp_engine continuation"

[ -d "$P" ] || { echo "REFUSED: no psym dir at $P"; exit 93; }
[ -f "$DIGEST" ] || { echo "REFUSED: no section digest at $DIGEST"; exit 93; }

rc=0
note() { echo "ASSERT $1   $2"; [ "$1" = "FAIL" ] && rc=1; return 0; }

echo "BYTE_VERIFY_START $(date -Is)"
echo "psym dir: $P"
echo

for CFG in Release Debug; do
  case $CFG in
    Release) WANT_EXC="" ;;
    Debug)   WANT_EXC="CMakeFiles/hven.dir/src/qp/qp_engine.cpp.o" ;;
  esac
  echo "=== $CFG   declared exceptions: ${WANT_EXC:-(none)}"

  # 1 -- zero DIFFERS lines in the comparison.
  n_differs=$(grep -c '^DIFFERS' "$P/compare-$CFG.txt")
  [ "$n_differs" = "0" ] \
    && note OK   "$CFG: the P-SYM comparison carries 0 DIFFERS lines" \
    || note FAIL "$CFG: the P-SYM comparison carries $n_differs DIFFERS line(s)"

  # 2 -- the two manifests name the same objects.
  if diff -q <(sort "$P/before-$CFG/.psym-manifest") \
             <(sort "$P/after-$CFG/.psym-manifest") > /dev/null; then
    note OK   "$CFG: before and after manifests name the same $(wc -l < "$P/after-$CFG/.psym-manifest") object(s)"
  else
    note FAIL "$CFG: the before and after manifests differ"
  fi

  # 3 -- byte identity over every library object, with the declared exceptions.
  same=0; diffr=0; got_exc=""
  while IFS= read -r o; do
    [ -n "$o" ] || continue
    if [ ! -f "$P/before-obj-$CFG/$o" ] || [ ! -f "$P/after-obj-$CFG/$o" ]; then
      note FAIL "$CFG: $o is missing from one of the two saved arms"
      continue
    fi
    if cmp -s "$P/before-obj-$CFG/$o" "$P/after-obj-$CFG/$o"; then
      same=$((same + 1))
    else
      diffr=$((diffr + 1)); got_exc="$got_exc $o"
    fi
  done < "$P/libobjs-$CFG.txt"
  total=$(grep -c . "$P/libobjs-$CFG.txt")
  echo "  $CFG: $total library objects -- $same byte-identical, $diffr differing"
  if [ "$(echo $got_exc)" = "$(echo $WANT_EXC)" ]; then
    note OK   "$CFG: the differing set is exactly the declared one"
  else
    note FAIL "$CFG: the differing set is '$(echo $got_exc)' and the declaration was '$(echo $WANT_EXC)'"
  fi

  # 4 -- and each declared exception differs in DEBUG SECTIONS ONLY.
  for o in $got_exc; do
    b=$(mktemp); a=$(mktemp)
    python3 -B "$DIGEST" "$P/before-obj-$CFG/$o" > "$b" || { note FAIL "$CFG: digest refused $o (before)"; rm -f "$b" "$a"; continue; }
    python3 -B "$DIGEST" "$P/after-obj-$CFG/$o"  > "$a" || { note FAIL "$CFG: digest refused $o (after)";  rm -f "$b" "$a"; continue; }
    if [ "$(wc -l < "$b")" != "$(wc -l < "$a")" ]; then
      note FAIL "$CFG: $o has a different NUMBER of sections on the two arms"
    fi
    moved=$(diff "$b" "$a" | grep -E '^[<>]' | sed 's/^[<>] //' | cut -f1 | sort -u)
    nondbg=$(printf '%s\n' "$moved" | grep -v '^\.\(z\)\?debug' | grep -v '^\.rela\?\.debug' | grep -c .)
    echo "  $CFG: $o -- sections that moved: $(printf '%s ' $moved)"
    [ "$nondbg" = "0" ] \
      && note OK   "$CFG: $o moved in DEBUG SECTIONS ONLY ($nondbg non-debug section(s))" \
      || note FAIL "$CFG: $o moved in $nondbg NON-DEBUG section(s)"
    rm -f "$b" "$a"
  done

  # 5 -- the negative control, object by object, by name.
  n_ctl=0
  for name in $INCLUDERS; do
    o=$(grep -E "/${name}\.cpp\.o$" "$P/libobjs-$CFG.txt" | head -1)
    if [ -z "$o" ]; then
      note FAIL "$CFG: $name.cpp.o -- an object the derivation named is NOT in the library object list"
      continue
    fi
    if cmp -s "$P/before-obj-$CFG/$o" "$P/after-obj-$CFG/$o"; then
      n_ctl=$((n_ctl + 1)); echo "  CONTROL $CFG  BYTE-IDENTICAL  $o"
    else
      echo "  CONTROL $CFG  DIFFERS         $o"
    fi
  done
  want_ctl=8; [ "$CFG" = "Debug" ] && want_ctl=7
  [ "$n_ctl" = "$want_ctl" ] \
    && note OK   "$CFG: $n_ctl of the 8 qp_walk_trace.h includers are byte-identical (declared $want_ctl)" \
    || note FAIL "$CFG: $n_ctl of the 8 qp_walk_trace.h includers are byte-identical, declared $want_ctl"
  echo
done

echo "BYTE_VERIFY_RC=$rc"
echo "BYTE_VERIFY_END $(date -Is)"
exit $rc
