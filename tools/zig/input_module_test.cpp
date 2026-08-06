#include "../../Sources/src/Input/StdAfx.h"
#include "../../Sources/src/Input/Input.h"
#include "../../Sources/src/Platform/DynamicLibrary.h"

#include <cstdio>

static bool Check(bool condition, const char *message)
{
	if (!condition) std::fprintf(stderr, "input module test failed: %s\n", message);
	return condition;
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
	IInput *input = static_cast<IInput *>( object );
	if (!Check(input->Init(), "first input initialization succeeds")) return 1;
	if (!Check(input->Done(), "first input shutdown succeeds")) return 1;
	if (!Check(input->Init(), "second input initialization succeeds")) return 1;
	if (!Check(input->Done(), "second input shutdown succeeds")) return 1;
	object->Release();
	std::puts("input module factory lifecycle passed");
	return 0;
}
