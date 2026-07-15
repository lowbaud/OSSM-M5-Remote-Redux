#include "BuildInfo.h"

#include <cstdio>
#include <cstring>

namespace m5_redux {

namespace {

constexpr BuildInfo kBuildInfo{
    APP_VERSION,
    APP_GIT_COMMIT,
    APP_GIT_SHORT_COMMIT,
    APP_BUILD_DIRTY != 0,
    APP_BUILD_METADATA_AVAILABLE != 0,
    APP_BUILD_RELEASE != 0,
};

}  // namespace

const BuildInfo& buildInfo() {
    return kBuildInfo;
}

void formatBuildVersion(char* output, std::size_t outputSize) {
    const BuildInfo& info = buildInfo();

    if (info.release) {
        std::snprintf(output, outputSize, "v%s", info.version);
        return;
    }

    const char* separator = std::strchr(info.version, '+') ? "." : "+";

    if (!info.metadataAvailable) {
        std::snprintf(output, outputSize, "v%s%sunknown", info.version, separator);
    } else if (info.dirty) {
        std::snprintf(
            output, outputSize, "v%s%s%s.dirty", info.version, separator, info.shortCommit);
    } else {
        std::snprintf(output, outputSize, "v%s%s%s", info.version, separator, info.shortCommit);
    }
}

}  // namespace m5_redux
