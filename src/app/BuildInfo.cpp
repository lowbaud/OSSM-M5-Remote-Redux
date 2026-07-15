#include "BuildInfo.h"

namespace m5_redux {

namespace {

constexpr BuildInfo kBuildInfo{
    APP_VERSION,
    APP_BUILD_VERSION,
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

}  // namespace m5_redux
