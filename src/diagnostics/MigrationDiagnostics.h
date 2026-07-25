#pragma once

#include <cstdint>

namespace m5_redux {
namespace migration_diagnostics {

// Temporary migration-only instrumentation. Removing MIGRATION_DIAGNOSTICS from the build
// compiles every call below to a no-op.
#if defined(MIGRATION_DIAGNOSTICS) && MIGRATION_DIAGNOSTICS

void printSnapshot(const char* checkpoint);
void printRuntimeSnapshotOnce(std::uint32_t nowMs);

#else

inline void printSnapshot(const char*) {}
inline void printRuntimeSnapshotOnce(std::uint32_t) {}

#endif

}  // namespace migration_diagnostics
}  // namespace m5_redux
