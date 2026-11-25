#!/usr/bin/env python3
"""Q9 build-cost probe: run a command, sampling the summed RSS of its whole process tree every 0.25 s.
usage: q9_measure.py OUT.json -- command...
Reports wall-clock, peak concurrent summed RSS (MiB), the largest single-process RSS seen by the sampler, user+sys CPU of the children (from wait4), compile-command count (lines of the form
`[n/m] Building CXX object` in the command's stdout), and the exit status. Wall-clock is informational (CLAUDE.md section 7); the box must be otherwise idle (flock /tmp/box-build.lock)."""
import json, os, subprocess, sys, time, threading, resource
out = sys.argv[1]; cmd = sys.argv[sys.argv.index('--') + 1:]
PAGE = os.sysconf('SC_PAGE_SIZE')
def tree_rss(root):
    kids, rss = {}, {}
    for d in os.listdir('/proc'):
        if not d.isdigit(): continue
        try:
            st = open('/proc/%s/stat' % d).read()
            ppid = int(st[st.rindex(')') + 2:].split()[1])
            kids.setdefault(ppid, []).append(int(d))
            rss[int(d)] = int(open('/proc/%s/statm' % d).read().split()[1]) * PAGE
        except Exception: pass
    tot, mx, stack = 0, 0, [root]
    while stack:
        p = stack.pop()
        if p in rss: tot += rss[p]; mx = max(mx, rss[p])
        stack.extend(kids.get(p, []))
    return tot, mx
t0 = time.time()
p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
peak, big, stop = [0], [0], threading.Event()
def sample():
    while not stop.is_set():
        t, m = tree_rss(p.pid)
        peak[0] = max(peak[0], t); big[0] = max(big[0], m); time.sleep(0.25)
th = threading.Thread(target=sample); th.start()
text = p.communicate()[0]
wall = time.time() - t0; stop.set(); th.join()
ru = resource.getrusage(resource.RUSAGE_CHILDREN)
n = sum(1 for l in text.splitlines() if 'Building CXX object' in l)
res = dict(cmd=cmd, rc=p.returncode, wall_s=round(wall, 1), peak_concurrent_rss_mib=round(peak[0] / 2**20), largest_process_rss_mib_sampled=round(big[0] / 2**20),
           largest_process_rss_mib_rusage=round(ru.ru_maxrss / 1024), cpu_user_s=round(ru.ru_utime, 1), cpu_sys_s=round(ru.ru_stime, 1), compile_commands=n)
open(out, 'w').write(json.dumps(res, indent=1) + '\n'); open(out + '.log', 'w').write(text)
print(json.dumps(res))
sys.exit(p.returncode)
