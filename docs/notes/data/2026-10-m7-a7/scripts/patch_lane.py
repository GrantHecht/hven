# A7 scratch-clone lane edit: the two labels (each one field from its base) and the
# A7_FIRE_LOG observation hook. Run from the bench clone root.
NL = "\\n"


def edit(p, pairs):
    s = open(p).read()
    for old, new in pairs:
        assert s.count(old) == 1, (p, old[:70], s.count(old))
        s = s.replace(old, new)
    open(p, 'w').write(s)


edit('lanes/hven/hven_bench_cell.cpp', [
    ("""    if (label == "default" || label == "sqp-qn") {
        o.column_scaling = hven::solvers::SqpColumnScaling::kAuto;
    }
    if (label == "default") {
        return o;
    }""", """    if (label == "default" || label == "sqp-qn" || label == "sqp-wsrule" ||
        label == "sqp-qn-wsrule") {
        o.column_scaling = hven::solvers::SqpColumnScaling::kAuto;
    }
    // hven M7 W2 A7 (scratch clone only): `sqp-wsrule` / `sqp-qn-wsrule` are `default` /
    // `sqp-qn` plus ONE field, SqpOptions::ws_algebra_rule = kMeasuredCost.
    if (label == "sqp-wsrule" || label == "sqp-qn-wsrule") {
        o.ws_algebra_rule = hven::solvers::SqpWsAlgebraRule::kMeasuredCost;
    }
    if (label == "default" || label == "sqp-wsrule") {
        return o;
    }"""),
    ("""    } else if (label == "sqp-qn") {
        // M7 W2 RETAKE14""", """    } else if (label == "sqp-qn" || label == "sqp-qn-wsrule") {
        // M7 W2 RETAKE14"""),
    ("""    if (lane == "hven-sqp" && label == "sqp-qn") {
        return "none";
    }""", """    if (lane == "hven-sqp" && (label == "sqp-qn" || label == "sqp-qn-wsrule")) {
        return "none";
    }"""),
    ("""    report.factorizations = static_cast<long long>(sqp_result.counters.factorizations);
""", """    report.factorizations = static_cast<long long>(sqp_result.counters.factorizations);
    // hven M7 W2 A7 OBSERVATION HOOK (scratch clone only): whether the rule fired, appended to
    // the file A7_FIRE_LOG names. No row, column or dump reads it.
    if (const char *fire_log = std::getenv("A7_FIRE_LOG"); fire_log != nullptr) {
        if (std::FILE *f = std::fopen(fire_log, "a"); f != nullptr) {
            std::fprintf(f, "%s %s ws_algebra_switches=%lld ws_algebra_switched_qps=%lld""" + NL + """",
                         source.problem().c_str(), options_label.c_str(),
                         static_cast<long long>(sqp_result.counters.ws_algebra_switches),
                         static_cast<long long>(sqp_result.counters.ws_algebra_switched_qps));
            std::fclose(f);
        }
    }
"""),
])
edit('schema/columns.py', [
    ("""    ("hven-sqp", "sqp-qn"): "none",
}""", """    ("hven-sqp", "sqp-qn"): "none",
    ("hven-sqp", "sqp-qn-wsrule"): "none",  # hven M7 W2 A7 scratch clone: sqp-qn + one field
}"""),
])
print("patched")
