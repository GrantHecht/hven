#!/usr/bin/env bash
# M8 DN-0 IPM r1 provenance stamp (docs/measurement.md section 7): toolchain, hardware, date, commits,
# binaries, and effective compile flags read from the builds' own compile_commands.json / build.ninja.
# Usage: provenance.sh HARNESS_ROOT HVEN_WORKTREE CORE
set -uo pipefail
HB="$1"; HV="$2"; CORE="${3:-2}"
echo "# M8 DN-0 IPM r1 provenance (collected $(date -u +%FT%TZ))"
echo "## commits"
echo "hven worktree HEAD: $(git -C "${HV}" rev-parse HEAD) ($(git -C "${HV}" branch --show-current))"
echo "hven pin (measured library): $(sed -n 's/^HVEN_PIN_SHA //p' "${HB}/.hven/install-0a34c8a2adba/HVEN_PIN.txt")"
echo "harness HEAD: $(git -C "${HB}" rev-parse HEAD) ($(git -C "${HB}" branch --show-current)); dirty: $(git -C "${HB}" status --porcelain --untracked-files=no | wc -l) tracked changes"
echo "## pin stamp"; sed 's/^/  /' "${HB}/.hven/install-0a34c8a2adba/HVEN_PIN.txt"
echo "## toolchain"
echo "clang: $(clang++ --version | head -1)"
echo "cmake: $(cmake --version | head -1)"
echo "ninja: $(ninja --version 2>/dev/null || ninja-build --version)"
echo "perf: $(perf --version)"
echo "python: $(python3 --version)"
echo "gfortran: $(gfortran --version | head -1)"
mkl_root="$(sed -n 's/^MKL_ROOT:PATH=//p;s/^MKL_INCLUDE_DIR:PATH=//p' "${HB}/.hven/build-0a34c8a2adba/CMakeCache.txt" | head -2 | tr '\n' ' ')"
echo "MKL (from the pin build's CMakeCache): ${mkl_root}"
grep -h "define INTEL_MKL_VERSION\|define __INTEL_MKL_BUILD_DATE" /opt/intel/oneapi/mkl/latest/include/mkl_version.h 2>/dev/null | sed 's/^/  /'
echo "libiomp5: $(readlink -f /opt/intel/oneapi/compiler/latest/lib/libiomp5.so)"
echo "CUTEst: $(readlink -f "${HOME}/Software/cutest/lib/libcutest_double.so")"
echo "## hardware / OS"
echo "host: $(hostname)"
echo "kernel: $(uname -r)"
lscpu | grep -E "Model name|^CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|Socket\(s\)|L3 cache" | sed 's/^/  /'
echo "measurement core: ${CORE}; its SMT siblings: $(cat /sys/devices/system/cpu/cpu${CORE}/topology/thread_siblings_list)"
echo "governor(cpu${CORE}): $(cat /sys/devices/system/cpu/cpu${CORE}/cpufreq/scaling_governor 2>/dev/null)"
echo "boost: $(cat /sys/devices/system/cpu/cpufreq/boost 2>/dev/null || echo n/a)"
echo "memory: $(grep MemTotal /proc/meminfo)"
echo "perf_event_paranoid: $(cat /proc/sys/kernel/perf_event_paranoid)"
echo "perf_event_max_sample_rate: $(cat /proc/sys/kernel/perf_event_max_sample_rate) (kernel-lowered this boot: $(journalctl -k -b 2>/dev/null | grep -c 'lowering kernel.perf_event_max_sample_rate') event(s))"
journalctl -k -b 2>/dev/null | grep 'lowering kernel.perf_event_max_sample_rate' | sed 's/^/  /'
echo "perf_event_mlock_kb: $(cat /proc/sys/kernel/perf_event_mlock_kb); RLIMIT_MEMLOCK: $(ulimit -l) KB"
echo "perf reader core: 4; its SMT siblings: $(cat /sys/devices/system/cpu/cpu4/topology/thread_siblings_list)"
echo "## binaries (sha256)"
(cd "${HB}" && sha256sum build/lanes/hven/hven_bench_cell .scratch/dn0r1/build-cfi/hven_bench_cell \
    .scratch/dn0r1/build-dump/lanes/hven/hven_bench_cell \
    .scratch/dn0r1/build-tools/dn0_replay .scratch/dn0r1/build-tools/dn0_replay_verify \
    .hven/install-0a34c8a2adba/lib64/libhven.a .scratch/dn0r1/hven-cfi/install/lib64/libhven.a) | sed 's/^/  /'
echo "## effective compile flags"
echo "libhven.a (pin build build.ninja, symmetric_factor_mkl.cpp.o):"
awk '/symmetric_factor_mkl.cpp.o:/ {f=1} f && /FLAGS =/ {print "  " $0; f=0}' "${HB}/.hven/build-0a34c8a2adba/build.ninja" | head -1
awk '/symmetric_factor_mkl.cpp.o:/ {f=1} f && /DEFINES =/ {print "  " $0; f=0}' "${HB}/.hven/build-0a34c8a2adba/build.ninja" | head -1
echo "measured lane binary (build/build.ninja FLAGS for hven_bench_cell.cpp):"
awk '/hven_bench_cell.cpp.o:/ {f=1} f && /FLAGS =/ {print "  " $0; f=0}' "${HB}/build/build.ninja" | head -1
awk '/hven_bench_cell.cpp.o:/ {f=1} f && /DEFINES =/ {print "  " $0; f=0}' "${HB}/build/build.ninja" | head -1
echo "CFI libhven.a (hven-cfi build.ninja, symmetric_factor_mkl.cpp.o; the launcher appends -fasynchronous-unwind-tables):"
awk '/symmetric_factor_mkl.cpp.o:/ {f=1} f && /FLAGS =/ {print "  " $0; f=0}' "${HB}/.scratch/dn0r1/hven-cfi/build/build.ninja" | head -1
echo "  launcher: $(sed -n 's/^CCACHE_PROGRAM:[A-Z]*=//p' "${HB}/.scratch/dn0r1/hven-cfi/build/CMakeCache.txt")"
echo "RECORD binary: the measured link command with libhven.a swapped (.scratch/dn0r1/build-cfi/link.diff):"
sed 's/^/  /' "${HB}/.scratch/dn0r1/build-cfi/link.diff"
echo "dump build: link closure objects byte-identical to the measured build's (identity/dump-vs-measured-objects.txt), link adds -Wl,--wrap=pardiso + pardiso_wrap.c.o"
echo "replay (build-tools compile_commands.json):"
python3 - "${HB}/.scratch/dn0r1/build-tools/compile_commands.json" <<'EOF'
import json, sys
cc = json.load(open(sys.argv[1]))
for e in cc:
    print("  " + e["file"].split("/")[-1] + ": " + e.get("command", ""))
EOF
