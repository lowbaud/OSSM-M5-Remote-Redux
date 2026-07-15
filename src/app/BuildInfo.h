#pragma once

namespace m5_redux {

struct BuildInfo {
    const char* version;
    const char* buildVersion;
    const char* commit;
    const char* shortCommit;
    bool dirty;
    bool metadataAvailable;
    bool release;
};

const BuildInfo& buildInfo();

}  // namespace m5_redux
