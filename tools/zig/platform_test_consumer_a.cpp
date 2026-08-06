#include "PlatformABI/PlatformClient.h"

extern "C" BK_PLATFORM_EXPORT uint64_t bk_platform_consumer_a_generation() {
    return BkPlatform::Client::Generation();
}

extern "C" BK_PLATFORM_EXPORT uint32_t bk_platform_consumer_a_attach() {
    return BkPlatform::Client::Attach() ? BK_PLATFORM_OK : BK_PLATFORM_ERROR_UNSUPPORTED_VERSION;
}
