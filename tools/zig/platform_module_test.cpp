#include <stdio.h>
#include <string.h>

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
    const char *suffix = TargetSuffix();
#if defined(_WIN32) || defined(_WIN64)
    const char *candidates[] = {"Anim.dll", "StreamIO.dll", "notes.txt", "UI.dll", "StreamIO.debug.dll"};
    const char *expected[] = {"Anim.dll", "StreamIO.dll", "UI.dll"};
#elif defined(__APPLE__)
    const char *candidates[] = {"libUI.dylib", "libStreamIO.dylib", "notes.txt", "libAnim.dylib", "libStreamIO.debug.dylib"};
    const char *expected[] = {"libAnim.dylib", "libStreamIO.dylib", "libUI.dylib"};
#else
    const char *candidates[] = {"libUI.so", "libStreamIO.so", "notes.txt", "libAnim.so", "libStreamIO.debug.so"};
    const char *expected[] = {"libAnim.so", "libStreamIO.so", "libUI.so"};
#endif
    const char *filtered[5];
    size_t filtered_count = 0;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (strstr(candidates[i], suffix) == NULL || strstr(candidates[i], ".debug") != NULL) continue;
        filtered[filtered_count++] = candidates[i];
    }
    for (size_t i = 0; i < filtered_count; ++i) {
        for (size_t j = i + 1; j < filtered_count; ++j) {
            if (strcmp(filtered[j], filtered[i]) < 0) {
                const char *swap = filtered[i];
                filtered[i] = filtered[j];
                filtered[j] = swap;
            }
        }
    }
    if (filtered_count != 3 || strcmp(filtered[0], expected[0]) != 0 ||
        strcmp(filtered[1], expected[1]) != 0 || strcmp(filtered[2], expected[2]) != 0) return 1;

    int module_types[2];
    size_t module_type_count = 0;
    const int types[] = {3, 3, 7};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        int duplicate = 0;
        for (size_t j = 0; j < module_type_count; ++j) {
            if (module_types[j] == types[i]) duplicate = 1;
        }
        if (duplicate) {
            if (types[i] != 3) return 1;
        } else {
            if (module_type_count >= sizeof(module_types) / sizeof(module_types[0])) return 1;
            module_types[module_type_count++] = types[i];
        }
    }
    if (module_type_count != 2 || module_types[0] != 3 || module_types[1] != 7) return 1;
    puts("platform module contract tests passed");
    return 0;
}
