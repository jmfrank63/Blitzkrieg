#include "Paths.h"
#if !defined(BLITZKRIEG_PATHS_TEST) && defined(_WIN32)
#include <SDL3/SDL.h>
#endif
#if !defined(_WIN32)
#include <cstdlib>
#include <unistd.h>
#endif
#include <filesystem>

namespace {
std::string gBase, gUser;
bool gInitialized = false;
std::string ensureSeparator(std::string value) {
    const char separator =
#if defined(_WIN32)
        '\\';
#else
        '/';
#endif
    for (char &c : value) if (c == '/' || c == '\\') c = separator;
    if (value.empty() || value.back() != separator) value += separator;
    return value;
}
std::string join(const std::string &root, const char *name) { return ensureSeparator(root) + name; }
void createWritableRoots() {
    std::error_code error;
    std::filesystem::create_directories(gUser, error);
    std::filesystem::create_directories(join(gUser, "saves"), error);
    std::filesystem::create_directories(join(gUser, "logs"), error);
    std::filesystem::create_directories(join(gUser, "cache"), error);
    std::filesystem::create_directories(join(gUser, "screenshots"), error);
}
#if !defined(_WIN32)
std::string executableRoot() {
    std::error_code error;
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) return executable.parent_path().string();
    return std::filesystem::current_path(error).string();
}
std::string preferenceRoot() {
    const char *xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return join(xdg, "Nival/Blitzkrieg");
    const char *home = std::getenv("HOME");
    if (home && *home) return join(home, ".local/share/Nival/Blitzkrieg");
    return {};
}
#endif
}

namespace NPlatform::Paths {
bool Initialize() {
    if (gInitialized) return !gBase.empty() && !gUser.empty();
#if defined(BLITZKRIEG_PATHS_TEST)
    return false;
#elif defined(_WIN32)
    const char *base = SDL_GetBasePath();
    char *preference = SDL_GetPrefPath("Nival", "Blitzkrieg");
    if (base) gBase = ensureSeparator(base);
    if (preference) { gUser = ensureSeparator(preference); SDL_free(preference); }
#else
    gBase = ensureSeparator(executableRoot());
    gUser = ensureSeparator(preferenceRoot());
#endif
    gInitialized = !gBase.empty() && !gUser.empty();
    if (gInitialized) createWritableRoots();
    return gInitialized;
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
// These are package directory names, not case-folded Windows paths.  Keeping
// the canonical spelling here is required on case-sensitive filesystems.
const std::string &DataRoot() { static std::string value; value = join(BaseRoot(), "Data"); return value; }
const std::string &ShaderRoot() { static std::string value; value = join(BaseRoot(), "Shaders"); return value; }
const std::string &ModuleRoot() { return BaseRoot(); }
const std::string &ConfigPath() { static std::string value; value = join(UserRoot(), "config.cfg"); return value; }
const std::string &SaveRoot() { static std::string value; value = join(UserRoot(), "saves"); return value; }
// A sibling of saves, not a child of it: screenshots are not save games and
// the save game browser enumerates that directory.
const std::string &ScreenshotRoot() { static std::string value; value = join(UserRoot(), "screenshots"); return value; }
const std::string &LogPath() { static std::string value; value = join(join(UserRoot(), "logs"), "log.txt"); return value; }
const std::string &ErrorLogPath() { static std::string value; value = join(join(UserRoot(), "logs"), "error.txt"); return value; }
const std::string &CacheRoot() { static std::string value; value = join(UserRoot(), "cache"); return value; }
const std::string &DataArchivePattern() { static std::string value; value = join(DataRoot(), "*.pak"); return value; }
}
