#!/usr/bin/env python3
"""Apply the T8.9r single-row interior lever to a T8.3/T8.4-era arm extraction.

The committed lever (d5931e8) cannot be `git apply`-ed to these trees: at
510a4bb and 9cebbbe `write_interior_provenance` has no `parts2` parameter (that
is T8.9's), and the surrounding text has moved.  This script performs the SAME
EDITS against anchors those trees do carry, and the arm's own `diff -u` against
its unpatched extraction is retained beside it as the adapted patch.

Applied IDENTICALLY to both arms: their bench/bench_corpus.cpp and
bench/ipm_corpus_leg.h are byte-identical to each other, so the two adapted
patches are byte-identical too, and the lever is common-mode.

The tests/ half of the committed lever is NOT applied: these builds are
-DHVEN_BUILD_TESTS=OFF and compile no test target.
"""
import sys, pathlib

arm = pathlib.Path(sys.argv[1])

def edit(rel, pairs):
    p = arm / rel
    s = p.read_text()
    for old, new in pairs:
        assert s.count(old) == 1, f"{rel}: anchor not unique/found: {old[:70]!r}"
        s = s.replace(old, new, 1)
    p.write_text(s)

# ---- bench/ipm_corpus_leg.h -------------------------------------------------
edit("bench/ipm_corpus_leg.h", [(
"""/// @brief The row's key: the cell id joined to the treatment by a slash, plus""",
"""/// @brief The treatment named by @p tag, spelled as interior_treatment_tag
///        spells it.
FixedVariableTreatments interior_treatment_from_tag(std::string_view tag);

/// @brief Runs EXACTLY ONE BASE ROW of this leg, in process, and returns it.
///        The routing only: @p cell_id selects between run_interior_hs071 and
///        run_interior_cell at interior_base_variant(), exactly as the leg's
///        own two loops do.
InteriorRow run_interior_single_row(std::string_view cell_id, FixedVariableTreatments treatment,
                                    const InteriorLevers &levers);

/// @brief The row's key: the cell id joined to the treatment by a slash, plus""")])

# ---- bench/ipm_corpus_leg.cpp ----------------------------------------------
edit("bench/ipm_corpus_leg.cpp", [(
"""std::string interior_row_key(const InteriorRow &row) {""",
"""FixedVariableTreatments interior_treatment_from_tag(std::string_view tag) {
    for (const FixedVariableTreatments treatment : interior_treatments()) {
        if (tag == interior_treatment_tag(treatment)) {
            return treatment;
        }
    }
    throw std::invalid_argument(fmt::format("interior_treatment_from_tag: '{}' is not one of "
                                            "MakeParameter|MakeConstraint|RelaxBounds",
                                            tag));
}

InteriorRow run_interior_single_row(std::string_view cell_id, FixedVariableTreatments treatment,
                                    const InteriorLevers &levers) {
    if (cell_id == kHs071FixedCellId) {
        return run_interior_hs071(treatment, levers, interior_base_variant());
    }
    const CorpusCell *cell = find_cell(std::string(cell_id));
    if (cell == nullptr) {
        throw std::invalid_argument(
            fmt::format("run_interior_single_row: unknown cell id '{}' (a corpus cell id, or "
                        "'{}')",
                        cell_id, kHs071FixedCellId));
    }
    return run_interior_cell(*cell, treatment, levers, interior_base_variant());
}

std::string interior_row_key(const InteriorRow &row) {""")])

# ---- bench/bench_corpus.cpp -------------------------------------------------
edit("bench/bench_corpus.cpp", [
 # (a) using declarations
 ("using hven::solvers::corpus::interior_treatment_tag;\n",
  "using hven::solvers::corpus::interior_treatment_from_tag;\n"
  "using hven::solvers::corpus::interior_treatment_tag;\n"),
 ("using hven::solvers::corpus::run_interior_infeasible;\n",
  "using hven::solvers::corpus::run_interior_infeasible;\n"
  "using hven::solvers::corpus::run_interior_single_row;\n"),
 # (b) usage: synopsis + the flag
 ('    "       hven_sqp_corpus --dump-qp <cell> --dump-qp-out <path>\\n"\n',
  '    "       hven_sqp_corpus --dump-qp <cell> --dump-qp-out <path>\\n"\n'
  '    "       hven_sqp_corpus --internal-run-one <cell> --engine interior\\n"\n'
  '    "                       --treatment MakeParameter|MakeConstraint|RelaxBounds\\n"\n'
  '    "                       --internal-out <path>\\n"\n'),
 ('    "  --ssn-prox-carry  MEASUREMENT ARM. Set SqpOptions::ssn_prox_carry (a real,\\n"\n',
  '    "  --treatment T     THE SINGLE-ROW INTERIOR MODE (M6 W5 T8.9r), and the only\\n"\n'
  '    "                    form in which --internal-run-one is a DOCUMENTED\\n"\n'
  '    "                    surface. With --internal-run-one <cell> --engine\\n"\n'
  '    "                    interior --internal-out <path> it runs EXACTLY ONE BASE\\n"\n'
  '    "                    ROW of the interior leg, in process -- no variant row,\\n"\n'
  '    "                    no other cell, no fork -- and writes it under the leg\'s\\n"\n'
  '    "                    own column header, behind a provenance header of its\\n"\n'
  '    "                    own. T is MakeParameter | MakeConstraint | RelaxBounds;\\n"\n'
  '    "                    <cell> is a dual-bindable corpus cell id or\\n"\n'
  '    "                    hs071_x1_fixed. REFUSED with any other invocation.\\n"\n'
  '    "  --ssn-prox-carry  MEASUREMENT ARM. Set SqpOptions::ssn_prox_carry (a real,\\n"\n'),
 # (c) Args field
 ("    std::optional<std::string> internal_out;\n",
  "    std::optional<std::string> internal_out;\n"
  "    // THE SINGLE-ROW INTERIOR MODE'S one extra word (M6 W5 T8.9r).\n"
  "    std::optional<std::string> treatment;\n"),
 # (d) parser
 ('        } else if (arg == "--internal-out") {\n            a.internal_out = next_value(arg);\n',
  '        } else if (arg == "--internal-out") {\n            a.internal_out = next_value(arg);\n'
  '        } else if (arg == "--treatment") {\n            a.treatment = next_value(arg);\n'),
 # (e) provenance split -- the ARM signature has no parts2 parameter
 ("""void write_interior_provenance(std::ostream &os, int argc, char **argv,
                               const InteriorLevers &levers,
                               const std::vector<std::string> &refusals) {
    std::string invocation;""",
  """void write_interior_provenance_head(std::ostream &os, int argc, char **argv,
                                    const InteriorLevers &levers) {
    std::string invocation;"""),
 ("""    os << "# treatments: MakeParameter,MakeConstraint,RelaxBounds -- one row each, per cell\\n";
    os << "# key: column 0 is <cell_id>/<fixed_treatment>, plus /<variant> on a non-base row\\n";
    os << fmt::format("# {}\\n", interior_variant_stamp(interior_base_variant()));
    for (const InteriorVariant &variant : interior_exit_variants()) {""",
  """}

void write_interior_single_row_provenance(std::ostream &os, int argc, char **argv,
                                          const InteriorLevers &levers) {
    write_interior_provenance_head(os, argc, argv, levers);
    os << "# single row: this file carries EXACTLY ONE BASE ROW -- one cell, one treatment, the "
          "base variant -- written in process by --internal-run-one --engine interior. No variant "
          "row, no other cell, no fork.\\n";
    os << "# key: column 0 is <cell_id>/<fixed_treatment>; this mode never writes the third "
          "(variant) segment\\n";
    os << fmt::format("# {}\\n", interior_variant_stamp(interior_base_variant()));
}

void write_interior_provenance(std::ostream &os, int argc, char **argv,
                               const InteriorLevers &levers,
                               const std::vector<std::string> &refusals) {
    write_interior_provenance_head(os, argc, argv, levers);
    os << "# treatments: MakeParameter,MakeConstraint,RelaxBounds -- one row each, per cell\\n";
    os << "# key: column 0 is <cell_id>/<fixed_treatment>, plus /<variant> on a non-base row\\n";
    os << fmt::format("# {}\\n", interior_variant_stamp(interior_base_variant()));
    for (const InteriorVariant &variant : interior_exit_variants()) {"""),
 # (f) the single-row runner, after the artifact writer class
 ("""    std::set<std::string> keys_;
    std::size_t written_ = 0;
};
""",
  """    std::set<std::string> keys_;
    std::size_t written_ = 0;
};

void run_internal_interior_one(const std::string &cell_id, const std::string &treatment_tag,
                               const std::string &out_path, int argc, char **argv) {
    const FixedVariableTreatments treatment = interior_treatment_from_tag(treatment_tag);
    const InteriorLevers levers;
    InteriorArtifactWriter writer(out_path);
    write_interior_single_row_provenance(writer.stream(), argc, argv, levers);
    writer.stream() << interior_csv_header();
    writer.stream().flush();
    writer.require_ok("the header");
    writer.write_row(run_interior_single_row(cell_id, treatment, levers));
    writer.close();
}
"""),
 # (g) the dispatch
 ("""        if (args.internal_run_one) {
            if (!args.engine || !args.internal_out) {
                throw_usage("--internal-run-one requires --engine and --internal-out");
            }
            EngineLevers levers;""",
  """        if (args.treatment &&
            !(args.internal_run_one && args.engine && *args.engine == "interior")) {
            throw_usage("--treatment applies to --internal-run-one --engine interior only: it "
                        "names the one fixed-variable treatment that mode's single row runs "
                        "under, and the --engine interior leg writes all three per cell");
        }
        if (args.internal_run_one) {
            if (!args.engine || !args.internal_out) {
                throw_usage("--internal-run-one requires --engine and --internal-out");
            }
            if (*args.engine == "interior") {
                if (!args.treatment) {
                    throw_usage("--internal-run-one --engine interior requires --treatment "
                                "MakeParameter|MakeConstraint|RelaxBounds: the leg writes one row "
                                "per treatment and a single-row process writes exactly one");
                }
                if (args.ssn_prox_carry || args.ssn_certify_from_face ||
                    args.ssn_sigma_rule != SsnSigmaRule::kLadder ||
                    args.ssn_hint_rule != SsnHintRule::kIterationZeroFree ||
                    args.ssn_infeasibility_rule != SsnInfeasibilityRule::kSymptoms ||
                    args.score_model_surface || args.internal_force_setup_budget_s ||
                    args.internal_force_solve_budget_s || args.internal_force_child_throw ||
                    args.internal_force_child_abort) {
                    throw_usage("--internal-run-one --engine interior takes none of the SSN "
                                "measurement levers, the model-surface hook or the hidden child "
                                "levers: it runs the interior leg in process, drives no "
                                "SqpDriver, has no parent polling a marker and no child to force");
                }
                run_internal_interior_one(*args.internal_run_one, *args.treatment,
                                          *args.internal_out, argc, argv);
                return 0;
            }
            EngineLevers levers;"""),
])
print(f"adapted: {arm}")
