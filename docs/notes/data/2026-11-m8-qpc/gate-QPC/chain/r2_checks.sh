# DECLARATION-QPC-r2 section r2-3 mechanical bars (runner glue; sourced by the drivers after env.sh). Needs $G $HB $stop().
r2_check_decoded() { # r2-3(a): every required CUTEst problem has decoded/<P>/libpb.so
  local p miss=""
  for p in $(cat $G/cells/decoded-required.txt); do [ -f $HB/.scratch/h3/decoded/$p/libpb.so ] || miss="$miss $p"; done
  [ -z "$miss" ] || stop "r2-3(a): decoded/<P>/libpb.so missing for:$miss"
}
r2_check_exceptions() { # r2-3(b): r2_check_exceptions <store> <arm-name-prefix...>; zero solver_exceptions.csv data rows under the arm(s)
  local store=$1; shift; local a f n tot=0
  for a in "$@"; do
    for f in $(find $store -path "*/$a/*" -name solver_exceptions.csv 2>/dev/null); do
      n=$(( $(wc -l < $f) - 1 )); [ $n -gt 0 ] && { echo "r2-3(b): $n exception rows in $f"; tot=$((tot+n)); }
    done
  done
  [ $tot -eq 0 ] || stop "r2-3(b): $tot solver_exceptions.csv rows under $* in $store (e.g. $(find $store -path "*/$1/*" -name solver_exceptions.csv -size +60c | head -1))"
}
