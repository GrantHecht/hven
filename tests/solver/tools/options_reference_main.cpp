// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// hven_options_reference: writes the generated options reference (docs/options-reference.md) to the
// file named by argv[1], or to stdout without one. Regenerate the checked-in file with
// `cmake --build <dir> --target options-reference`.

#include <fstream>
#include <iostream>

#include <hven/solver/options_reference.h>

int main(int argc, char **argv) {
    if (argc > 2) {
        std::cerr << "usage: hven_options_reference [output-file]\n";
        return 2;
    }
    if (argc == 2) {
        std::ofstream out(argv[1], std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "hven_options_reference: cannot open " << argv[1] << " for writing\n";
            return 1;
        }
        hven::write_options_reference(out);
        out.flush();
        if (!out) {
            std::cerr << "hven_options_reference: write to " << argv[1] << " failed\n";
            return 1;
        }
        return 0;
    }
    hven::write_options_reference(std::cout);
    return 0;
}
