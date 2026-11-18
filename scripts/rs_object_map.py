#!/usr/bin/env python3
"""Generate a `psym_compare.sh --object-map` file from a move map.

The code-organisation restructure (RS) moves and renames source files.  A moved
translation unit's object file moves with it, so P-SYM (scripts/psym_compare.sh)
has to be told which new object is the old one's counterpart.  Writing that by
hand for ~350 files is how an instrument drifts, so this script derives it.

usage:
  rs_object_map.py --move-map MAP --before MANIFEST --after MANIFEST
                   [--target-map OLD=NEW ...] [--phase PREFIX ...]
                   [--unity-dir-move OLD=NEW ...] [-o OUT]

  MAP        one `old -> new` per line (`old -> DELETE ...` for a deletion);
             paths are repo-relative source paths.  Blank lines and `#` lines
             are ignored.
  MANIFEST   the `.psym-manifest` of a P-SYM snapshot: one build-relative
             object path per line (and `libhven.a`, which is ignored here).
  --target-map  a CMake target that was renamed between the two builds.
  --phase    restrict the move map to entries whose OLD path starts with one of
             these prefixes (a group of the restructure applies its own moves
             only; entries outside it are treated as unmoved).

How an object is paired.  An object path under a target directory is
`<dir>/CMakeFiles/<target>.dir/<source-relative-to-dir>.o`, with `..` spelled
`__`.  That gives every object a KEY, (target, repo-relative source path).  The
key of the old object is pushed through the move map, and the object carrying
the new key in the after manifest is its counterpart.  Output lines are
`<old object> => <new object>`, only for objects whose build-relative path
changed; an object whose key and path are both unchanged needs no line.

Everything must be accounted for, and any gap is an ERROR (exit 1):
  * a before object whose new key is absent from the after manifest, unless
    the move map deleted its source;
  * an after object that no before object maps to (a NEW object);
  * two before objects that map to one after object;
  * a unity-build object (`Unity/...`), which carries no source path, whose
    path is not identical in both manifests -- unless the difference is exactly
    a directory move declared with --unity-dir-move OLD=NEW (the CMake directory
    of the unity target moved, e.g. tests/interior -> tests/ipm); such an object
    is paired by (target, unity name) and written to the map.  Its contents are
    then compared per symbol by P-SYM like any other object; that the batches
    hold the same sources is not asserted here (the per-symbol comparison is
    the evidence).
Deleted sources are listed on stderr, never silently dropped.
"""
import argparse
import posixpath
import sys


def parse_move_map(path, phases):
    moves, deletes = {}, set()
    for n, raw in enumerate(open(path, encoding="utf-8"), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if " -> " not in line:
            sys.exit("%s:%d: not an `old -> new` line: %r" % (path, n, line))
        old, new = [x.strip() for x in line.split(" -> ", 1)]
        if phases and not any(old.startswith(p) for p in phases):
            continue
        if old in moves or old in deletes:
            sys.exit("%s:%d: %s listed twice" % (path, n, old))
        if new.startswith("DELETE"):
            deletes.add(old)
        else:
            moves[old] = new
    return moves, deletes


def key_of(rel):
    """(target, source) for a build-relative object path, or None."""
    if not rel.endswith(".o"):
        return None
    marker = "CMakeFiles/"
    i = rel.find(marker)
    if i < 0:
        return None
    prefix = rel[:i].rstrip("/")
    rest = rel[i + len(marker):]
    if "/" not in rest:
        return None
    tdir, objpath = rest.split("/", 1)
    if not tdir.endswith(".dir"):
        return None
    target = tdir[: -len(".dir")]
    objpath = objpath[: -len(".o")]
    if objpath.startswith("Unity/"):
        return (target, "unity:" + objpath)
    parts = [".." if seg == "__" else seg for seg in objpath.split("/")]
    src = posixpath.normpath(posixpath.join(prefix, *parts))
    return (target, src)


def load_manifest(path, target_map):
    out, dup = {}, []
    for raw in open(path, encoding="utf-8"):
        rel = raw.strip()
        if not rel or rel == "libhven.a":
            continue
        k = key_of(rel)
        if k is None:
            sys.exit("%s: cannot derive a (target, source) key from %r" % (path, rel))
        k = (target_map.get(k[0], k[0]), k[1])
        if k in out:
            dup.append(k)
        out[k] = rel
    if dup:
        sys.exit("%s: duplicate keys: %s" % (path, dup[:5]))
    return out


def generate(moves, deletes, before, after, unity_dirmoves=()):
    errors, lines, deleted = [], [], []
    claimed = {}
    for (tgt, src), brel in sorted(before.items()):
        if src.startswith("unity:"):
            arel = after.get((tgt, src))
            declared = any(arel is not None and brel.startswith(o + "/") and arel == n + brel[len(o):]
                           for o, n in unity_dirmoves)
            if arel == brel:
                claimed[(tgt, src)] = brel
            elif declared:
                claimed[(tgt, src)] = brel
                lines.append("%s => %s" % (brel, arel))
            else:
                errors.append("UNITY-UNMAPPED %s (no identical path in the after manifest)" % brel)
            continue
        if src in deletes:
            deleted.append(brel)
            continue
        nsrc = moves.get(src, src)
        nk = (tgt, nsrc)
        arel = after.get(nk)
        if arel is None:
            errors.append("MISSING %s -> no after object for %s (target %s)" % (brel, nsrc, tgt))
            continue
        if nk in claimed:
            errors.append("MANY-TO-ONE %s and %s both map to %s" % (claimed[nk], brel, arel))
            continue
        claimed[nk] = brel
        if arel != brel:
            lines.append("%s => %s" % (brel, arel))
    for k, arel in sorted(after.items()):
        if k not in claimed:
            errors.append("NEW %s (no before object maps to it)" % arel)
    return lines, deleted, errors


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--move-map", required=True)
    ap.add_argument("--before", required=True)
    ap.add_argument("--after", required=True)
    ap.add_argument("--target-map", action="append", default=[])
    ap.add_argument("--phase", action="append", default=[])
    ap.add_argument("--unity-dir-move", action="append", default=[],
                    help="OLD=NEW: a unity object may differ between the manifests by exactly this build-relative directory prefix")
    ap.add_argument("-o", "--out")
    a = ap.parse_args(argv)
    tmap = {}
    for t in a.target_map:
        o, _, n = t.partition("=")
        if not n:
            sys.exit("--target-map wants OLD=NEW, got %r" % t)
        tmap[o] = n
    moves, deletes = parse_move_map(a.move_map, a.phase)
    before = load_manifest(a.before, {})
    after = load_manifest(a.after, {})
    # a renamed target is renamed on the BEFORE side, so keys line up
    before = {(tmap.get(t, t), s): r for (t, s), r in before.items()}
    udm = []
    for t in a.unity_dir_move:
        o, _, n = t.partition("=")
        if not n:
            sys.exit("--unity-dir-move wants OLD=NEW, got %r" % t)
        udm.append((o.rstrip("/"), n.rstrip("/")))
    lines, deleted, errors = generate(moves, deletes, before, after, udm)
    text = "".join(l + "\n" for l in lines)
    if a.out:
        open(a.out, "w", encoding="utf-8").write(text)
    else:
        sys.stdout.write(text)
    print("rs_object_map: %d before objects, %d after objects, %d map lines, %d deleted, %d errors"
          % (len(before), len(after), len(lines), len(deleted), len(errors)), file=sys.stderr)
    for d in deleted:
        print("DELETED %s" % d, file=sys.stderr)
    for e in errors:
        print("ERROR %s" % e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
