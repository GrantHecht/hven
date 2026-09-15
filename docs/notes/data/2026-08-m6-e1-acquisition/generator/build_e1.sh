#!/usr/bin/env bash
# build_e1.sh — builds the E1 acquisition experiment's two tools in this
# disposable scratch workspace.
#
# PIQP itself is installed by the ARCHIVED bridge's own build_piqp.sh
# (prototypes/piqp_bridge/build_piqp.sh, copied here unmodified): PIQP v0.6.3,
# EXTERNAL, under ~/Software/piqp, never vendored into any repository. Run that
# first (it is idempotent and skips the clone/build when a healthy v0.6.3
# install is already present). This script only compiles this directory's own
# Apache-2.0 sources against it.
#
# The workspace is SELF-CONTAINED: tycho_sqp's public headers, its
# tests/support headers, and tycho's vendored Eigen/fmt are all COPIES taken
# from the read-only archives at setup time. Nothing here is edited in place in
# any repository, and nothing here migrates into one.

set -euo pipefail

W="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIQP_DIR="${PIQP_DIR:-$HOME/Software/piqp}"

if [ ! -f "$PIQP_DIR/include/piqp/piqp.hpp" ]; then
    echo "ERROR: no PIQP install at $PIQP_DIR -- run bridge/build_piqp.sh first." >&2
    exit 1
fi

COMMON=(-O3 -DNDEBUG -std=c++20 -DFMT_HEADER_ONLY
        -I"$W/tycho_sqp_include"
        -isystem "$W/third_party/eigen"
        -isystem "$W/third_party/fmt/include")

echo "== e1_generate =="
clang++ "${COMMON[@]}" "$W/bridge/e1_generate.cpp" -o "$W/bridge/e1_generate"

echo "== piqp_e1_driver =="
clang++ "${COMMON[@]}" -DPIQP_WITH_TEMPLATE_INSTANTIATION \
    -isystem "$PIQP_DIR/include" \
    "$W/bridge/piqp_e1_driver.cpp" -o "$W/bridge/piqp_e1_driver" \
    -L"$PIQP_DIR/lib64" -lpiqp -Wl,-rpath,"$PIQP_DIR/lib64"

echo "== done =="
ldd "$W/bridge/piqp_e1_driver"
