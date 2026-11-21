set -o pipefail
cd /home/ghecht/Projects/hven/.claude/worktrees/pf1-riders/build-riders
rm -rf objA objB; mkdir objA objB
(cd objA && ar x ../base/libhven.a); (cd objB && ar x ../rel/libhven.a)
ls objA | wc -l; ls objB | wc -l
diff <(ls objA) <(ls objB) && echo MEMBER_LISTS_EQUAL
n=0; d=0
for f in objA/*; do b=$(basename $f); n=$((n+1)); if ! cmp -s $f objB/$b; then d=$((d+1)); echo "DIFF $b"; fi; done
echo "objects=$n differ=$d"
