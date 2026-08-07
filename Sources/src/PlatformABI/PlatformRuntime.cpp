#include "PlatformABI/PlatformState.h"
#include "Platform/Clock.h"
#include "Platform/Socket.h"

#include <cstdio>
#include <cstring>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void);
#else
#include <dlfcn.h>
#endif

namespace {
struct SocketEntry {
    NPlatform::SocketHandle native;
    uint32_t generation;
};

std::mutex socket_mutex;
std::unordered_map<uint32_t, SocketEntry> socket_entries;
std::unordered_map<uint32_t, uint32_t> socket_generations;
std::vector<uint32_t> free_socket_slots;
uint32_t next_socket_slot = 1;
uint32_t socket_runtime_refs = 0;
bool socket_runtime_ready = false;

void shutdown_sync_objects();
void shutdown_libraries();

struct LibraryEntry {
    void *native = nullptr;
    uint32_t generation = 0;
};

constexpr uint64_t library_type_shift = 62;
constexpr uint64_t library_type = UINT64_C(3);
constexpr uint64_t library_generation_mask = UINT64_C(0x3fffffff);
std::mutex library_mutex;
std::vector<LibraryEntry> library_slots(1);
std::vector<uint32_t> free_library_slots;

BkPlatformHandle make_library_handle(uint32_t slot, uint32_t generation) {
    return (library_type << library_type_shift) |
           (static_cast<BkPlatformHandle>(generation) << 32) | slot;
}

bool decode_library_handle(BkPlatformHandle handle, uint32_t *slot, uint32_t *generation) {
    if (handle == 0 || slot == nullptr || generation == nullptr || (handle >> library_type_shift) != library_type) return false;
    *slot = static_cast<uint32_t>(handle & UINT32_MAX);
    *generation = static_cast<uint32_t>((handle >> 32) & library_generation_mask);
    return *slot != 0 && *generation != 0;
}

void close_native_library(void *native) {
    if (native == nullptr) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(native));
#else
    dlclose(native);
#endif
}

void *open_native_library(const std::string &path) {
#if defined(_WIN32)
    const int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), nullptr, 0);
    if (wide_length <= 0) return nullptr;
    std::wstring wide_path(static_cast<size_t>(wide_length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), wide_path.data(), wide_length) != wide_length) return nullptr;
    return reinterpret_cast<void *>(LoadLibraryW(wide_path.c_str()));
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void *lookup_native_symbol(void *native, const std::string &name) {
#if defined(_WIN32)
    return native == nullptr ? nullptr : reinterpret_cast<void *>(GetProcAddress(reinterpret_cast<HMODULE>(native), name.c_str()));
#else
    return native == nullptr ? nullptr : dlsym(native, name.c_str());
#endif
}

BkPlatformSocketHandle make_socket_handle(uint32_t slot, uint32_t generation) {
    return (static_cast<BkPlatformSocketHandle>(generation) << 32) | slot;
}

bool split_socket_handle(BkPlatformSocketHandle handle, uint32_t *slot, uint32_t *generation) {
    if (handle == 0 || slot == nullptr || generation == nullptr) return false;
    *slot = static_cast<uint32_t>(handle & 0xffffffffu);
    *generation = static_cast<uint32_t>(handle >> 32);
    return *slot != 0 && *generation != 0;
}

bool socket_native(BkPlatformSocketHandle handle, NPlatform::SocketHandle *out_native) {
    uint32_t slot = 0;
    uint32_t generation = 0;
    if (!split_socket_handle(handle, &slot, &generation) || out_native == nullptr) return false;
    std::lock_guard<std::mutex> lock(socket_mutex);
    const auto it = socket_entries.find(slot);
    if (it == socket_entries.end() || it->second.generation != generation) return false;
    *out_native = it->second.native;
    return true;
}

BkPlatformSocketHandle register_socket(NPlatform::SocketHandle native) {
    if (native == NPlatform::InvalidSocket) return 0;
    std::lock_guard<std::mutex> lock(socket_mutex);
    uint32_t slot = 0;
    if (!free_socket_slots.empty()) {
        slot = free_socket_slots.back();
        free_socket_slots.pop_back();
    } else {
        slot = next_socket_slot++;
        if (slot == 0) slot = next_socket_slot++;
    }
    const auto generation_it = socket_generations.find(slot);
    const uint32_t generation = generation_it == socket_generations.end() || generation_it->second == UINT32_MAX ? 1u : generation_it->second + 1u;
    socket_generations[slot] = generation;
    socket_entries[slot] = SocketEntry{native, generation};
    return make_socket_handle(slot, generation);
}

bool unregister_socket(BkPlatformSocketHandle handle, NPlatform::SocketHandle *out_native) {
    uint32_t slot = 0;
    uint32_t generation = 0;
    if (!split_socket_handle(handle, &slot, &generation) || out_native == nullptr) return false;
    std::lock_guard<std::mutex> lock(socket_mutex);
    const auto it = socket_entries.find(slot);
    if (it == socket_entries.end() || it->second.generation != generation) return false;
    *out_native = it->second.native;
    socket_entries.erase(it);
    free_socket_slots.push_back(slot);
    return true;
}

void shutdown_sockets() {
    std::vector<NPlatform::SocketHandle> native_handles;
    bool had_runtime = false;
    {
        std::lock_guard<std::mutex> lock(socket_mutex);
        for (const auto &entry : socket_entries) {
            native_handles.push_back(entry.second.native);
            free_socket_slots.push_back(entry.first);
        }
        socket_entries.clear();
        socket_runtime_refs = 0;
        had_runtime = socket_runtime_ready;
        socket_runtime_ready = false;
    }
    for (const NPlatform::SocketHandle native : native_handles) NPlatform::Close(native);
    if (had_runtime) NPlatform::SocketRuntimeDone();
}

BkPlatformResult require_socket_runtime() {
    std::lock_guard<std::mutex> lock(socket_mutex);
    return socket_runtime_ready ? BK_PLATFORM_OK : BK_PLATFORM_ERROR_NOT_INITIALIZED;
}

NPlatform::SocketAddress to_native_address(const BkPlatformSocketAddress &address) {
    NPlatform::SocketAddress native{};
    native.family = address.family;
    std::memcpy(native.data, address.data, sizeof(native.data));
    return native;
}

void from_native_address(const NPlatform::SocketAddress &native, BkPlatformSocketAddress *address) {
    if (address == nullptr) return;
    address->family = native.family;
    std::memcpy(address->data, native.data, sizeof(address->data));
}

BkPlatformResult socket_runtime_init() {
    std::lock_guard<std::mutex> lock(socket_mutex);
    if (socket_runtime_refs == 0 && !NPlatform::SocketRuntimeInit()) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    socket_runtime_ready = true;
    ++socket_runtime_refs;
    return BK_PLATFORM_OK;
}

BkPlatformResult BK_PLATFORM_CALL socket_runtime_done() {
    bool shutdown = false;
    {
        std::lock_guard<std::mutex> lock(socket_mutex);
        if (socket_runtime_refs == 0) return BK_PLATFORM_ERROR_NOT_INITIALIZED;
        --socket_runtime_refs;
        shutdown = socket_runtime_refs == 0;
    }
    if (shutdown) shutdown_sockets();
    return BK_PLATFORM_OK;
}

BkPlatformResult BK_PLATFORM_CALL socket_runtime_init_call() { return socket_runtime_init(); }

BkPlatformResult open_socket(bool tcp, BkPlatformSocketHandle *out_handle) {
    if (out_handle == nullptr || require_socket_runtime() != BK_PLATFORM_OK) return out_handle == nullptr ? BK_PLATFORM_ERROR_INVALID_ARGUMENT : BK_PLATFORM_ERROR_NOT_INITIALIZED;
    const NPlatform::SocketHandle native = tcp ? NPlatform::OpenTcpSocket() : NPlatform::OpenUdpSocket();
    const BkPlatformSocketHandle handle = register_socket(native);
    if (handle == 0) {
        NPlatform::Close(native);
        return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    *out_handle = handle;
    return BK_PLATFORM_OK;
}

BkPlatformResult BK_PLATFORM_CALL socket_open_tcp(BkPlatformSocketHandle *out_handle) { return open_socket(true, out_handle); }
BkPlatformResult BK_PLATFORM_CALL socket_open_udp(BkPlatformSocketHandle *out_handle) { return open_socket(false, out_handle); }

BkPlatformResult BK_PLATFORM_CALL socket_bind(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, uint16_t port) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    if (require_socket_runtime() != BK_PLATFORM_OK || !socket_native(handle, &native_socket)) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    NPlatform::SocketAddress native_address{};
    NPlatform::SocketAddress *native_address_ptr = nullptr;
    if (address != nullptr) { native_address = to_native_address(*address); native_address_ptr = &native_address; }
    if (!NPlatform::Bind(native_socket, native_address_ptr, port)) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    if (address != nullptr) from_native_address(native_address, address);
    return BK_PLATFORM_OK;
}

BkPlatformResult BK_PLATFORM_CALL socket_listen(BkPlatformSocketHandle handle, int32_t backlog) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    return require_socket_runtime() == BK_PLATFORM_OK && socket_native(handle, &native_socket) && NPlatform::Listen(native_socket, backlog) ? BK_PLATFORM_OK : BK_PLATFORM_ERROR_INVALID_ARGUMENT;
}

BkPlatformResult BK_PLATFORM_CALL socket_connect(BkPlatformSocketHandle handle, const BkPlatformSocketAddress *address) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    if (address == nullptr || require_socket_runtime() != BK_PLATFORM_OK || !socket_native(handle, &native_socket)) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    return NPlatform::Connect(native_socket, to_native_address(*address)) ? BK_PLATFORM_OK : BK_PLATFORM_ERROR_INVALID_ARGUMENT;
}

BkPlatformResult BK_PLATFORM_CALL socket_accept(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, BkPlatformSocketHandle *out_handle) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    if (out_handle == nullptr || require_socket_runtime() != BK_PLATFORM_OK || !socket_native(handle, &native_socket)) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    NPlatform::SocketAddress native_address{};
    const NPlatform::SocketHandle accepted = NPlatform::Accept(native_socket, address != nullptr ? &native_address : nullptr);
    const BkPlatformSocketHandle portable = register_socket(accepted);
    if (portable == 0) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    if (address != nullptr) from_native_address(native_address, address);
    *out_handle = portable;
    return BK_PLATFORM_OK;
}

int32_t socket_io(BkPlatformSocketHandle handle, void *data, int32_t size, bool receive) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    if (require_socket_runtime() != BK_PLATFORM_OK || data == nullptr || size < 0 || !socket_native(handle, &native_socket)) return -1;
    return receive ? NPlatform::Receive(native_socket, data, size) : NPlatform::Send(native_socket, data, size);
}

int32_t BK_PLATFORM_CALL socket_send(BkPlatformSocketHandle handle, const void *data, int32_t size) { return socket_io(handle, const_cast<void *>(data), size, false); }
int32_t BK_PLATFORM_CALL socket_receive(BkPlatformSocketHandle handle, void *data, int32_t size) { return socket_io(handle, data, size, true); }

int32_t BK_PLATFORM_CALL socket_send_to(BkPlatformSocketHandle handle, const BkPlatformSocketAddress *address, const void *data, int32_t size) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    if (address == nullptr || data == nullptr || size < 0 || require_socket_runtime() != BK_PLATFORM_OK || !socket_native(handle, &native_socket)) return -1;
    return NPlatform::SendTo(native_socket, to_native_address(*address), data, size);
}

int32_t BK_PLATFORM_CALL socket_receive_from(BkPlatformSocketHandle handle, BkPlatformSocketAddress *address, void *data, int32_t size) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    if (data == nullptr || size < 0 || require_socket_runtime() != BK_PLATFORM_OK || !socket_native(handle, &native_socket)) return -1;
    NPlatform::SocketAddress native_address{};
    const int32_t result = NPlatform::ReceiveFrom(native_socket, address != nullptr ? &native_address : nullptr, data, size);
    if (result >= 0 && address != nullptr) from_native_address(native_address, address);
    return result;
}

BkPlatformResult BK_PLATFORM_CALL socket_set_nonblocking(BkPlatformSocketHandle handle, uint32_t enabled) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    return require_socket_runtime() == BK_PLATFORM_OK && socket_native(handle, &native_socket) && NPlatform::SetNonBlocking(native_socket, enabled != 0) ? BK_PLATFORM_OK : BK_PLATFORM_ERROR_INVALID_ARGUMENT;
}

BkPlatformResult BK_PLATFORM_CALL socket_wait_readable(BkPlatformSocketHandle handle, int32_t timeout_milliseconds) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    return timeout_milliseconds >= 0 && require_socket_runtime() == BK_PLATFORM_OK && socket_native(handle, &native_socket) && NPlatform::WaitReadable(native_socket, timeout_milliseconds) ? BK_PLATFORM_OK : BK_PLATFORM_ERROR_TIMEOUT;
}

BkPlatformResult BK_PLATFORM_CALL socket_resolve_ipv4(const char *host, uint16_t port, BkPlatformSocketAddress *address) {
    if (host == nullptr || address == nullptr || require_socket_runtime() != BK_PLATFORM_OK) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    NPlatform::SocketAddress native{};
    if (!NPlatform::ResolveIPv4(host, port, &native)) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    from_native_address(native, address);
    return BK_PLATFORM_OK;
}

BkPlatformSocketError BK_PLATFORM_CALL socket_last_error() { return static_cast<BkPlatformSocketError>(NPlatform::LastError()); }

BkPlatformResult BK_PLATFORM_CALL socket_close(BkPlatformSocketHandle handle) {
    NPlatform::SocketHandle native_socket = NPlatform::InvalidSocket;
    if (!unregister_socket(handle, &native_socket)) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    NPlatform::Close(native_socket);
    return BK_PLATFORM_OK;
}

BkPlatformResult set_error(BkPlatformResult result, const char *message) {
    BkPlatformState &state = bk_platform_state();
    std::snprintf(state.last_error, sizeof(state.last_error), "%s", message);
    return result;
}

BkPlatformResult BK_PLATFORM_CALL library_open(BkPlatformUtf8Span path, BkPlatformHandle *out_handle) {
    if (out_handle == nullptr || path.struct_size < sizeof(BkPlatformUtf8Span) || path.data == nullptr || path.length == 0)
        return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "invalid dynamic library path");
    std::string path_string;
    try {
        path_string.assign(path.data, path.length);
    } catch (...) {
        return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "dynamic library path allocation failed");
    }
    void *native = open_native_library(path_string);
    if (native == nullptr) return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "dynamic library load failed");

    std::lock_guard<std::mutex> lock(library_mutex);
    uint32_t slot = 0;
    if (!free_library_slots.empty()) {
        slot = free_library_slots.back();
        free_library_slots.pop_back();
    } else {
        try {
            library_slots.emplace_back();
        } catch (...) {
            close_native_library(native);
            return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "dynamic library registry allocation failed");
        }
        slot = static_cast<uint32_t>(library_slots.size() - 1);
    }
    LibraryEntry &entry = library_slots[slot];
    entry.generation = entry.generation == static_cast<uint32_t>(library_generation_mask) ? 1u : entry.generation + 1u;
    if (entry.generation == 0) entry.generation = 1;
    entry.native = native;
    *out_handle = make_library_handle(slot, entry.generation);
    return BK_PLATFORM_OK;
}

BkPlatformResult BK_PLATFORM_CALL library_symbol(BkPlatformHandle handle, BkPlatformUtf8Span name, void **out_symbol) {
    if (out_symbol == nullptr || name.struct_size < sizeof(BkPlatformUtf8Span) || name.data == nullptr || name.length == 0)
        return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "invalid dynamic library symbol name");
    uint32_t slot = 0;
    uint32_t generation = 0;
    if (!decode_library_handle(handle, &slot, &generation)) return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "invalid dynamic library handle");
    std::string name_string;
    try {
        name_string.assign(name.data, name.length);
    } catch (...) {
        return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "dynamic library symbol allocation failed");
    }
    void *native = nullptr;
    {
        std::lock_guard<std::mutex> lock(library_mutex);
        if (slot >= library_slots.size() || library_slots[slot].generation != generation || library_slots[slot].native == nullptr)
            return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "invalid dynamic library handle");
        native = library_slots[slot].native;
    }
    *out_symbol = lookup_native_symbol(native, name_string);
    if (*out_symbol == nullptr) return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "dynamic library symbol lookup failed");
    return BK_PLATFORM_OK;
}

BkPlatformResult BK_PLATFORM_CALL library_close(BkPlatformHandle handle) {
    uint32_t slot = 0;
    uint32_t generation = 0;
    if (!decode_library_handle(handle, &slot, &generation)) return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "invalid dynamic library handle");
    void *native = nullptr;
    {
        std::lock_guard<std::mutex> lock(library_mutex);
        if (slot >= library_slots.size() || library_slots[slot].generation != generation || library_slots[slot].native == nullptr)
            return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "invalid dynamic library handle");
        native = library_slots[slot].native;
        library_slots[slot].native = nullptr;
        try {
            free_library_slots.push_back(slot);
        } catch (...) {
            close_native_library(native);
            return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "dynamic library registry allocation failed");
        }
    }
    close_native_library(native);
    return BK_PLATFORM_OK;
}

void shutdown_libraries() {
    std::lock_guard<std::mutex> lock(library_mutex);
    free_library_slots.clear();
    for (uint32_t slot = 1; slot < library_slots.size(); ++slot) {
        LibraryEntry &entry = library_slots[slot];
        if (entry.native != nullptr) {
            close_native_library(entry.native);
            entry.native = nullptr;
        }
        try {
            free_library_slots.push_back(slot);
        } catch (...) {
        }
    }
}

BkPlatformResult BK_PLATFORM_CALL runtime_create(const BkPlatformCreateInfo *create_info) {
    BkPlatformState &state = bk_platform_state();
    if (create_info == nullptr || create_info->struct_size < sizeof(BkPlatformCreateInfo))
        return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "invalid create info");
    if (create_info->requested_abi_version != BK_PLATFORM_ABI_VERSION)
        return set_error(BK_PLATFORM_ERROR_UNSUPPORTED_VERSION, "unsupported ABI version");
    if (state.initialized)
        return set_error(BK_PLATFORM_ERROR_ALREADY_INITIALIZED, "runtime already initialized");
    state.initialized = true;
    state.generation += 1;
    state.log = create_info->log;
    state.user_data = create_info->user_data;
    state.last_error[0] = 0;
    return BK_PLATFORM_OK;
}

void BK_PLATFORM_CALL runtime_destroy() {
    BkPlatformState &state = bk_platform_state();
    shutdown_libraries();
    shutdown_sockets();
    shutdown_sync_objects();
    state.initialized = false;
    state.log = nullptr;
    state.user_data = nullptr;
    state.last_error[0] = 0;
}

BkPlatformResult BK_PLATFORM_CALL get_last_error(char *dst, uint32_t capacity, uint32_t *required) {
    BkPlatformState &state = bk_platform_state();
    const uint32_t needed = static_cast<uint32_t>(std::strlen(state.last_error) + 1);
    if (required != nullptr) *required = needed;
    if (dst == nullptr || capacity < needed)
        return BK_PLATFORM_ERROR_BUFFER_TOO_SMALL;
    std::memcpy(dst, state.last_error, needed);
    return BK_PLATFORM_OK;
}

uint64_t BK_PLATFORM_CALL get_runtime_generation() {
    return bk_platform_state().generation;
}

uint64_t BK_PLATFORM_CALL get_monotonic_nanoseconds() { return NPlatform::MonotonicNanoseconds(); }
void BK_PLATFORM_CALL sleep_milliseconds(uint32_t milliseconds) { NPlatform::SleepMilliseconds(milliseconds); }
uint32_t BK_PLATFORM_CALL atomic_exchange_u32(uint32_t *value, uint32_t replacement) { return NPlatform::AtomicExchangeU32(value, replacement); }
uint32_t BK_PLATFORM_CALL atomic_increment_u32(uint32_t *value) { return NPlatform::AtomicIncrementU32(value); }
uint32_t BK_PLATFORM_CALL atomic_decrement_u32(uint32_t *value) { return NPlatform::AtomicDecrementU32(value); }
uint32_t BK_PLATFORM_CALL atomic_compare_exchange_u32(uint32_t *value, uint32_t expected, uint32_t replacement) {
    return NPlatform::AtomicCompareExchangeU32(value, expected, replacement);
}

struct SyncEvent {
    std::mutex mutex;
    std::condition_variable condition;
    bool signaled;
    bool manual_reset;
};
struct SyncMutex { std::mutex mutex; };

enum class SyncType : uint8_t {
    none = 0,
    event = 1,
    mutex = 2,
};

struct SyncSlot {
    uint32_t generation = 0;
    SyncType type = SyncType::none;
    std::shared_ptr<void> object;
};

struct DecodedSyncHandle {
    SyncType type;
    uint32_t slot;
    uint32_t generation;
};

constexpr uint64_t sync_type_shift = 62;
constexpr uint64_t sync_generation_mask = UINT64_C(0x3fffffff);
constexpr uint32_t sync_slot_mask = UINT32_MAX;

std::mutex sync_registry_mutex;
std::vector<SyncSlot> sync_slots(1);
std::vector<uint32_t> free_sync_slots;

BkPlatformHandle make_sync_handle(SyncType type, uint32_t slot, uint32_t generation) {
    return (static_cast<BkPlatformHandle>(type) << sync_type_shift) |
           (static_cast<BkPlatformHandle>(generation) << 32) | slot;
}

bool decode_sync_handle(BkPlatformHandle handle, DecodedSyncHandle *decoded) {
    if (handle == 0 || decoded == nullptr) return false;
    const uint64_t type = handle >> sync_type_shift;
    const uint32_t slot = static_cast<uint32_t>(handle & sync_slot_mask);
    const uint32_t generation = static_cast<uint32_t>((handle >> 32) & sync_generation_mask);
    if (type < static_cast<uint64_t>(SyncType::event) || type > static_cast<uint64_t>(SyncType::mutex) || slot == 0 || generation == 0) return false;
    decoded->type = static_cast<SyncType>(type);
    decoded->slot = slot;
    decoded->generation = generation;
    return true;
}

template <typename T>
BkPlatformHandle register_sync(std::shared_ptr<T> object, SyncType type) {
    std::lock_guard<std::mutex> lock(sync_registry_mutex);
    uint32_t slot = 0;
    if (!free_sync_slots.empty()) {
        slot = free_sync_slots.back();
        free_sync_slots.pop_back();
        SyncSlot &entry = sync_slots[slot];
        entry.generation = entry.generation == static_cast<uint32_t>(sync_generation_mask) ? 1u : entry.generation + 1u;
        entry.type = type;
        entry.object = std::move(object);
        ++bk_platform_state().live_sync_handles;
        return make_sync_handle(type, slot, entry.generation);
    }
    if (sync_slots.size() >= static_cast<size_t>(UINT32_MAX)) return 0;
    try {
        sync_slots.emplace_back();
    } catch (...) {
        return 0;
    }
    slot = static_cast<uint32_t>(sync_slots.size() - 1);
    SyncSlot &entry = sync_slots[slot];
    entry.generation = 1;
    entry.type = type;
    entry.object = std::move(object);
    ++bk_platform_state().live_sync_handles;
    return make_sync_handle(type, slot, entry.generation);
}

std::shared_ptr<SyncEvent> lookup_event(BkPlatformHandle handle) {
    DecodedSyncHandle decoded{};
    if (!decode_sync_handle(handle, &decoded) || decoded.type != SyncType::event) return {};
    std::lock_guard<std::mutex> lock(sync_registry_mutex);
    if (decoded.slot >= sync_slots.size()) return {};
    const SyncSlot &entry = sync_slots[decoded.slot];
    if (entry.type != SyncType::event || entry.generation != decoded.generation || !entry.object) return {};
    return std::static_pointer_cast<SyncEvent>(entry.object);
}

std::shared_ptr<SyncMutex> lookup_mutex(BkPlatformHandle handle) {
    DecodedSyncHandle decoded{};
    if (!decode_sync_handle(handle, &decoded) || decoded.type != SyncType::mutex) return {};
    std::lock_guard<std::mutex> lock(sync_registry_mutex);
    if (decoded.slot >= sync_slots.size()) return {};
    const SyncSlot &entry = sync_slots[decoded.slot];
    if (entry.type != SyncType::mutex || entry.generation != decoded.generation || !entry.object) return {};
    return std::static_pointer_cast<SyncMutex>(entry.object);
}

bool destroy_event_handle(BkPlatformHandle handle) {
    DecodedSyncHandle decoded{};
    if (!decode_sync_handle(handle, &decoded) || decoded.type != SyncType::event) return false;
    std::lock_guard<std::mutex> lock(sync_registry_mutex);
    if (decoded.slot >= sync_slots.size()) return false;
    SyncSlot &entry = sync_slots[decoded.slot];
    if (entry.type != SyncType::event || entry.generation != decoded.generation || !entry.object) return false;
    try {
        free_sync_slots.push_back(decoded.slot);
    } catch (...) {
        return false;
    }
    entry.object.reset();
    entry.type = SyncType::none;
    --bk_platform_state().live_sync_handles;
    return true;
}

bool destroy_mutex_handle(BkPlatformHandle handle) {
    DecodedSyncHandle decoded{};
    if (!decode_sync_handle(handle, &decoded) || decoded.type != SyncType::mutex) return false;
    std::lock_guard<std::mutex> lock(sync_registry_mutex);
    if (decoded.slot >= sync_slots.size()) return false;
    SyncSlot &entry = sync_slots[decoded.slot];
    if (entry.type != SyncType::mutex || entry.generation != decoded.generation || !entry.object) return false;
    try {
        free_sync_slots.push_back(decoded.slot);
    } catch (...) {
        return false;
    }
    entry.object.reset();
    entry.type = SyncType::none;
    --bk_platform_state().live_sync_handles;
    return true;
}

void shutdown_sync_objects() {
    std::lock_guard<std::mutex> lock(sync_registry_mutex);
    for (size_t i = 1; i < sync_slots.size(); ++i) {
        SyncSlot &entry = sync_slots[i];
        entry.object.reset();
        entry.type = SyncType::none;
    }
    bk_platform_state().live_sync_handles = 0;
}

BkPlatformResult BK_PLATFORM_CALL event_create(uint32_t initial_state, uint32_t manual_reset, BkPlatformHandle *out_handle) {
    if (out_handle == nullptr) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    std::shared_ptr<SyncEvent> event;
    try {
        event = std::make_shared<SyncEvent>();
    } catch (...) {
        return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    event->signaled = initial_state != 0;
    event->manual_reset = manual_reset != 0;
    const BkPlatformHandle handle = register_sync(std::move(event), SyncType::event);
    if (handle == 0) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    *out_handle = handle;
    return BK_PLATFORM_OK;
}
BkPlatformResult BK_PLATFORM_CALL event_destroy(BkPlatformHandle handle) {
    return destroy_event_handle(handle) ? BK_PLATFORM_OK : BK_PLATFORM_ERROR_INVALID_ARGUMENT;
}
BkPlatformResult BK_PLATFORM_CALL event_set(BkPlatformHandle handle) {
    const std::shared_ptr<SyncEvent> event = lookup_event(handle);
    if (!event) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    { std::lock_guard<std::mutex> lock(event->mutex); event->signaled = true; }
    if (event->manual_reset) event->condition.notify_all(); else event->condition.notify_one();
    return BK_PLATFORM_OK;
}
BkPlatformResult BK_PLATFORM_CALL event_reset(BkPlatformHandle handle) {
    const std::shared_ptr<SyncEvent> event = lookup_event(handle);
    if (!event) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    std::lock_guard<std::mutex> lock(event->mutex);
    event->signaled = false;
    return BK_PLATFORM_OK;
}
BkPlatformResult BK_PLATFORM_CALL event_wait(BkPlatformHandle handle, uint32_t timeout_milliseconds) {
    const std::shared_ptr<SyncEvent> event = lookup_event(handle);
    if (!event) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    std::unique_lock<std::mutex> lock(event->mutex);
    const auto ready = [&event] { return event->signaled; };
    if (timeout_milliseconds == UINT32_MAX) event->condition.wait(lock, ready);
    else if (!event->condition.wait_for(lock, std::chrono::milliseconds(timeout_milliseconds), ready)) return BK_PLATFORM_ERROR_TIMEOUT;
    if (!event->manual_reset) event->signaled = false;
    return BK_PLATFORM_OK;
}
BkPlatformResult BK_PLATFORM_CALL mutex_create(BkPlatformHandle *out_handle) {
    if (out_handle == nullptr) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    std::shared_ptr<SyncMutex> mutex;
    try {
        mutex = std::make_shared<SyncMutex>();
    } catch (...) {
        return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    const BkPlatformHandle handle = register_sync(std::move(mutex), SyncType::mutex);
    if (handle == 0) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    *out_handle = handle;
    return BK_PLATFORM_OK;
}
BkPlatformResult BK_PLATFORM_CALL mutex_destroy(BkPlatformHandle handle) {
    return destroy_mutex_handle(handle) ? BK_PLATFORM_OK : BK_PLATFORM_ERROR_INVALID_ARGUMENT;
}
BkPlatformResult BK_PLATFORM_CALL mutex_lock(BkPlatformHandle handle) {
    const std::shared_ptr<SyncMutex> mutex = lookup_mutex(handle);
    if (!mutex) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    mutex->mutex.lock();
    return BK_PLATFORM_OK;
}
BkPlatformResult BK_PLATFORM_CALL mutex_unlock(BkPlatformHandle handle) {
    const std::shared_ptr<SyncMutex> mutex = lookup_mutex(handle);
    if (!mutex) return BK_PLATFORM_ERROR_INVALID_ARGUMENT;
    mutex->mutex.unlock();
    return BK_PLATFORM_OK;
}
uint32_t BK_PLATFORM_CALL get_live_sync_handles() {
    std::lock_guard<std::mutex> lock(sync_registry_mutex);
    return bk_platform_state().live_sync_handles;
}
BkPlatformResult BK_PLATFORM_CALL diagnostic_write(uint32_t level, BkPlatformUtf8Span message) {
    BkPlatformState &state = bk_platform_state();
    if (message.struct_size < sizeof(BkPlatformUtf8Span) || (message.length != 0 && message.data == nullptr)) return set_error(BK_PLATFORM_ERROR_INVALID_ARGUMENT, "invalid diagnostic message");
    if (state.log != nullptr) state.log(state.user_data, level, message);
    else if (message.data != nullptr) {
        std::fwrite(message.data, 1, message.length, stderr);
        std::fflush(stderr);
    }
    return BK_PLATFORM_OK;
}
uint32_t BK_PLATFORM_CALL is_debugger_attached() {
#if defined(_WIN32)
    return IsDebuggerPresent() != 0 ? 1u : 0u;
#else
    return 0;
#endif
}

const BkPlatformApi api = {
    BK_PLATFORM_ABI_VERSION,
    sizeof(BkPlatformApi),
    &runtime_create,
    &runtime_destroy,
    &get_last_error,
    &get_runtime_generation,
    &get_monotonic_nanoseconds,
    &sleep_milliseconds,
    &atomic_exchange_u32,
    &atomic_increment_u32,
    &atomic_decrement_u32,
    &atomic_compare_exchange_u32,
    &event_create,
    &event_destroy,
    &event_set,
    &event_reset,
    &event_wait,
    &mutex_create,
    &mutex_destroy,
    &mutex_lock,
    &mutex_unlock,
    &get_live_sync_handles,
    &diagnostic_write,
    &is_debugger_attached,
    &socket_runtime_init_call,
    &socket_runtime_done,
    &socket_open_tcp,
    &socket_open_udp,
    &socket_bind,
    &socket_listen,
    &socket_connect,
    &socket_accept,
    &socket_send,
    &socket_receive,
    &socket_send_to,
    &socket_receive_from,
    &socket_set_nonblocking,
    &socket_wait_readable,
    &socket_resolve_ipv4,
    &socket_last_error,
    &socket_close,
    &library_open,
    &library_symbol,
    &library_close,
};
}

BkPlatformState &bk_platform_state() {
    static BkPlatformState state = { false, 0, nullptr, nullptr, 0, { 0 } };
    return state;
}

extern "C" BK_PLATFORM_EXPORT const BkPlatformApi *BK_PLATFORM_CALL bk_platform_get_api(uint32_t requested_version) {
    if (requested_version != BK_PLATFORM_ABI_VERSION) return nullptr;
    return &api;
}
