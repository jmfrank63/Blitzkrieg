#ifndef BK_PLATFORM_STATE_H
#define BK_PLATFORM_STATE_H

#include "PlatformABI/platform_c.h"

struct BkPlatformState {
    bool initialized;
    uint64_t generation;
    BkPlatformLogFn log;
    void *user_data;
    char last_error[256];
};

BkPlatformState &bk_platform_state();

#endif
