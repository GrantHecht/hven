#pragma once

// e1_dump.h — the E1 acquisition experiment's QP container and its on-disk
// text format.
//
// The format is bench/bench_corpus.cpp's own `write_qp_dump` triplet text
// format (tycho_sqp archive, commit 91c4ec1), EXTENDED — never altered — with
// four `e1_*` metadata tags and two `E1_*` count-led sections that carry the
// cell's taxonomy coordinates and its CONSTRUCTED ground truth:
//
//   e1_kind <constructed|anchor>     how the cell's g/be/bi were produced
//   e1_layout <scattered|contiguous> how the active set is laid out in the row
//                                    index (the follow-on variant's axis)
//   e1_active_offset <long>          contiguous layout: the block's first row;
//                                    -1 when the layout is scattered
//   e1_active_fraction <double>      nominal active fraction (constructed)
//   e1_margin_class <double>         nominal near-activity margin (constructed)
//   e1_seed <unsigned long long>     generator RNG seed
//   E1_ACTIVE_TRUE <k>               k lines, each one 0-based ACTIVE row index
//   E1_ROWSCALE_VEC <mi>             mi lines, the per-row scale the margins
//                                    are expressed in (see e1_generate.cpp)
//
// Every original tag keeps its original meaning, so the archive's own
// piqp_f7_driver would still read one of these dumps correctly (it ignores
// unrecognised tags) — the extension is additive.
//
// A cell's PRIMAL/DUAL ground truth (x*, lambda_e, lambda_i) is NOT in the
// dump; it lives in a sidecar `.sol` file, so the QP a solver is handed never
// carries its own answer. The driver reads the sidecar only to REPORT
// agreement, never to seed a solve (PIQP has no seeding surface anyway).

#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Dense>

namespace e1 {

using Vec = Eigen::VectorXd;

struct Triplet {
    long row, col;
    double value;
};

struct GenericQp {
    long n = 0, me = 0, mi = 0;
    std::vector<Triplet> h; // upper triangle only, row <= col
    Vec g;
    std::vector<Triplet> ae;
    Vec be;
    std::vector<Triplet> ai;
    Vec bi;
    Vec lower, upper;
    std::string cell_id, taxonomy, window;
    long n_nodes = 0;

    // E1 extensions.
    std::string kind = "unknown";
    std::string layout = "scattered"; // "scattered" | "contiguous"
    long active_offset = -1;          // contiguous layout: first active row index
    double active_fraction = -1.0;
    double margin_class = -1.0;
    unsigned long long seed = 0;
    std::vector<long> active_true; // 0-based row indices, ascending
    Vec row_scale;                 // mi
};

// The ground-truth sidecar (constructed cells only).
struct GroundTruth {
    Vec x_star, lambda_e, lambda_i;
};

inline GenericQp read_dump(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("e1: could not read dump '" + path + "'");
    }
    GenericQp out;
    std::string line;
    bool ended = false;
    auto read_vec = [&](long count) {
        Vec v(count);
        for (long i = 0; i < count; ++i) {
            std::getline(in, line);
            v(i) = std::stod(line);
        }
        return v;
    };
    auto read_triplets = [&](long count) {
        std::vector<Triplet> t;
        t.reserve(static_cast<std::size_t>(count));
        for (long i = 0; i < count; ++i) {
            std::getline(in, line);
            Triplet tr{};
            const char *p = line.c_str();
            char *end = nullptr;
            tr.row = std::strtol(p, &end, 10);
            tr.col = std::strtol(end, &end, 10);
            tr.value = std::strtod(end, &end);
            t.push_back(tr);
        }
        return t;
    };
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line == "PIQP_QP_DUMP 1") {
            continue;
        }
        std::stringstream ss(line);
        std::string tag;
        ss >> tag;
        long count = 0;
        if (tag == "cell") {
            ss >> out.cell_id;
        } else if (tag == "taxonomy") {
            ss >> out.taxonomy;
        } else if (tag == "window") {
            ss >> out.window;
        } else if (tag == "n_nodes") {
            ss >> out.n_nodes;
        } else if (tag == "n") {
            ss >> out.n;
        } else if (tag == "me") {
            ss >> out.me;
        } else if (tag == "mi") {
            ss >> out.mi;
        } else if (tag == "e1_kind") {
            ss >> out.kind;
        } else if (tag == "e1_layout") {
            ss >> out.layout;
        } else if (tag == "e1_active_offset") {
            ss >> out.active_offset;
        } else if (tag == "e1_active_fraction") {
            ss >> out.active_fraction;
        } else if (tag == "e1_margin_class") {
            ss >> out.margin_class;
        } else if (tag == "e1_seed") {
            ss >> out.seed;
        } else if (tag == "E1_ACTIVE_TRUE") {
            ss >> count;
            out.active_true.clear();
            out.active_true.reserve(static_cast<std::size_t>(count));
            for (long i = 0; i < count; ++i) {
                std::getline(in, line);
                out.active_true.push_back(std::strtol(line.c_str(), nullptr, 10));
            }
        } else if (tag == "E1_ROWSCALE_VEC") {
            ss >> count;
            out.row_scale = read_vec(count);
        } else if (tag == "H_NNZ") {
            ss >> count;
            out.h = read_triplets(count);
        } else if (tag == "G_VEC") {
            ss >> count;
            out.g = read_vec(count);
        } else if (tag == "AE_NNZ") {
            ss >> count;
            out.ae = read_triplets(count);
        } else if (tag == "BE_VEC") {
            ss >> count;
            out.be = read_vec(count);
        } else if (tag == "AI_NNZ") {
            ss >> count;
            out.ai = read_triplets(count);
        } else if (tag == "BI_VEC") {
            ss >> count;
            out.bi = read_vec(count);
        } else if (tag == "LOWER_VEC") {
            ss >> count;
            out.lower = read_vec(count);
        } else if (tag == "UPPER_VEC") {
            ss >> count;
            out.upper = read_vec(count);
        } else if (tag == "END") {
            ended = true;
        }
    }
    if (!ended) {
        throw std::runtime_error("e1: dump '" + path + "' has no END marker -- truncated write?");
    }
    return out;
}

inline void write_dump(const std::string &path, const GenericQp &q,
                       const std::string &provenance_comment) {
    std::FILE *f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        throw std::runtime_error("e1: could not open '" + path + "' for writing");
    }
    std::fprintf(f, "PIQP_QP_DUMP 1\n");
    std::fprintf(f, "# %s\n", provenance_comment.c_str());
    std::fprintf(f, "cell %s\n", q.cell_id.c_str());
    std::fprintf(f, "taxonomy %s\n", q.taxonomy.c_str());
    std::fprintf(f, "window %s\n", q.window.c_str());
    std::fprintf(f, "n_nodes %ld\n", q.n_nodes);
    std::fprintf(f, "n %ld\n", q.n);
    std::fprintf(f, "me %ld\n", q.me);
    std::fprintf(f, "mi %ld\n", q.mi);
    std::fprintf(f, "e1_kind %s\n", q.kind.c_str());
    std::fprintf(f, "e1_layout %s\n", q.layout.c_str());
    std::fprintf(f, "e1_active_offset %ld\n", q.active_offset);
    std::fprintf(f, "e1_active_fraction %.17g\n", q.active_fraction);
    std::fprintf(f, "e1_margin_class %.17g\n", q.margin_class);
    std::fprintf(f, "e1_seed %llu\n", q.seed);
    std::fprintf(f, "E1_ACTIVE_TRUE %zu\n", q.active_true.size());
    for (const long j : q.active_true) {
        std::fprintf(f, "%ld\n", j);
    }
    std::fprintf(f, "E1_ROWSCALE_VEC %ld\n", static_cast<long>(q.row_scale.size()));
    for (long i = 0; i < q.row_scale.size(); ++i) {
        std::fprintf(f, "%.17g\n", q.row_scale(i));
    }
    auto dump_triplets = [&](const char *tag, const std::vector<Triplet> &t) {
        std::fprintf(f, "%s %zu\n", tag, t.size());
        for (const Triplet &tr : t) {
            std::fprintf(f, "%ld %ld %.17g\n", tr.row, tr.col, tr.value);
        }
    };
    auto dump_vec = [&](const char *tag, const Vec &v) {
        std::fprintf(f, "%s %ld\n", tag, static_cast<long>(v.size()));
        for (long i = 0; i < v.size(); ++i) {
            std::fprintf(f, "%.17g\n", v(i));
        }
    };
    dump_triplets("H_NNZ", q.h);
    dump_vec("G_VEC", q.g);
    dump_triplets("AE_NNZ", q.ae);
    dump_vec("BE_VEC", q.be);
    dump_triplets("AI_NNZ", q.ai);
    dump_vec("BI_VEC", q.bi);
    dump_vec("LOWER_VEC", q.lower);
    dump_vec("UPPER_VEC", q.upper);
    std::fprintf(f, "END\n");
    if (std::fclose(f) != 0) {
        throw std::runtime_error("e1: failed to close '" + path + "' cleanly");
    }
}

inline void write_solution(const std::string &path, const GroundTruth &gt) {
    std::FILE *f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        throw std::runtime_error("e1: could not open '" + path + "' for writing");
    }
    auto dump_vec = [&](const char *tag, const Vec &v) {
        std::fprintf(f, "%s %ld\n", tag, static_cast<long>(v.size()));
        for (long i = 0; i < v.size(); ++i) {
            std::fprintf(f, "%.17g\n", v(i));
        }
    };
    std::fprintf(f, "E1_SOL 1\n");
    dump_vec("XSTAR_VEC", gt.x_star);
    dump_vec("LAMBDA_E_VEC", gt.lambda_e);
    dump_vec("LAMBDA_I_VEC", gt.lambda_i);
    std::fprintf(f, "END\n");
    if (std::fclose(f) != 0) {
        throw std::runtime_error("e1: failed to close '" + path + "' cleanly");
    }
}

inline GroundTruth read_solution(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("e1: could not read solution sidecar '" + path + "'");
    }
    GroundTruth gt;
    std::string line;
    bool ended = false;
    auto read_vec = [&](long count) {
        Vec v(count);
        for (long i = 0; i < count; ++i) {
            std::getline(in, line);
            v(i) = std::stod(line);
        }
        return v;
    };
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || line == "E1_SOL 1") {
            continue;
        }
        std::stringstream ss(line);
        std::string tag;
        long count = 0;
        ss >> tag;
        if (tag == "XSTAR_VEC") {
            ss >> count;
            gt.x_star = read_vec(count);
        } else if (tag == "LAMBDA_E_VEC") {
            ss >> count;
            gt.lambda_e = read_vec(count);
        } else if (tag == "LAMBDA_I_VEC") {
            ss >> count;
            gt.lambda_i = read_vec(count);
        } else if (tag == "END") {
            ended = true;
        }
    }
    if (!ended) {
        throw std::runtime_error("e1: solution sidecar '" + path + "' has no END marker");
    }
    return gt;
}

// ---- small shared linear-algebra helpers over the triplet containers ----

// y += H_sym * x, where `h` holds only the upper triangle.
inline void add_h_times(const std::vector<Triplet> &h, const Vec &x, Vec &y) {
    for (const Triplet &tr : h) {
        y(tr.row) += tr.value * x(tr.col);
        if (tr.row != tr.col) {
            y(tr.col) += tr.value * x(tr.row);
        }
    }
}

// returns A * x for a triplet-stored A with `rows` rows.
inline Vec a_times(const std::vector<Triplet> &a, const Vec &x, long rows) {
    Vec y = Vec::Zero(rows);
    for (const Triplet &tr : a) {
        y(tr.row) += tr.value * x(tr.col);
    }
    return y;
}

// y += A^T * v.
inline void add_at_times(const std::vector<Triplet> &a, const Vec &v, Vec &y) {
    for (const Triplet &tr : a) {
        y(tr.col) += tr.value * v(tr.row);
    }
}

} // namespace e1
