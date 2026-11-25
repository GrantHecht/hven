#!/usr/bin/env python3
# M8 DN-1 M leg (../DECLARATION-dn1legs.txt). Criterion (DECLARATION.txt M, verbatim there):
#   (kNever - kAlways)/kNever solver instructions >= 0.20, process-once OpenMP init excluded, on BOTH hs043 and HS84
#   (SQP), serial perf stat, at BOTH the default thread setting and MKL_NUM_THREADS=1. Holding at only one setting is
#   a thread-policy finding reported to the settler, not a pass. Raw reading reported beside.
# Reading, per (cell, setting, arm):
#   raw          the median of the arm's three perf stat instructions:u (the whole process, all threads);
#   init share   from the arm's perf record repetitions: the period-weighted share of samples whose call chain holds
#                a process-once initialization frame -- a symbol matching INIT below (the OpenMP runtime's serial /
#                middle / parallel initialization, MKL's thread-environment read, hven's ensure_solver_initialized);
#   excluded     raw * (1 - init share).
# Saving = (N - A) / N on each reading. Also reported: the share of samples on threads other than the process's
# main thread (OpenMP workers), never excluded.
# usage: m_analyze.py M_DIR
import glob
import gzip
import os
import re
import statistics
import sys

INIT = re.compile(r'(__kmp_(do_)?(serial|middle|parallel)_initialize|__kmp_internal_begin|'
                  r'MKL_Read_ThreadENV_Count_Cores|ensure_solver_initialized)')
M = sys.argv[1]


def stat_instr(path):
    for line in open(path):
        parts = line.split(',')
        if len(parts) > 2 and parts[2].startswith('instructions'):
            return int(parts[0])
    raise SystemExit(f'no instructions in {path}')


def samples(path):
    opener = gzip.open if path.endswith('.gz') else open
    with opener(path, 'rt', errors='replace') as f:
        head, frames = None, []
        for line in f:
            if not line.strip():
                if head is not None:
                    yield head, frames
                head, frames = None, []
                continue
            if line[0] not in ' \t':
                if head is not None:
                    yield head, frames
                parts = line.split()
                pid, _, tid = parts[1].partition('/')
                head = (parts[0], int(pid), int(tid or pid), int(parts[-1].rstrip(':')))
                frames = []
                continue
            m = re.match(r'\s*([0-9a-f]+)\s+(.*)\s+\((.*)\)\s*$', line)
            frames.append(m.group(2).strip() if m else line.strip())
        if head is not None:
            yield head, frames


def record_shares(path):
    # perf script -F comm,pid,tid,...: the lane process's main thread is tid == pid; any other tid is an
    # OpenMP worker (or another thread of the process).
    total = init = other_thread = 0
    n = 0
    for (comm, pid, tid, period), frames in samples(path):
        n += 1
        total += period
        if tid != pid:
            other_thread += period
        if any(INIT.search(fr) for fr in frames):
            init += period
    return n, total, init, other_thread


def runs_info(path):
    lines = open(path).read().splitlines()
    capped = sum('CAPPED' in line for line in lines)
    lost = sum(1 for line in lines if 'LOST' in line and not re.search(r'LOST\w*\s+0\b', line))
    return len(lines), capped, lost


verdict_rows = []
print('cell     setting  arm  raw(median)   stat rounds                         rec samples  init share  '
      'other-thread share  excluded')
results = {}
for cell in ('hs043', 'HS84'):
    for setting in ('1', 'default'):
        pre = os.path.join(M, f'{cell}.t{setting}')
        for arm in ('N', 'A'):
            stats = sorted(stat_instr(p) for p in glob.glob(f'{pre}.stat-{arm}.s*.stat'))
            if len(stats) != 3:
                raise SystemExit(f'{pre} {arm}: {len(stats)} stat rounds')
            raw = statistics.median(stats)
            script = f'{pre}.rec-{arm}.script'
            if not os.path.exists(script):
                script += '.gz'
            n, total, init, other = record_shares(script)
            reps, capped, lost = runs_info(f'{pre}.rec-{arm}.runs')
            share = init / total if total else float('nan')
            oshare = other / total if total else float('nan')
            excl = raw * (1 - share)
            results[(cell, setting, arm)] = (raw, excl)
            print(f'{cell:8s} {setting:8s} {arm:4s} {raw:12.0f}  {stats}  {n:6d} ({reps} reps, capped {capped}, '
                  f'lost-lines {lost})  {share:.4f}  {oshare:.4f}  {excl:12.0f}')
print()
holds = {}
for cell in ('hs043', 'HS84'):
    for setting in ('1', 'default'):
        (rn, en), (ra, ea) = results[(cell, setting, 'N')], results[(cell, setting, 'A')]
        sr, se = (rn - ra) / rn, (en - ea) / en
        holds[(cell, setting)] = se >= 0.20
        print(f'M {cell} {"MKL_NUM_THREADS=1" if setting == "1" else "default threads"}: saving init-excluded '
              f'{se:.4f} (gate >= 0.20: {"holds" if se >= 0.20 else "DOES NOT HOLD"}), raw {sr:.4f}')
s1 = all(holds[(c, '1')] for c in ('hs043', 'HS84'))
sd = all(holds[(c, 'default')] for c in ('hs043', 'HS84'))
print('\n== VERDICTS')
if s1 and sd:
    print('M_PASS (both cells, both thread settings)')
elif s1 or sd:
    print(f'M_THREAD_POLICY_FINDING (holds at {"MKL_NUM_THREADS=1" if s1 else "the default threads"} only; '
          'reported to the settler, not a pass)')
else:
    print('M_FAIL')
