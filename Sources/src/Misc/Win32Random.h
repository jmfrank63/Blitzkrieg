#ifndef __WIN32RANDOM_H__
#define __WIN32RANDOM_H__
#pragma ONCE
namespace NWin32Random
{
	// Random() is the Win32 rand() generator reimplemented here so a mission
	// plays back identically everywhere, and it always yields 0..0x7fff. The
	// float overload has to scale by that range, NOT by the platform's
	// RAND_MAX: that macro is 0x7fff only under MSVC, and 0x7fffffff in libc,
	// where dividing by it collapsed every float random to fMin. The weather
	// was the visible casualty - RandomizeSnowFlake/RandomizeRainDrop place
	// every particle with this overload, so all of them landed on the map
	// origin and neither snow nor rain was ever on screen.
	static const unsigned int RANDOM_MAX = 0x7fff;
	void Seed( const int nSeed );
	unsigned int Random();
	__forceinline unsigned int Random( const unsigned int uMax ) { return NWin32Random::Random() % uMax; }
	__forceinline int Random( const int nMin, const int nMax ) { return nMin + (int)NWin32Random::Random( (const unsigned int)(nMax - nMin + 1) ); }
	__forceinline float Random( const float fMin, const float fMax ) { return fMin + float( float(NWin32Random::Random()) / float(NWin32Random::RANDOM_MAX) * (fMax - fMin) ); }

	__forceinline unsigned int RandomCheck( const unsigned int uMax ) { return uMax == 0 ? 0 : NWin32Random::Random( uMax ); }
	__forceinline int RandomCheck( const int nMin, const int nMax ) { return nMax < nMin ? nMin : NWin32Random::Random( nMin, nMax ); }
	__forceinline float RandomCheck( const float fMin, const float fMax ) { return fMax < fMin ? fMin : NWin32Random::Random( fMin, fMax ); }
};
#endif // __WIN32RANDOM_H__
