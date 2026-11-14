#!/usr/bin/env bash
# usage: engine_bytes.sh <armB sha256>: the riders head's fresh Release corpus binary vs arm B's pin, and every
# library object (CMakeFiles/hven.dir/**/*.o) of build-pfe/rel-r vs arm B's build-pfe/rel.
source "$(dirname "$0")/common.sh"
PIN=${1:?}
got=$(sha256sum "$BP/rel-r/bench/hven_sqp_corpus" | cut -d' ' -f1)
pinf=$(sha256sum "$BP/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
echo "armB pin      $PIN"; echo "armBbin now   $pinf"; echo "rel-r corpus  $got"
rc=0
[ "$pinf" = "$PIN" ] || { echo ARMB_PIN_MOVED; rc=1; }
[ "$got" = "$PIN" ] && echo CORPUS_IDENTICAL || { echo CORPUS_DIFFERS; rc=1; }
objs() { (cd "$1/CMakeFiles/hven.dir" && find . -name '*.o' | sort); }
if diff -q <(objs "$BP/rel") <(objs "$BP/rel-r") > /dev/null; then echo "object sets equal: $(objs "$BP/rel" | wc -l)"; else echo OBJECT_SETS_DIFFER; rc=1; fi
nd=0
while read -r o; do cmp -s "$BP/rel/CMakeFiles/hven.dir/$o" "$BP/rel-r/CMakeFiles/hven.dir/$o" || { echo "DIFFERS $o"; nd=$((nd+1)); }; done < <(objs "$BP/rel")
echo "objects differing: $nd"; [ $nd -eq 0 ] || rc=1
fin $rc
