#pragma once

#include <string>

namespace workbench {

// Semantic version of the core library. Phase-0 smoke target: proves that the core library,
// the test framework and CTest are wired together. Real modules arrive from Phase 2 onward.
std::string core_version();

}  // namespace workbench
