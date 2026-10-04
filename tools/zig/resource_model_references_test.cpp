// T04 test for Sources/src/ResourceModel/{references,combos,localization}.
// Builds a References over the tracked fixture tree and asserts each of the 20
// EReferenceType lists has its expected count; also checks the AI-class and
// player-sides combos and the three-file localization read. One stderr line
// per type: "REF <name> count=<n>".

#include <cstdio>
#include <cstring>
#include <filesystem>

#include "../../Sources/src/ResourceModel/combos.h"
#include "../../Sources/src/ResourceModel/localization.h"
#include "../../Sources/src/ResourceModel/references.h"

using namespace NResourceModel;

static int g_fail = 0;
#define CHECK( c ) do { if ( !( c ) ) { std::fprintf( stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while ( 0 )

int main( int argc, char **argv )
{
	std::filesystem::path root = argc > 1 ? argv[1] : "tools/zig/fixtures/resource_editor/references_root";
	References refs;
	std::size_t total = refs.rebuild( root );
	// E_ACTIONS_REF is a MultiSelDialog with no file-system list, so it is 0.
	const std::size_t expected[kReferenceTypeCount] = { 3, 3, 3, 3, 3, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
	std::size_t sum = 0;
	for ( int i = 0; i < kReferenceTypeCount; ++i )
	{
		EReferenceType t = (EReferenceType)i;
		std::fprintf( stderr, "REF %s count=%zu\n", ReferenceTypeName( t ), refs.count( t ) );
		CHECK( refs.count( t ) == expected[i] );
		sum += expected[i];
	}
	CHECK( total == sum );
	CHECK( refs.enumerate( EReferenceType::E_ANIMATIONS_REF ).front() == "a" );
	CHECK( refs.enumerate( EReferenceType::E_ASKS_REF ).front() == "sounds\\ack\\grp\\a" );
	CHECK( refs.enumerate( EReferenceType::E_ACTIONS_REF ).empty() );
	// Missing root: every list empty, no crash.
	References none;
	CHECK( none.rebuild( root / "does-not-exist" ) == 0 );

	CHECK( aiClasses().size() == 4 && aiClasses()[0] == "wheel" && aiClasses()[3] == "human" );
	CHECK( playerSides().size() > 0 );
	CHECK( readPlayerSides( root / "partys.xml" ) == playerSides() );
	std::fprintf( stderr, "COMBO ai=%zu sides=%zu\n", aiClasses().size(), playerSides().size() );

	SLocalizationItem loc;
	CHECK( loadLocalization( root / "locale", &loc ) );
	CHECK( loc.hasStats && loc.stats == "Stats\n" );
	CHECK( loc.name == "Name \xe9\r\n" );  // raw bytes, no re-encoding
	CHECK( !loadLocalization( root / "nope", &loc ) );

	std::fprintf( stderr, g_fail ? "resource-model-references FAILED (%d)\n" : "resource-model-references OK\n", g_fail );
	return g_fail ? 1 : 0;
}
