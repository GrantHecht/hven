# M8 AP-5 G4 -- DECLARATION-r4 (successor to DECLARATION-r3.md; committed before any r4 leg; never edited afterwards)

## Why a successor
r3's first leg (P2, runtime-r3/run_all.sh, interior mode) VOIDED before any measurement: runtime-r3/ was copied from runtime/ without
analyze_r5.py, which runtime_leg.sh calls from its own directory for the per-process excluded-symbol count (PARSE_ERROR interior-r1-A-all,
leg rc=6). VERDICTS-r3.txt and MANIFEST-r3.txt record it (evidence commit afd28dc57). The r3 driver did not stop on the failure and started
the IC leg, which its owner stopped before any analysis (not evidence). No other r3 leg ran. An instrument defect, not a code, counter or
timing fault.

## The change (instruments only)
runtime-r4/, ic-r4/, legs-r4/ are copies of runtime-r3/, ic-r3/, legs-r3/ with -r3 -> -r4 and g4r3 -> g4r4 in code lines (evidence
directories *-r4, raw .scratch/g4r4-*, build scripts and mutate.py from legs-r4/), plus runtime-r4/analyze_r5.py copied from runtime/ (the
missing file). Every .sh/.py of runtime/, ic/, legs/ has its counterpart in the -r4 copies (checked by listing before this commit).
manifest-r4.py lists .scratch/g4r4-*. mutate.py, the mutation list, the reference rows (G3 r4's), the TSan filters and the mutation tree
wipe are r3's unchanged. The r4 driver stops at the first failed leg.

## Unchanged from DECLARATION-r3.md
Code head 73a97fb4dff30af7f8f70ac4debeda99643654c0 over origin/m8 10ec3df92; the merge M-1..M-4; expected movers NONE; deviations D-a..D-h;
P1 count 3747; arm A build-g4/armAbin/hven_sqp_corpus sha256 8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78; arm B
build-g4/armBbin/hven_sqp_corpus sha256 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77 (build-g4/rel has not been
rebuilt since it was frozen); the proofs and pass criteria P0, P1, P2, IC, P3, P4, P5, P6, P8 and M (must-kill a c d e f g h i j k l m n o
q r s t u v w x y z Z; b, p may survive, reported either way). Raw dumps stay under the ignored .scratch/; MANIFEST-r4.txt committed.

## Order (each under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first, setsid, foreground, MKL_NUM_THREADS=1)
1. runtime-r4/run_all.sh 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77 8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78   -- FIRST, serial, solo
2. ic-r4/run_ic.sh 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77 8c530207b47180f7baf6ad53109bed8ea252a4d3b1348a37923603ef2fb2bb78       -- serial, solo
3. legs-r4/chain.sh 73a97fb4dff30af7f8f70ac4debeda99643654c0 663b1138968b6b82811b730dc49d278e868b5dad56fd8ccf7b8700bcbc199a77
ON FAIL  stop, commit the evidence, report; a successor -r5 keeps these criteria unchanged.

## Pre-declaration runs (NOT evidence, disclosed)
Those of DECLARATION-r3.md, plus r3's voided runtime leg and its stopped IC start (VERDICTS-r3.txt). `bash -n` on every -r4 shell script and
py_compile on mutate.py and analyze_r5.py. Nothing of r4 has run.
