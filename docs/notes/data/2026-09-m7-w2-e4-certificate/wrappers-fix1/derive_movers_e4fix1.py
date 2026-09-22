#!/usr/bin/env python3
"""M7 W2 E4 fix round 1 -- THE PRODUCTION MOVER SHAPE, DERIVED FROM GIT.

For a round whose every added line is meant to sit inside `#ifdef HVEN_TESTING`,
this reads BASE and HEAD of each changed file and says what a PRODUCTION compile
(HVEN_TESTING undefined) can see move. It is the value the runner's pre-flight
compares with the declared shape before any leg runs; P-SYM and the section
check then MEASURE it.

A production compile skips every line inside an `#ifdef HVEN_TESTING` block
(nesting-aware; `#else` of such a block is production-visible), but the skipped
lines still COUNT toward the line numbers of what follows. So, per file, the
production-visible content is the list of (line number, text) pairs outside
those blocks, and:

Lines are compared as TOKENS (whitespace and // comments dropped; lines with
no token dropped), so a re-wrap or a comment edit is not a change.

  src/*.cpp  NON-DEBUG MOVER   the visible TOKEN STREAM differs, or the (line, text)
                               of a line carrying a line-bearing construct
                               (assert(, __LINE__, source_location) differs
                               -- a moved `__LINE__` immediate is an
                               instruction-stream change in Debug;
             DEBUG-ONLY (Debug) the visible tokens are identical but some
                               token-bearing line's NUMBER moved: the DWARF line
                               programme differs, nothing else can;
             (Release has no line tables, so a DEBUG-ONLY object is
              byte-identical in Release.)
  include/   PRODUCTION-VISIBLE the visible (line, tokens) list differs, or a
                               NEW header is #included from a
                               production-visible line of any file under
                               include/ or src/ at HEAD.

Usage: derive_movers_e4fix1.py REPO BASE HEAD
Prints KEY=value lines; exits 0, or 2 on a usage/IO error.
"""
import re
import subprocess
import sys

LINE_BEARING = re.compile(r"\bassert\s*\(|__LINE__|source_location")
IF_OPEN = re.compile(r"^\s*#\s*if(n?def)?\b")
ENDIF = re.compile(r"^\s*#\s*endif\b")
ELSE = re.compile(r"^\s*#\s*el(se|if)\b")
TESTING_OPEN = re.compile(r"^\s*#\s*ifdef\s+HVEN_TESTING\b")


def git(repo, *args):
    return subprocess.run(["git", "-C", repo, *args], check=True, capture_output=True,
                          text=True).stdout


def show(repo, rev, path):
    r = subprocess.run(["git", "-C", repo, "show", f"{rev}:{path}"], capture_output=True,
                       text=True)
    return r.stdout if r.returncode == 0 else None


def visible(text):
    """(line number, text) for every line a production compile processes.

    An HVEN_TESTING block's own directives (#ifdef / #else / #endif) emit
    nothing and are not listed; its body is skipped, and its #else branch is
    production code. Every skipped line still counts toward later numbers.
    """
    out = []
    stack = []  # 'T' = HVEN_TESTING body (skipped), 'E' = its #else, 'O' = any other #if
    for no, line in enumerate(text.splitlines(), start=1):
        skipping = "T" in stack
        if IF_OPEN.match(line):
            if TESTING_OPEN.match(line):
                stack.append("T")
            else:
                if not skipping:
                    out.append((no, line))
                stack.append("O")
            continue
        if ELSE.match(line) and stack:
            if stack[-1] == "T":
                stack[-1] = "E"
            elif stack[-1] == "O" and not skipping:
                out.append((no, line))
            continue
        if ENDIF.match(line) and stack:
            kind = stack.pop()
            if kind == "O" and "T" not in stack:
                out.append((no, line))
            continue
        if not skipping:
            out.append((no, line))
    return out


def norm(line):
    """The line's TOKENS: a trailing // comment dropped (when the // is not
    inside a string literal, judged by quote parity) and all whitespace
    removed, so a clang-format re-wrap or a comment edit is not a change."""
    i = line.find("//")
    while i >= 0:
        if line[:i].count('"') % 2 == 0:
            line = line[:i]
            break
        i = line.find("//", i + 2)
    return re.sub(r"\s+", "", line)


def code(vis):
    """(line number, tokens) for the visible lines that carry any token."""
    return [(n, norm(t)) for n, t in vis if norm(t)]


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    repo, base, head = sys.argv[1:]
    changed = git(repo, "diff", "--name-only", f"{base}..{head}", "--", "src/", "include/")
    changed = [p for p in changed.split() if p]
    non_debug, debug_only, hdr_visible = [], [], []
    new_headers = []
    for path in changed:
        before = show(repo, base, path)
        after = show(repo, head, path)
        vb = visible(before) if before is not None else None
        va = visible(after) if after is not None else None
        if path.startswith("src/") and path.endswith(".cpp"):
            obj = f"CMakeFiles/hven.dir/{path}.o"
            if vb is None or va is None:
                non_debug.append(obj)
                continue
            cb, ca = code(vb), code(va)
            lb = [(n, t) for n, t in cb if LINE_BEARING.search(t)]
            la = [(n, t) for n, t in ca if LINE_BEARING.search(t)]
            if "".join(t for _, t in cb) != "".join(t for _, t in ca) or lb != la:
                non_debug.append(obj)
            elif cb != ca:
                debug_only.append(obj)
        elif path.startswith("include/"):
            if before is None:
                new_headers.append(path)
            elif code(vb) != code(va):
                hdr_visible.append(path)
    # A NEW header is production-visible iff some production-visible line
    # anywhere under include/ or src/ at HEAD includes it.
    if new_headers:
        files = [p for p in git(repo, "ls-tree", "-r", "--name-only", head, "--", "include/",
                                "src/").split() if p]
        for hdr in new_headers:
            needle = hdr[len("include/"):]
            for f in files:
                txt = show(repo, head, f)
                if txt is None or needle not in txt:
                    continue
                if any("#include" in t and needle in t for _, t in visible(txt)):
                    hdr_visible.append(f"{hdr} (included visibly by {f})")
    print(f"CHANGED={' '.join(changed)}")
    print(f"NEW_HEADERS={' '.join(new_headers)}")
    print(f"NON_DEBUG_MOVERS={' '.join(sorted(non_debug))}")
    print(f"NON_DEBUG_MOVER_COUNT={len(non_debug)}")
    print(f"DEBUG_ONLY_DEBUG={' '.join(sorted(debug_only))}")
    print(f"DEBUG_ONLY_DEBUG_COUNT={len(debug_only)}")
    print(f"PRODUCTION_VISIBLE_HEADERS={' | '.join(hdr_visible)}")
    print(f"PRODUCTION_VISIBLE_HEADER_COUNT={len(hdr_visible)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
