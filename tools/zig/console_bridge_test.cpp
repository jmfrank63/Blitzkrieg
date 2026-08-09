// Drives the console bridge that ships in StreamIOOptionsAbi through the real
// engine interface. The bridge used to declare IConsoleBuffer with
// `const unsigned short *` where StreamIO/Globals.h declares `const wchar_t *`.
// The vtable slots line up either way, so nothing complained: a write stopped
// at the first character, because the high half of a 32-bit wchar_t reads as a
// NUL, and a read then ran off the end of the buffer looking for a second one.
// The top-left messages came out as "GG", "OO" and "RR" plus a garbage glyph.
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#define BK_STDCALL __stdcall
#else
#define BK_STDCALL
#endif

// Must mirror IConsoleBuffer in Sources/src/StreamIO/Globals.h.
// Slot-for-slot with IRefCount in options_bridge.cpp; two missing entries here
// would silently shift every call below it.
struct IRefCountLike {
    virtual void BK_STDCALL AddRef(int = 1, int = 0x7fffffff) = 0;
    virtual void BK_STDCALL Release(int = 1, int = 0x7fffffff) = 0;
    virtual bool BK_STDCALL IsValid() const = 0;
    virtual IRefCountLike *BK_STDCALL QI(int) { return 0; }
    virtual int BK_STDCALL operator&(void *) { return 0; }
};
struct IConsoleBufferLike : public IRefCountLike {
    virtual bool BK_STDCALL Configure(const char *) = 0;
    virtual void BK_STDCALL Write(int, const wchar_t *, unsigned long, bool) = 0;
    virtual void BK_STDCALL WriteASCII(int, const char *, unsigned long, bool) = 0;
    virtual const wchar_t *BK_STDCALL Read(int, unsigned long *) = 0;
    virtual const char *BK_STDCALL ReadASCII(int, unsigned long *) = 0;
    virtual bool BK_STDCALL DumpLog(int) = 0;
};

int main(int argc, char **argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: console-bridge-test <path to StreamIOOptionsAbi>\n"); return 2; }

    typedef void *(*CreateFn)();
#if defined(_WIN32) || defined(_WIN64)
    HMODULE library = LoadLibraryA(argv[1]);
    if (!library) { std::fprintf(stderr, "cannot load %s\n", argv[1]); return 2; }
    CreateFn create = reinterpret_cast<CreateFn>(GetProcAddress(library, "bk_console_bridge_create"));
#else
    void *library = dlopen(argv[1], RTLD_NOW);
    if (!library) { std::fprintf(stderr, "cannot load %s: %s\n", argv[1], dlerror()); return 2; }
    CreateFn create = reinterpret_cast<CreateFn>(dlsym(library, "bk_console_bridge_create"));
#endif
    if (!create) { std::fprintf(stderr, "bk_console_bridge_create missing\n"); return 2; }

    IConsoleBufferLike *console = static_cast<IConsoleBufferLike *>(create());
    assert(console != 0);
    console->AddRef();

    // The exact shape that used to fail: a multi-character wide string has to
    // survive the round trip whole, not collapse to its first letter.
    const int channel = 4;                       // CONSOLE_STREAM_CHAT
    const wchar_t *sent = L"Game Speed +1";
    console->Write(channel, sent, 0xff00ff00, false);

    unsigned long color = 0;
    const wchar_t *got = console->Read(channel, &color);
    assert(got != 0);
    if (std::wcscmp(got, sent) != 0) {
        std::fprintf(stderr, "round trip lost characters: expected %d, got %d\n",
                     static_cast<int>(std::wcslen(sent)), static_cast<int>(std::wcslen(got)));
        for (const wchar_t *it = got; *it; ++it) std::fprintf(stderr, "  %u\n", static_cast<unsigned>(*it));
        return 1;
    }
    assert(std::wcslen(got) == std::wcslen(sent));
    assert(color == 0xff00ff00);

    // A second line must not hand back the first, and draining must report empty.
    console->Write(channel, L"Objective received", 0xffff0000, false);
    const wchar_t *second = console->Read(channel, &color);
    assert(second != 0 && std::wcscmp(second, L"Objective received") == 0);
    assert(console->Read(channel, &color) == 0);

    console->Release();
    std::puts("console bridge round trip passed");
    return 0;
}
