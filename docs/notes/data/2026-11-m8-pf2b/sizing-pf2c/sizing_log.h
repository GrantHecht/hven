// SCRATCH ONLY (PF-2c sizing): env-gated event log; never committed to the library.
#pragma once
#include <cstdio>
#include <cstdlib>
namespace hven::detail::sizing {
inline std::FILE *fp() {
    static std::FILE *f = [] {
        const char *p = std::getenv("HVEN_SIZING_LOG");
        if (p == nullptr) {
            return static_cast<std::FILE *>(nullptr);
        }
        std::FILE *g = std::fopen(p, "w");
        if (g != nullptr) {
            std::atexit([] { std::fflush(fp()); });
        }
        return g;
    }();
    return f;
}
} // namespace hven::detail::sizing
