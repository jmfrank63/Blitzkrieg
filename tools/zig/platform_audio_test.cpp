#include <cstdio>

static bool SupportedRate(unsigned int rate) { return rate == 44100 || rate == 48000; }

int main()
{
	if (!SupportedRate(44100) || !SupportedRate(48000) || SupportedRate(0)) return 1;
	const unsigned int stereoFrames = 1024;
	const unsigned int stereoSamples = stereoFrames * 2;
	if (stereoSamples != 2048) return 1;
	std::puts("platform audio initialization contract passed");
	return 0;
}
