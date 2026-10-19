#include "StdAfx.h"
#include "SFX.h"
#include "Platform/DynamicLibrary.h"
#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static bool Check(bool value, const char* message)
{
	if (!value)
		std::fprintf(stderr, "sfx module test failed: %s\n", message);
	return value;
}

// Voices must start, stop and pause without a click: the backend renders a
// constant 0.5 at volume 64/255 offline (BkSFXRenderVoiceTest) and every
// step between two frames must be a ramp's, never a jump.
static bool CheckVoiceRamps(NPlatform::DynamicLibrary& module)
{
	using RenderVoice = int (STDCALL*)(int, float*, int);
	const RenderVoice renderVoice = reinterpret_cast<RenderVoice>(module.GetFunction("BkSFXRenderVoiceTest"));
	if (!Check(renderVoice != nullptr, "voice render test export"))
		return false;

	const int nFrames = 8820;
	const float fLevel = 0.5f * 64.0f / 255.0f;
	const float fMaxStep = 0.01f;
	const char* names[] = { "", "start", "stop", "pause" };
	for (int nScenario = 1; nScenario <= 3; ++nScenario)
	{
		std::vector<float> mix(nFrames * 2, 0.0f);
		const int nRendered = renderVoice(nScenario, mix.data(), nFrames);
		if (!Check(nRendered == nFrames, "voice render test ran"))
			return false;
		float fPeak = 0.0f;
		float fStep = std::fabs(mix[0]);
		for (int i = 0; i < nFrames; ++i)
		{
			fPeak = std::fabs(mix[i * 2]) > fPeak ? std::fabs(mix[i * 2]) : fPeak;
			if (i > 0 && std::fabs(mix[i * 2] - mix[(i - 1) * 2]) > fStep)
				fStep = std::fabs(mix[i * 2] - mix[(i - 1) * 2]);
		}
		const float fMid = mix[(nFrames / 2 - 1) * 2];
		const float fEnd = mix[(nFrames - 1) * 2];
		std::printf("sfx voice %s: peak %.4f (mix level %.4f), largest step %.5f, at 100 ms %.4f, at the end %.4f\n", names[nScenario], fPeak, fLevel, fStep, fMid, fEnd);
		if (!Check(fStep < fMaxStep, "a voice changes level by ramps, never by a jump (click)"))
			return false;
		if (!Check(fPeak <= fLevel * 1.02f, "a voice never plays louder than its mix volume"))
			return false;
		if (!Check(std::fabs(fMid - fLevel) < fLevel * 0.02f, "a voice reaches its mix volume"))
			return false;
		if (!Check(std::fabs(fEnd - (nScenario == 1 ? fLevel : 0.0f)) < 0.001f, nScenario == 1 ? "a started voice keeps playing" : "a stopped or paused voice falls silent"))
			return false;
	}
	return true;
}

// What the game's UnloadAllModules does before the modules go: the module's
// globals loader started StreamIO's core services (console, options, storage
// buffers), which only the singleton's Done frees. SDL keeps the base path
// Platform's Paths asked it for until SDL_Quit. Both were blocks every run
// left at the exit's leak report.
static void ReleaseEngine(const char* pszModule)
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
	using GetSingleton = ISingleton* (STDCALL*)();
	const GetSingleton getSingleton = streamio.IsLoaded() ? reinterpret_cast<GetSingleton>(streamio.GetFunction("GetSingletonGlobal_Hook")) : nullptr;
	if (ISingleton* singleton = getSingleton != nullptr ? getSingleton() : nullptr)
		singleton->Done();
	SDL_Quit();
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
	if (!CheckVoiceRamps(module))
		return 11;

	ISFX* sfx = static_cast<ISFX*>(descriptor->pFactory->CreateObject(SFX_SFX));
	if (!Check(sfx != nullptr, "SFX factory object"))
		return 7;
	// A new object's count is 0 (CTRefCount); the Release below must take it
	// to 0 and delete it, as a CPtr would, not to -1.
	sfx->AddRef();
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

	// Opt-in, because CI runners have no playback device: open the real one.
	// With BK_AUDIO_FAIL_DEFAULT=1 as well, the system default is made to fail
	// and the engine has to come up on a fallback device instead; the log says
	// which. Nothing is played.
	const char* pszRealDevice = std::getenv("BK_TEST_AUDIO_DEVICE");
	const bool bRealDevice = pszRealDevice != nullptr && pszRealDevice[0] != 0 && pszRealDevice[0] != '0';
	if (bRealDevice)
	{
		if (!Check(sfx->Init(0, SFX_OUTPUT_DSOUND, 44100, 32) && sfx->IsInitialized(), "real playback device initialization"))
		{
			sfx->Done();
			sfx->Release();
			return 10;
		}
		sfx->Done();
	}
	sfx->Release();
	ReleaseEngine(argv[1]);

	std::printf("sfx module lifecycle passed: Sound v0100 types=6 no-device restart%s\n", bRealDevice ? " real-device" : "");
	return 0;
}
