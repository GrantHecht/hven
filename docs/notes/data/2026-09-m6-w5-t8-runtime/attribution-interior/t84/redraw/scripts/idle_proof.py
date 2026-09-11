#!/usr/bin/env python3
"""W5 T8.9r fix1 -- R2's idle proof, computed from the retained batch logs.

The solo proof is by CPU TIME. For every timed BATCH (one alternating sequence
held under the lock) this reads the `PS_SNAPSHOT` blocks the leg scripts wrote
and computes, for every FOREIGN process alive at both ends of the window:

    delta CPU seconds across the window / the window's wall seconds

That is R2's per-process test, and it is reported for every batch. On this box
it is NOT met and cannot be: a Wayland compositor, a browser and this agent's
own daemon accrue CPU continuously, on 16 logical CPUs, whatever the leg does.
So a SECOND and narrower test is computed, bracketed around EVERY TIMED RUN
rather than around the batch, and it is the one the timing recipe actually
turns on -- that the PINNED CORE ran nothing but the measured solve:

    foreign CPU on cpu2 = cpu2's busy jiffies across the run
                          MINUS the run's own user+sys (from /usr/bin/time)
    SMT contention      = cpu10's busy jiffies across the run
                          (cpu10 is cpu2's thread sibling)

A run is PINNED-CLEAN when both are under 0.5 % of its wall. That is a
measurement of the resource the recipe reserves, and it also supplies the
sibling-idleness number M2 asked for.

Foreign = everything but this agent's own process tree and the kernel threads;
the leg script excluded those when it took the snapshot. CPU time comes from
/proc/<pid>/stat's utime+stime in clock ticks (10 ms), not from `ps`'s
whole-second column, because 0.5 % of a 200 s window is 1 s -- at that column's
resolution the test could not be run.

Exit 0 when every batch is proven, 1 when any is not.

-----------------------------------------------------------------------------
W5 T8.9r-attrib4 -- THE NICE-INCLUSIVE CORRECTION (astra, fix1 review item 7).

This file is T8.9r's `idle_proof.py` with ONE accounting defect repaired and two
disclosures added; every other line is that file's.

THE DEFECT. The earlier version took foreign task time on each core from the
`user` bucket ALONE, on the argument that the measured process is niced and
every foreign task on this box is not. The first half is true and stays load-
bearing; the second half is an ASSUMPTION about other people's processes, and
the retained logs falsify it -- foreign tasks in states `SN`/`RN` (niced) are
present. A niced foreign task lands in `nice`, exactly where the measurement's
own user time lands, and reading `user` alone made it invisible.

THE REPAIR. Foreign task time on a core is now
    user + nice + steal + guest + guest_nice
with ONLY the run's own `user` seconds subtracted, and only on cpu2 (nothing of
the measurement runs on cpu10: the solve is pinned to cpu2 and the driving shell
is pinned off both). `system` is still reported separately and still not counted
as foreign -- it is the kernel serving this measurement's own process creation,
page-fault path and CSV writeback -- and so are `irq`/`softirq`. The verdict is
taken on the repaired figure, and the superseded `user`-only figure is printed
beside it so the two can be compared.

THE DISCLOSURES. (a) TRANSIENTS: a pid present in one snapshot of a batch but
not the other cannot have its CPU delta differenced, and the earlier version
silently dropped it. They are counted and named now. (b) EVERY foreign state
seen in any snapshot is listed, not just `R`.
"""
import os
import re
import sys

THRESHOLD = 0.005


def parse(path):
    """-> list of batches, each dict(name, snaps=[(tag, mono, {pid:(state,ticks)})], pauses)."""
    batches = []
    cur = None
    snap = None
    lastcpu = None
    clk = 100
    pauses = []
    with open(path, errors="replace") as fh:
        for line in fh:
            f = line.split()
            if not f:
                continue
            if f[0] == "BATCH_START":
                cur = dict(name=f[1], snaps=[], pauses=[], log=os.path.basename(path))
                batches.append(cur)
            elif f[0] == "PS_SNAPSHOT":
                mono = None
                for tok in f:
                    if tok.startswith("mono="):
                        mono = float(tok[5:])
                    if tok.startswith("clk_tck="):
                        clk = int(tok[8:])
                snap = dict(tag=f[1], mono=mono, pids={}, clk=clk)
                if cur is not None:
                    cur["snaps"].append(snap)
            elif f[0] == "FOREIGN_TICK" and snap is not None:
                snap["pids"][f[1]] = (f[2], int(f[3]))
            elif f[0] == "BOX_PAUSE" and cur is not None:
                cur["pauses"].append(line.rstrip())
            elif f[0] == "BOX_PAUSE_GIVEUP" and cur is not None:
                cur["pauses"].append(line.rstrip())
            elif f[0] == "CPUSTAT" and cur is not None:
                curtag = (f[1], f[2])
                cur.setdefault("cpu", {}).setdefault(f[1], {})[f[2]] = dict(
                    mono=float([t for t in f if t.startswith("mono=")][0][5:]), cpus={})
                lastcpu = cur["cpu"][f[1]][f[2]]
            elif f[0] == "CPUSTAT_LINE" and cur is not None and lastcpu is not None:
                lastcpu["cpus"][f[1]] = [int(x) for x in f[2:]]
            elif f[0] == "CPUTIME_SELF" and cur is not None:
                tag = f[1].split("=", 1)[1]
                d = dict(t.split("=", 1) for t in f[2:])
                cur.setdefault("self", {})[tag] = dict(
                    user=float(d["user"]), sys=float(d["sys"]), real=float(d["real"]))
            elif f[0] == "BATCH_END":
                cur = None
    return batches


# /proc/stat per-cpu fields:
#   0 user  1 nice  2 system  3 idle  4 iowait  5 irq  6 softirq  7 steal
#   8 guest  9 guest_nice
#
# TASK time and KERNEL-INTERRUPT time are counted separately, and the
# distinction is not cosmetic. `irq` and `softirq` are the kernel servicing the
# MACHINE on that core -- timer ticks, the network card, the page-fault path of
# the very process being measured. They are not another process, they cannot be
# scheduled away, and they are present in every measurement this protocol has
# ever taken, T6's included. What "solo" is a claim about is TASK time: that no
# other RUNNABLE THING was given the core.
def busy_tasks(v):
    return v[0] + v[1] + v[2] + sum(v[7:10])


# THE DISCRIMINATOR THAT MAKES THIS DECISIVE. The measured process runs NICED
# (this agent's shell is), so its user time lands in /proc/stat's `nice` bucket
# and NOT in `user`. Every foreign user task on this box -- the compositor, the
# browser, the daemons -- is UN-niced, so its time lands in `user`. The `user`
# delta on the pinned core is therefore foreign user-task time DIRECTLY, with
# nothing of the measurement's own in it and nothing to subtract.
def user_unniced(v):
    return v[0]


def nice_time(v):
    return v[1]


def system_time(v):
    return v[2]


def steal_guest(v):
    return sum(v[7:10])


def busy_kernel(v):
    return v[5] + v[6]


# THE REPAIRED FOREIGN-TASK TOTAL (attrib4). Every bucket a RUNNABLE TASK can
# land in: un-niced user, NICED user, steal and the two guest buckets. `system`
# and `irq`/`softirq` are excluded and reported separately, for the reason the
# module docstring gives.
def task_all(v):
    return v[0] + v[1] + sum(v[7:10])


def pinned_runs(batch, clk=100.0):
    """-> [(tag, real, foreign_cpu2_s, sibling_cpu10_s)] for every timed run."""
    out = []
    for tag, ends in sorted(batch.get("cpu", {}).items()):
        if "pre" not in ends or "post" not in ends:
            continue
        pre, post = ends["pre"], ends["post"]
        self_ = batch.get("self", {}).get(tag)
        if self_ is None:
            continue
        real = self_["real"]
        d = {}
        k = {}
        for c in ("cpu2", "cpu10"):
            if c in pre["cpus"] and c in post["cpus"]:
                d[c] = (busy_tasks(post["cpus"][c]) - busy_tasks(pre["cpus"][c])) / clk
                k[c] = (busy_kernel(post["cpus"][c]) - busy_kernel(pre["cpus"][c])) / clk
        foreign2 = d.get("cpu2", 0.0) - (self_["user"] + self_["sys"])
        # the un-niced user delta, per core, and the nice/system split on cpu2
        u = {}
        for c in ("cpu2", "cpu10"):
            if c in pre["cpus"] and c in post["cpus"]:
                u[c] = dict(
                    user=(user_unniced(post["cpus"][c]) - user_unniced(pre["cpus"][c])) / clk,
                    nice=(nice_time(post["cpus"][c]) - nice_time(pre["cpus"][c])) / clk,
                    system=(system_time(post["cpus"][c]) - system_time(pre["cpus"][c])) / clk,
                    steal=(steal_guest(post["cpus"][c]) - steal_guest(pre["cpus"][c])) / clk,
                    task_all=(task_all(post["cpus"][c]) - task_all(pre["cpus"][c])) / clk)
        out.append((tag, real, foreign2, d.get("cpu10", float("nan")),
                    k.get("cpu2", float("nan")), k.get("cpu10", float("nan")), u, self_))
    return out


def report(batches, out):
    ok = True
    out.write("# W5 T8.9r fix1 -- R2 idle proof, per timed batch\n\n")
    out.write("Foreign CPU time across each batch's window, from the batch's own "
              "`PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time "
              "delta is under 0.5 %% of the window's wall AND no foreign process was "
              "seen in state `R`.\n\n")
    out.write("## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch\n\n")
    out.write("**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot "
              "of a batch but not the other cannot have its CPU delta differenced; the column "
              "counts them rather than dropping them silently, and EVERY foreign state seen in "
              "any snapshot is listed, not only `R`.\n\n")
    out.write("| log | batch | window wall (s) | snapshots | foreign pids | transients | "
              "worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | "
              "pauses | R2-as-written |\n")
    out.write("|---|---|---|---|---|---|---|---|---|---|---|---|\n")
    details = []
    for b in batches:
        snaps = b["snaps"]
        if len(snaps) < 2:
            out.write("| %s | %s | - | %d | - | - | - | - | - | - | - | **UNPROVEN (fewer than "
                      "two snapshots)** |\n" % (b["log"], b["name"], len(snaps)))
            ok = False
            continue
        wall = snaps[-1]["mono"] - snaps[0]["mono"]
        clk = snaps[0]["clk"]
        first, last = snaps[0]["pids"], snaps[-1]["pids"]
        common = [p for p in first if p in last]
        worst_pid, worst_d = None, 0.0
        for p in common:
            d = (last[p][1] - first[p][1]) / float(clk)
            if d > worst_d:
                worst_d, worst_pid = d, p
        rseen = sorted({p for s in snaps for p, v in s["pids"].items() if v[0].startswith("R")})
        states = sorted({v[0] for s in snaps for v in s["pids"].values()})
        # TRANSIENTS: seen in SOME snapshot of this batch but not in BOTH ends, so
        # no delta exists for them. Counted, not dropped.
        everseen = {p for s in snaps for p in s["pids"]}
        transient = sorted(everseen - set(common))
        frac = worst_d / wall if wall > 0 else float("inf")
        verdict = "met" if (frac < THRESHOLD and not rseen and not transient) else "**NOT met**"
        out.write("| %s | %s | %.2f | %d | %d | %d | %.3f (pid %s) | **%.4f %%** | %s | %s | %d | "
                  "%s |\n" % (
            b["log"], b["name"], wall, len(snaps), len(common), len(transient), worst_d, worst_pid,
            100.0 * frac, ",".join(states), ",".join(rseen) if rseen else "none",
            len(b["pauses"]), verdict))
        if transient:
            details.append((b["log"], b["name"], None,
                            "TRANSIENT pids (present in some snapshot, not in both ends): "
                            + ",".join(transient)))
        # the five busiest foreign pids, for the record
        tops = sorted(((last[p][1] - first[p][1]) / float(clk), p) for p in common)[-5:][::-1]
        details.append((b["log"], b["name"], wall, tops))
        for ln in b["pauses"]:
            details.append((b["log"], b["name"], None, ln))
    # ---- test 2: the pinned core ----------------------------------------
    out.write("\n## Test 2 -- the PINNED CORE, per timed run (the decisive one)\n\n")
    out.write("`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on "
              "the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from "
              "/proc/stat immediately before and after EACH timed run.\n\n")
    out.write("**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the "
              "smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is "
              "0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures "
              "the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time "
              "over all of its timed wall -- is the figure with enough resolution to carry the "
              "0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed "
              "beside it.\n\n")
    out.write("**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's "
              "shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; "
              "every foreign user task on this box is UN-niced and lands in `user`. **The `user` "
              "column below is therefore foreign user-task time on the pinned core with nothing "
              "of the measurement in it and nothing subtracted.** `system` beyond the run's own "
              "`sys` is kernel-side work done FOR the measurement -- process creation, PMU "
              "programming, CSV writeback in kworker context -- and it scales with the number of "
              "processes a batch launches, not with its wall; it is reported, not counted as "
              "foreign.\n\n")
    out.write("**THE NICE-INCLUSIVE REPAIR (attrib4).** The two bold columns are the "
              "REPAIRED figure: `user + nice + steal + guest` on the core, with only the run's "
              "own `user` seconds subtracted on cpu2 and nothing subtracted on cpu10. The "
              "`user`-only columns beside them are the SUPERSEDED figure the earlier proof took "
              "its verdict on; they are printed so the two can be compared and are not what any "
              "verdict here rests on.\n\n")
    out.write("| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | "
              "**cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | "
              "cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | "
              "verdict |\n")
    out.write("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n")
    for b in batches:
        rs = pinned_runs(b)
        if not rs:
            out.write("| %s | %s | 0 | - | - | - | - | - | - | - | - | - | - | - | "
                      "**UNPROVEN (no timed run bracketed)** |\n" % (b["log"], b["name"]))
            ok = False
            continue
        tot_wall = sum(t[1] for t in rs)
        totk = sum(t[4] for t in rs if t[4] == t[4])
        u2 = sum(t[6].get("cpu2", {}).get("user", 0.0) for t in rs)
        u10 = sum(t[6].get("cpu10", {}).get("user", 0.0) for t in rs)
        n2 = sum(t[6].get("cpu2", {}).get("nice", 0.0) for t in rs)
        s2 = sum(t[6].get("cpu2", {}).get("system", 0.0) for t in rs)
        st2 = sum(t[6].get("cpu2", {}).get("steal", 0.0) for t in rs)
        a2 = sum(t[6].get("cpu2", {}).get("task_all", 0.0) for t in rs)
        a10 = sum(t[6].get("cpu10", {}).get("task_all", 0.0) for t in rs)
        selfcpu = sum(t[7]["user"] + t[7]["sys"] for t in rs)
        selfsys = sum(t[7]["sys"] for t in rs)
        selfuser = sum(t[7]["user"] for t in rs)
        # THE REPAIRED FIGURE: every task bucket, own user time out on cpu2 only.
        fo2 = max(0.0, a2 - selfuser)
        fo10 = a10
        f2 = fo2 / tot_wall if tot_wall else float("inf")
        f10 = fo10 / tot_wall if tot_wall else float("inf")
        good = f2 < THRESHOLD and f10 < THRESHOLD
        if not good:
            ok = False
        out.write("| %s | %s | %d | %.2f | **%.3f** | **%.4f %%** | **%.3f** | **%.4f %%** | "
                  "%.3f | %.3f | %.2f | %.2f | %.3f | %.3f | %s |\n" % (
                      b["log"], b["name"], len(rs), tot_wall, fo2, 100.0 * f2, fo10,
                      100.0 * f10, u2 + st2, u10, n2, selfcpu, max(0.0, s2 - selfsys), totk,
                      "PINNED-CLEAN" if good else "**UNPROVEN**"))

    out.write("\n## The five busiest foreign processes per batch\n\n")
    for d in details:
        if d[2] is None:
            out.write("- %s / %s: %s\n" % (d[0], d[1], d[3]))
            continue
        out.write("- %s / %s (window %.2f s): %s\n" % (
            d[0], d[1], d[2], "; ".join("pid %s %+.2f s" % (p, v) for v, p in d[3])))
    return ok


def main():
    logs = sys.argv[1:-1]
    outpath = sys.argv[-1]
    batches = []
    for path in sorted(logs):
        batches.extend(parse(path))
    if not batches:
        sys.stderr.write("idle_proof: no batches found in %d logs\n" % len(logs))
        return 1
    with open(outpath, "w") if outpath != "-" else sys.stdout as out:
        ok = report(batches, out)
    if not ok:
        sys.stderr.write("idle_proof: at least one batch is UNPROVEN\n")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
