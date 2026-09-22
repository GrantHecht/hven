#!/usr/bin/env python3
"""M7 W2 M5r -- build the rank probe by REPLAYING pass 1's probe build commands.

The probe needs NO test seam (it builds no solver), so -- unlike M4's and M5's
arm probes, which replayed the whole LIBRARY with -DHVEN_TESTING=1 -- it is
compiled against the INSTALLED hven exactly as M3c's pass-1 probe was: the four
compile edges and the one link edge of `.scratch/m7w2m3c/probebuild/build.ninja`
(the probe TU plus the harness bridge's three TUs), expanded from that tree's
own rules and handed to `/bin/sh -c` as Ninja would. The Ninja reader and
expander are imported from `replay_build_m4.py` beside this file, which is a
BYTE COPY of M4's (checked by sha256 below, never edited).

DECLARED DIFFERENCES, and nothing else:
  * the probe TU's SOURCE path, OBJECT path and DEP file (it is this task's
    probe, not pass 1's);
  * the link's TARGET_FILE (the executable's name) and the probe object in the
    link's inputs.
Every other token of every command -- compiler, DEFINES (including
-DEIGEN_MAX_ALIGN_BYTES=32), INCLUDES (the install's flattened headers, the
harness root, CUTEst's include), FLAGS, LINK_LIBRARIES (the INSTALLED
libhven.a, pinned by M3c's recorded sha) -- is compared token by token and a
residual difference REFUSES. The harness's own CMake is never entered;
hven-bench is read only (three of its sources are compiled, into this
directory).
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
M4_ORIG = os.path.join(HERE, "..", "..", "2026-09-m7-w2-m4-anticycling-arm", "wrappers",
                       "replay_build_m4.py")
sys.path.insert(0, HERE)
import replay_build_m4 as rb  # noqa: E402


def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--probebuild", required=True)
    ap.add_argument("--install-lib", required=True)
    ap.add_argument("--lib-pin", required=True, help="M3c's library-sha256.txt")
    ap.add_argument("--probe-src", required=True)
    ap.add_argument("--exe-name", default="m5r_rank_probe")
    ap.add_argument("--out", required=True)
    ap.add_argument("--transcript", required=True)
    a = ap.parse_args()

    tr = open(a.transcript, "w")

    def say(s=""):
        print(s)
        tr.write(s + "\n")

    mine, theirs = sha(os.path.join(HERE, "replay_build_m4.py")), sha(M4_ORIG)
    say(f"replay_build_m4.py (copy) sha256 {mine}")
    say(f"replay_build_m4.py (M4)   sha256 {theirs}")
    if mine != theirs:
        say("REFUSED: the Ninja reader is not a byte copy of M4's")
        return 96
    want = open(a.lib_pin).read().split()[0]
    got = sha(a.install_lib)
    say(f"installed libhven.a sha256 {got}")
    say(f"M3c's recorded pin         {want}")
    if want != got:
        say("REFUSED: the installed archive is not the one M3c pinned")
        return 94

    out = os.path.abspath(a.out)
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)
    rules, edges, _ = rb.parse_ninja(os.path.join(a.probebuild, "build.ninja"))
    comp = [e for e in edges if e["rule"].startswith("CXX_COMPILER__m3c_probe")]
    link = [e for e in edges if e["rule"].startswith("CXX_EXECUTABLE_LINKER__m3c_probe")]
    if len(comp) != 4 or len(link) != 1:
        say(f"REFUSED: expected 4 compile + 1 link edges, found {len(comp)} + {len(link)}")
        return 94
    link = link[0]
    local_src = os.path.join(out, os.path.basename(a.probe_src))
    shutil.copyfile(a.probe_src, local_src)
    probe_obj_new = "CMakeFiles/m3c_probe.dir/" + os.path.basename(a.probe_src) + ".o"
    probe_obj_old = None
    jobs = []
    for e in comp:
        icmd = rb.edge_command(rules, e)
        if e["ins"][0].endswith("/m3c_probe.cpp"):
            probe_obj_old = e["outs"][0]
            v = dict(e["vars"])
            v["DEP_FILE"] = probe_obj_new + ".d"
            e2 = dict(e, vars=v, ins=[local_src], outs=[probe_obj_new])
            rcmd = rb.edge_command(rules, e2)
            decl = {e["ins"][0], local_src, e["outs"][0], probe_obj_new,
                    e["vars"].get("DEP_FILE", ""), probe_obj_new + ".d"}
            kind = "PROBE TU"
            o = probe_obj_new
        else:
            rcmd, decl, kind, o = icmd, set(), "BRIDGE  ", e["outs"][0]
        d = rb.diff_tokens(icmd, rcmd, decl)
        say(f"  {kind} {os.path.basename(e['ins'][0])}: "
            f"{'RESIDUAL DIFFERENCE' if d else 'no residual difference'}"
            + ("  (declared: source, object, dep file)" if decl else ""))
        for line in d:
            say(f"      {line}")
        if d:
            say("REFUSED")
            return 92
        jobs.append((o, rcmd))
    say(f"  one command in full: {jobs[0][1]}")
    if "-DEIGEN_MAX_ALIGN_BYTES=32" not in jobs[0][1]:
        say("REFUSED: -DEIGEN_MAX_ALIGN_BYTES=32 is not on the probe TU's command")
        return 92
    for o, cmd in jobs:
        os.makedirs(os.path.join(out, os.path.dirname(o)), exist_ok=True)
        rc, txt = rb.run_cmd(cmd, out)
        if rc != 0:
            say(f"REFUSED: compile of {o} failed rc={rc}\n{txt[-3000:]}")
            return 92
    say(f"  {len(jobs)} objects produced")

    ins = [probe_obj_new if i == probe_obj_old else i for i in link["ins"]]
    icmd = rb.edge_command(rules, link)
    lcmd = rb.edge_command(rules, dict(link, ins=ins),
                           extra={"TARGET_FILE": a.exe_name, "in": " ".join(ins)})
    d = rb.diff_tokens(icmd, lcmd, {probe_obj_old, probe_obj_new, "m3c_probe", a.exe_name})
    say(f"  LINK: {'RESIDUAL DIFFERENCE' if d else 'no residual difference'} "
        "(declared: probe object, target name)")
    if d:
        for line in d:
            say(f"      {line}")
        return 95
    if os.path.abspath(a.install_lib) not in lcmd:
        say("REFUSED: the link does not name the installed libhven.a")
        return 95
    say(f"  {lcmd}")
    rc, txt = rb.run_cmd(lcmd, out)
    if rc != 0:
        say(f"REFUSED: link failed rc={rc}\n{txt[-3000:]}")
        return 95
    exe = os.path.join(out, a.exe_name)
    say(f"BINARY {exe}")
    say(f"BINARY_SHA256 {sha(exe)}")
    tr.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
