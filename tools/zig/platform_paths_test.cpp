#include "Platform/Paths.h"
#include <cassert>
#include <filesystem>

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
    // Screenshots sit beside the saves, not inside them: the save game browser
    // enumerates SaveRoot and a screenshots directory in there is not a save.
    assert(NPlatform::Paths::ScreenshotRoot().find("screenshots") != std::string::npos);
    assert(NPlatform::Paths::ScreenshotRoot().find("saves") == std::string::npos);
    assert(std::filesystem::path(NPlatform::Paths::ScreenshotRoot()).parent_path() ==
           std::filesystem::path(NPlatform::Paths::SaveRoot()).parent_path());
    assert(std::filesystem::exists(user / "saves"));
    assert(std::filesystem::exists(user / "logs"));
    assert(std::filesystem::exists(user / "cache"));
    // Created at startup, so a first F9 never has to make it.
    assert(std::filesystem::exists(user / "screenshots"));
    NPlatform::Paths::ClearInjectedRootsForTest();
    std::filesystem::remove_all(root);
    return 0;
}
