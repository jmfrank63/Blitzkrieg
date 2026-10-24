#include "Platform/Paths.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

static void SetTraceEnv(const char *value)
{
#if defined(_WIN32)
    _putenv_s("BK_DEBUG_LOG", value ? value : "");
#else
    if (value) setenv("BK_DEBUG_LOG", value, 1); else unsetenv("BK_DEBUG_LOG");
#endif
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() / "blitzkrieg-paths-test";
    const auto user = root / "user";
    std::filesystem::remove_all(root);
    NPlatform::Paths::SetInjectedRootsForTest(root.string().c_str(), user.string().c_str());
    assert(NPlatform::Paths::BaseRoot().find("blitzkrieg-paths-test") != std::string::npos);
    // Canonical capitalisation, not the case-folded Windows spelling: these are
    // real directory names on a case-sensitive filesystem, and Paths.cpp keeps
    // them that way deliberately.
    assert(NPlatform::Paths::DataRoot().find("Data") != std::string::npos);
    assert(NPlatform::Paths::ShaderRoot().find("Shaders") != std::string::npos);
    assert(NPlatform::Paths::ModuleRoot() == NPlatform::Paths::BaseRoot());
    assert(NPlatform::Paths::ConfigPath().find("config.cfg") != std::string::npos);
    assert(NPlatform::Paths::SaveRoot().find("saves") != std::string::npos);
    assert(NPlatform::Paths::LogPath().find("logs") != std::string::npos);
    assert(NPlatform::Paths::CacheRoot().find("cache") != std::string::npos);
    // Screenshots live in the game directory, beside the saves the game
    // actually writes: missions save to GetBaseDir() + modname + "saves".
    assert(NPlatform::Paths::ScreenshotRoot().find("screenshots") != std::string::npos);
    // BaseRoot carries a trailing separator, so compare by prefix rather than
    // by parent_path, which would differ only by that separator.
    assert(NPlatform::Paths::ScreenshotRoot().rfind(NPlatform::Paths::BaseRoot(), 0) == 0);
    assert(NPlatform::Paths::ScreenshotRoot().substr(NPlatform::Paths::BaseRoot().size()) == "screenshots");
    // The generated season textures sit beside Data, not in it: a staged Data
    // may be a link into the source tree (stage-game --link-data).
    assert(NPlatform::Paths::SeasonDataRoot().rfind(NPlatform::Paths::BaseRoot(), 0) == 0);
    assert(NPlatform::Paths::SeasonDataRoot().substr(NPlatform::Paths::BaseRoot().size()) == "SeasonData");
    assert(NPlatform::Paths::SeasonDataArchivePattern().rfind(NPlatform::Paths::SeasonDataRoot(), 0) == 0);
    assert(NPlatform::Paths::SeasonDataArchivePattern().size() == NPlatform::Paths::SeasonDataRoot().size() + 6);
    assert(NPlatform::Paths::SeasonDataArchivePattern().substr(NPlatform::Paths::SeasonDataRoot().size() + 1) == "*.pak");
    // A multiplayer client stores the map under the name the host sends. Only a
    // plain relative data name may pass; anything else would write outside the
    // directory it is joined to.
    assert(NPlatform::Paths::IsRelativeDataName("maps\\Arnhem.bzm"));
    assert(NPlatform::Paths::IsRelativeDataName("maps/multiplayer/Arnhem.lua"));
    assert(!NPlatform::Paths::IsRelativeDataName(""));
    assert(!NPlatform::Paths::IsRelativeDataName("maps\\"));
    assert(!NPlatform::Paths::IsRelativeDataName("..\\..\\config.cfg"));
    assert(!NPlatform::Paths::IsRelativeDataName("maps\\..\\..\\x.bzm"));
    assert(!NPlatform::Paths::IsRelativeDataName("maps/./x.bzm"));
    assert(!NPlatform::Paths::IsRelativeDataName("\\etc\\passwd"));
    assert(!NPlatform::Paths::IsRelativeDataName("/etc/passwd"));
    assert(!NPlatform::Paths::IsRelativeDataName("C:\\Windows\\x.bzm"));
    assert(!NPlatform::Paths::IsRelativeDataName(std::string("maps\\a\0b.bzm", 12)));
    assert(std::filesystem::exists(user / "saves"));
    assert(std::filesystem::exists(user / "logs"));
    assert(std::filesystem::exists(user / "cache"));
    // Created at startup, so a first F9 never has to make it.
    assert(std::filesystem::exists(root / "screenshots"));
    // Trace files go to the user root's logs folder, only when tracing is on, and
    // never into the game folder (a stray "\load_trace.log" once landed there).
    SetTraceEnv(nullptr);
    assert(NPlatform::Paths::OpenTraceLog("load_trace.log") == nullptr);
    SetTraceEnv("1");
    std::FILE *trace = NPlatform::Paths::OpenTraceLog("load_trace.log");
    assert(trace != nullptr);
    std::fclose(trace);
    assert(std::filesystem::exists(user / "logs" / "load_trace.log"));
    assert(!std::filesystem::exists(root / "load_trace.log"));
    SetTraceEnv(nullptr);
    NPlatform::Paths::ClearInjectedRootsForTest();
    std::filesystem::remove_all(root);
    return 0;
}
