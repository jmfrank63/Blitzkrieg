// test-resource-model-comparator: the D-11 comparator (Sources/src/ResourceModel/
// comparator.*) proved on the shipped game data, then the golden comparison.
//
//   1. Every shipped stats file the Resource Editor's kinds export is copied
//      into the scratch folder and compared with itself through the engine's
//      own reader: objects.xml's objects (the GameDB::GetRPGStats switch, sound
//      excepted: no sub-editor exports sounds), the weapons, medals, missions,
//      chapters, campaigns, effects, particles, tilesets, crossets, 3D roads
//      and rivers. Each must be EQUAL with every node read: a real MFC export
//      has no unknown field.
//   2. Planted changes to a copy must fail with a precise message: a float one
//      ulp off, a dropped field, an extra field the reader does not read, a
//      byte of an _h.dds, a DXT header and a DXT pixel block.
//   3. The golden comparison: each fixture's golden/ folder against the port's
//      export. Goldens come from MFC editor.exe on win-home
//      (tools/zig/win-home/export-goldens.ps1). An extension without them is
//      "pending: golden missing", never PASS.
//
// The log, one line per check, is <scratch>/resource_model/comparator.log.
//
// argv: <installation> <scratch> <repo Data> <fixture root>
#include "StdAfx.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include "../../Sources/src/ResourceModel/comparator.h"
#include "../../Sources/src/ResourceModel/xml.h"

namespace fs = std::filesystem;
using namespace NResourceModel;

static int g_nFailures = 0;
static std::string g_szLog;

static void Log( const std::string &szLine )
{
	std::printf( "%s\n", szLine.c_str() );
	g_szLog += szLine + "\n";
}

static bool Check( bool bCondition, const std::string &szWhat )
{
	Log( std::string( bCondition ? "PASS " : "FAIL " ) + szWhat );
	if ( !bCondition )
		++g_nFailures;
	return bCondition;
}

static bool ReadBytes( const fs::path &path, std::string *pBytes )
{
	std::ifstream file( path, std::ios::binary );
	if ( !file )
		return false;
	pBytes->assign( std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() );
	return true;
}

static bool WriteBytes( const fs::path &path, const std::string &bytes )
{
	std::error_code error;
	fs::create_directories( path.parent_path(), error );
	std::ofstream file( path, std::ios::binary | std::ios::trunc );
	file.write( bytes.data(), static_cast<std::streamsize>( bytes.size() ) );
	return file.good();
}

static bool SameNoCase( const std::string &a, const std::string &b )
{
	return a.size() == b.size() && std::equal( a.begin(), a.end(), b.begin(),
	       []( char x, char y ) { return tolower( ( unsigned char )x ) == tolower( ( unsigned char )y ); } );
}

// A storage name (backslashes, any case) as the disk under root spells it;
// empty when a component is not there.
static fs::path FindNoCase( const fs::path &root, const std::string &szName )
{
	fs::path path = root;
	std::string::size_type nStart = 0;
	while ( nStart < szName.size() )
	{
		std::string::size_type nEnd = szName.find_first_of( "\\/", nStart );
		if ( nEnd == std::string::npos )
			nEnd = szName.size();
		const std::string szPart = szName.substr( nStart, nEnd - nStart );
		nStart = nEnd + 1;
		if ( szPart.empty() )
			continue;
		std::error_code error;
		if ( fs::exists( path / szPart, error ) )
		{
			path /= szPart;
			continue;
		}
		bool bFound = false;
		for ( fs::directory_iterator it( path, error ), end; !error && it != end; it.increment( error ) )
			if ( SameNoCase( it->path().filename().string(), szPart ) )
			{
				path = it->path();
				bFound = true;
				break;
			}
		if ( !bFound )
			return fs::path();
	}
	return path;
}

struct SShipped
{
	EExportKind kind;
	fs::path file;   // relative to Data
};

// Every 1.xml (or every *.xml) under Data/<dir>, any depth.
static void AddTree( const fs::path &data, const char *pszDir, bool bOnlyOne, EExportKind kind, std::vector<SShipped> *pFiles )
{
	const fs::path root = FindNoCase( data, pszDir );
	if ( root.empty() )
	{
		Check( false, std::string( "shipped Data has " ) + pszDir );
		return;
	}
	std::vector<fs::path> found;
	std::error_code error;
	for ( fs::recursive_directory_iterator it( root, error ), end; !error && it != end; it.increment( error ) )
	{
		if ( !it->is_regular_file() )
			continue;
		const std::string szName = it->path().filename().string();
		if ( bOnlyOne ? SameNoCase( szName, "1.xml" ) : SameNoCase( it->path().extension().string(), ".xml" ) )
			found.push_back( fs::relative( it->path(), data ) );
	}
	std::sort( found.begin(), found.end() );
	for ( const fs::path &file : found )
		pFiles->push_back( { kind, file } );
}

static std::string Text( const NResourceXml::Node &item, const char *pszName )
{
	const NResourceXml::Node *pChild = NResourceXml::FindChild( item, pszName );
	if ( !pChild )
		return std::string();
	std::string szText = pChild->text;
	for ( const NResourceXml::Node &child : pChild->children )
		if ( child.kind == NResourceXml::Node::Text )
			szText += child.text;
	return szText;
}

// objects.xml through CObjectsDB::GetRPGStats's switch: which struct, and the
// object's <path>\1.xml.
static void AddObjects( const fs::path &data, std::vector<SShipped> *pFiles )
{
	std::string bytes;
	NResourceXml::Document doc;
	std::string szError;
	if ( !Check( ReadBytes( data / "objects.xml", &bytes ) && NResourceXml::Parse( bytes, doc, szError ), "objects.xml reads " + szError ) )
		return;
	const NResourceXml::Node *pObjects = NResourceXml::FindChild( doc.root, "Objects" );
	if ( !Check( pObjects != 0, "objects.xml has <Objects>" ) )
		return;
	int nMissing = 0;
	for ( const NResourceXml::Node &item : pObjects->children )
	{
		if ( item.kind != NResourceXml::Node::Element )
			continue;
		const std::string szGame = Text( item, "game_type" ), szVis = Text( item, "type" );
		EExportKind kind = EExportKind::OBJECT;
		if ( szGame == "sound" )
			continue;
		else if ( szGame == "unit" )
			kind = szVis == "sprite" ? EExportKind::INFANTRY : EExportKind::MECH_UNIT;
		else if ( szGame == "tank_pit" )
			kind = EExportKind::MECH_UNIT;
		else if ( szGame == "building" || szGame == "fortification" )
			kind = EExportKind::BUILDING;
		else if ( szGame == "fence" )
			kind = EExportKind::FENCE;
		else if ( szGame == "entrenchment" )
			kind = EExportKind::ENTRENCHMENT;
		else if ( szGame == "bridge" )
			kind = EExportKind::BRIDGE;
		else if ( szGame == "mine" )
			kind = EExportKind::MINE;
		else if ( szGame == "squad" )
			kind = EExportKind::SQUAD;
		const fs::path file = FindNoCase( data, Text( item, "path" ) + "\\1.xml" );
		if ( file.empty() )
		{
			// GetRPGStats tolerates an object without a stats file
			// (dessau.bzm's Logs08); there is nothing to compare.
			++nMissing;
			continue;
		}
		pFiles->push_back( { kind, fs::relative( file, data ) } );
	}
	Log( "objects.xml: " + std::to_string( nMissing ) + " objects have no stats file in Data" );
}

static void ShippedSelfCompare( const fs::path &data, const fs::path &scratch, std::map<EExportKind, fs::path> *pSamples )
{
	std::vector<SShipped> files;
	AddObjects( data, &files );
	// GetAddStats( WEAPON ): weapons\<name>.xml.
	AddTree( data, "Weapons", false, EExportKind::WEAPON, &files );
	AddTree( data, "Medals", true, EExportKind::MEDAL, &files );
	AddTree( data, "Scenarios\\ScenarioMissions", true, EExportKind::MISSION, &files );
	AddTree( data, "Scenarios\\TemplateMissions", true, EExportKind::MISSION, &files );
	AddTree( data, "Scenarios\\Chapters", true, EExportKind::CHAPTER, &files );
	// Campaigns are Scenarios\Campaigns\<side>\<side>.xml, not 1.xml.
	AddTree( data, "Scenarios\\Campaigns", false, EExportKind::CAMPAIGN, &files );
	AddTree( data, "Effects\\Effects", false, EExportKind::EFFECT, &files );
	AddTree( data, "Effects\\Particles", false, EExportKind::PARTICLE, &files );
	const fs::path sets = FindNoCase( data, "Terrain\\sets" );
	std::error_code error;
	for ( fs::directory_iterator it( sets, error ), end; !error && it != end; it.increment( error ) )
	{
		if ( !it->is_directory() )
			continue;
		const std::string szSet = "Terrain\\sets\\" + it->path().filename().string();
		const fs::path tileset = FindNoCase( data, szSet + "\\tileset.xml" ), crosset = FindNoCase( data, szSet + "\\crosset.xml" );
		if ( !tileset.empty() )
			files.push_back( { EExportKind::TILESET, fs::relative( tileset, data ) } );
		if ( !crosset.empty() )
			files.push_back( { EExportKind::CROSSET, fs::relative( crosset, data ) } );
		if ( !FindNoCase( data, szSet + "\\Roads3D" ).empty() )
			AddTree( data, ( szSet + "\\Roads3D" ).c_str(), false, EExportKind::VSO, &files );
		if ( !FindNoCase( data, szSet + "\\Rivers" ).empty() )
			AddTree( data, ( szSet + "\\Rivers" ).c_str(), false, EExportKind::VSO, &files );
	}

	std::map<EExportKind, int> counted, failed, fields, stale;
	int nShown = 0;
	for ( const SShipped &shipped : files )
	{
		// Read from a copy: shipped Data is read-only for every tier.
		const fs::path copy = scratch / "shipped" / shipped.file;
		std::string bytes;
		if ( !ReadBytes( data / shipped.file, &bytes ) || !WriteBytes( copy, bytes ) )
		{
			Check( false, "copy " + shipped.file.generic_string() );
			continue;
		}
		const SCompareResult result = CompareStats( shipped.kind, copy.string(), copy.string() );
		++counted[shipped.kind];
		fields[shipped.kind] += result.nFieldsCompared;
		stale[shipped.kind] += result.nStale;
		if ( result.status != ECompareStatus::EQUAL || result.nFieldsCompared == 0 )
		{
			++failed[shipped.kind];
			if ( nShown++ < 40 )
			{
				Log( std::string( "  " ) + CompareStatusName( result.status ) + " " + shipped.file.generic_string() + " fields=" + std::to_string( result.nFieldsCompared ) );
				for ( size_t i = 0; i < result.messages.size() && i < 6; ++i )
					Log( "    " + result.messages[i] );
			}
		}
		else if ( pSamples->find( shipped.kind ) == pSamples->end() )
			( *pSamples )[shipped.kind] = copy;
	}
	for ( EExportKind kind : AllExportKinds() )
	{
		const SExportKindInfo &info = GetExportKindInfo( kind );
		Check( counted[kind] > 0 && failed[kind] == 0,
		       std::string( "shipped " ) + info.pszName + " via " + info.pszReader + ": " + std::to_string( counted[kind] ) + " files equal to themselves, " +
		       std::to_string( failed[kind] ) + " not, " + std::to_string( fields[kind] ) + " fields, " + std::to_string( stale[kind] ) + " stale nodes" );
	}
}

static bool HasMessage( const SCompareResult &result, const std::string &szNeedle )
{
	for ( const std::string &szMessage : result.messages )
		if ( szMessage.find( szNeedle ) != std::string::npos )
			return true;
	return false;
}

static std::string Messages( const SCompareResult &result )
{
	std::string szAll;
	for ( size_t i = 0; i < result.messages.size() && i < 3; ++i )
		szAll += " | " + result.messages[i];
	return szAll;
}

// Replaces the first Name="value" attribute of the element <szElement ...>.
static bool EditAttribute( std::string *pXml, const std::string &szElement, const std::string &szName, const std::string *pNewValue, std::string *pOldValue )
{
	const std::string::size_type nElement = pXml->find( "<" + szElement + " " );
	if ( nElement == std::string::npos )
		return false;
	const std::string::size_type nEnd = pXml->find( '>', nElement );
	const std::string::size_type nAttr = pXml->find( " " + szName + "=\"", nElement );
	if ( nAttr == std::string::npos || nAttr > nEnd )
		return false;
	const std::string::size_type nValue = nAttr + szName.size() + 3;
	const std::string::size_type nClose = pXml->find( '"', nValue );
	*pOldValue = pXml->substr( nValue, nClose - nValue );
	if ( pNewValue )
		pXml->replace( nValue, nClose - nValue, *pNewValue );
	else
		pXml->erase( nAttr, nClose + 1 - nAttr );
	return true;
}

static void PlantedChanges( const fs::path &scratch, const std::map<EExportKind, fs::path> &samples )
{
	const auto weapon = samples.find( EExportKind::WEAPON );
	if ( !Check( weapon != samples.end(), "a shipped weapon to plant changes in" ) )
		return;
	std::string original;
	ReadBytes( weapon->second, &original );
	const fs::path planted = scratch / "planted" / "weapon.xml";

	// A float one ulp off: RPG/Dispersion is a float field of SWeaponRPGStats.
	{
		std::string xml = original, szOld;
		EditAttribute( &xml, "RPG", "Dispersion", 0, &szOld );
		const float fNext = std::nextafter( static_cast<float>( std::atof( szOld.c_str() ) ), 1e30f );
		char buffer[64];
		std::snprintf( buffer, sizeof( buffer ), "%.9g", fNext );
		const std::string szNew = buffer;
		xml = original;
		Check( EditAttribute( &xml, "RPG", "Dispersion", &szNew, &szOld ) && WriteBytes( planted, xml ), "plant Dispersion " + szOld + " -> " + szNew );
		const SCompareResult result = CompareStats( EExportKind::WEAPON, planted.string(), weapon->second.string() );
		Check( result.status == ECompareStatus::DIFFERENT && HasMessage( result, "field RPG/Dispersion: port " ) && HasMessage( result, "float 0x" ),
		       std::string( "one-ulp float change fails: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
	// A dropped field: RPG/RangeMax removed; the struct keeps its default.
	{
		std::string xml = original, szOld;
		Check( EditAttribute( &xml, "RPG", "RangeMax", 0, &szOld ) && WriteBytes( planted, xml ), "plant: drop RPG/RangeMax" );
		const SCompareResult result = CompareStats( EExportKind::WEAPON, planted.string(), weapon->second.string() );
		Check( result.status == ECompareStatus::DIFFERENT && HasMessage( result, "dropped field RPG/RangeMax" ),
		       std::string( "dropped field fails: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
	// An extra field no reader reads, on either side.
	{
		std::string xml = original;
		const std::string::size_type nAt = xml.find( "<KeyName>" );
		xml.insert( nAt, "<SomethingTheEngineDoesNotRead>1</SomethingTheEngineDoesNotRead>" );
		Check( nAt != std::string::npos && WriteBytes( planted, xml ), "plant: extra RPG/SomethingTheEngineDoesNotRead" );
		const SCompareResult port = CompareStats( EExportKind::WEAPON, planted.string(), weapon->second.string() );
		Check( port.status == ECompareStatus::UNKNOWN_FIELD && HasMessage( port, "UNKNOWN FIELD port RPG/SomethingTheEngineDoesNotRead" ),
		       std::string( "extra unknown field in the port export fails: " ) + CompareStatusName( port.status ) + Messages( port ) );
		const SCompareResult golden = CompareStats( EExportKind::WEAPON, weapon->second.string(), planted.string() );
		Check( golden.status == ECompareStatus::UNKNOWN_FIELD && HasMessage( golden, "UNKNOWN FIELD golden RPG/SomethingTheEngineDoesNotRead" ),
		       std::string( "extra unknown field in the golden fails: " ) + CompareStatusName( golden.status ) + Messages( golden ) );
	}
	// A stale node (comparator.cpp kStaleFields) on one side only: the
	// particle's old KeyData/Position, taken out of or put into a shipped copy.
	const auto particle = samples.find( EExportKind::PARTICLE );
	if ( Check( particle != samples.end(), "a shipped particle to plant a stale node in" ) )
	{
		std::string xml;
		ReadBytes( particle->second, &xml );
		const std::string::size_type nAt = xml.find( "<Position " );
		const bool bHad = nAt != std::string::npos;
		if ( bHad )
			xml.erase( nAt, xml.find( "/>", nAt ) + 2 - nAt );
		else
			xml.insert( xml.find( "<TextureName>" ), "<Position x=\"0\" y=\"0\" z=\"0\"/>" );
		const fs::path stale = scratch / "planted" / "particle.xml";
		WriteBytes( stale, xml );
		const SCompareResult result = CompareStats( EExportKind::PARTICLE, stale.string(), particle->second.string() );
		Check( result.status == ECompareStatus::DIFFERENT &&
		           HasMessage( result, bHad ? "stale field KeyData/Position: in the golden, not in the port export" : "stale field KeyData/Position: in the port export, not in the golden" ),
		       std::string( "a stale node on one side only fails: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
	// A wrong kind. GetGameStats<SMedalStats> opens a weapon's <RPG> without
	// complaint and reads none of it, so every weapon field is unknown; the
	// effect reader's <effect> base is not there at all.
	{
		const SCompareResult medal = CompareStats( EExportKind::MEDAL, weapon->second.string(), weapon->second.string() );
		Check( medal.status == ECompareStatus::UNKNOWN_FIELD && HasMessage( medal, "UNKNOWN FIELD port RPG/" ),
		       std::string( "a weapon read as a medal fails with unknown fields: " ) + CompareStatusName( medal.status ) + Messages( medal ) );
		const SCompareResult effect = CompareStats( EExportKind::EFFECT, weapon->second.string(), weapon->second.string() );
		Check( effect.status == ECompareStatus::UNREADABLE && HasMessage( effect, "opens <effect>" ),
		       std::string( "a weapon read as an effect is UNREADABLE: " ) + CompareStatusName( effect.status ) + Messages( effect ) );
	}
}

static fs::path FirstFile( const fs::path &data, const std::string &szSuffix )
{
	std::vector<fs::path> found;
	std::error_code error;
	for ( fs::recursive_directory_iterator it( FindNoCase( data, "Units" ), error ), end; !error && it != end; it.increment( error ) )
	{
		std::string szName = it->path().filename().string();
		std::transform( szName.begin(), szName.end(), szName.begin(), []( unsigned char c ) { return static_cast<char>( tolower( c ) ); } );
		if ( it->is_regular_file() && szName.size() > szSuffix.size() && szName.compare( szName.size() - szSuffix.size(), szSuffix.size(), szSuffix ) == 0 )
			found.push_back( it->path() );
	}
	std::sort( found.begin(), found.end() );
	return found.empty() ? fs::path() : found.front();
}

static void BytesAndDxt( const fs::path &data, const fs::path &scratch )
{
	std::string bytes;
	const fs::path height = FirstFile( data, "_h.dds" );
	if ( Check( !height.empty() && ReadBytes( height, &bytes ) && WriteBytes( scratch / "bytes" / "a_h.dds", bytes ), "a shipped _h.dds: " + height.generic_string() ) )
	{
		const fs::path copy = scratch / "bytes" / "a_h.dds";
		Check( CompareBytes( copy.string(), copy.string() ).status == ECompareStatus::EQUAL, "_h.dds equal to itself" );
		bytes[bytes.size() / 2] ^= 1;
		WriteBytes( scratch / "bytes" / "b_h.dds", bytes );
		const SCompareResult result = CompareBytes( ( scratch / "bytes" / "b_h.dds" ).string(), copy.string() );
		Check( result.status == ECompareStatus::DIFFERENT && HasMessage( result, "offset " + std::to_string( bytes.size() / 2 ) ),
		       std::string( "_h.dds with one bit flipped fails at its offset: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
	const fs::path colour = FirstFile( data, "_c.dds" );
	if ( Check( !colour.empty() && ReadBytes( colour, &bytes ) && bytes.size() > 160 && WriteBytes( scratch / "bytes" / "a_c.dds", bytes ), "a shipped _c.dds: " + colour.generic_string() ) )
	{
		const fs::path copy = scratch / "bytes" / "a_c.dds";
		Check( CompareDxt( copy.string(), copy.string() ).status == ECompareStatus::EQUAL, "_c.dds equal to itself" );
		std::string pixels = bytes;
		pixels[128 + 4] ^= 0x55;
		WriteBytes( scratch / "bytes" / "pixels_c.dds", pixels );
		const SCompareResult pending = CompareDxt( ( scratch / "bytes" / "pixels_c.dds" ).string(), copy.string() );
		Check( pending.status == ECompareStatus::PENDING_DXT_GATE, std::string( "_c.dds with a changed block is PENDING_DXT_GATE, not EQUAL: " ) + CompareStatusName( pending.status ) );
		std::string header = bytes;
		header[28] = static_cast<char>( header[28] + 1 );
		WriteBytes( scratch / "bytes" / "mips_c.dds", header );
		const SCompareResult mips = CompareDxt( ( scratch / "bytes" / "mips_c.dds" ).string(), copy.string() );
		Check( mips.status == ECompareStatus::DIFFERENT && HasMessage( mips, "DDS mip count" ), std::string( "_c.dds with another mip count fails: " ) + CompareStatusName( mips.status ) + Messages( mips ) );
	}
}

// The golden comparison. A golden folder holds MFC's export of the fixture
// project; the port's export of the same project is compared with it file by
// file. Neither the goldens (win-home) nor the port's exporter exist on this
// machine yet: report pending and do not pass.
static void Goldens( const fs::path &fixtures )
{
	static const char *const kExtensions[] = { "wpn", "mcp", "trc", "scp", "spt", "unt", "msh", "obt", "fnc", "bld", "bdg",
	                                           "pcp", "eff", "til", "3rd", "3rv", "mip", "chc", "cgc", "mdc" };
	int nPending = 0;
	for ( const char *pszExt : kExtensions )
	{
		const fs::path golden = fixtures / pszExt / "golden";
		int nFiles = 0;
		std::error_code error;
		for ( fs::recursive_directory_iterator it( golden, error ), end; !error && it != end; it.increment( error ) )
		{
			const std::string szName = it->path().filename().string();
			if ( it->is_regular_file() && szName != ".gitkeep" && szName != "README.md" )
				++nFiles;
		}
		++nPending;
		if ( nFiles == 0 )
			Log( std::string( "GOLDEN " ) + pszExt + " pending: golden missing (run tools/zig/win-home/export-goldens.ps1 on win-home)" );
		else
			Log( std::string( "GOLDEN " ) + pszExt + " pending: " + std::to_string( nFiles ) + " golden files, the port has no exporter to compare them with yet" );
	}
	Log( "GOLDEN_SUMMARY extensions=20 pass=0 pending=" + std::to_string( nPending ) );
}

int main( int argc, char **argv )
{
	if ( argc < 5 )
	{
		std::printf( "usage: %s <installation> <scratch> <repo Data> <fixture root>\n", argv[0] );
		return 2;
	}
	const fs::path scratch = fs::path( argv[2] ) / "resource_model" / "comparator";
	const fs::path data = argv[3], fixtures = argv[4];
	std::error_code error;
	fs::remove_all( scratch, error );
	fs::create_directories( scratch, error );

	std::string szError;
	if ( !Check( StartEngineReaders( &szError ), "the engine's StreamIO loads beside the executable " + szError ) )
		return 1;
	std::map<EExportKind, fs::path> samples;
	ShippedSelfCompare( data, scratch, &samples );
	PlantedChanges( scratch, samples );
	BytesAndDxt( data, scratch );
	Goldens( fixtures );

	Log( g_nFailures == 0 ? "VERDICT=PASS (goldens pending)" : "VERDICT=FAIL failures=" + std::to_string( g_nFailures ) );
	std::ofstream log( fs::path( argv[2] ) / "resource_model" / "comparator.log", std::ios::binary | std::ios::trunc );
	log << g_szLog;
	return g_nFailures == 0 ? 0 : 1;
}
