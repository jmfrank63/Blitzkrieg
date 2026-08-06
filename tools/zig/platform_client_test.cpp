#include "PlatformABI/PlatformClient.h"

#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
using LibraryHandle = HMODULE;
static LibraryHandle open_library(const char *path) { return LoadLibraryA(path); }
static void *lookup(LibraryHandle handle, const char *name) { return reinterpret_cast<void *>(GetProcAddress(handle, name)); }
static void close_library(LibraryHandle handle) { FreeLibrary(handle); }
#else
#include <dlfcn.h>
using LibraryHandle = void *;
static LibraryHandle open_library(const char *path) { return dlopen(path, RTLD_NOW); }
static void *lookup(LibraryHandle handle, const char *name) { return dlsym(handle, name); }
static void close_library(LibraryHandle handle) { dlclose(handle); }
#endif

using AttachFn = uint32_t (*)();
using GenerationFn = uint64_t (*)();

int main(int argc, char **argv) {
    if (argc != 3) return 10;
    LibraryHandle consumer_a = open_library(argv[1]);
    LibraryHandle consumer_b = open_library(argv[2]);
    if (consumer_a == nullptr || consumer_b == nullptr) return 11;
    auto attach_a = reinterpret_cast<AttachFn>(lookup(consumer_a, "bk_platform_consumer_a_attach"));
    auto attach_b = reinterpret_cast<AttachFn>(lookup(consumer_b, "bk_platform_consumer_b_attach"));
    auto generation_a = reinterpret_cast<GenerationFn>(lookup(consumer_a, "bk_platform_consumer_a_generation"));
    auto generation_b = reinterpret_cast<GenerationFn>(lookup(consumer_b, "bk_platform_consumer_b_generation"));
    if (attach_a == nullptr || attach_b == nullptr || generation_a == nullptr || generation_b == nullptr) return 12;
    if (attach_a() != BK_PLATFORM_OK || attach_b() != BK_PLATFORM_OK) return 13;
    if (!BkPlatform::Client::Attach()) return 14;
    BkPlatformCreateInfo info = {};
    info.struct_size = sizeof(info);
    info.requested_abi_version = BK_PLATFORM_ABI_VERSION;
    if (BkPlatform::Client::Create(info) != BK_PLATFORM_OK) return 15;
    const uint64_t generation = BkPlatform::Client::Generation();
    if (generation == 0 || generation_a() != generation || generation_b() != generation) return 16;
    const uint64_t clock_before = BkPlatform::Client::MonotonicNanoseconds();
    BkPlatform::Client::SleepMilliseconds(1);
    if (BkPlatform::Client::MonotonicNanoseconds() < clock_before) return 17;
    uint32_t atomic = 10;
    if (BkPlatform::Client::AtomicExchangeU32(&atomic, 20) != 10 || atomic != 20) return 18;
    if (BkPlatform::Client::AtomicIncrementU32(&atomic) != 21) return 19;
    if (BkPlatform::Client::AtomicDecrementU32(&atomic) != 20) return 20;
    if (BkPlatform::Client::AtomicCompareExchangeU32(&atomic, 20, 42) != 20 || atomic != 42) return 21;
    BkPlatform::Client::Destroy();
    close_library(consumer_b);
    close_library(consumer_a);
    std::printf("platform client runtime generation: %llu\n", static_cast<unsigned long long>(generation));
    return 0;
}
