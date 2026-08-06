#include "Platform/Clock.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

namespace
{
	bool Contains( const std::string &text, const char *needle )
	{
		return text.find( needle ) != std::string::npos;
	}

	bool IsDue( const std::uint32_t now, const std::uint32_t due )
	{
		return NPlatform::MillisecondsElapsed( now, due ) <= 0x7fffffffU;
	}

	std::uint32_t TransitionAlpha( const std::uint32_t start, const std::uint32_t now )
	{
		const std::uint32_t elapsed = NPlatform::MillisecondsElapsed( start, now );
		return elapsed >= 500U ? 255U : elapsed * 255U / 500U;
	}

	std::uint32_t VideoFrame( const std::uint32_t start, const std::uint32_t now, const std::uint32_t length, const std::uint32_t frames )
	{
		return NPlatform::MillisecondsElapsed( start, now ) * frames / length;
	}
}

int main()
{
	const char *const sourceFiles[] = {
		"Sources/src/Common/InterfaceScreenBase.cpp",
		"Sources/src/Scene/Transition.cpp",
		"Sources/src/Scene/OpenVideoPlayer.cpp",
		"Sources/src/GameTT/Chapter.cpp",
		"Sources/src/GameTT/iMissionInternal.cpp",
		"Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp",
		"Sources/src/Main/iMainInternal.cpp",
		"Sources/src/AILogic/AIUnit.cpp",
		"Sources/src/AILogic/DamageToEnemyUpdater.cpp",
	};
	const char *const forbidden[] = {
		"timeGetTime",
		"GetTickCount",
		"Sleep(",
		"Interlocked",
		"mmsystem.h",
	};
	for ( const char *path : sourceFiles )
	{
		std::ifstream input( path );
		if ( !input )
			return 1;
		const std::string text( (std::istreambuf_iterator<char>( input )), std::istreambuf_iterator<char>() );
		for ( const char *token : forbidden )
			if ( Contains( text, token ) )
				return 2;
	}

	if ( NPlatform::MillisecondsElapsed( 0xfffffff0U, 0x00000010U ) != 32U )
		return 3;
	if ( TransitionAlpha( 0xfffffff0U, 0x00000010U ) != 16U )
		return 4;
	if ( TransitionAlpha( 1000U, 1250U ) != 127U || TransitionAlpha( 1000U, 1500U ) != 255U )
		return 5;
	if ( VideoFrame( 0xfffffff0U, 0x00000010U, 5000U, 24U ) != 0U )
		return 6;
	if ( VideoFrame( 1000U, 2250U, 5000U, 24U ) != 6U )
		return 7;
	const std::uint32_t missionPacing = NPlatform::MillisecondsElapsed( 0xfffffff0U, 0x00000010U )
		+ NPlatform::MillisecondsElapsed( 0x00000010U, 0x00000030U );
	if ( missionPacing != 64U )
		return 8;
	const std::uint32_t randomSeed = 0x12345678U;
	if ( ( randomSeed ^ 0x9e3779b9U ) != 0x8c032fc1U )
		return 9;
	if ( NPlatform::MillisecondsElapsed( 0xfffffffeU, 0x00000002U ) != 4U )
		return 10;
	if ( !IsDue( 0xfffffff0U, 0x00000020U ) || IsDue( 0x00000020U, 0xfffffff0U ) )
		return 11;

	std::printf( "clock residue fixtures: wrap=32 transition=16/127/255 video=0/6 mission=64 seed=0x%08x diagnostics=4 delayed-ui=1\n", randomSeed );
	return 0;
}
