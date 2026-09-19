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
    // Screenshots live in the game directory, beside the saves the game
    // actually writes: missions save to GetBaseDir() + modname + "saves".
    assert(NPlatform::Paths::ScreenshotRoot().find("screenshots") != std::string::npos);
    // BaseRoot carries a trailing separator, so compare by prefix rather than
    // by parent_path, which would differ only by that separator.
    assert(NPlatform::Paths::ScreenshotRoot().rfind(NPlatform::Paths::BaseRoot(), 0) == 0);
    assert(NPlatform::Paths::ScreenshotRoot().substr(NPlatform::Paths::BaseRoot().size()) == "screenshots");
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
    NPlatform::Paths::ClearInjectedRootsForTest();
    std::filesystem::remove_all(root);
    return 0;
}
