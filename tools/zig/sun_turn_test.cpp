// The sun's turn (Sources/src/Main/SunTurn.h): the pick a mission makes as it
// starts, the light index the camera's step and the sun's turn give, and the
// turn whose step-0 shadow is laid where the data holds none for a light. The
// header reads global vars through GetGlobalVar, which a module's StdAfx.h
// brings in; this test gives it a table of its own, as remastered_test.cpp
// does.
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

static std::map<std::string, std::string> vars;

static int GetGlobalVar( const char *pszName, int nDefault )
{
	std::map<std::string, std::string>::const_iterator it = vars.find( pszName );
	return it == vars.end() ? nDefault : std::atoi( it->second.c_str() );
}

#include "../../Sources/src/Main/SunTurn.h"

static int failures = 0;

#define CHECK( condition ) \
	do { \
		if ( !( condition ) ) { \
			std::fprintf( stderr, "sun turn check failed (line %d): %s\n", __LINE__, #condition ); \
			++failures; \
		} \
	} while ( false )

int main()
{
	// The pick. Where the player may not turn the camera the sun is the 2003
	// sun, whatever the number and whatever came before.
	for ( unsigned int nRandom = 0; nRandom < 64; ++nRandom )
	{
		for ( int nPrevious = -1; nPrevious < 4; ++nPrevious )
			CHECK( NCameraTurn::PickSunTurn( false, nPrevious, nRandom ) == 0 );
	}
	// The first pick of a process covers all four turns evenly.
	{
		int counts[4] = { 0, 0, 0, 0 };
		for ( unsigned int nRandom = 0; nRandom < 400; ++nRandom )
		{
			const int nSun = NCameraTurn::PickSunTurn( true, -1, nRandom );
			CHECK( nSun >= 0 && nSun <= 3 );
			++counts[nSun & 3];
		}
		for ( int i = 0; i < 4; ++i )
			CHECK( counts[i] == 100 );
	}
	// A later pick is never the one before it, and covers the other three
	// evenly.
	for ( int nPrevious = 0; nPrevious < 4; ++nPrevious )
	{
		int counts[4] = { 0, 0, 0, 0 };
		for ( unsigned int nRandom = 0; nRandom < 300; ++nRandom )
		{
			const int nSun = NCameraTurn::PickSunTurn( true, nPrevious, nRandom );
			CHECK( nSun >= 0 && nSun <= 3 );
			CHECK( nSun != nPrevious );
			++counts[nSun & 3];
		}
		for ( int i = 0; i < 4; ++i )
			CHECK( counts[i] == ( i == nPrevious ? 0 : 100 ) );
	}
	// A chain of picks, as missions follow each other: no two in a row alike,
	// all four seen. The numbers are a fixed sequence, not a generator's.
	{
		int nPrevious = -1;
		bool seen[4] = { false, false, false, false };
		unsigned int nRandom = 12345;
		for ( int i = 0; i < 200; ++i )
		{
			nRandom = nRandom * 1103515245u + 12345u;
			const int nSun = NCameraTurn::PickSunTurn( true, nPrevious, nRandom >> 16 );
			CHECK( nSun != nPrevious );
			seen[nSun & 3] = true;
			nPrevious = nSun;
		}
		CHECK( seen[0] && seen[1] && seen[2] && seen[3] );
	}
	// A previous turn out of range counts as none.
	CHECK( NCameraTurn::PickSunTurn( true, 7, 2 ) == 2 );

	// BK_SUN_TURN: one digit 0 to 3 forces the turn, anything else does not.
	CHECK( NCameraTurn::ForcedSunTurn( 0 ) == -1 );
	CHECK( NCameraTurn::ForcedSunTurn( "" ) == -1 );
	CHECK( NCameraTurn::ForcedSunTurn( "0" ) == 0 );
	CHECK( NCameraTurn::ForcedSunTurn( "1" ) == 1 );
	CHECK( NCameraTurn::ForcedSunTurn( "2" ) == 2 );
	CHECK( NCameraTurn::ForcedSunTurn( "3" ) == 3 );
	CHECK( NCameraTurn::ForcedSunTurn( "4" ) == -1 );
	CHECK( NCameraTurn::ForcedSunTurn( "12" ) == -1 );
	CHECK( NCameraTurn::ForcedSunTurn( "-1" ) == -1 );
	CHECK( NCameraTurn::ForcedSunTurn( "x" ) == -1 );

	// The light index: the step plus the sun's turn, in quarter turns. Under
	// the 2003 sun it is the step, as before the sun turned.
	for ( int nStep = 0; nStep < 4; ++nStep )
	{
		CHECK( NCameraTurn::LightIndex( nStep, 0 ) == nStep );
		for ( int nSun = 0; nSun < 4; ++nSun )
		{
			CHECK( NCameraTurn::LightIndex( nStep, nSun ) == ( nStep + nSun ) % 4 );
			// A full turn of the camera under one sun shows every light once.
			CHECK( NCameraTurn::LightIndex( ( nStep + 2 ) & 3, nSun ) == ( NCameraTurn::LightIndex( nStep, nSun ) + 2 ) % 4 );
		}
	}
	// The maintainer's case: looking north with the light from the left (light
	// index 0), a quarter turn of the camera looks against the light or with
	// it (1 or 3), the half turn has it from the right (2).
	CHECK( NCameraTurn::LightIndex( 1, 0 ) == 1 && NCameraTurn::LightIndex( 2, 0 ) == 2 && NCameraTurn::LightIndex( 3, 0 ) == 3 );

	// The sun's turn through the global vars: the mission's where the player's
	// Remastered option holds, the 2003 sun without it.
	vars.clear();
	CHECK( !NCameraTurn::Allowed() );
	CHECK( NCameraTurn::SunTurn() == 0 );
	vars[NCameraTurn::pszSunVar] = "2";
	CHECK( NCameraTurn::SunTurn() == 0 );
	CHECK( NCameraTurn::LightIndex( 1 ) == 1 );
	vars[NRemastered::pszOptionVar] = "1";
	CHECK( NCameraTurn::Allowed() );
	CHECK( NCameraTurn::SunTurn() == 2 );
	CHECK( NCameraTurn::LightIndex( 1 ) == 3 );
	CHECK( NCameraTurn::LightIndex( 3 ) == 1 );
	// It is the player's own, so multiplayer and a replay keep it, as they keep
	// the camera's step.
	vars["MultiplayerGame"] = "1";
	CHECK( NCameraTurn::SunTurn() == 2 );
	vars.erase( "MultiplayerGame" );
	vars["History.Playing"] = "1";
	CHECK( NCameraTurn::SunTurn() == 2 );
	vars.erase( "History.Playing" );
	vars[NCameraTurn::pszSunVar] = "7";
	CHECK( NCameraTurn::SunTurn() == 3 );
	vars.erase( NCameraTurn::pszSunVar );
	CHECK( NCameraTurn::SunTurn() == 0 );

	// The laid shadow's turn: turn k = facing + step under light l = step + sun
	// lays the shadow of turn facing - sun, whatever the step.
	for ( int nFacing = 0; nFacing < 4; ++nFacing )
	{
		for ( int nSun = 0; nSun < 4; ++nSun )
		{
			for ( int nStep = 0; nStep < 4; ++nStep )
				CHECK( NCameraTurn::LaidShadowTurn( ( nFacing + nStep ) & 3, NCameraTurn::LightIndex( nStep, nSun ) ) == ( nFacing - nSun + 4 ) % 4 );
		}
	}

	// The sun's direction: turn t is the 2003 sun turned t times by
	// (x, y) -> (-y, x); four turns bring it back.
	{
		float fX = 1, fY = 1;
		NCameraTurn::TurnSun( &fX, &fY, 0 );
		CHECK( fX == 1 && fY == 1 );
		NCameraTurn::TurnSun( &fX, &fY, 1 );
		CHECK( fX == -1 && fY == 1 );
		NCameraTurn::TurnSun( &fX, &fY, 1 );
		CHECK( fX == -1 && fY == -1 );
		NCameraTurn::TurnSun( &fX, &fY, 2 );
		CHECK( fX == 1 && fY == 1 );
		float fA = 3, fB = -2;
		NCameraTurn::TurnSun( &fA, &fB, 3 );
		CHECK( fA == -2 && fB == -3 );
		double fC = 0.5, fD = 0.25;
		NCameraTurn::TurnSun( &fC, &fD, 5 );
		CHECK( fC == -0.25 && fD == 0.5 );
	}

	// The ground's shade under a turned sun (the terrain's vertices). The 2003
	// sun, (1, 1, -2) normalised, and three normals: flat, leaning towards the
	// sun's side (-x, -y) and away from it.
	{
		const float fSun = 0.40824829f, fSunZ = -0.81649658f;
		const float fLean = 0.2f, fUp = 0.9591663f;				// (0.2, 0.2, up) and its mirror are unit normals
		// The light model: the cosine held between 0.6 and 1.
		CHECK( NCameraTurn::GroundLight( 0, 0, 1, fSun, fSun, fSunZ ) > 0.816f && NCameraTurn::GroundLight( 0, 0, 1, fSun, fSun, fSunZ ) < 0.817f );
		CHECK( NCameraTurn::GroundLight( 0, 0, 1, 0, 0, -1 ) == 1.0f );
		CHECK( NCameraTurn::GroundLight( 1, 0, 0, 1, 0, 0 ) == 0.6f );
		const float fLit = NCameraTurn::GroundLight( -fLean, -fLean, fUp, fSun, fSun, fSunZ );
		const float fShaded = NCameraTurn::GroundLight( fLean, fLean, fUp, fSun, fSun, fSunZ );
		CHECK( fLit > 0.94f && fShaded < 0.63f );
		for ( int nBaked = 0; nBaked < 256; nBaked += 15 )
		{
			// Turn 0 keeps the map's shade, whatever the normal; so does flat
			// ground at every turn.
			CHECK( NCameraTurn::TurnedShade( nBaked, -fLean, -fLean, fUp, fSun, fSun, fSunZ, 0 ) == nBaked );
			CHECK( NCameraTurn::TurnedShade( nBaked, fLean, fLean, fUp, fSun, fSun, fSunZ, 4 ) == nBaked );
			for ( int nSun = 1; nSun < 4; ++nSun )
				CHECK( NCameraTurn::TurnedShade( nBaked, 0, 0, 1, fSun, fSun, fSunZ, nSun ) == nBaked );
		}
		// A slope the map shades as the light model does: under the half turn
		// the lit slope takes the shaded one's shade and the other way round.
		const int nLit = int( fLit * 255.0f ), nShaded = int( fShaded * 255.0f );
		CHECK( abs( NCameraTurn::TurnedShade( nLit, -fLean, -fLean, fUp, fSun, fSun, fSunZ, 2 ) - nShaded ) <= 1 );
		CHECK( abs( NCameraTurn::TurnedShade( nShaded, fLean, fLean, fUp, fSun, fSun, fSunZ, 2 ) - nLit ) <= 1 );
		// At a quarter turn both stand side on to the sun: one shade, between.
		const int nSideA = NCameraTurn::TurnedShade( nLit, -fLean, -fLean, fUp, fSun, fSun, fSunZ, 1 );
		const int nSideB = NCameraTurn::TurnedShade( nShaded, fLean, fLean, fUp, fSun, fSun, fSunZ, 1 );
		CHECK( abs( nSideA - nSideB ) <= 1 && nSideA < nLit && nSideA > nShaded );
		CHECK( NCameraTurn::TurnedShade( nLit, -fLean, -fLean, fUp, fSun, fSun, fSunZ, 3 ) == nSideA );
		// A slope leaning towards (+x, -y) is lit by the sun turned once: its
		// light then travels along (-1, 1).
		CHECK( NCameraTurn::TurnedShade( 200, fLean, -fLean, fUp, fSun, fSun, fSunZ, 1 ) > 200 );
		CHECK( NCameraTurn::TurnedShade( 200, fLean, -fLean, fUp, fSun, fSun, fSunZ, 3 ) < 200 );
		// A shade set by hand keeps its own look and takes only the turn's
		// change, within the model's range (153 to 255) or its own below it.
		CHECK( NCameraTurn::TurnedShade( 208, -fLean, -fLean, fUp, fSun, fSun, fSunZ, 2 ) == 208 - ( nLit - nShaded ) || NCameraTurn::TurnedShade( 208, -fLean, -fLean, fUp, fSun, fSun, fSunZ, 2 ) == 153 );
		CHECK( NCameraTurn::TurnedShade( 250, fLean, fLean, fUp, fSun, fSun, fSunZ, 2 ) == 255 );
		CHECK( NCameraTurn::TurnedShade( 100, -fLean, -fLean, fUp, fSun, fSun, fSunZ, 2 ) == 100 );
		CHECK( NCameraTurn::TurnedShade( 100, fLean, fLean, fUp, fSun, fSun, fSunZ, 2 ) > 100 );
	}

	return failures == 0 ? 0 : 1;
}
