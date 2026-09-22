#!/usr/bin/env python3
"""M7 W2 M3c RULE-P2A section 3 -- build an arm's probe by REPLAYING, byte for
byte, the compile commands the install build and pass 1's own probe build ran.

Nothing here approximates a flag. The install build tree's Ninja files are read,
each edge's command is expanded against its rule exactly as Ninja expands it
(`$DEFINES $INCLUDES $FLAGS -MD -MT $out -MF $DEP_FILE -o $out -c $in`, the
`${LAUNCHER}` ccache prefix included), and the resulting STRING is handed to
`/bin/sh -c`, which is how Ninja runs it. The arm directory mirrors the install
tree's relative object layout and the replay runs with that directory as its
cwd, so the command string is IDENTICAL -- output path included -- rather than
merely equivalent.

The single permitted difference, on arms B/C only, is `-DHVEN_TESTING=1` added
to each of hven's 42 compile commands, inserted where CMake's own alphabetical
DEFINES ordering would have put it. Every command line pair is written to the
transcript and compared token by token after removing that one token; a single
residual difference is a REFUSAL and this script exits non-zero.

Nothing under src/ or include/ is touched. The harness tree is READ-ONLY (three
of its sources are compiled, exactly the three pass 1 compiled). No solver runs.
"""

import argparse
import concurrent.futures
import hashlib
import os
import re
import shutil
import subprocess
import sys

# --------------------------------------------------------------------------
# A minimal Ninja reader: enough for CMake's generated files, and refusing
# rather than guessing on anything it does not understand.
# --------------------------------------------------------------------------

_VAR_RE = re.compile(r"\$(?:\{(\w+)\}|(\w+)|(\$))")


def _unfold(text):
    """Join Ninja's `$`-at-end-of-line continuations."""
    out = []
    pending = ""
    for raw in text.split("\n"):
        line = pending + raw
        pending = ""
        if line.endswith("$") and not line.endswith("$$"):
            pending = line[:-1].rstrip() + " "
            continue
        out.append(line)
    if pending:
        out.append(pending)
    return out


def parse_ninja(path, _seen=None):
    """Return (rules, edges, globals). `include` directives are followed."""
    _seen = _seen if _seen is not None else set()
    path = os.path.abspath(path)
    if path in _seen:
        return {}, [], {}
    _seen.add(path)
    with open(path, "r") as fh:
        lines = _unfold(fh.read())
    base = os.path.dirname(path)
    rules, edges, gvars = {}, [], {}
    cur = None  # ("rule", name) or ("build", index)
    for line in lines:
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line[0] in " \t":
            key, _, val = line.strip().partition("=")
            key, val = key.strip(), val.strip()
            if cur is None:
                continue
            if cur[0] == "rule":
                rules[cur[1]][key] = val
            else:
                edges[cur[1]]["vars"][key] = val
            continue
        cur = None
        if line.startswith("rule "):
            name = line[5:].strip()
            rules[name] = {}
            cur = ("rule", name)
        elif line.startswith("build "):
            body = line[6:]
            # Split outputs from the rest at the first UNESCAPED colon.
            i, depth = 0, None
            while i < len(body):
                if body[i] == "$":
                    i += 2
                    continue
                if body[i] == ":":
                    depth = i
                    break
                i += 1
            if depth is None:
                raise SystemExit(f"REFUSED: unparseable build line in {path}: {line[:80]}")
            outs = body[:depth].split()
            rest = body[depth + 1:].split()
            if not rest:
                raise SystemExit(f"REFUSED: build line with no rule in {path}")
            rule = rest[0]
            ins, implicit, order = [], [], []
            bucket = ins
            for tok in rest[1:]:
                if tok == "|":
                    bucket = implicit
                elif tok == "||":
                    bucket = order
                else:
                    bucket.append(tok)
            edges.append({"outs": outs, "rule": rule, "ins": ins,
                          "implicit": implicit, "order": order, "vars": {}})
            cur = ("build", len(edges) - 1)
        elif line.startswith("include ") or line.startswith("subninja "):
            sub = line.split(None, 1)[1].strip()
            sub = sub if os.path.isabs(sub) else os.path.join(base, sub)
            if os.path.exists(sub):
                r2, e2, g2 = parse_ninja(sub, _seen)
                rules.update(r2)
                edges.extend(e2)
                gvars.update(g2)
        else:
            key, sep, val = line.partition("=")
            if sep:
                gvars[key.strip()] = val.strip()
    return rules, edges, gvars


def expand(template, scope):
    """Ninja's own single-pass `$VAR` expansion; unknown variables are empty."""
    def sub(m):
        if m.group(3):
            return "$"
        name = m.group(1) or m.group(2)
        return scope.get(name, "")
    return _VAR_RE.sub(sub, template)


def edge_command(rules, edge, extra=None):
    rule = rules.get(edge["rule"])
    if rule is None or "command" not in rule:
        raise SystemExit(f"REFUSED: no command for rule {edge['rule']}")
    scope = dict(edge["vars"])
    scope["in"] = " ".join(edge["ins"])
    scope["out"] = " ".join(edge["outs"])
    if extra:
        scope.update(extra)
    return expand(rule["command"], scope)


# --------------------------------------------------------------------------
# The replay itself
# --------------------------------------------------------------------------

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def insert_define(defines, token):
    """Insert -DHVEN_TESTING=1 where CMake's alphabetical DEFINES order puts it."""
    toks = defines.split()
    key = token[2:].split("=")[0]
    out, placed = [], False
    for t in toks:
        if not placed and t.startswith("-D") and t[2:].split("=")[0] > key:
            out.append(token)
            placed = True
        out.append(t)
    if not placed:
        out.append(token)
    return " ".join(out)


def diff_tokens(a, b, drop):
    """Token sequences of `a` and `b` after removing every token in `drop`."""
    ta = [t for t in a.split() if t not in drop]
    tb = [t for t in b.split() if t not in drop]
    if ta == tb:
        return []
    out = []
    import difflib
    for line in difflib.unified_diff(ta, tb, "install", "replay", lineterm="", n=0):
        out.append(line)
    return out


def run_cmd(cmd, cwd):
    proc = subprocess.run(["/bin/sh", "-c", cmd], cwd=cwd,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return proc.returncode, proc.stdout.decode("utf-8", "replace")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--arm", required=True, choices=["A", "B"])
    ap.add_argument("--rel", required=True, help="the install build tree (build.ninja)")
    ap.add_argument("--probebuild", required=True, help="pass 1's probe build tree")
    ap.add_argument("--install-lib", required=True, help="the installed libhven.a")
    ap.add_argument("--probe-src", required=True, help="wrappers/m3c_p2_probe.cpp")
    ap.add_argument("--out", required=True, help="the arm directory to build in")
    ap.add_argument("--transcript", required=True, help="the harvest-diff transcript (appended)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    args = ap.parse_args()

    for p in (args.rel, args.probebuild, args.install_lib, args.probe_src):
        if not os.path.exists(p):
            print(f"REFUSED: missing {p}")
            return 90

    testing = args.arm == "B"
    armdir = os.path.abspath(args.out)
    if os.path.exists(armdir):
        shutil.rmtree(armdir)
    os.makedirs(armdir)

    tr = open(args.transcript, "a")

    def say(msg=""):
        print(msg)
        tr.write(msg + "\n")

    say("=" * 74)
    say(f"ARM {args.arm}   HVEN_TESTING on hven's sources: {'YES' if testing else 'NO'}")
    say(f"install build tree : {os.path.abspath(args.rel)}")
    say(f"installed libhven.a: {os.path.abspath(args.install_lib)}")
    say(f"                sha: {sha256(args.install_lib)}")
    say(f"pass-1 probe build : {os.path.abspath(args.probebuild)}")
    say(f"arm directory      : {armdir}")
    say("=" * 74)

    rules, edges, _ = parse_ninja(os.path.join(args.rel, "build.ninja"))
    comp = [e for e in edges if e["rule"].startswith("CXX_COMPILER__hven")]
    arch = [e for e in edges if e["rule"].startswith("CXX_STATIC_LIBRARY_LINKER__hven")]
    if len(arch) != 1:
        say(f"REFUSED: expected exactly one hven archive edge, found {len(arch)}")
        return 91
    arch = arch[0]
    members = list(arch["ins"])
    tu_edges = [e for e in comp if e["outs"][0] in set(members)]
    other = [e for e in comp if e["outs"][0] not in set(members)]
    say(f"hven compile edges in the install build : {len(comp)}")
    say(f"objects in the install archive edge     : {len(members)}")
    say(f"compile edges that are NOT archive members: {len(other)}  "
        f"{[e['outs'][0] for e in other]}")
    if len(tu_edges) < 30:
        say("REFUSED: fewer than 30 hven TUs -- that is not the library")
        return 91
    if sorted(e["outs"][0] for e in tu_edges) != sorted(members):
        say("REFUSED: the set of compiled TUs is not the set of archive members")
        return 91
    say("the set of compiled TUs IS the set of archive members (both directions)")
    for e in other:
        if not e["outs"][0].endswith(".pch"):
            say(f"REFUSED: an hven compile edge that is neither an archive member nor a "
                f"precompiled header: {e['outs'][0]}")
            return 91
    say()
    say("--- THE PRECOMPILED HEADER, found by that refusal and handled explicitly")
    say(f"    src/CMakeLists.txt:332 target_precompile_headers(hven PRIVATE hven_pch.h),")
    say(f"    with a per-source opt-in list. In this build tree {len(other)} PCH edge(s) exist and")
    pch_consumers = [e["outs"][0] for e in tu_edges
                     if "include-pch" in e["vars"].get("FLAGS", "")]
    say(f"    {len(pch_consumers)} TUs consume the PCH by ABSOLUTE path into the install tree:")
    for c in pch_consumers:
        say(f"      {c}")
    rel_abs = os.path.abspath(args.rel)
    pch_abs_install = os.path.join(rel_abs, "CMakeFiles/hven.dir/cmake_pch.hxx.pch")
    pch_abs_arm = os.path.join(armdir, "CMakeFiles/hven.dir/cmake_pch.hxx.pch")
    if testing:
        say("    ARM B: clang REFUSES a TU whose macro state differs from the PCH it includes,")
        say("    so arm B's PCH is rebuilt from the SAME command plus the SAME single define,")
        say("    and its four consumers are pointed at arm B's own PCH. That path token is the")
        say("    SECOND declared difference on arm B and it is declared here, before any leg;")
        say("    it is a path, not a flag, and the PCH behind it differs from the install's by")
        say("    exactly the one define. RULE-P2A section 3.3's 'and nothing else' is amended by")
        say("    AMENDMENT 1 in RULE-P2A.md, committed before any leg ran.")
    else:
        say("    ARM A: the commands are replayed unchanged, so its four consumers read the")
        say("    INSTALL tree's own PCH -- exactly the file the install build read.")

    # ---- hven's TUs -------------------------------------------------------
    define = "-DHVEN_TESTING=1"
    jobs = []
    for e in other + tu_edges:
        install_cmd = edge_command(rules, e)
        vars2 = dict(e["vars"])
        if testing:
            vars2["DEFINES"] = insert_define(vars2.get("DEFINES", ""), define)
            if pch_abs_install in vars2.get("FLAGS", ""):
                vars2["FLAGS"] = vars2["FLAGS"].replace(pch_abs_install, pch_abs_arm)
        e2 = dict(e, vars=vars2)
        replay_cmd = edge_command(rules, e2)
        jobs.append((e["outs"][0], e["ins"][0], install_cmd, replay_cmd))

    drop = {define, pch_abs_install, pch_abs_arm} if testing else set()
    say()
    say(f"--- THE {len(jobs)} COMPILE COMMANDS, install vs replay")
    say(f"    excluded from the comparison: {sorted(drop) if drop else '(nothing)'}")
    say("    output paths are NOT excluded: the arm directory mirrors the install")
    say("    tree's relative layout, so the strings are identical, not equivalent.")
    residual = 0
    for out, src, icmd, rcmd in jobs:
        d = diff_tokens(icmd, rcmd, drop)
        if d:
            residual += 1
            say(f"  DIFFERS {out}")
            for line in d:
                say(f"      {line}")
    say(f"    TUs with a residual difference: {residual}  (must be 0)")
    if residual:
        say("REFUSED: the replay is not the install's command line. STOP.")
        return 92
    say("    one example, in full:")
    say(f"      SRC     {jobs[-1][1]}")
    say(f"      INSTALL {jobs[-1][2]}")
    say(f"      REPLAY  {jobs[-1][3]}")

    for out, _, _, _ in jobs:
        os.makedirs(os.path.join(armdir, os.path.dirname(out)), exist_ok=True)

    say()
    say(f"--- COMPILING: {len(other)} PCH edge(s) first, then {len(tu_edges)} TUs "
        f"(cwd = the arm directory)")
    for out, _, _, rcmd in jobs[:len(other)]:
        rc, outp = run_cmd(rcmd, armdir)
        if rc != 0:
            say(f"REFUSED: the PCH replay failed rc={rc} for {out}\n{outp[-2000:]}")
            return 92
        say(f"    {out}  sha256 {sha256(os.path.join(armdir, out))}")
        inst = os.path.join(args.rel, out)
        if os.path.exists(inst):
            say(f"    install's own        sha256 {sha256(inst)}   identical: "
                f"{sha256(inst) == sha256(os.path.join(armdir, out))}")
    failures = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futs = {ex.submit(run_cmd, j[3], armdir): j for j in jobs[len(other):]}
        for f in concurrent.futures.as_completed(futs):
            rc, outp = f.result()
            if rc != 0:
                failures.append((futs[f][0], rc, outp[-800:]))
    if failures:
        for name, rc, outp in failures:
            say(f"  FAILED rc={rc} {name}")
            say(outp)
        say("REFUSED: a replayed compile failed. STOP.")
        return 92
    say(f"    all {len(tu_edges)} objects produced")
    jobs = jobs[len(other):]

    # ---- object identity against the install's own objects ---------------
    say()
    say("--- OBJECT IDENTITY vs the install build's own objects (RULE-P2A section 3.4 #1)")
    same = absent = differ = 0
    for out, _, _, _ in jobs:
        a = os.path.join(armdir, out)
        b = os.path.join(args.rel, out)
        if not os.path.exists(b):
            absent += 1
            continue
        if sha256(a) == sha256(b):
            same += 1
        else:
            differ += 1
    say(f"    byte-identical: {same} / {len(jobs)}   differing: {differ}   "
        f"install object absent: {absent}")
    say(f"    (arm A expects {len(jobs)} identical; arm B expects 0 -- the define changes the code)")

    # ---- the archive ------------------------------------------------------
    libname = f"libhven_p2a_arm{args.arm}.a"
    acmd = edge_command(rules, arch, extra={"TARGET_FILE": libname})
    say()
    say("--- THE ARCHIVE, replayed from the install's own archive edge")
    say(f"    {acmd}")
    rc, outp = run_cmd(acmd, armdir)
    if rc != 0:
        say(f"REFUSED: the archive replay failed rc={rc}\n{outp}")
        return 92
    libpath = os.path.join(armdir, libname)
    say(f"    sha256 {sha256(libpath)}  {libname}")
    say(f"    sha256 {sha256(args.install_lib)}  the installed libhven.a")
    say(f"    archives byte-identical: {sha256(libpath) == sha256(args.install_lib)}")

    # ---- the seam is where it is claimed to be ---------------------------
    say()
    say("--- THE SEAM (RULE-P2A section 3.4 #2): QpWalkObserver symbols in this arm's archive")
    nm = subprocess.run(["nm", "-C", libpath], stdout=subprocess.PIPE,
                        stderr=subprocess.DEVNULL).stdout.decode("utf-8", "replace")
    nsym = sum(1 for ln in nm.split("\n") if "QpWalkObserver" in ln)
    say(f"    count: {nsym}")
    if testing and nsym < 1:
        say("REFUSED: arm B's archive carries no observer symbol -- the define did not arm the seam")
        return 93
    if not testing and nsym != 0:
        say("REFUSED: arm A's archive carries an observer symbol -- it is not a plain build")
        return 93
    say("    as required: 0 on arm A, >= 1 on arm B. Both directions are asserted.")

    # ---- the probe's own TUs and the link, replayed from pass 1's tree ----
    prules, pedges, _ = parse_ninja(os.path.join(args.probebuild, "build.ninja"))
    pcomp = [e for e in pedges if e["rule"].startswith("CXX_COMPILER__m3c_probe")]
    plink = [e for e in pedges if e["rule"].startswith("CXX_EXECUTABLE_LINKER__m3c_probe")]
    if len(plink) != 1 or len(pcomp) != 4:
        say(f"REFUSED: pass 1's probe tree has {len(pcomp)} compile edges and "
            f"{len(plink)} link edges; expected 4 and 1")
        return 94
    plink = plink[0]

    say()
    say("--- THE PROBE'S OWN TUs, replayed from pass 1's probe build tree")
    probe_src_abs = os.path.abspath(args.probe_src)
    local_src = os.path.join(armdir, "m3c_p2_probe.cpp")
    shutil.copyfile(probe_src_abs, local_src)
    probe_obj = None
    pjobs = []
    for e in pcomp:
        src = e["ins"][0]
        icmd = edge_command(prules, e)
        is_probe = src.endswith("m3c_probe.cpp")
        if is_probe:
            # The probe's own source NECESSARILY differs: this is the pass-2
            # probe, not pass 1's. Its object, dep file and the HVEN_TESTING
            # define are the declared differences and nothing else moves.
            out = "CMakeFiles/m3c_probe.dir/m3c_p2_probe.cpp.o"
            vars2 = dict(e["vars"])
            vars2["DEFINES"] = insert_define(vars2.get("DEFINES", ""), define)
            vars2["DEP_FILE"] = out + ".d"
            e2 = dict(e, vars=vars2, ins=[local_src], outs=[out])
            rcmd = edge_command(prules, e2)
            probe_obj = out
            decl = {define, local_src, src, out, e["outs"][0],
                    e["vars"].get("DEP_FILE", ""), out + ".d"}
        else:
            e2 = e
            rcmd = icmd
            out = e["outs"][0]
            decl = set()
        d = diff_tokens(icmd, rcmd, decl)
        say(f"  {'PROBE TU' if is_probe else 'BRIDGE  '} {os.path.basename(src)}")
        if decl:
            say(f"      declared differences: source file, object path, dep file, {define}")
        if d:
            say("      RESIDUAL DIFFERENCE:")
            for line in d:
                say(f"        {line}")
            say("REFUSED: a probe-side command differs beyond what is declared. STOP.")
            return 94
        say("      no residual difference")
        pjobs.append((out, rcmd))

    if probe_obj is None:
        say("REFUSED: pass 1's probe tree has no edge for the probe's own source")
        return 94
    for out, _ in pjobs:
        os.makedirs(os.path.join(armdir, os.path.dirname(out)), exist_ok=True)
    for out, cmd in pjobs:
        rc, outp = run_cmd(cmd, armdir)
        if rc != 0:
            say(f"REFUSED: compiling {out} failed rc={rc}\n{outp[-2000:]}")
            return 94
    say(f"    {len(pjobs)} probe-side objects produced")

    # ---- the link ---------------------------------------------------------
    exe = f"m3c_p2a_probe_arm{args.arm}"
    ll = plink["vars"].get("LINK_LIBRARIES", "")
    install_lib_abs = os.path.abspath(args.install_lib)
    if install_lib_abs not in ll:
        say(f"REFUSED: pass 1's link line does not name {install_lib_abs}")
        return 95
    ll2 = ll.replace(install_lib_abs, libpath)
    objs = [probe_obj if o.endswith("m3c_probe.cpp.o") and "dir/m3c_probe" in o else o
            for o in plink["ins"]]
    lcmd = edge_command(prules, dict(plink, ins=objs),
                        extra={"TARGET_FILE": exe, "LINK_LIBRARIES": ll2,
                               "in": " ".join(objs)})
    say()
    say("--- THE LINK (RULE-P2A section 3.4 #3)")
    say(f"    {lcmd}")
    if install_lib_abs in lcmd:
        say("REFUSED: this arm's link names the INSTALLED libhven.a")
        return 95
    say("    the link names this arm's own replayed archive and NOT the installed libhven.a")
    probe_pattern = install_lib_abs
    constructed = f"clang++ -o x foo.o {probe_pattern} -lm"
    if probe_pattern not in constructed:
        say("REFUSED: the no-installed-library check cannot match a line that DOES name it")
        return 95
    say("    non-vacuity: the same test DOES match a constructed line naming the installed path")
    rc, outp = run_cmd(lcmd, armdir)
    if rc != 0:
        say(f"REFUSED: the link failed rc={rc}\n{outp[-3000:]}")
        return 95
    exepath = os.path.join(armdir, exe)
    say(f"    binary {exepath}")
    say(f"    sha256 {sha256(exepath)}")
    nm = subprocess.run(["nm", "-C", exepath], stdout=subprocess.PIPE,
                        stderr=subprocess.DEVNULL).stdout.decode("utf-8", "replace")
    say(f"    QpWalkObserver symbols in the binary: "
        f"{sum(1 for ln in nm.split(chr(10)) if 'QpWalkObserver' in ln)}"
        " (the probe's own TU defines them in every arm; see RULE-P2A section 1)")
    say(f"ARM {args.arm} BUILD OK")
    say()
    tr.close()
    print(f"ARM_{args.arm}_BINARY={exepath}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
