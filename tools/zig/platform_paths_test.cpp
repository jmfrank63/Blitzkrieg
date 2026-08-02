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
    assert(NPlatform::Paths::DataRoot().find("data") != std::string::npos);
    assert(NPlatform::Paths::ShaderRoot().find("shaders") != std::string::npos);
    assert(NPlatform::Paths::ModuleRoot() == NPlatform::Paths::BaseRoot());
    assert(NPlatform::Paths::ConfigPath().find("config.cfg") != std::string::npos);
    assert(NPlatform::Paths::SaveRoot().find("saves") != std::string::npos);
    assert(NPlatform::Paths::LogPath().find("logs") != std::string::npos);
    assert(NPlatform::Paths::CacheRoot().find("cache") != std::string::npos);
    assert(std::filesystem::exists(user / "saves"));
    assert(std::filesystem::exists(user / "logs"));
    assert(std::filesystem::exists(user / "cache"));
    NPlatform::Paths::ClearInjectedRootsForTest();
    std::filesystem::remove_all(root);
    return 0;
}
