#ifndef BLITZKRIEG_PLATFORM_PATHS_H
#define BLITZKRIEG_PLATFORM_PATHS_H

#include <string>

namespace NPlatform::Paths {
bool Initialize();
void SetInjectedRootsForTest(const char *base, const char *preference);
void ClearInjectedRootsForTest();
const std::string &BaseRoot();
const std::string &UserRoot();
const std::string &DataRoot();
const std::string &ShaderRoot();
const std::string &ModuleRoot();
const std::string &ConfigPath();
const std::string &SaveRoot();
const std::string &LogPath();
const std::string &ErrorLogPath();
const std::string &CacheRoot();
const std::string &DataArchivePattern();
}

#endif
