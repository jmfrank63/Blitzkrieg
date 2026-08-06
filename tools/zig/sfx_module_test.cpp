#include "StdAfx.h"
#include "SFX.h"
#include "Platform/DynamicLibrary.h"

#include <cstdio>
#include <cstring>

static bool Check(bool value, const char* message)
{
	if (!value)
		std::fprintf(stderr, "sfx module test failed: %s\n", message);
	return value;
}

int main(int argc, char** argv)
{
	if (!Check(argc == 2, "module path argument"))
		return 1;
	NPlatform::DynamicLibrary module(argv[1]);
	if (!Check(module.IsLoaded(), module.GetError()))
		return 2;
	using GetDescriptor = const SModuleDescriptor* (STDCALL*)();
	const GetDescriptor getDescriptor = reinterpret_cast<GetDescriptor>(module.GetFunction("GetModuleDescriptor"));
	if (!Check(getDescriptor != nullptr, "descriptor export"))
		return 3;
	const SModuleDescriptor* descriptor = getDescriptor();
	if (!Check(descriptor != nullptr && descriptor->pszName != nullptr && std::strcmp(descriptor->pszName, "Sound") == 0, "descriptor identity"))
		return 4;
	if (!Check(descriptor->nType == SFX_SFX && descriptor->nVersion == 0x0100 && descriptor->pFactory != nullptr, "descriptor metadata"))
		return 5;
	if (!Check(descriptor->pFactory->GetNumKnownTypes() == 6, "factory type count"))
		return 6;

	ISFX* sfx = static_cast<ISFX*>(descriptor->pFactory->CreateObject(SFX_SFX));
	if (!Check(sfx != nullptr, "SFX factory object"))
		return 7;
	if (!Check(sfx->Init(0, SFX_OUTPUT_NO, 44100, 2), "no-device initialization"))
	{
		sfx->Release();
		return 8;
	}
	sfx->SetSFXMasterVolume(0.5f);
	sfx->SetStreamMasterVolume(0.25f);
	sfx->EnableSFX(false);
	sfx->EnableStreaming(false);
	sfx->PlayStream("missing-audio.wav", false, 0);
	sfx->StopStream(0);
	sfx->Done();
	if (!Check(sfx->Init(0, SFX_OUTPUT_NO, 48000, 2), "restart initialization"))
	{
		sfx->Release();
		return 9;
	}
	sfx->Done();
	sfx->Done();
	sfx->Release();

	std::puts("sfx module lifecycle passed: Sound v0100 types=6 no-device restart");
	return 0;
}
