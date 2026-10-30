// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.
//
// M7 W0 T5(a): the transcription entry became a public header of its own, and
// this TU is what proves it stands alone -- it declares two functions over
// three forward-declared types and must therefore need nothing but <memory>.

#include "hven/model/make_nlp_program.h"

namespace hven_install_smoke {
int standalone_include_make_nlp_program() { return 1; }
} // namespace hven_install_smoke
