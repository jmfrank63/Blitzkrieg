#ifndef BLITZKRIEG_PLATFORM_PATHS_H
#define BLITZKRIEG_PLATFORM_PATHS_H

#include <string>

namespace NPlatform {
namespace Paths {
bool Initialize();
// Point the engine at an installation. The editor needs this for real - the
// user picks which game directory to edit - so it is not only a test seam;
// SetInjectedRootsForTest forwards here and keeps its name for the tests that
// already call it.
void SetRoots(const char *base, const char *preference);
void SetInjectedRootsForTest(const char *base, const char *preference);
void ClearInjectedRootsForTest();
const std::string &BaseRoot();
const std::string &UserRoot();
const std::string &DataRoot();
const std::string &ShaderRoot();
const std::string &ModuleRoot();
const std::string &ConfigPath();
const std::string &SaveRoot();
const std::string &ScreenshotRoot();
const std::string &LogPath();
const std::string &ErrorLogPath();
const std::string &CacheRoot();
const std::string &DataArchivePattern();

// Whether a name, as another machine sends it, is a plain relative data name
// ("maps\\x.bzm"): not empty, not rooted, no drive, no "." or ".." component
// and no empty one. Inline so callers in any dylib can use it.
inline bool IsRelativeDataName(const std::string &name)
{
    if (name.empty() || name.find(':') != std::string::npos || name.find('\0') != std::string::npos)
        return false;
    std::string::size_type begin = 0;
    while (true) {
        const std::string::size_type end = name.find_first_of("\\/", begin);
        const std::string part = name.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        if (part.empty() || part == "." || part == "..")
            return false;
        if (end == std::string::npos)
            return true;
        begin = end + 1;
    }
}
}
}

#endif
