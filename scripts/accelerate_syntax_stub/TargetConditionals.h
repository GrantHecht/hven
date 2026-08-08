// Minimal STUB, not part of hven's own Accelerate surface -- see the sibling
// AvailabilityMacros.h stub's comment for why this exists at all: googletest's
// gtest-port-arch.h reads TARGET_OS_IPHONE (from this real macOS SDK header)
// to distinguish macOS from iOS whenever `__APPLE__` is defined. hven's own
// code never targets iOS and reads nothing from this header; defining
// TARGET_OS_IPHONE to 0 selects gtest's "macOS, not iOS" branch, which is the
// only one relevant to this repository's Accelerate backend (macOS-only).
#pragma once

#define TARGET_OS_IPHONE 0
