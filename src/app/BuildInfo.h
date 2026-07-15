#pragma once

#include <cstddef>

namespace m5_redux {

constexpr std::size_t kBuildVersionTextSize = 96;

struct BuildInfo {
    const char* version;
    const char* commit;
    const char* shortCommit;
    bool dirty;
    bool metadataAvailable;
    bool release;
};

const BuildInfo& buildInfo();
void formatBuildVersion(char* output, std::size_t outputSize);

}  // namespace m5_redux
