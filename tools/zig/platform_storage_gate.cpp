#include <cassert>
#include <cstdio>
#include <string>

static std::string Read(const char *path) {
    std::FILE *file = std::fopen(path, "rb");
    if (!file) return {};
    std::string value;
    char buffer[4096];
    while (const std::size_t count = std::fread(buffer, 1, sizeof(buffer), file)) value.append(buffer, count);
    std::fclose(file);
    return value;
}
static void Write(const char *path, const std::string &value) {
    std::FILE *file = std::fopen(path, "wb");
    assert(file);
    std::fwrite(value.data(), 1, value.size(), file);
    std::fclose(file);
}

int main() {
    const std::string packaged = Read("Data/consts.xml");
    assert(!packaged.empty());
    const char *config = "storage-gate-config.cfg";
    const char *save = "storage-gate-save.sav";
    const std::string config_text = "GFX.Mode=1920x1080x32\nSound.Volume=75\n";
    const std::string save_text = "fixture-save-v1\n";
    Write(config, config_text);
    Write(save, save_text);
    assert(Read(config) == config_text);
    assert(Read(save) == save_text);
    Write(config, Read(config));
    Write(save, Read(save));
    assert(Read(config).find("Sound.Volume=75") != std::string::npos);
    assert(Read(save) == save_text);
    assert(Read("Data/consts.xml") == packaged);
    std::remove(config);
    std::remove(save);
    return 0;
}
