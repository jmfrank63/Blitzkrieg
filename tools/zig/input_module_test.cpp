#include "../../Sources/src/Input/StdAfx.h"
#include "../../Sources/src/Input/Input.h"
#include "../../Sources/src/Platform/DynamicLibrary.h"
#include <SDL3/SDL.h>

#include <cstdio>
#include <string>

static bool Check(bool condition, const char *message)
{
	if (!condition) std::fprintf(stderr, "input module test failed: %s\n", message);
	return condition;
}

// What the game's UnloadAllModules does before the modules go, as in the SFX
// module test: Input's globals loader started StreamIO's core services
// (console, options, storage buffers), which only the singleton's Done frees,
// and SDL keeps the base path Platform's Paths asked it for until SDL_Quit.
static void ReleaseEngine(const char *pszModule)
{
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

int main(int argc, char **argv)
{
	if (!Check(argc == 2, "module path argument")) return 1;
	NPlatform::DynamicLibrary module(argv[1]);
	if (!Check(module.IsLoaded(), module.GetError())) return 2;
	using GetDescriptor = const SModuleDescriptor *(STDCALL *)();
	const GetDescriptor getDescriptor = reinterpret_cast<GetDescriptor>(module.GetFunction("GetModuleDescriptor"));
	if (!Check(getDescriptor != nullptr, "descriptor export")) return 3;
	const SModuleDescriptor *descriptor = getDescriptor();
	if (!Check(descriptor != 0 && descriptor->pFactory != 0, "module descriptor and factory are exported")) return 1;
	if (!Check(descriptor->nType == INPUT_INPUT && descriptor->nVersion == 0x0200, "module identity is stable")) return 1;
	if (!Check(descriptor->pFactory->GetNumKnownTypes() >= 3, "factory registers input object types")) return 1;
	IRefCount *object = descriptor->pFactory->CreateObject( INPUT_INPUT );
	if (!Check(object != 0, "factory creates the input object")) return 1;
	// A factory object starts at no references; the Release below without this AddRef
	// takes the count to -1, which never deletes.
	object->AddRef();
	IInput *input = static_cast<IInput *>( object );
	if (!Check(input->Init(), "first input initialization succeeds")) return 1;
	if (!Check(input->Done(), "first input shutdown succeeds")) return 1;
	if (!Check(input->Init(), "second input initialization succeeds")) return 1;
	if (!Check(input->Done(), "second input shutdown succeeds")) return 1;
	object->Release();
	ReleaseEngine(argv[1]);
	return 0;
}
