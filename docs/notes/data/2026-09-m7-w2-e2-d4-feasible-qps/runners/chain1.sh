#!/usr/bin/env bash
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
until grep -q "DONE e2off" $D/logs/matrix1.log; do sleep 10; done
$D/tools/build.sh v3 > $D/logs/build-v3.out 2>&1 || { echo BUILD_FAIL; exit 1; }
$D/tools/focus.sh structfirst,unlock structfirst,greys,greyd,unlock structfirst,greys,greyd,unlock,f5,f5s structfirst,greys,greyd,unlock,f5,f5s,leakpass f5,f5s
