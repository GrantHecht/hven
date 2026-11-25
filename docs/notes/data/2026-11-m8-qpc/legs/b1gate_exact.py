#!/usr/bin/env python3
"""B1Gate's certified answer checked in EXACT rational arithmetic from the model's data (DECLARATION-QPC section 7;
pre-freeze review M6). The model (tests/sqp/test_warm_ingest_complementarity.cpp, B1ScaledStalePrice):
min f(x) = -S x + x^2/2  s.t.  c(x) = x - eps <= 0,  -1 <= x <= 1;  x* = eps, f* = -S eps + eps^2/2.
usage: hven_sqp_tests --gtest_filter='B1Gate.TheWatchdogRestore*' | b1gate_exact.py
Reads the test's `[ b1gate ] S=.. eps=.. x=.. lambda=.. z=.. f=..` hexfloat line; every quantity below is the exact
rational value of those doubles (no rounding until printing). rc 0 iff stationarity |-S + x + lambda - z| <= 1e-9 S,
feasibility x - eps <= 0 (exactly) or within 1e-12, lambda >= 0, |lambda c| <= 1e-6 (the test's optimality_tol), and
|f - f*| <= 1e-9 (f the solver's reported objective, and separately f(x) exactly)."""
import re
import sys
from fractions import Fraction as F

line = next((l for l in sys.stdin if l.startswith("[ b1gate ]")), None)
if line is None:
    sys.exit("no [ b1gate ] line on stdin")
v = {k: F(float.fromhex(h)) for k, h in re.findall(r"(\w+)=(\S+)", line)}
S, eps, x, lam, z, f = (v[k] for k in ("S", "eps", "x", "lambda", "z", "f"))
grad_l = -S + x + lam - z
c = x - eps
compl = lam * c
f_x = -S * x + x * x / 2
f_star = -S * eps + eps * eps / 2
rows = [("x - x*", x - eps, F(1, 10**12)), ("|grad L|", abs(grad_l), F(1, 10**9) * S),
        ("feasibility max(0, c)", max(F(0), c), F(1, 10**12)), ("lambda >= 0 (-lambda)", -lam, F(0)),
        ("|lambda c|", abs(compl), F(1, 10**6)), ("|f_reported - f*|", abs(f - f_star), F(1, 10**9)),
        ("|f(x) - f*|", abs(f_x - f_star), F(1, 10**9))]
ok = True
for name, val, bound in rows:
    good = val <= bound if name != "x - x*" else abs(val) <= bound
    ok &= good
    print(f"{name:24s} = {float(val): .6e}   bound {float(bound):.1e}   {'ok' if good else 'FAIL'}")
print("EXACT CHECK " + ("PASS" if ok else "FAIL"))
sys.exit(0 if ok else 1)
