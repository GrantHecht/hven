#pragma once

namespace hven {

// Returns the hven release version as a null-terminated string (e.g.
// "0.1.0"). The returned pointer has static storage duration and is valid
// for the lifetime of the program.
const char *version() noexcept;

} // namespace hven
