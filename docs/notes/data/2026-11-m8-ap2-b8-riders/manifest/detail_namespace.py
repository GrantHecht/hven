#!/usr/bin/env python3
"""AP-2 B8 riders (C242 (2)): move the declarations of include/hven/detail/** headers that sit in `namespace
hven {` into `namespace hven::detail {`. Per header: `namespace hven {` -> `namespace hven::detail {` (and its
closing comment); a nested `namespace detail {` / `} // namespace detail` pair inside it is removed (its
contents were hven::detail already). A forward declaration of a PUBLIC type (one a public header defines)
is hoisted into its own `namespace hven { ... }` block in front, so it keeps naming the public type.
Writes the moved top-level names (types, functions, variables, aliases) to <out-names>, one per line.
The written list was copied to detail_moved_names.txt after a filter (names also spelled in a public header
dropped, the four moved types a public header names as detail::X added back: IpmSolverImpl, SqpSolverImpl,
NonLinearProgram, GlobalizationStrategy) and one hand addition the name pattern misses (an array:
kInteriorPointSolverPresets), found by the dev P-SYM comparison.
usage: detail_namespace.py <worktree> <out-names>"""
import os, re, sys
wt, out = sys.argv[1], sys.argv[2]
STRIP = re.compile(r'"(?:\\.|[^"\\\n])*"|//[^\n]*|/\*(?:.|\n)*?\*/')
pub = set()
for dp, _, fs in os.walk(f'{wt}/include/hven'):
    if '/detail' in dp:
        continue
    for f in fs:
        s = STRIP.sub('', open(os.path.join(dp, f)).read())
        pub |= set(re.findall(r'\b(?:class|struct|enum class|enum)\s+(\w+)\s*(?:final\s*)?[:{]', s))
names = set()
for dp, _, fs in os.walk(f'{wt}/include/hven/detail'):
    for f in sorted(fs):
        p = os.path.join(dp, f)
        s = open(p).read()
        if not re.search(r'^namespace hven \{$', s, re.M) or re.search(r'^namespace hven \{\n(?:(?:class|struct) \w+;\n)*\}$', s, re.M) and s.count('namespace hven {') == 1:
            continue
        lines = s.split('\n')
        res, stack, hoist, blk_hoist = [], [], [], []
        for ln in lines:
            st = ln.strip()
            if st == 'namespace hven {' and not stack:
                stack.append('hven'); opened = len(res); blk_hoist = []
                res.append('namespace hven::detail {'); continue
            if ln == '}' and stack == ['hven'] and all(re.match(r'^\s*((class|struct) \w+;)?\s*$', x) for x in res[opened + 1:]):
                # a bare-brace block (forward declarations of public types): left in hven as it was
                stack.pop(); res[opened] = 'namespace hven {'
                res[opened + 1:opened + 1] = blk_hoist; res.append(ln); continue
            if st == '} // namespace hven' and stack == ['hven']:
                stack.pop(); hoist += blk_hoist; res.append('} // namespace hven::detail'); continue
            if stack == ['hven'] and st == 'namespace detail {':
                stack.append('inner'); continue
            if stack and stack[-1] == 'inner' and st == '} // namespace detail':
                stack.pop(); continue
            m = re.match(r'^(class|struct) (\w+);$', st)
            if m and stack == ['hven'] and m.group(2) in pub:
                blk_hoist.append(st); continue
            if st.startswith('namespace ') and st.endswith('{'):
                stack.append('other')
            elif st.startswith('} // namespace') and stack and stack[-1] == 'other':
                stack.pop()
            res.append(ln)
        if stack:
            raise SystemExit(f'UNBALANCED {p} {stack}')
        t = '\n'.join(res)
        if hoist:
            i = t.index('namespace hven::detail {')
            t = t[:i] + 'namespace hven {\n' + '\n'.join(hoist) + '\n} // namespace hven\n\n' + t[i:]
        open(p, 'w').write(t)
        # top-level names of the moved blocks
        body = STRIP.sub(' ', t)
        for blk in re.finditer(r'namespace hven::detail \{', body):
            i, depth, j = blk.end(), 1, blk.end()
            top = []
            while depth and j < len(body):
                c = body[j]
                if c == '{':
                    if depth == 1: top.append(body[i:j]); 
                    depth += 1
                elif c == '}':
                    depth -= 1
                    if depth == 1: i = j + 1
                j += 1
            top.append(body[i:j])
            # re-scan depth-1 text only
            d1, depth, k = [], 1, blk.end()
            while k < j - 1:
                c = body[k]
                if c == '{': depth += 1
                elif c == '}': depth -= 1
                elif depth == 1: d1.append(c)
                if depth == 1 and c in '{}': d1.append(' ')
                k += 1
            d1 = ''.join(d1)
            for m in re.finditer(r'\b(?:class|struct|enum class|enum|union)\s+(?:alignas\([^)]*\)\s*)?(\w+)', d1):
                names.add(m.group(1))
            for m in re.finditer(r'\busing\s+(\w+)\s*=', d1):
                names.add(m.group(1))
            for m in re.finditer(r'\b(\w+)\s*\((?![^;]*\)\s*;\s*$)', d1):
                pass
            for m in re.finditer(r'(?:^|[;}\s])(?:[\w:<>,*&\s]+?)\s[*&]*(\w+)\s*\(', d1):
                n = m.group(1)
                if n not in ('if', 'for', 'while', 'switch', 'return', 'sizeof', 'decltype', 'static_assert',
                             'alignas', 'noexcept', 'operator', 'requires'):
                    names.add(n)
            for m in re.finditer(r'\b(?:inline\s+)?(?:constexpr|const)\s+[\w:<>]+\s+(\w+)\s*=', d1):
                names.add(m.group(1))
        print(p.replace(wt + '/', ''), 'hoisted' if hoist else '')
open(out, 'w').write('\n'.join(sorted(names)) + '\n')
