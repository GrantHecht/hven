#!/usr/bin/env python3
"""M7 WX2 gates: the OBJECT CENSUS (runner structural addition W2-S7).

WHY IT EXISTS. P-SYM (scripts/psym_compare.sh) captures a fixed universe of build targets and the runner deliberately keeps the
seam executables out of it. This range's CF-1-R1 puts EIGEN_MAX_ALIGN_BYTES=32 on the vendored Eigen target, which changes the compile
command of every translation unit that includes Eigen without linking hven::hven -- the golden-rig support, the fault-injection /
IPQP / QP-walk / SQP seam executables -- and in a Debug build (where SIMD flags are absent) that changes the Eigen allocator variant
those objects inline. P-SYM cannot see them, so this tool covers EVERY object of the build tree, both arms, both configurations:

  inputs  commands-{before,after}   `ninja -t commands all` of each arm (same build directory)
          sha-{before,after}        `sha256sum` of every .o of each arm
          deps-after                `ninja -t deps` of the after arm
          unity dir                 the generated Unity/*.cxx of the after arm (constituent lists)
          --changed-sources / --changed-headers   the runner's git-derived lists (repo-relative)
  classes (an object whose bytes DIFFER must carry at least one; UNEXPLAINED = carries none):
          M  its compile source (or, for a unity object, one of the unity constituents) is MODIFIED in the range
          C  its compile command changed by the declared token set (`-DEIGEN_MAX_ALIGN_BYTES=32`): the CF-1-R1 class
          B  its after-arm dependency list names a changed header (a line-shift candidate, as the P-SYM class B)
          S  its compile command carries a commit stamp (-DNAME="<12 hex>", e.g. HVEN_RIG_COMMIT) whose VALUE differs between the arms by construction and is each arm's own commit
  C_pure_differing = objects whose bytes differ and whose ONLY class is C (the CF-1-R1 effect alone: no modified source, no changed header, no commit stamp); declared per configuration.
  eigen_delta = objects whose compile command gained -DEIGEN_MAX_ALIGN_BYTES=32; declared per configuration.
  fail-closed refusals (exit 1): the object sets of the two arms differ; a command delta that is not purely the declared added
          tokens; a command delta in a token that is declared (below) but REMOVED; an object whose bytes differ with no class;
          the declared expectations (--expect) not equal to the measurement.
  Added tokens declared admissible: -DEIGEN_MAX_ALIGN_BYTES=32 (class C) and -DHVEN_TEST_REFERENCE_DISPATCH=<0|1> (WX-R13's
          add_compile_definitions: it only reaches TUs that include route_reference.h, and those are MODIFIED or class B anyway;
          an object that carries ONLY this token delta and differs is UNEXPLAINED unless it is class M or B).
usage: object_census_wx2.py CFG REPO UNIVERSE_MANIFEST BEFORE_CMDS AFTER_CMDS BEFORE_SHA AFTER_SHA AFTER_DEPS UNITY_DIR BUILD_DIR \
         --changed-sources a b .. --changed-headers h1 h2 .. --before-sha FULLSHA --after-sha FULLSHA --expect-eigen-delta N --expect-c-pure-differing N
"""
import hashlib, os, re, shlex, sys

STAMP_RE = re.compile(r'^(-D[A-Za-z0-9_]+)=\\?"?([0-9a-f]{12})\\?"?$')   # a commit stamp (e.g. HVEN_SQP_CORPUS_GIT_DESCRIBE, HVEN_RIG_COMMIT): -DNAME="<12 hex>"; its VALUE differs between the arms by construction (class S), and is checked to be the arm's own commit

def die(msg, rc=1):
    print("CENSUS_REFUSED: " + msg); sys.exit(rc)

def parse_cmds(path, build):
    out = {}
    for line in open(path, errors="replace"):
        if " -c " not in line or " -o " not in line:
            continue
        t = shlex.split(line)
        o = t[t.index("-o") + 1]
        if not o.endswith(".o"):
            continue   # a precompiled header (.pch) is an intermediate; every object that uses it is censused itself
        src = t[len(t) - 1 - t[::-1].index("-c") + 1]
        if o in out:
            die("object %s has two compile commands" % o)
        toks = [x for i, x in enumerate(t) if not (i and t[i-1] in ("-o", "-MF", "-MT")) and x not in ("-o", "-MF", "-MT")]
        stamp = [x for x in toks if STAMP_RE.match(x)]
        toks = sorted(STAMP_RE.match(x).group(1) + "=<stamp>" if STAMP_RE.match(x) else x for x in toks)
        stamp = tuple(STAMP_RE.match(x).group(2) for x in stamp)
        out[o] = (src, toks, tuple(stamp))
    return out

def parse_sha(path):
    d = {}
    for line in open(path):
        h, p = line.rstrip("\n").split("  ", 1)
        d[os.path.normpath(p)] = h
    return d

def parse_deps(path):
    d, cur = {}, None
    for line in open(path, errors="replace"):
        if line.startswith(" ") or line.startswith("\t"):
            if cur is not None:
                d[cur].add(os.path.normpath(line.strip()))
        elif ": #deps" in line:
            cur = os.path.normpath(line.split(": #deps")[0]); d[cur] = set()
    return d

def main():
    a = sys.argv[1:]
    if len(a) < 10: die("usage", 2)
    cfg, repo, universe, bc, ac, bs, as_, ad, unity, build = a[:10]
    rest = a[10:]
    cs, ch, ec_d, ec_i = [], [], None, None
    before_sha = after_sha = None
    mode = None
    i = 0
    while i < len(rest):
        x = rest[i]
        if x == "--changed-sources": mode = "s"
        elif x == "--changed-headers": mode = "h"
        elif x == "--before-sha": before_sha = rest[i+1]; i += 1; mode = None
        elif x == "--after-sha": after_sha = rest[i+1]; i += 1; mode = None
        elif x == "--expect-eigen-delta": ec_i = int(rest[i+1]); i += 1; mode = None
        elif x == "--expect-c-pure-differing": ec_d = int(rest[i+1]); i += 1; mode = None
        elif mode == "s": cs.append(x)
        elif mode == "h": ch.append(x)
        i += 1
    if before_sha is None or after_sha is None: die("--before-sha and --after-sha are required", 2)
    if ec_d is None or ec_i is None: die("--expect-eigen-delta and --expect-c-pure-differing are required", 2)
    if not cs or not ch: die("empty changed-source or changed-header list")
    repo = os.path.realpath(repo)
    changed_src = {os.path.normpath(os.path.join(repo, p)) for p in cs}
    changed_hdr = {os.path.normpath(os.path.join(repo, p)) for p in ch}
    uni = {os.path.normpath(l.strip()) for l in open(universe) if l.strip()}
    B, A = parse_cmds(bc, build), parse_cmds(ac, build)
    SB, SA = parse_sha(bs), parse_sha(as_)
    DEPS = parse_deps(ad)
    if not B or not A: die("no compile commands parsed")
    if set(A) != set(B): die("the object sets of the two arms differ: only-before %s only-after %s" % (sorted(set(B) - set(A))[:5], sorted(set(A) - set(B))[:5]))
    if set(SA) != set(SB) or not SA: die("the sha manifests of the two arms differ or are empty")
    objs = sorted(A)
    for o in objs:
        if os.path.normpath(o) not in SA: die("object %s has a command but no sha entry" % o)
    unity_members = {}
    if os.path.isdir(unity):
        for root, _, fs in os.walk(unity):
            for f in fs:
                if f.endswith(".cxx"):
                    rel = os.path.normpath(os.path.relpath(os.path.join(root, f), unity))
                    unity_members[rel] = [os.path.normpath(m) for m in re.findall(r'#include\s+"([^"]+)"', open(os.path.join(root, f), errors="replace").read())]
    ADMIT_ADD = re.compile(r"^-DEIGEN_MAX_ALIGN_BYTES=32$|^-DHVEN_TEST_REFERENCE_DISPATCH=[01]$")
    n_delta = n_c = n_diff = 0
    rows, unexpl = [], []
    c_diff = c_same = 0
    for o in objs:
        tok_b, tok_a = B[o][1], A[o][1]
        src = A[o][0]
        if os.path.normpath(B[o][0]) != os.path.normpath(A[o][0]): die("object %s compiles a different source in the two arms" % o)
        add = list(tok_a); rem = []
        for t in tok_b:
            if t in add: add.remove(t)
            else: rem.append(t)
        if rem: die("object %s: tokens REMOVED from its compile command: %s" % (o, rem))
        bad = [t for t in add if not ADMIT_ADD.match(t)]
        if bad: die("object %s: compile-command tokens ADDED beyond the declared set: %s" % (o, bad))
        for v in B[o][2]:
            if not before_sha.startswith(v): die("object %s: before-arm stamp %s is not a prefix of the BEFORE commit %s" % (o, v, before_sha))
        for v in A[o][2]:
            if not after_sha.startswith(v): die("object %s: after-arm stamp %s is not a prefix of the HEAD commit %s" % (o, v, after_sha))
        has_s = B[o][2] != A[o][2]
        has_c = any(t.startswith("-DEIGEN_MAX_ALIGN_BYTES=") for t in add)
        if add: n_delta += 1
        if has_c: n_c += 1
        differs = SA[os.path.normpath(o)] != SB[os.path.normpath(o)]
        src_abs = os.path.normpath(src if os.path.isabs(src) else os.path.join(build, src))
        src_rel = os.path.normpath(os.path.relpath(src_abs, build))
        cands = [src_abs] + unity_members.get(src_rel, [])
        is_m = any(c in changed_src for c in cands)
        dps = DEPS.get(os.path.normpath(o), set())
        is_b = bool(dps & changed_hdr)
        if differs: n_diff += 1
        if has_c and differs: c_diff += 1
        if has_c and not differs: c_same += 1
        cls = "".join(k for k, f in (("M", is_m), ("C", has_c), ("B", is_b), ("S", has_s)) if f) or "-"
        rows.append((o, differs, cls, os.path.normpath(o) in uni))
        if differs and cls == "-": unexpl.append(o)
    n_out_diff = sum(1 for r in rows if r[1] and not r[3])
    print("CENSUS_%s objects=%d universe=%d command_delta=%d eigen_delta=%d differing=%d differing_outside_universe=%d" %
          (cfg, len(objs), len([r for r in rows if r[3]]), n_delta, n_c, n_diff, n_out_diff))
    for o, d, c, u in rows:
        if d: print("CENSUS_DIFFERS %s %s %s %s" % (cfg, "universe" if u else "outside", c, o))
    for o, d, c, u in rows:
        if "C" in c and not d: print("CENSUS_C_IDENTICAL %s %s %s" % (cfg, "universe" if u else "outside", o))
    c_pure = sum(1 for r in rows if r[1] and r[2] == "C")
    print("CENSUS_%s C_differing=%d C_identical=%d C_pure_differing=%d UNEXPLAINED=%d" % (cfg, c_diff, c_same, c_pure, len(unexpl)))
    for o in unexpl: print("CENSUS_UNEXPLAINED %s %s" % (cfg, o))
    if unexpl: die("%d object(s) differ with no class" % len(unexpl))
    if (c_pure, n_c) != (ec_d, ec_i):
        die("C_pure_differing / eigen_delta measured %d / %d, declared %d / %d" % (c_pure, n_c, ec_d, ec_i))
    print("CENSUS_%s_OK" % cfg)

if __name__ == "__main__":
    main()
