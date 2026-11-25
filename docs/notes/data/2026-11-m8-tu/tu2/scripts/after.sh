#!/bin/bash
cd /home/ghecht/Projects/hven-bench-tu2
while ! grep -q "ARM B1 DONE\|ARM [A-B][0-9] FAILED rc=1$" <(tail -1 .scratch/logs/driver.log); do sleep 30; done
tail -1 .scratch/logs/driver.log | grep -q DONE && .scratch/runarm.sh A4 A5
