# M6 W0.5 — No-copy claim-stream path — SPEC v4 (spec of record)

Supersedes v1–v3. Codex rounds 1–4 are decided by `w05-spec-r1-rulings.md` R-1..R-17, binding
here: round 2 resolved all eleven prior items and raised three, settled as R-11/R-14 (built-against stamp with generation counter),
R-12 (one arena, honest cost), R-13 (gate vs discharge). Discharges the KEPT-OPEN `Transcribe_16seg`
+2.45% deviation (m5-ledger:240-244) against m4-ledger:2210-2226. Every code claim carries a file:line
checked 2026-08-28.

## §0 Scope / non-scope

- **IN.** The path by which a laid `NonLinearProgram` publishes its claim stream so a consumer reads
  the host's arrays instead of copying them: (a) hven builds the claim-convention arrays at lay, (b)
  publishes them as non-owning views under a stated validity term, (c) `AggregateEvalSeam`'s
  objective-gradient copy goes away, (d) tycho's
  `LaidSnapshot`/`snapshot_layout()`/`materialize_stream()`/`stream_current_` DELETE.
- **OUT.** hven's slot order, `kkt_coeff_rows_/cols_/part_ids_`, `kkt_locations_`, the structural-key
  conjuncts, every hash VALUE — layout determinism is HARD, which is why §2 rejects "re-lay hven
  domain-major". ALSO OUT, the IPM: it consumes `NonLinearProgram` through
  `get_mat_space`/`get_kkt_space`, never through `AggregateEvalSeam`, and W0.5 adds arrays BESIDE the
  raw ones, renumbering nothing — asserted in §4(a), not assumed.
- **OUT — registered elsewhere, not folded in.**
  - The deterministic-fold vs no-copy-SCATTER mode pair (m4-ledger:322-331); `publish_matrix`'s
    per-evaluate segment copy (`aggregate_eval_seam.cpp:534-539`) is that item's. **R-10 rider:** a
    direct-scatter path needs a raw↔claim permutation or a second location table, since
    `kkt_locations_[i]` is indexed by RAW emission slot (`:1046-1055`). Not built here; TRIGGER = the
    first scatter-mode design pass.
  - **R-9 — W0.5b candidate.** Per-lay copies outside the claim stream: `analyze_partitioning()`
    refilling the partition vectors (`non_linear_program.cpp:733-760`) with `TypeStorage`'s
    deep-cloning copy ctor (`type_storage.h:94-98`); the staged→laid bounds copy (`:700`);
    validation's bounds vector (`aggregate_declaration.cpp:109-115`). TRIGGER and required first step:
    add a `clone` cell to the layout_time probe so this share of `transcribe` is MEASURED before
    anyone designs around it.

## §1 tycho-side requirement, and what deletes

- **WHAT TYCHO PAYS TODAY.** `TranscribedAggregate`'s ctor calls `snapshot_layout()`
  (`tycho/src/solvers/transcribed_aggregate.cpp:53-59`): one `make_unique_for_overwrite<int[]>(3N +
  G)` plus four memcpys of `kkt_coeff_rows_/cols_/part_ids_` and `rhs_coeff_rows_ + pgx_data_start_`
  (:218-235). The bench cell builds the provider inside `transcribe()`
  (`ode_phase_base.cpp:1552-1557`), so that allocation and cold round trip IS the deviation;
  `materialize_stream()` (:248-411) is first-accessor only (:127-130) and no solve path reads it.
  16seg N≈1380, G≈2 → ≈16.6 KB; 64seg N≈5460 → ≈65 KB.
- **WHY A VIEW OF THE RAW ARRAYS DOES NOT SUFFICE.** hven claims partition-major — per partition,
  objective-Hessian, equality (Jac+Hess), inequality (`non_linear_program.cpp:841-852`) — while the
  contract wants one contiguous run per DOMAIN (`claim_stream_source.h:35-40`). Two restatements ride
  on the permutation: a constraint row carries the slack offset (`:827-829`) the assembled space
  lacks, and a Hessian pair is in walk order, deliberately NOT normalized (`:985-993`). Views are
  reachable only if the HOST lays claim-convention arrays.
- **REQUIREMENTS**, ratified by the tycho lane: (a) domain-contiguous views, one run per domain; (b)
  the claim convention AT LAY — square space n+me+mi, no slack block, Hessian upper triangle; (c)
  declaration-space identities surviving an elimination-only re-lay; (d) partition attribution; (e)
  objective-gradient rows of the primal block, in claim order. (e) is free at an unreduced lay —
  `pgx_data_start_ == 0` (:949) makes `rhs_coeff_rows_.head(num_pgx_elems_)` already it, so tycho's
  fourth memcpy is waste — but not after an elimination, where the emitter returns -1
  (`indexing_data.h:169-175`); hence (c)'s retained copy.
- **WHAT DELETES tycho-side** (`tycho/.../transcribed_aggregate.h:235-328` entire): `DeclaredShape`,
  `LaidSnapshot`, `publish()`, `refresh_if_relaid()`, `snapshot_layout()`, `materialize_stream()`,
  `read_at_shape_`, `laid_`, `stream_current_`, the four `mutable VectorXi` members. The class becomes
  a forwarding shim, the shape its other overrides already have (:114-175).

## §2 The hven contract change

**R-8 — a view-bearing `AggregateDeclaration` is REJECTED AS A CHOICE, NOT A NECESSITY.** Views are
movable and `declaration_key` excludes pieces and partitioning (`aggregate_declaration.cpp:280-316`),
so the value form is not forced; it is CHOSEN, because no consumer needs one (the claim-stream views
are a separate surface, ratified by tycho) and it leaves `adopt_declaration`'s move-out
(`non_linear_program.cpp:88, 170-172`) and the declaration-key path untouched.

**PUBLIC SURFACE** (`include/hven/model/non_linear_program.h`). Non-virtual, valid only after a lay;
`NonLinearProgram` does NOT become a `ClaimStreamSource`, `TranscribedAggregate` forwards (Q2). All
ten return `Ref` views into ONE owned arena (R-12); no accessor owns storage.

    Eigen::Ref<const Eigen::VectorXi> kkt_claim_rows() const;
    Eigen::Ref<const Eigen::VectorXi> kkt_claim_cols() const;
    Eigen::Ref<const Eigen::VectorXi> objective_gradient_claim_rows() const;
    ClaimBlock hessian_claims() const;
    ClaimBlock equality_jacobian_claims() const;
    ClaimBlock inequality_jacobian_claims() const;
    StructureEpoch claim_stream_epoch() const;
    // R-4: one table per domain, length num_partitions_ + 1.
    Eigen::Ref<const Eigen::VectorXi> hessian_claim_partition_offsets() const;
    Eigen::Ref<const Eigen::VectorXi> equality_jacobian_claim_partition_offsets() const;
    Eigen::Ref<const Eigen::VectorXi> inequality_jacobian_claim_partition_offsets() const;

**R-4 encoding.** `offsets_d[p] .. offsets_d[p+1]` is partition p's run WITHIN domain d, relative to
that domain's `ClaimBlock::start_`; an empty partition gives equal adjacent offsets; `offsets_d[0] ==
0`, `offsets_d[P] == block_d.count_`. Lossless because each domain run is partition-major by
construction (the partition loop is the outer one, `:841-852`). tycho keeps one O(partitions) check
(monotone, last == run length) instead of its per-slot exclusivity scan.

    // Private. R-11: the stamp is what the arena was last built against -- compared, never a
    // flag. R-12: ONE owned arena of 2N + G + 3(P+1) ints, cut into
    // [rows | cols | gradient rows | offsets_H | offsets_Ae | offsets_Ai]; every accessor Refs it.
    struct ClaimStamp {
        std::uint64_t declaration_generation_ = 0; // R-14: bumped where master piece lists are
                                                   // replaced (make_nlp / adopt_declaration)
        int user_kkt_elems_ = 0, pgx_elems_ = 0, partitions_ = 0;
        friend bool operator==(const ClaimStamp &, const ClaimStamp &) = default;
    };
    VectorXi claim_arena_;
    int claim_slots_ = 0, claim_grad_ = 0, claim_parts_ = 0;  // widths the Refs are cut at
    ClaimBlock claim_hessian_, claim_equality_, claim_inequality_;
    StructureEpochCounter claim_stream_epoch_;   // structure_identity.h:290-313
    ClaimStamp claim_built_against_{};

**R-1 — FILL STRATEGY AND ITS TRUE COST.**

- The fused "in registers" fill of v1 is WITHDRAWN: pieces emit inside opaque loops through
  `KktClaimSpace`'s one cursor per call (`claim_space.h:107`, `solver_function_base.h:92`), and W0.5
  does NOT change the piece contract. Instead, **one extra O(N) integer pass at lay**,
  `restate_claim_stream()`, after `get_rhs_space()` and before the epoch bumps — the pass tycho
  already runs in `materialize_stream()` (`transcribed_aggregate.cpp:340-400`), relocated rather than
  invented and tuned there (:354-358). Run lengths need no counting prepass: `num_kkt_elements(dojac,
  dohess)` is additive in its flags at every implementer in either tree (`nlp_adapter.h:509-511`,
  `:370-372`; `fixed_variable_row.h:115`; `dense_function_base.h:1311-1329`) and `count_elems()`
  already issues MIXED-flag calls (`non_linear_program.cpp:712, 716, 721`), so it splits per domain at
  zero new traversal; if additivity ever breaks, the fallback is a counting prepass.
- Domain classification costs one compare per slot. `get_mat_space()`'s partition loop already
  computes `kkstart`/`kklen` (:842, :849); it also records two intra-partition cursor marks, after
  `claim(part_obj_)` and `claim(part_eq_)`. Slots before the first mark are objective-Hessian; inside
  the two mixed ranges a single `row < reduced_primal_vars_count_` separates that segment's Hessian
  from its Jacobian. Fallback if review rejects the marks: `kkt_coeff_part_ids_` (:851) plus the row
  band, at a third read per slot.
- **OFFSETS ARE DERIVED; MARKS ARE NOT OFFSETS (R-12).** A mark is an absolute RAW-slot cursor; a
  published offset is CLAIM-order, domain-relative. The marks delimit each partition's raw segment;
  `restate_claim_stream()` samples its own three OUTPUT cursors at each partition boundary and writes
  `offsets_d[p] = out_cursor_d(at the start of partition p) - block_d.start_` for d in {H, Ae, Ai},
  closing each table with `offsets_d[P] = block_d.count_`; `offsets_d[0] == 0` holds by construction,
  since `out_cursor_d` starts at `block_d.start_`.
- **TRUE COST, honestly (R-12)**, stated so §4(c) can falsify it. Per REBUILD: ONE arena allocation of
  2N + G + 3(P+1) ints (the third N tycho snapshots is its per-slot partition array, which W0.5 does
  not publish), built as a local and swapped in — `VectorXi` zero-fills in this build
  (`CMakeLists.txt:224-237`), an O(arena) write already inside the traffic figure; 2N warm int reads (`get_mat_space` just wrote
  them) and 2N writes; a G-int gradient copy; 3(P+1) offset writes; no sort. Against it tycho stops
  paying one allocation of 3N + G ints and its cold memcpy traffic, charged to every transcribe. The
  change is therefore allocation-NEUTRAL on a fresh transcription — one removed, one added — and in
  WORD ACCESSES roughly neutral too (R-17): the zero-fill of 2N+G+3(P+1) plus 2N reads + 2N+G writes
  against tycho's 3N+G reads + 3N+G writes nets to ≈ +G + 6(P+1) accesses, i.e. no word-count lever
  at all. The lever is QUALITATIVE: one fewer cold allocation-and-memcpy on the consumer side, all
  hven writes landing on lines get_mat_space() just touched, and on an elimination-only re-lay hven
  allocates and writes NOTHING where the old path re-snapshotted. A modest net, NOT a free win — see
  R5.

**R-2 / R-11 — RETAINED SET AND THE BUILT-AGAINST STAMP.**

- **RETAINED SET:** {claim rows, claim cols, declaration-space gradient claim rows, the three
  `ClaimBlock`s, the three partition offset tables} — all six living in `claim_arena_`.
- **THE ENTRY-SET DIRTY FLAG IS WITHDRAWN (R-11):** no set of mutation sites is complete. `make_nlp()`
  may be re-run with new dimensions and re-lays itself (`non_linear_program.cpp:22-38`, `:78-85`);
  `adopt_declaration` with fixing rows rebuilds TWICE, in `make_nlp` and again after
  `splice_fixed_variable_rows` (`:186-199`); those rows move `equal_cons_` and the claim counts
  (`:575-592`, `:595-609`); `configure_variable_treatment` discards and installs them on paths that
  also eliminate (`:1170-1179`, `:1346-1392`) — MakeConstraint→MakeParameter being the case an
  entry-set flag consumes too early.
- **INSTEAD, A STAMP (R-14).** `claim_built_against_` records {`declaration_generation_`,
  `num_user_kkt_elems_`, `num_pgx_elems_`, `num_partitions_`}. The generation is a plain counter on
  `NonLinearProgram`, incremented at the two sites that REPLACE the master piece lists —
  `make_nlp()` (`non_linear_program.cpp:22-85`) and `adopt_declaration` (:88) — and nowhere else;
  `declaration_identity_digest` is NOT in the stamp: it hashes dimensions only
  (`aggregate_declaration.cpp:316-338`), so two declarations with equal dimensions and equal claim
  counts but different sparsity would collide on it, and the arena would go stale. The generation
  cannot collide: any path that can change the claim pattern replaces the pieces. Comparing the
  stamp is O(1).
  `rebuild_structures` (:202-243) computes the current tuple AFTER the raw rebuild, once
  `count_elems()` and `get_mat_space()` have set the counts, and rebuilds the arena IFF it differs;
  otherwise the arena is retained untouched. Only the two generation sites carry bookkeeping, and both
  already replace the pieces. Fixing-row adoption rebuilds TWICE (`:186-199`): the second rebuild ADDS
  the fixing rows' KKT slots (`:575-592`, `fixed_variable_row.h:113-115`), so the counts differ and the
  arena is restated AGAIN — correct, and charged honestly as one extra restatement on that path only.
  A repeated `make_nlp()` bumps the generation and rebuilds. `invalidate_laid_state` (:326) must not
  clear the arena or the stamp.
- **DEFINITION.** An **elimination-only re-lay IS a re-lay whose stamp tuple is unchanged** — the
  `:1346-1392` path where declaration, claim counts and partition count all stand and only
  `reduced_primal_vars_count_` and the output maps move. A MakeConstraint→MakeParameter switch CHANGES
  the declaration, the counts differ, and a rebuild there is CORRECT, not a bug.
- **REFUSAL.** When the stamp DIFFERS while `is_reduced()` (`non_linear_program.h:437`), a rebuild
  would have to restate a REDUCED layout into declaration space, which it cannot; the accessors throw
  `std::invalid_argument` saying so — tycho's `DeclaredShape` refusal
  (`transcribed_aggregate.cpp:140-165`) relocated to where the data is. Stamp unchanged while reduced
  is the RETAIN path, not a refusal.
- **SIGNAL.** `claim_stream_epoch()` bumps only when the arena is rebuilt; `structure_epoch()` keeps
  its meaning and still bumps on every re-lay (:242). View validity keys on `claim_stream_epoch()`;
  pointer identity is NOT the contract.

**R-3 — EXCEPTION-SAFETY COMMIT RULE (contract sentence).** *The claim arena is built as a local and
committed by one `VectorXi::swap`, with the three `ClaimBlock`s and the stamp, only after the raw
rebuild has succeeded and before either epoch is bumped; a throw during a re-lay therefore leaves the
previously published views valid under the epoch they were read at.* Placement: after
`get_rhs_space()`/`publish_location_tables()`, before `bump_structure_epoch()` (:230-242, which cannot
throw). CONTRACT TEXT: `claim_stream_source.h`'s VIEW VALIDITY (:42-45) is amended to name
`claim_stream_epoch()` and state the elimination-survival term; `NlpModelAggregate` returns its
structure epoch there (no eliminating re-lay) and documents that identity. STREAM SHAPE (:35-40) is
unchanged: claim ORDER does not move.

### §2 AMENDMENT BLOCK (2026-08-28, implementation review round 1)

Two rulings from the round-1 implementation review supersede the corresponding sentences
above. Both are mechanism, not contract: nothing a consumer can observe changes, and the
close gate §4(a) is untouched by either.

**R-18 — ARENA RETAIN-AND-REUSE, replacing "built as a fresh local per rebuild".** The
provider keeps **TWO** arena buffers, LIVE and SPARE. A rebuild builds into the SPARE —
resized only when the required width DIFFERS from the width it already has, and never
`setZero`-ed on top of that resize, since every word of it is written before any is read —
and commits by swapping spare↔live before the epoch bump. R-3 is preserved exactly: a
throw during the build leaves the live arena, its blocks, its stamp and its epoch
untouched. The swap is also the RECYCLE: what was live becomes the next rebuild's spare,
so **an equal-width rebuild performs no allocation at all**.

RATIONALE, from the §4(c) reading. §2's "one allocation per rebuild, allocation-neutral
against tycho's removed snapshot" was true as arithmetic and wrong as cost. At 22528
claims the arena is 196 KB, past glibc's `mmap` threshold, so a fresh arena per lay is an
`mmap`/`munmap` pair and the page faults behind it — which is what made the probe's
`construct` cell BIMODAL at head only (+77 / +159 µs at n=1024, base unimodal). Reuse
removes the cycle rather than making the allocation cheaper. The `setZero`-on-top-of-resize
that C1 found goes with it.

PINS: an equal-width rebuild allocates nothing (the published storage address ALTERNATES
A, B, A, B across four rebuilds at one claim structure, and no third address appears);
throw-mid-rebuild leaves the live views standing (the existing R-3 pin).

**R-19 — THE RESTATEMENT LOOP.** Three changes, none of them contract:
(a) the equality and inequality Jacobian segments are written as SEPARATE loops, each
advancing one cursor BY NAME — the previous `inequality ? cursor_inequality :
cursor_equality` reference selection forced all three cursors to memory for the whole
walk, and that, not the allocation, was ~92% of the +76 µs;
(b) the array-wide refusals — a negative coordinate, a column outside the declared
variables, a gradient row outside them — are hoisted OUT of the per-slot walk into Eigen
reductions taken once, with a scalar re-walk on the failure path alone so the refusal still
names its slot;
(c) the per-emit overrun guards STAY: they are the second half of the additivity check and
they are one predictable compare.

The §4(c) probe is re-run against these (all cells including `construct`, both arms,
serialized, reps stated); the reading is INFORMATIONAL per CLAUDE.md §7, as before.

## §3 Per-lay copies — cost and disposition

| # | site | cost per lay | W0.5 |
|---|------|--------------|------|
| 1 | tycho `snapshot_layout` alloc + 4 memcpy (`transcribed_aggregate.cpp:218-235`) | 1 alloc + 3N+G ints, cold (≈16.6 KB @16seg) | **REMOVED** — the deviation's content |
| 2 | seam `gradient_rows_` vector assign (`aggregate_eval_seam.cpp:463-464`) | G ints + owning vector | **REMOVED** — the epoch term answers the stated reason ("the provider's storage moves under a re-lay"); the retained array carries a dropped-row sentinel verbatim |
| 3 | seam `read_claims` owning vectors (`aggregate_eval_seam.cpp:93-120`, called :430-440) | 2 owning `vector<int>`, 2N ints, plus validation | **LEFT.** `build_domain` feeds `setFromTriplets`, which SORTS; the sorted order needs an owning buffer. A permutation over a view halves it but rewrites the pattern-build path and must be proven bit-identical on the pattern's value order. Follow-on |
| 4 | seam bounds copy (`aggregate_eval_seam.cpp:267-274`) | 1 `vector<VariableBound>` + 2 Eigen vectors, O(n) | **LEFT.** An intersection — a derived value with no stored array to view. A declaration read, not a claim-stream read |
| 5 | `materialize_declaration_pieces` copy-once (`non_linear_program.cpp:398-434`) | 3 piece-list copies + thaw, lazy, once per lay | **LEFT.** §2 R-8. Not on the deviation path |
| 6 | `publish_matrix` segment copy (`aggregate_eval_seam.cpp:534-539`) | O(domain nnz) doubles, PER EVALUATE | **LEFT — out of scope**, m4-ledger:322-331 |
| 7 | `analyze_partitioning` deep clone of type-erased pieces (`non_linear_program.cpp:733-760`, `type_storage.h:94-98`) | O(pieces) `clone_into` per lay | **LEFT — registered R-9** (§0). Measure first |
| 8 | staged→laid bounds copy (`non_linear_program.cpp:700`) + validation's bounds vector (`aggregate_declaration.cpp:109-115`) | O(n) per lay, twice | **LEFT — registered R-9** (§0) |

## §4 Acceptance — the CLOSE GATE, then the informational readings (R-13)

- **(a) CLOSE GATE — counters and bit-identity ONLY, non-negotiable.** Wall-clock is informational per
  CLAUDE.md §7 (`:204-213`), so nothing timed in (b) or (c) asserts anything; (b) is the criterion on
  which the DEVIATION RECORD discharges, not a gate on the work.
  - **REFERENCE RESTATEMENT, hven standalone (R-13 leg i).** A test computes an INDEPENDENT reference
    stream from the raw laid arrays — written against the §1 convention (constraint row minus the
    slack offset, `min`/`max` upper triangle, domain runs in partition-index order), sort-free, NOT
    against `restate_claim_stream()`'s code — and asserts equality element for element on the
    published rows, cols, three `ClaimBlock`s, gradient rows, and the offset tables EXPANDED to
    per-slot partition ids. Over the U0 corpus plus the elimination fixtures (elimination-only re-lay,
    MakeConstraint switch, partition renegotiation, repeated `make_nlp()`). This gate leg outlives
    tycho's materializer.
  - **OLD-MATERIALIZER ORACLE, tycho (R-13 leg ii).** On consume STEP 1, before step 2 deletes it, a
    test asserts the new hven stream equals `materialize_stream()`'s output bit-exact
    (`tycho/.../transcribed_aggregate.cpp:248-411`) on the bench fixtures, per-slot partitions
    included. Used once, then deleted.
  - U0 27-cell counter replay bit-identical on BOTH arms (m6-ledger:93, :340); `kkt_locations_` and
    every `get_mat_space` / `get_kkt_space` output byte-identical — the concrete form of "the IPM path
    is untouched".
  - R-7: `claim_digest()` keeps hashing the RAW rows/cols in RAW order
    (`non_linear_program.cpp:467-471`); the arena is NEVER hashed, normalized, or reordered in place.
    The pinned literal `14789870936883269507ULL` (`test_pattern_hash.cpp:133`, `:343`) and the
    RELATIONAL order checks in `test_model_structure_key.cpp:128-140` stay green UNCHANGED (v1
    misattributed the literal to the latter file).
- **(b) DEVIATION-RECORD DISCHARGE — INFORMATIONAL wall, owned by the TYCHO lane.** The record was
  opened on a wall reading by owner ruling, so it closes on one, recorded as informational with the §7
  provenance stamp and never quoted as an assertion. `BM_Phase_Transcribe_16seg`/`_64seg` at the tycho
  commit that CONSUMES the new surface and DELETES §1's machinery, against a FRESH `bench_track`
  baseline at the commit immediately preceding it — same box and mode, serialized per §7 (5 reps,
  alone, provenance stamped). Anchor 2026-08-28: ≈30.6 / ≈204 µs at tycho 05871e17. Criterion: within
  ±1%, directionally at or below. `_64seg` is bimodal there (`tycho/bench/BASELINE.md:35`) — record
  which mode BOTH arms settled into, or the cell is not quotable. Discharges on the pair.
- **(c) LAYOUT PROBE — INFORMATIONAL neutrality (R-5 as corrected by R-13).** The IPM wall leg is NOT
  the instrument: band 3.0%, not v1's 0.50%, and blind to layout by construction
  (`m4-ipm-wall-leg/README.md:39-41, :92-97`). The instrument is that directory's `layout_time.cpp`:
  cells `transcribe`, `+decl`, `+key`, base vs head, serialized, reps stated. PRE-STATED EXPECTATION:
  `transcribe` moves by at most §2's restatement pass, `+decl`/`+key` by the SAME and no more — W0.5
  touches neither the declaration cache nor the two LAZY digests (claim / bound); the stamp compare is
  an O(1) integer tuple, no hash. A falsified expectation does NOT
  mechanically block; it triggers SETTLER ADJUDICATION, recorded before close. EAGER is the default; a
  latch only as a DECLARED fallback with the probe reading as its reason.
- **(d) NO-COPY ASSERTED — R-6, no `HVEN_TESTING` counter.** Withdrawn: `docs/testing.md:26` reserves
  the seam convention for facts no boundary observation can reach, and this one is reachable. No-copy
  is asserted by (i) `.data()` identity between the hven view and the tycho accessor, in a tycho
  consume test (`transcribed_aggregate.cpp:87-90`); (ii) deletion of the seam's
  `gradient_rows_.assign(...)` (`aggregate_eval_seam.cpp:463-464`), a diff fact; (iii) eager-vs-latch
  as a code-review fact.
- **(e) DECLARED DOC-ONLY CHANGE — R-7.** `ModelStructureKey`'s "taken over the claim stream a
  provider handed out, in claim order" (`structure_identity.h:21-24`) and `claim_stream_digest`'s "the
  claim stream in claim order" (`:185-188`) become "the provider's laid claim slots in EMISSION
  order". Value and digest unchanged, declared in the ledger as doc-only; without it the header
  contradicts itself once a second, domain-major stream exists.

## §5 Risks

- **R1.** MOVE-in consumers of `AggregateDeclaration` (`non_linear_program.cpp:88, 170-172`;
  `sqp_driver.cpp`, `warm_start_data.h`, `nlp_model_aggregate.h`). MITIGATED BY DESIGN — §2 R-8 leaves
  it a value; if a later review pushes toward view-bearing, this becomes the blocker.
- **R2.** Crossover stamp. `declaration_key` (`structure_identity.h:262`,
  `aggregate_declaration.cpp:316`) is a pure function of the declaration value and MUST NOT MOVE. W0.5
  does not touch the declaration; assert that in §4(a) rather than reason about it.
- **R3.** `bindings/`. No `declaration`/`claim` reference there today, so no nanobind keep-alive
  policy is needed — RE-CONFIRM at implementation; a future binding adopts the epoch term.
- **R4.** Other `ClaimStreamSource` implementers must satisfy the strengthened lifetime term:
  `NlpModelAggregate`, `tests/sqp/support/claim_stream_double.h`,
  `tests/model/support/fake_aggregate.h`. A double that re-lays under a treatment must be checked.
- **R5.** **NET MAY UNDERSHOOT.** The deviation is +2.45% and §2's honest accounting is
  allocation-neutral and word-access-neutral (≈ +G+6(P+1)) — the lever is locality and one fewer
  cold memcpy, a smaller lever than v1 assumed. If §4(b) lands
  short, the close gate §4(a) is unaffected; what is at stake is the record's discharge, and the
  answer is the R-9 copies (§0) or the §3 row-3 permutation — not a silent re-scoping of W0.5.
- **R6.** tycho's three internal-consistency refusals become unreachable once hven guarantees the
  convention at lay: a claimed slack row, a negative coordinate in an unreduced layout, an
  out-of-range gradient row (`tycho/.../test_level2_provider.cpp:185-215`). Re-homed to hven or
  retired — a DECLARED break per CLAUDE.md §7.
- **R7.** STAMP COMPLETENESS. The generation counter is the load-bearing conjunct: a piece swap at
  identical dimensions and claim counts changes the SPARSITY and hence the stream, and only the
  generation catches it. Any future path that replaces or reorders master pieces without passing
  through `make_nlp()` / `adopt_declaration` MUST bump the generation — pinned by a state-machine
  test that swaps pieces at equal counts and asserts a rebuild.

## §6 Implementation sequencing — ONE hven implementer

1. **Contract text, no behavior.** Amend `claim_stream_source.h`'s VIEW VALIDITY (:42-45) for
   `claim_stream_epoch()` and elimination survival; apply §4(e)'s `structure_identity.h` amendment.
   Tests green unchanged.
2. **Arena + epoch + stamp.** Split `count_elems()` per domain; add `ClaimStamp`, `claim_arena_`, the
   widths and the cursor marks in `get_mat_space()`; add `restate_claim_stream()` (restatement,
   derived offsets, R-3 swap); add the stamp compare in `rebuild_structures`; add the ten `Ref`
   accessors and the reduced-layout refusal.
3. **Reference-restatement test — its own verifiable step, §4(a) leg (i).** From the convention,
   sort-free, independent of step 2's code, over the U0 corpus plus the elimination fixtures; must
   pass before step 4. Verify the rest of §4(a) — U0 replay, `kkt_locations_`, the pins — here too.
4. **Seam.** Replace `gradient_rows_`'s owning vector with a view of `objective_gradient_claim_rows()`
   (`aggregate_eval_seam.cpp:459-468`), keeping the sentinel pass-through and the bound check.
5. **State-machine tests.** Stamp-unchanged re-lay retains and does not bump `claim_stream_epoch()`
   while `structure_epoch()` moves; stamp-changed re-lay rebuilds and bumps both; `adopt_declaration`
   with fixing rows restates on both rebuilds (counts differ); repeated `make_nlp()` rebuilds; equal-
   count piece swap rebuilds (generation); MakeConstraint→MakeParameter rebuilds; R-3 throw-mid-relay; R-4 offset invariants; the stamp-differs-while-reduced refusal.
   Re-home R6's three refusals.
6. **Probe.** Run §4(c) base vs head against the pre-stated expectation. Add R-9's `clone` cell while
   the probe is open — cheap, and §0 requires it first.

PAIRED STEPS, owned by the TYCHO lane. STEP 1, consume + ORACLE: forward the accessors, adopt the
O(partitions) offset check, assert `.data()` identity (§4(d)(i)) AND §4(a) leg (ii) bit-exact against
the still-present `materialize_stream()`. STEP 2, deletion: §1's deletions including the (e) memcpy,
only once step 1 is green — the oracle may not be deleted before use. Then the §4(b) reading.

## §7 Open questions for the OWNER

None. R-1..R-13 settle every point Codex raised across both rounds, and the two declared breaks (§4(e)
wording, R6's re-homed refusals) follow CLAUDE.md §7's declare-and-re-derive protocol, not an owner
ruling. ESCALATE only if §4(b) lands short (R5): a partial DISCHARGE of the deviation record versus
opening W0.5b is the owner's call — the close gate §4(a) is not in that conversation.

### Note (2026-08-28, settler): ClaimStreamSource declares NO per-slot partition accessor
(`include/hven/model/claim_stream_source.h` — the only partition mentions are the STREAM SHAPE
ordering prose); tycho's `kkt_claim_partitions()` is tycho-local and deletes freely. The offset tables
are a NonLinearProgram surface, not added to ClaimStreamSource in W0.5.
