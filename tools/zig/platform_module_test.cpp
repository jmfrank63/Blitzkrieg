#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

static const char *TargetSuffix() {
#if defined(_WIN32) || defined(_WIN64)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

int main() {
    const std::string suffix = TargetSuffix();
    std::vector<std::string> candidates = {"libUI" + suffix, "libStreamIO" + suffix, "notes.txt", "libAnim" + suffix, "libStreamIO.debug" + suffix};
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [](const std::string &name) {
        return name.find(TargetSuffix()) == std::string::npos || name.find(".debug") != std::string::npos;
    }), candidates.end());
    std::sort(candidates.begin(), candidates.end());
    assert(candidates.size() == 3);
    assert(candidates[0] == "libAnim" + suffix);
    assert(candidates[1] == "libStreamIO" + suffix);
    assert(candidates[2] == "libUI" + suffix);

    std::vector<int> module_types;
    auto accept = [&module_types](int type) {
        if (std::find(module_types.begin(), module_types.end(), type) != module_types.end()) return false;
        module_types.push_back(type);
        return true;
    };
    assert(accept(3));
    assert(!accept(3));
    assert(accept(7));
    std::puts("platform module contract tests passed");
    return 0;
}
