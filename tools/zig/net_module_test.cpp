#include "StdAfx.h"
#include "NetDriver.h"
#include "Platform/DynamicLibrary.h"
#include <SDL3/SDL.h>

#include <cstdio>
#include <string>

static bool Check(bool value, const char *message) {
    if (!value) std::fprintf(stderr, "net module test failed: %s\n", message);
    return value;
}

// What the game's UnloadAllModules does before the modules go, as in the SFX
// module test: Net's globals loader started StreamIO's core services (console,
// options, storage buffers), which only the singleton's Done frees, and SDL
// keeps the base path Platform's Paths asked it for until SDL_Quit. Without
// both, every run left 9 blocks at the exit's leak report.
static void ReleaseEngine(const char *pszModule) {
    std::string szPath(pszModule);
    const size_t nSlash = szPath.find_last_of("/\\");
    szPath = nSlash == std::string::npos ? std::string() : szPath.substr(0, nSlash + 1);
#if defined(_WIN32)
    szPath += "StreamIO.dll";
#elif defined(__APPLE__)
    szPath += "libStreamIO.dylib";
#else
    szPath += "libStreamIO.so";
#endif
    NPlatform::DynamicLibrary streamio(szPath.c_str());
    using GetSingleton = ISingleton *(STDCALL *)();
    const GetSingleton getSingleton = streamio.IsLoaded() ? reinterpret_cast<GetSingleton>(streamio.GetFunction("GetSingletonGlobal_Hook")) : nullptr;
    if (ISingleton *singleton = getSingleton != nullptr ? getSingleton() : nullptr)
        singleton->Done();
    SDL_Quit();
}

int main(int argc, char **argv) {
    if (!Check(argc == 2, "module path argument")) return 1;
    NPlatform::DynamicLibrary module(argv[1]);
    if (!Check(module.IsLoaded(), module.GetError())) return 2;
    const GETMODULEDESCRIPTOR getDescriptor = reinterpret_cast<GETMODULEDESCRIPTOR>(module.GetFunction("GetModuleDescriptor"));
    if (!Check(getDescriptor != nullptr, "descriptor export")) return 3;
    const SModuleDescriptor *descriptor = getDescriptor();
    if (!Check(descriptor != nullptr && descriptor->pszName != nullptr && std::strcmp(descriptor->pszName, "Network") == 0, "descriptor identity")) return 4;
    if (!Check(descriptor->nType == NET_NET && descriptor->nVersion == 0x0100 && descriptor->pFactory != nullptr, "descriptor metadata")) return 5;
    if (!Check(descriptor->pFactory->GetNumKnownTypes() == 2, "factory type count")) return 6;

    IRefCount *address = descriptor->pFactory->CreateObject(NET_NODE_ADDRESS);
    if (!Check(address != nullptr && descriptor->pFactory->GetObjectTypeID(address) == NET_NODE_ADDRESS, "node address factory")) return 7;
    // A factory object starts at no references: a Release without the AddRef takes the
    // count to -1, which never deletes, and the address was the last block at exit.
    address->AddRef();
    address->Release();
    ReleaseEngine(argv[1]);
    return 0;
}
