#include "StdAfx.h"
#include "NetDriver.h"
#include "Platform/DynamicLibrary.h"

#include <cstdio>

static bool Check(bool value, const char *message) {
    if (!value) std::fprintf(stderr, "net module test failed: %s\n", message);
    return value;
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
    address->Release();

    std::puts("net module factory lifecycle passed: Network v0100 types=2");
    return 0;
}
