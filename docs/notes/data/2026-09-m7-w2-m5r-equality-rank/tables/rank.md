# M7 W2 M5r -- the equality-Jacobian rank table

Every number is read from `rows/<CELL>.txt` by `wrappers/tables_m5r.py`; the cell name IS the citation. No solver ran.

**Deficiency triples** are `Eigen default / 1e-10 / 1e-8` relative thresholds on the SAME R (ColPivHouseholderQR of the transpose). 1e-8 is M5's arm `dependency_tol` (added as a third read, declared). `(eqz a/b/c)` is the same triple on the ROW-EQUILIBRATED rows, shown only where it differs from the raw one. **skel** = [Ae; e_i, l_i = u_i]; **entry** = [Ae; e_i for every bound active within 1e-8 absolute, fixed included]. **excess** = max(0, rows - n): the part of a deficiency that the row COUNT alone forces. **cond** = |R_11|/|R_rr| at the default-threshold rank. Cells marked * are M5 Q1's six.

## 1. At the decisive point (x0; the projected start where x0 is unevaluable)

| cell | pt | n | me | mi | fixed | finite bnds | rank(Ae) | Ae deficiency | skel deficiency | entry deficiency (excess) | cond Ae | cond skel | Ae linear rows (cstats, all m) | dependent Ae rows (default cut) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `ACOPP118` | x0 | 344 | 236 | 372 | 0 | 452 | 236 | 0/0/0 | 0/0/0 | 0/0/0 (0) | 219 | 219 | 0 | none |
| `ACOPP57` | x0 | 128 | 114 | 160 | 0 | 142 | 114 | 0/0/0 | 0/0/0 | 0/0/0 (0) | 182 | 182 | 0 | none |
| `ACOPR118` | x0 | 344 | 236 | 490 | 0 | 216 | 236 | 0/0/0 | 0/0/0 | 0/0/0 (0) | 220 | 220 | 0 | none |
| `ACOPR57` | x0 | 128 | 114 | 217 | 0 | 28 | 114 | 0/0/0 | 0/0/0 | 0/0/0 (0) | 180 | 180 | 0 | none |
| `BIGBANK` | x0 | 2230 | 1112 | 0 | 308 | 4460 | 1111 | 1/1/1 | 150/150/150 | 223/223/223 (223) | 52 | 52 | 1112 | c935:N936 |
| `BRITGAS` | x0 | 450 | 360 | 0 | 0 | 474 | 360 | 0/0/0 | 0/0/0 | 0/0/0 (0) | 5.61 | 5.61 | 0 | none |
| `DALLASM`* | x0 | 196 | 151 | 0 | 0 | 392 | 150 | 1/1/1 | 1/1/1 | 5/5/5 (5) | 9.31 | 9.31 | 151 | c3:N4 |
| `MSS2` | x0 | 756 | 703 | 0 | 0 | 0 | 378 | 325/325/325 | 325/325/325 | 325/325/325 (0) | 104 | 104 | 0 | 325 rows (first: c393:C393, c2:C2, c245:C245, c246:C246, ...) |
| `MSS3` | x0 | 2070 | 1981 | 0 | 0 | 0 | 1035 | 946/946/946 | 946/946/946 | 946/946/946 (0) | 264 | 264 | 0 | 946 rows (first: c1439:C1439, c3:C3, c485:C485, c1457:C1457, ...) |
| `NET3` | xproj | 464 | 394 | 127 | 37 | 487 | 394 | 0/0/0 | 0/0/0 | 166/166/166 (166) | 1.02e+05 | 1.02e+05 | 305 | none |
| `READING6` | x0 | 102 | 50 | 0 | 1 | 104 | 50 | 0/0/0 | 0/0/0 | 0/0/0 (0) | 2.45 | 2.94 | 0 | none |
| `READING8` | x0 | 2002 | 1000 | 0 | 1 | 2004 | 1000 | 0/0/0 | 0/0/0 | 0/0/0 (0) | 8.86 | 8.9 | 0 | none |
| `SAWPATH`* | x0 | 583 | 578 | 196 | 2 | 4 | 578 | 0/574/575 (eqz 0/69/234) | 1/576/577 (eqz 0/69/234) | 1/576/577 (eqz 0/69/234) (0) | 6.02e+10 | 1.84e+11 | 578 | none |
| `SMBANK`* | x0 | 117 | 64 | 0 | 0 | 234 | 63 | 1/1/1 | 1/1/1 | 9/9/9 (9) | 4.33 | 4.33 | 64 | c61:N62 |
| `STEENBRB`* | x0 | 468 | 108 | 0 | 0 | 468 | 96 | 12/12/12 | 12/12/12 | 12/12/12 (0) | 2.42 | 2.42 | 108 | c72:N19, c0:N11, c99:N112, c54:N17, c81:N110, c63:N18, c45:N16, c27:N14, c41:N65, c18:N13, c9:N12, c95:N611 |
| `STEENBRC`* | x0 | 540 | 126 | 0 | 0 | 540 | 112 | 14/14/14 | 14/14/14 | 14/14/14 (0) | 2.42 | 2.42 | 126 | c36:N15, c9:N12, c27:N14, c0:N11, c122:N614, c104:N612, c63:N18, c77:N69, c18:N13, c50:N66, c54:N17, c81:N110, c113:N613, c90:N111 |
| `STEENBRD` | x0 | 468 | 108 | 0 | 0 | 468 | 96 | 12/12/12 | 12/12/12 | 12/12/12 (0) | 2.42 | 2.42 | 108 | c72:N19, c0:N11, c99:N112, c54:N17, c81:N110, c63:N18, c45:N16, c27:N14, c41:N65, c18:N13, c9:N12, c95:N611 |
| `STEENBRE` | x0 | 540 | 126 | 0 | 0 | 540 | 112 | 14/14/14 | 14/14/14 | 14/14/14 (0) | 2.42 | 2.42 | 126 | c36:N15, c9:N12, c27:N14, c0:N11, c122:N614, c104:N612, c63:N18, c77:N69, c18:N13, c50:N66, c54:N17, c81:N110, c113:N613, c90:N111 |
| `STEENBRG`* | x0 | 540 | 126 | 0 | 0 | 540 | 112 | 14/14/14 | 14/14/14 | 14/14/14 (0) | 2.42 | 2.42 | 126 | c36:N15, c9:N12, c27:N14, c0:N11, c122:N614, c104:N612, c63:N18, c77:N69, c18:N13, c50:N66, c54:N17, c81:N110, c113:N613, c90:N111 |
| `TAX213322` | x0 | 1512 | 0 | 570781 | 0 | 1512 | - | me=0 | me=0 | 0/0/0 (0) | - | - | 1 | - |
| `TAX53322` | x0 | 360 | 0 | 32221 | 0 | 360 | - | me=0 | me=0 | 0/0/0 (0) | - | - | 1 | - |
| `YORKNET` | x0 | 312 | 256 | 0 | 0 | 288 | 256 | 0/0/0 | 0/0/0 | 40/40/40 (16) | 5.08e+03 | 5.08e+03 | 136 | none |
| `ZAMB2-8` | x0 | 138 | 48 | 0 | 6 | 276 | 48 | 0/0/0 | 0/0/0 | 0/0/0 (0) | 2.21 | 3.84 | 0 | none |

## 2. Structural reading (INTERPRETATION of each row's own `*_DEP` lines)

| cell | reading |
|---|---|
| `ACOPP118` | full rank in Ae and the skeleton; nothing to read |
| `ACOPP57` | full rank in Ae and the skeleton; nothing to read |
| `ACOPR118` | full rank in Ae and the skeleton; nothing to read |
| `ACOPR57` | full rank in Ae and the skeleton; nothing to read |
| `BIGBANK` | network balance, ONE redundant row in Ae (+-1 over 1112 rows); the skeleton adds 149 more: every one a +-1 combination of node balances and FIXED arc flows, i.e. fixed arcs cut the network and each cut piece carries its own redundant balance |
| `BRITGAS` | full rank in Ae and the skeleton; nothing to read |
| `DALLASM` | network balance, ONE redundant row: all 151 rows, +-1 (linear 151/151) |
| `MSS2` | NONLINEAR rows (0 linear); 325 dependent at x0 AND at the perturbed point; combinations are not +-1 (mostly support 7, coefficients 0.5/1); structure NOT identified |
| `MSS3` | NONLINEAR rows (0 linear); 946 dependent at x0 AND at the perturbed point; same shape as MSS2; structure NOT identified |
| `NET3` | x0 UNEVALUABLE (Jacobian non-finite); at the projected start Ae and the skeleton are full rank; the entry-set deficiency equals its row-count excess |
| `READING6` | full rank in Ae and the skeleton; nothing to read |
| `READING8` | full rank in Ae and the skeleton; nothing to read |
| `SAWPATH` | NO exact dependence: full rank at eps (QR and SVD agree), but ILL-CONDITIONED and BADLY SCALED -- row norms 1.41 .. 6.0e10, R-diagonal/singular values decay smoothly (no gap) to 3.5e-12 of the largest; at the arm's 1e-8 cut 575/578 rows read dependent (linear 578/578) |
| `SMBANK` | network balance, ONE redundant row: all 64 rows sum to 0 with +-1 coefficients (linear 64/64) |
| `STEENBRB` | 12 disjoint 9-node networks (one per period, rows N<node><period>): one redundant balance per copy, each a +-1 sum of its 9 rows (linear 108/108) |
| `STEENBRC` | 14 disjoint 9-node networks, one redundant balance per copy, +-1 (linear 126/126) |
| `STEENBRD` | as STEENBRB (the identical 12 dependent rows by name) |
| `STEENBRE` | as STEENBRC (the identical 14 dependent rows by name) |
| `STEENBRG` | as STEENBRC (the identical 14 dependent rows by name) |
| `TAX213322` | full rank in Ae and the skeleton; nothing to read |
| `TAX53322` | full rank in Ae and the skeleton; nothing to read |
| `YORKNET` | Ae and skeleton full rank; entry-set deficiency 40 at x0 (16 of it row-count excess), 64 at the projected start (56 excess) |
| `ZAMB2-8` | full rank in Ae and the skeleton; nothing to read |

## 3. Point dependence: x0, the projected start, and a perturbed point

`xpert` = x0 moved by 1e-3*(1+|x0_i|), alternating sign, projected into the box -- NOT a solve iterate; read ONLY for the equality Jacobian's deficiency and for how many equality rows' Jacobian values moved. No row carries the OFF solution's final x (M3c/M4/M5 rows carry counters, not x), so brief item 7 is SKIPPED.

| cell | f(x0) finite | Jac(x0) finite | outside box at x0 | projected start | Ae def. x0 | Ae def. xproj | entry def. xproj (excess) | eq rows moved at xpert | Ae def. xpert (dflt/1e-10/1e-8) |
|---|---|---|---|---|---|---|---|---|---|
| `ACOPP118` | 1 | 1 | 0 | NOT_EVALUATED | 0/0/0 | - | - | 236 | 0/0/0 |
| `ACOPP57` | 1 | 1 | 1 | EVALUATED | 0/0/0 | 0/0/0 | 0/0/0 (0) | 114 | 0/0/0 |
| `ACOPR118` | 1 | 1 | 0 | NOT_EVALUATED | 0/0/0 | - | - | 236 | 0/0/0 |
| `ACOPR57` | 1 | 1 | 0 | NOT_EVALUATED | 0/0/0 | - | - | 114 | 0/0/0 |
| `BIGBANK` | 1 | 1 | 0 | NOT_EVALUATED | 1/1/1 | - | - | 0 | 1/1/1 |
| `BRITGAS` | 1 | 1 | 0 | NOT_EVALUATED | 0/0/0 | - | - | 360 | 0/0/0 |
| `DALLASM` | 1 | 1 | 0 | NOT_EVALUATED | 1/1/1 | - | - | 0 | 1/1/1 |
| `MSS2` | 1 | 1 | 0 | NOT_EVALUATED | 325/325/325 | - | - | 703 | 325/325/325 |
| `MSS3` | 1 | 1 | 0 | NOT_EVALUATED | 946/946/946 | - | - | 1981 | 946/946/946 |
| `NET3` | 0 | 0 | 41 | EVALUATED | UNEVALUABLE | 0/0/0 | 166/166/166 (166) | - | UNEVALUABLE (a Jacobian at x0 or xpert is not finite) |
| `READING6` | 1 | 1 | 0 | NOT_EVALUATED | 0/0/0 | - | - | 50 | 0/0/0 |
| `READING8` | 1 | 1 | 0 | NOT_EVALUATED | 0/0/0 | - | - | 1000 | 0/0/0 |
| `SAWPATH` | 1 | 1 | 2 | EVALUATED | 0/574/575 (eqz 0/69/234) | 0/574/575 (eqz 0/69/234) | 1/576/577 (eqz 0/69/234) (0) | 0 | 0/574/575 |
| `SMBANK` | 1 | 1 | 0 | NOT_EVALUATED | 1/1/1 | - | - | 0 | 1/1/1 |
| `STEENBRB` | 1 | 1 | 0 | NOT_EVALUATED | 12/12/12 | - | - | 0 | 12/12/12 |
| `STEENBRC` | 1 | 1 | 0 | NOT_EVALUATED | 14/14/14 | - | - | 0 | 14/14/14 |
| `STEENBRD` | 0 | 1 | 2 | EVALUATED | 12/12/12 | 12/12/12 | 12/12/12 (0) | 0 | 12/12/12 |
| `STEENBRE` | 0 | 1 | 2 | EVALUATED | 14/14/14 | 14/14/14 | 14/14/14 (0) | 0 | 14/14/14 |
| `STEENBRG` | 1 | 1 | 0 | NOT_EVALUATED | 14/14/14 | - | - | 0 | 14/14/14 |
| `TAX213322` | 1 | 1 | 0 | NOT_EVALUATED | me=0 | - | - | 0 | - |
| `TAX53322` | 1 | 1 | 0 | NOT_EVALUATED | me=0 | - | - | 0 | - |
| `YORKNET` | 1 | 1 | 40 | EVALUATED | 0/0/0 | 0/0/0 | 64/64/64 (56) | 120 | 0/0/0 |
| `ZAMB2-8` | 1 | 1 | 43 | EVALUATED | 0/0/0 | 0/0/0 | 0/0/0 (0) | 32 | 0/0/0 |

## 4. Identity check and threshold agreement (per row file)

`identity` = rank([Ae; E_S]) == |S| + rank(Ae restricted to the columns not in S), computed by a SECOND QR on a different matrix, at default / 1e-10. `agree` = default and 1e-10 give the same rank on the raw rows.

| cell | pt | eq agree | skel agree | skel identity | entry agree | entry identity |
|---|---|---|---|---|---|---|
| `ACOPP118` | x0 | 1 | 1 | 1 | 1 | 1 |
| `ACOPP57` | x0 | 1 | 1 | 1 | 1 | 1 |
| `ACOPP57` | xproj | 1 | 1 | 1 | 1 | 1 |
| `ACOPR118` | x0 | 1 | 1 | 1 | 1 | 1 |
| `ACOPR57` | x0 | 1 | 1 | 1 | 1 | 1 |
| `BIGBANK` | x0 | 1 | 1 | 1 | 1 | 1 |
| `BRITGAS` | x0 | 1 | 1 | 1 | 1 | 1 |
| `DALLASM` | x0 | 1 | 1 | 1 | 1 | 1 |
| `MSS2` | x0 | 1 | 1 | 1 | 1 | 1 |
| `MSS3` | x0 | 1 | 1 | 1 | 1 | 1 |
| `NET3` | xproj | 1 | 1 | 1 | 1 | 1 |
| `READING6` | x0 | 1 | 1 | 1 | 1 | 1 |
| `READING8` | x0 | 1 | 1 | 1 | 1 | 1 |
| `SAWPATH` | x0 | 0 | 0 | 0 | 0 | 0 |
| `SAWPATH` | xproj | 0 | 0 | 0 | 0 | 0 |
| `SMBANK` | x0 | 1 | 1 | 1 | 1 | 1 |
| `STEENBRB` | x0 | 1 | 1 | 1 | 1 | 1 |
| `STEENBRC` | x0 | 1 | 1 | 1 | 1 | 1 |
| `STEENBRD` | x0 | 1 | 1 | 1 | 1 | 1 |
| `STEENBRD` | xproj | 1 | 1 | 1 | 1 | 1 |
| `STEENBRE` | x0 | 1 | 1 | 1 | 1 | 1 |
| `STEENBRE` | xproj | 1 | 1 | 1 | 1 | 1 |
| `STEENBRG` | x0 | 1 | 1 | 1 | 1 | 1 |
| `TAX213322` | x0 | - | - | - | 1 | 1 |
| `TAX53322` | x0 | - | - | - | 1 | 1 |
| `YORKNET` | x0 | 1 | 1 | 1 | 1 | 1 |
| `YORKNET` | xproj | 1 | 1 | 1 | 1 | 1 |
| `ZAMB2-8` | x0 | 1 | 1 | 1 | 1 | 1 |
| `ZAMB2-8` | xproj | 1 | 1 | 1 | 1 | 1 |

