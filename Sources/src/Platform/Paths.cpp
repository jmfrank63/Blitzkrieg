#include "Paths.h"
#if !defined(BLITZKRIEG_PATHS_TEST)
#include <SDL3/SDL.h>
#endif
#include <filesystem>

namespace {
std::string gBase, gUser;
bool gInitialized = false;
std::string ensureSeparator(std::string value) {
    for (char &c : value) if (c == '/') c = '\\';
    if (value.empty() || value.back() != '\\') value += '\\';
    return value;
}
std::string join(const std::string &root, const char *name) { return ensureSeparator(root) + name; }
void createWritableRoots() {
    std::error_code error;
    std::filesystem::create_directories(gUser, error);
    std::filesystem::create_directories(join(gUser, "saves"), error);
    std::filesystem::create_directories(join(gUser, "logs"), error);
    std::filesystem::create_directories(join(gUser, "cache"), error);
}
}

namespace NPlatform::Paths {
bool Initialize() {
    if (gInitialized) return !gBase.empty() && !gUser.empty();
#if defined(BLITZKRIEG_PATHS_TEST)
    return false;
#else
    const char *base = SDL_GetBasePath();
    char *preference = SDL_GetPrefPath("Nival", "Blitzkrieg");
    if (base) gBase = ensureSeparator(base);
    if (preference) { gUser = ensureSeparator(preference); SDL_free(preference); }
    gInitialized = !gBase.empty() && !gUser.empty();
    if (gInitialized) createWritableRoots();
    return gInitialized;
#endif
}
void SetInjectedRootsForTest(const char *base, const char *preference) {
    gBase = ensureSeparator(base ? base : "");
    gUser = ensureSeparator(preference ? preference : "");
    gInitialized = true;
    createWritableRoots();
}
void ClearInjectedRootsForTest() { gBase.clear(); gUser.clear(); gInitialized = false; }
const std::string &BaseRoot() { Initialize(); return gBase; }
const std::string &UserRoot() { Initialize(); return gUser; }
const std::string &DataRoot() { static std::string value; value = join(BaseRoot(), "data"); return value; }
const std::string &ShaderRoot() { static std::string value; value = join(BaseRoot(), "shaders"); return value; }
const std::string &ModuleRoot() { return BaseRoot(); }
const std::string &ConfigPath() { static std::string value; value = join(UserRoot(), "config.cfg"); return value; }
const std::string &SaveRoot() { static std::string value; value = join(UserRoot(), "saves"); return value; }
const std::string &LogPath() { static std::string value; value = join(UserRoot(), "logs\\log.txt"); return value; }
const std::string &ErrorLogPath() { static std::string value; value = join(UserRoot(), "logs\\error.txt"); return value; }
const std::string &CacheRoot() { static std::string value; value = join(UserRoot(), "cache"); return value; }
const std::string &DataArchivePattern() { static std::string value; value = DataRoot() + "\\*.pak"; return value; }
}
