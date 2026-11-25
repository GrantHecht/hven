#!/usr/bin/env python3
"""Classifies the P-SYM compare's DIFFERS findings by relocation-target delta.

usage: classify.py <compare.txt> <before-snapshot> <after-snapshot> <out.txt>

P-SYM's verdict is per symbol and binary. This reads each `DIFFERS` finding of the
compare (object, symbol) and, from the two captures' own object files, takes the set
of CALL/reference targets the symbol's body names on each side (objdump -dr, demangled).
A DIFFERS whose only change in that set is

    removed targets  within the throw machinery of an inline fmt::format + throw
                     (fmt::*, std::invalid_argument, __cxa_*, _Unwind_Resume, string
                     ctors/dtors, operator new/delete, mem*/str*, std::__throw_*,
                     typeinfo/vtable for the exception, the string-literal sections)
    added targets    only hven::solvers::detail::throw_* (the cold functions) and the
                     renamed handle/Impl symbols the symbol map declares

is class THROW-OUTLINED: the body lost an inline message construction and gained a call
to the out-of-line cold function. Any other target delta is class OTHER and is listed.
A DIFFERS with NO target delta at all (same callees, different instructions) is class
SAME-TARGETS and is listed: it is the one the relocation view cannot attribute.

This does not make P-SYM pass. It says which findings are the cold-throw conversion that
DECLARATION.txt names (fmt out of the headers) and which are not.
"""
import collections
import re
import subprocess
import sys

compare, before, after, out = sys.argv[1:5]
IMPL = [(re.compile(r'hven::solvers::IpmSolver::'), 'hven::solvers::IpmSolverImpl::'),
        (re.compile(r'hven::solvers::SqpSolver::'), 'hven::solvers::SqpSolverImpl::')]


def sym_map(name):
    for rx, rep in IMPL:
        name = rx.sub(rep, name)
    return name


THROW_OK = re.compile(
    r'(^|[^A-Za-z_])(fmt::|std::invalid_argument|std::logic_error|__cxa_|_Unwind_Resume|'
    r'std::__cxx11::basic_string|std::__throw_|operator (new|delete)|memcpy|memmove|memset|strlen|'
    r'__stack_chk|typeinfo|vtable|std::exception|\.L\.str|\.rodata|std::allocator|__gxx_personality|'
    r'std::char_traits|__clang_call_terminate|std::terminate|std::runtime_error|std::out_of_range|'
    r'std::length_error|std::bad_alloc|std::__detail|operator\(\))')
ADDED_OK = re.compile(r'hven::solvers::detail::throw_')
VALIDATOR = re.compile(
    r'hven::solvers::(validate_|detail::require_|claim_stream_digest|KktLocationTable::KktLocationTable|'
    r'RhsLocationTable::RhsLocationTable|is_legal_request|NlpAssembly::evaluate_candidate|'
    r'NlpAssembly::assemble|validate_request_destinations|validate_bound_destination|validate_eval_request)')
LABEL = re.compile(r'^\.L')

cache = {}


def disasm(path):
    if path in cache:
        return cache[path]
    res = subprocess.run(['objdump', '-d', '-r', '-C', '--no-show-raw-insn', path],
                         capture_output=True, text=True)
    blocks = {}
    cur = None
    for line in res.stdout.splitlines():
        m = re.match(r'^[0-9a-f]+ <(.*)>:$', line)
        if m:
            cur = m.group(1)
            blocks[cur] = set()
            continue
        if cur is None:
            continue
        m = re.search(r'R_X86_64_\w+\s+(\S.*?)(?:[+-]0x[0-9a-f]+)?$', line)
        if m:
            blocks[cur].add(m.group(1).strip())
    cache[path] = blocks
    return blocks


def lookup(blocks, name):
    if name in blocks:
        return blocks[name]
    # objdump -C prints the full demangled name; tolerate a trailing variant tag
    base = re.sub(r'\[[CD][0-9]\]$', '', name)
    for k in blocks:
        if k == base or k.startswith(base):
            return blocks[k]
    return None


# The PER-SYMBOL line of an object PRECEDES the per-symbol findings listed under it.
findings = []
cur_obj = None
for line in open(compare):
    m = re.match(r'^PER-SYMBOL\s+(\S+\.o):', line)
    if m:
        cur_obj = m.group(1)
        continue
    m = re.match(r'^\s+DIFFERS\s+(.*\S)\s*$', line)
    if m and not line.startswith('DIFFERS') and cur_obj:
        findings.append((cur_obj, m.group(1)))

counts = collections.Counter()
other = []
same = []
unresolved = []
for obj, name in findings:
    bpath = '%s/%s' % (before, obj)
    apath = '%s/%s' % (after, obj)
    try:
        b = lookup(disasm(bpath), sym_map(name)) or lookup(disasm(bpath), name)
        a = lookup(disasm(apath), name)
    except Exception as e:  # noqa
        b = a = None
    if b is None or a is None:
        counts['UNRESOLVED'] += 1
        unresolved.append((obj, name))
        continue
    b = {sym_map(t) for t in b}
    a = {sym_map(t) for t in a}  # the public handle and the Impl are one boundary here
    b = {t for t in b if not LABEL.match(t)}
    a = {t for t in a if not LABEL.match(t)}
    removed = b - a
    added = a - b
    if not removed and not added:
        counts['SAME-TARGETS'] += 1
        same.append((obj, name))
    elif all(THROW_OK.search(t) or VALIDATOR.search(t) for t in removed) and \
            all(ADDED_OK.search(t) or VALIDATOR.search(t) for t in added):
        counts['THROW-OUTLINED'] += 1
    else:
        counts['OTHER'] += 1
        other.append((obj, name, sorted(removed)[:6], sorted(added)[:6]))

with open(out, 'w') as f:
    f.write('DIFFERS findings: %d\n' % len(findings))
    for k, v in counts.most_common():
        f.write('  %-16s %d\n' % (k, v))
    f.write('\n--- OTHER (target delta is not the throw machinery) ---\n')
    for o in other:
        f.write('%s :: %s\n   removed %s\n   added   %s\n' % (o[0], o[1][:200], o[2], o[3]))
    f.write('\n--- SAME-TARGETS (no callee delta; instructions differ) ---\n')
    for o, n in same:
        f.write('%s :: %s\n' % (o, n[:240]))
    f.write('\n--- UNRESOLVED (symbol not found in a disassembly) ---\n')
    for o, n in unresolved:
        f.write('%s :: %s\n' % (o, n[:240]))
print(open(out).read()[:3000])
