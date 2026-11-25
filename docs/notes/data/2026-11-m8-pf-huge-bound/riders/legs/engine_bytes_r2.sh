#!/usr/bin/env bash
# DECLARATION-r2's R2' (successor of engine_bytes.sh, whose criterion missed the configure-time `git describe` stamp
# bench/CMakeLists.txt compiles into the bench drivers). usage: engine_bytes_r2.sh <armB sha256>
#  (a) every library object (CMakeFiles/hven.dir/**/*.o): build-pfe/rel-r == build-pfe/rel byte for byte, sets equal;
#  (b) every bench object: equal, except TUs whose compile command carries a *_GIT_DESCRIBE define (listed);
#  (c) the corpus binary: arm B's pin unmoved; `cmp -l` differs only inside the 12-byte stamp string and the 20-byte
#      NT_GNU_BUILD_ID descriptor; with the stamp replaced by arm B's and both descriptors zeroed, sha256 equal.
source "$(dirname "$0")/common.sh"
PIN=${1:?}
A=$BP/rel; B=$BP/rel-r
rc=0
[ "$(sha256sum "$BP/armBbin/hven_sqp_corpus" | cut -d' ' -f1)" = "$PIN" ] && echo "armB pin unmoved" || { echo ARMB_PIN_MOVED; rc=1; }
objs() { (cd "$1/$2" && find . -name '*.o' | sort); }
for part in CMakeFiles/hven.dir bench; do
  if diff -q <(objs "$A" "$part") <(objs "$B" "$part") > /dev/null; then echo "$part: object sets equal ($(objs "$A" "$part" | wc -l))"; else echo "$part: OBJECT_SETS_DIFFER"; rc=1; fi
done
nd=0
while read -r o; do cmp -s "$A/CMakeFiles/hven.dir/$o" "$B/CMakeFiles/hven.dir/$o" || { echo "LIB DIFFERS $o"; nd=$((nd+1)); }; done < <(objs "$A" CMakeFiles/hven.dir)
echo "library objects differing: $nd"; [ $nd -eq 0 ] || rc=1
python3 -I "$(dirname "$0")/stamp_normalize.py" "$A" "$B" "$BP/armBbin/hven_sqp_corpus" "$B/bench/hven_sqp_corpus" || rc=1
fin $rc
