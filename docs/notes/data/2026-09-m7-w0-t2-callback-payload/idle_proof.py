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

-----------------------------------------------------------------------------
W5 T8.9r FIX ROUND 3 -- THE EVIDENCE ACCOUNTING (settler ruling R13; astra's
fix2 review, item 1). NO NUMBER BELOW MOVES: every fraction, every delta and
every verdict on a fraction is computed from exactly the bytes the fix2 version
computed them from. What changes is what the tool SEES and what it is willing
to call proven.

(1) BOTH SNAPSHOT SOURCES ARE READ. The leg scripts write two lines per foreign
    process: `FOREIGN_TICK <pid> <state> <ticks>`, whose state is the single
    character in /proc/<pid>/stat, and `FOREIGN_PS <pid> <ppid> <state> ...`,
    whose state is `ps`'s full string (`Ss`, `Rsl`, `SN`, `D`). The fix2 version
    read only the first, so `4022442 Rsl` in L4-leg1-ssn-r2.log was invisible and
    that batch reported ``R`` seen: none. Both are read now, per snapshot, and
    the union is what the `R` test and the state list are taken on. THE CPU
    DELTAS ARE STILL TAKEN ON `FOREIGN_TICK` ALONE -- it is the only line
    carrying ticks -- so the fractions are unchanged by this.

(2) EVERY FOREIGN PID IS LISTED, WITH ITS OBSERVED STATES, PER BATCH, in the
    appendix file `--pids` names. The five-busiest list stays as a summary; it is
    no longer the whole of the disclosure.

(3) PAUSES ARE COUNTED ONCE. `BOX_PAUSE` and `BOX_PAUSE_GIVEUP` are separate
    columns. The fix2 table added them together and reported twice the pauses
    actually taken.

(4) TWO KINDS OF UNPROVEN, NAMED APART.
      UNPROVEN-FRACTION -- R2''s bar is measured and exceeded on cpu2 or cpu10.
      UNPROVEN-EVIDENCE -- the bar is met (or cannot be computed) but the
        evidence R2' requires is not in the log:
          (a) a foreign `R` was observed and NO RE-SNAPSHOT followed it. R2'
              says a foreign `R` is a pause AND a re-snapshot; the recipe these
              batches ran under paused, slept, printed `BOX_PAUSE_GIVEUP` and
              continued. A re-snapshot is a second `PS_SNAPSHOT` carrying the
              SAME tag as the one that saw the `R`; the tool looks for exactly
              that and finds none anywhere.
          (b) an ALTERNATION SNAPSHOT IS MISSING: two timed runs follow each
              other in the log with no `PS_SNAPSHOT` between them, so no foreign
              state was observed across that arm switch. PROVENANCE.txt (G2)
              already discloses which recipes do this; this makes the disclosure
              a per-batch flag that propagates.
          (c) NO TIMED-RUN BRACKET AT ALL -- `PS_SNAPSHOT` blocks but no
              `CPUSTAT`/`CPUTIME_SELF` pair around each run, so no pinned-core
              figure exists to test.
    A batch can carry both kinds; both are printed. THERE IS NO RETROSPECTIVE
    EXEMPTION: fix2 deferred the re-snapshot requirement to "the next
    measurement" and the settler's R13 withdraws that. The numbers stand and the
    flags travel with them.
"""
import os
import re
import sys

THRESHOLD = 0.005


def parse(path):
    """-> list of batches, each dict(name, snaps=[...], pauses, giveups, events, comms)."""
    batches = []
    cur = None
    snap = None
    lastcpu = None
    clk = 100
    with open(path, errors="replace") as fh:
        for line in fh:
            f = line.split()
            if not f:
                continue
            if f[0] == "BATCH_START":
                cur = dict(name=f[1], snaps=[], pauses=[], giveups=[], events=[],
                           comms={}, log=os.path.basename(path))
                batches.append(cur)
            elif f[0] == "PS_SNAPSHOT":
                mono = None
                for tok in f:
                    if tok.startswith("mono="):
                        mono = float(tok[5:])
                    if tok.startswith("clk_tck="):
                        clk = int(tok[8:])
                snap = dict(tag=f[1], mono=mono, pids={}, states={}, clk=clk)
                if cur is not None:
                    cur["snaps"].append(snap)
                    cur["events"].append(("snapshot", f[1]))
            elif f[0] == "FOREIGN_TICK" and snap is not None:
                # THE DELTA SOURCE, unchanged: the only line carrying ticks.
                snap["pids"][f[1]] = (f[2], int(f[3]))
                snap["states"].setdefault(f[1], set()).add(f[2])
            elif f[0] == "FOREIGN_PS" and snap is not None:
                # THE SECOND STATE SOURCE (fix3). `ps`'s full state string; no
                # ticks, so it feeds the state union and NOT the fractions.
                if len(f) > 3:
                    snap["states"].setdefault(f[1], set()).add(f[3])
                if cur is not None and len(f) > 7:
                    cur["comms"].setdefault(f[1], f[7])
            elif f[0] == "BOX_PAUSE" and cur is not None:
                cur["pauses"].append(line.rstrip())
            elif f[0] == "BOX_PAUSE_GIVEUP" and cur is not None:
                cur["giveups"].append(line.rstrip())
            elif f[0] == "CPUSTAT" and cur is not None:
                curtag = (f[1], f[2])
                cur.setdefault("cpu", {}).setdefault(f[1], {})[f[2]] = dict(
                    mono=float([t for t in f if t.startswith("mono=")][0][5:]), cpus={})
                lastcpu = cur["cpu"][f[1]][f[2]]
            elif f[0] == "CPUSTAT_LINE" and cur is not None and lastcpu is not None:
                lastcpu["cpus"][f[1]] = [int(x) for x in f[2:]]
            elif f[0] == "CPUTIME_SELF" and cur is not None:
                cur["events"].append(("run", f[1].split("=", 1)[1]))
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


# ---------------------------------------------------------------------------
# THE EVIDENCE TESTS (fix3, settler R13). These look at what the log CONTAINS,
# not at what it measured; none of them touches a number.
# ---------------------------------------------------------------------------
def r_observations(batch):
    """-> [(snapshot index, tag, {pid: sorted states})] for every snapshot with an `R`.

    The state union of BOTH sources -- /proc's single character and `ps`'s full
    string -- so `Rsl` counts, which is what fix2 missed.
    """
    out = []
    for i, sn in enumerate(batch["snaps"]):
        hit = {p: sorted(v) for p, v in sn["states"].items()
               if any(st.startswith("R") for st in v)}
        if hit:
            out.append((i, sn["tag"], hit))
    return out


def resnapshots(batch):
    """-> set of snapshot indices that are a RE-SNAPSHOT of the preceding one.

    R2' requires that a foreign `R` be answered by a pause AND a re-snapshot.
    The mechanical signature of a re-snapshot is a second `PS_SNAPSHOT` carrying
    the same tag as the one before it -- the recipe would have written the block
    again under the same tag. Nothing else in these logs can stand for it.
    """
    snaps = batch["snaps"]
    return {i for i in range(1, len(snaps)) if snaps[i]["tag"] == snaps[i - 1]["tag"]}


def alternation_gaps(batch):
    """-> [(previous run tag, next run tag)] for every pair of consecutive timed
    runs with NO `PS_SNAPSHOT` between them.

    Between two such runs the batch changed arm (or cell) without observing a
    single foreign state, so R2''s "every foreign pid and state seen in any
    snapshot" covers the switch only by inference. PROVENANCE.txt (G2) discloses
    which recipes place `box_guard` after their arm loops; this counts it.
    """
    gaps = []
    prev = None
    for kind, tag in batch["events"]:
        if kind == "run":
            if prev is not None:
                gaps.append((prev, tag))
            prev = tag
        else:
            prev = None
    return gaps


def evidence(batch):
    """-> (list of gap strings, bool complete). `R`-without-re-snapshot and
    missing alternation snapshots. The no-bracket case is added by the caller,
    which is the only place that knows whether a bracket was found."""
    gaps = []
    rs = r_observations(batch)
    if rs:
        answered = resnapshots(batch)
        unanswered = [(i, tag) for i, tag, _ in rs if (i + 1) not in answered]
        if unanswered:
            gaps.append("`R` at %s with NO re-snapshot" %
                        ",".join("`%s`" % t for _, t in unanswered))
    ag = alternation_gaps(batch)
    if ag:
        gaps.append("%d alternation snapshot(s) missing" % len(ag))
    return gaps, not gaps


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


def report(batches, out, pids_out=None, pids_rel=None):
    ok = True
    out.write("# W5 T8.9r fix1 -- R2 idle proof, per timed batch\n\n")
    out.write("Foreign CPU time across each batch's window, from the batch's own "
              "`PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time "
              "delta is under 0.5 %% of the window's wall AND no foreign process was "
              "seen in state `R`.\n\n")
    out.write("**FIX ROUND 3 (settler R13).** `FOREIGN_PS` is read beside `FOREIGN_TICK`, so "
              "`ps` states such as `Rsl` are seen; pauses and give-ups are separate columns; "
              "every foreign pid and its observed states is listed per batch in the appendix "
              "file; and a batch whose EVIDENCE is short of R2' -- an `R` with no re-snapshot, "
              "or a missing alternation snapshot -- is UNPROVEN-EVIDENCE, reported apart from "
              "UNPROVEN-FRACTION. **No fraction, delta or bar-verdict moved.**\n\n")
    out.write("## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch\n\n")
    out.write("**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot "
              "of a batch but not the other cannot have its CPU delta differenced; the column "
              "counts them rather than dropping them silently, and EVERY foreign state seen in "
              "any snapshot is listed, not only `R`.\n\n")
    out.write("| log | batch | window wall (s) | snapshots | foreign pids | transients | "
              "worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | "
              "pauses | give-ups | re-snapshots | alternation gaps | R2-as-written |\n")
    out.write("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n")
    details = []
    for b in batches:
        snaps = b["snaps"]
        if len(snaps) < 2:
            out.write("| %s | %s | - | %d | - | - | - | - | - | - | - | - | - | - | "
                      "**UNPROVEN (fewer than two snapshots)** |\n"
                      % (b["log"], b["name"], len(snaps)))
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
        # BOTH SOURCES (fix3): /proc's single character and `ps`'s full string.
        rseen = sorted({p for s in snaps for p, v in s["states"].items()
                        if any(st.startswith("R") for st in v)})
        states = sorted({st for s in snaps for v in s["states"].values() for st in v})
        # TRANSIENTS: seen in SOME snapshot of this batch but not in BOTH ends, so
        # no delta exists for them. Counted, not dropped.
        everseen = {p for s in snaps for p in s["pids"]}
        transient = sorted(everseen - set(common))
        frac = worst_d / wall if wall > 0 else float("inf")
        verdict = "met" if (frac < THRESHOLD and not rseen and not transient) else "**NOT met**"
        rs = resnapshots(b)
        ag = alternation_gaps(b)
        out.write("| %s | %s | %.2f | %d | %d | %d | %.3f (pid %s) | **%.4f %%** | %s | %s | %d | "
                  "%d | %d | %d | %s |\n" % (
            b["log"], b["name"], wall, len(snaps), len(common), len(transient), worst_d, worst_pid,
            100.0 * frac, ",".join(states), ",".join(rseen) if rseen else "none",
            len(b["pauses"]), len(b["giveups"]), len(rs), len(ag), verdict))
        if transient:
            details.append((b["log"], b["name"], None,
                            "TRANSIENT pids (present in some snapshot, not in both ends): "
                            + ",".join(transient)))
        # the five busiest foreign pids, for the record
        tops = sorted(((last[p][1] - first[p][1]) / float(clk), p) for p in common)[-5:][::-1]
        details.append((b["log"], b["name"], wall, tops))
        for ln in b["pauses"] + b["giveups"]:
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
    out.write("**THE VERDICT HAS TWO PARTS (fix3).** The FRACTION part is R2''s bar and is "
              "unchanged. The EVIDENCE part asks whether the log contains what R2' requires: a "
              "re-snapshot after every foreign `R`, and a snapshot between consecutive timed "
              "runs. A batch is PINNED-CLEAN only when both hold; otherwise the cell names which "
              "part failed, and both can fail at once.\n\n")
    out.write("| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | "
              "**cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | "
              "cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | "
              "evidence | verdict |\n")
    out.write("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n")
    for b in batches:
        egaps, _ = evidence(b)
        rs = pinned_runs(b)
        if not rs:
            out.write("| %s | %s | 0 | - | - | - | - | - | - | - | - | - | - | - | %s | "
                      "**UNPROVEN-EVIDENCE (no timed run bracketed)** |\n"
                      % (b["log"], b["name"],
                         "; ".join(egaps + ["no timed-run bracket"])))
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
        # THE FRACTION PART -- R2''s bar, computed exactly as before.
        good = f2 < THRESHOLD and f10 < THRESHOLD
        # THE EVIDENCE PART -- what the log contains (fix3, settler R13).
        which = []
        if not good:
            which.append("**UNPROVEN-FRACTION**")
        if egaps:
            which.append("**UNPROVEN-EVIDENCE**")
        if not good or egaps:
            ok = False
        out.write("| %s | %s | %d | %.2f | **%.3f** | **%.4f %%** | **%.3f** | **%.4f %%** | "
                  "%.3f | %.3f | %.2f | %.2f | %.3f | %.3f | %s | %s |\n" % (
                      b["log"], b["name"], len(rs), tot_wall, fo2, 100.0 * f2, fo10,
                      100.0 * f10, u2 + st2, u10, n2, selfcpu, max(0.0, s2 - selfsys), totk,
                      "; ".join(egaps) if egaps else "complete",
                      " + ".join(which) if which else "PINNED-CLEAN"))

    out.write("\n## The five busiest foreign processes per batch\n\n")
    if pids_rel:
        out.write("**Every foreign pid of every batch, with every state observed for it, is "
                  "`%s`** (settler R13). The five below are a summary of that file, not a "
                  "substitute for it.\n\n" % pids_rel)
    for d in details:
        if d[2] is None:
            out.write("- %s / %s: %s\n" % (d[0], d[1], d[3]))
            continue
        out.write("- %s / %s (window %.2f s): %s\n" % (
            d[0], d[1], d[2], "; ".join("pid %s %+.2f s" % (p, v) for v, p in d[3])))

    # ---- the evidence summary ------------------------------------------
    nfrac = nev = nclean = 0
    for b in batches:
        egaps, _ = evidence(b)
        rs = pinned_runs(b)
        if not rs:
            nev += 1
            continue
        tot_wall = sum(t[1] for t in rs)
        a2 = sum(t[6].get("cpu2", {}).get("task_all", 0.0) for t in rs)
        a10 = sum(t[6].get("cpu10", {}).get("task_all", 0.0) for t in rs)
        selfuser = sum(t[7]["user"] for t in rs)
        f2 = max(0.0, a2 - selfuser) / tot_wall if tot_wall else float("inf")
        f10 = a10 / tot_wall if tot_wall else float("inf")
        bad = not (f2 < THRESHOLD and f10 < THRESHOLD)
        if bad:
            nfrac += 1
        if egaps:
            nev += 1
        if not bad and not egaps:
            nclean += 1
    out.write("\n## The two kinds of UNPROVEN, counted (fix3)\n\n")
    out.write("| batches | PINNED-CLEAN | UNPROVEN-FRACTION | UNPROVEN-EVIDENCE |\n")
    out.write("|---:|---:|---:|---:|\n")
    out.write("| %d | %d | %d | %d |\n\n" % (len(batches), nclean, nfrac, nev))
    out.write("A batch failing both is counted in both unproven columns, so the three need not "
              "sum to the first.\n")

    # ---- the per-pid appendix ------------------------------------------
    if pids_out is not None:
        pids_out.write("# Every foreign pid of every batch, with every state observed for it\n\n")
        pids_out.write("Settler ruling R13 (W5 T8.9r fix round 3): R2' asks for every foreign pid "
                       "and state seen in any snapshot, listed -- not a count and a busiest-five. "
                       "This is that list, written by `scripts/idle_proof.py` from the same "
                       "snapshots the fractions are computed from.\n\n")
        pids_out.write("States are the UNION of `/proc/<pid>/stat`'s single character "
                       "(`FOREIGN_TICK`) and `ps`'s full string (`FOREIGN_PS`). A pid marked "
                       "**transient** was present in some snapshot of the batch but not in both "
                       "ends, so it has no CPU delta. `delta` is ticks/clk across the batch "
                       "window for pids present at both ends.\n")
        for b in batches:
            snaps = b["snaps"]
            pids_out.write("\n## `%s` / `%s`  (%d snapshots)\n\n" % (b["log"], b["name"], len(snaps)))
            if len(snaps) < 2:
                pids_out.write("Fewer than two snapshots; no delta is defined.\n")
            first = snaps[0]["pids"] if snaps else {}
            last = snaps[-1]["pids"] if snaps else {}
            clk = float(snaps[0]["clk"]) if snaps else 100.0
            allpids = sorted({p for s in snaps for p in s["states"]}, key=lambda v: int(v))
            pids_out.write("| pid | states seen | delta (s) | presence | command |\n")
            pids_out.write("|---|---|---|---|---|\n")
            for pid in allpids:
                st = sorted({x for s in snaps for x in s["states"].get(pid, ())})
                if pid in first and pid in last:
                    d = "%.2f" % ((last[pid][1] - first[pid][1]) / clk)
                    pres = "both ends"
                else:
                    d = "-"
                    pres = "**transient**"
                pids_out.write("| %s | `%s` | %s | %s | `%s` |\n" % (
                    pid, ",".join(st), d, pres, b["comms"].get(pid, "-")))
    return ok


def main():
    argv = sys.argv[1:]
    pidspath = pidsrel = None
    if "--pids" in argv:
        i = argv.index("--pids")
        pidspath = argv[i + 1]
        del argv[i:i + 2]
    if "--pids-link" in argv:
        i = argv.index("--pids-link")
        pidsrel = argv[i + 1]
        del argv[i:i + 2]
    if not argv or len(argv) < 2:
        sys.stderr.write("usage: idle_proof.py [--pids FILE] [--pids-link TEXT] LOG... OUT\n")
        return 2
    logs = argv[:-1]
    outpath = argv[-1]
    batches = []
    for path in sorted(logs):
        batches.extend(parse(path))
    if not batches:
        sys.stderr.write("idle_proof: no batches found in %d logs\n" % len(logs))
        return 1
    pids_out = open(pidspath, "w") if pidspath else None
    try:
        with open(outpath, "w") if outpath != "-" else sys.stdout as out:
            ok = report(batches, out, pids_out, pidsrel)
    finally:
        if pids_out is not None:
            pids_out.close()
    if not ok:
        sys.stderr.write("idle_proof: at least one batch is UNPROVEN\n")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
