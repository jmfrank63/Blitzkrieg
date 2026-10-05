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
//      byte of an _h.dds and a DXT header.
//   3. The DXT gate: listed shipped _c.dds re-encoded by NDxt are within
//      dxt-tolerance.json; planted colour and alpha regions, lost DXT1
//      punch-through, a truncated file, a changed uncompressed DDS and a
//      missing or malformed tolerance fail precisely.
//   4. The S06 exporters (wpn, mcp, trc, scp): each fixture project is
//      exported through FindExporter and its stats file read back by the
//      engine: no unknown field, and field-equal (CompareStats) with the
//      struct derived by hand from the fixture's values and written by the
//      engine's own writer. Their refusals and warnings name what is missing.
//   5. The golden comparison: each fixture's golden/ folder against the port's
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
#include "../../Sources/src/ResourceModel/exporter.h"
#include "../../Sources/src/ResourceModel/project.h"
#include "../../Sources/src/Main/RPGStats.h"

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
		Check( CompareDxt( copy.string(), copy.string(), SDxtTolerance() ).status == ECompareStatus::EQUAL, "_c.dds equal to itself, even with no tolerance loaded" );
		std::string header = bytes;
		header[28] = static_cast<char>( header[28] + 1 );
		WriteBytes( scratch / "bytes" / "mips_c.dds", header );
		const SCompareResult mips = CompareDxt( ( scratch / "bytes" / "mips_c.dds" ).string(), copy.string(), SDxtTolerance() );
		Check( mips.status == ECompareStatus::DIFFERENT && HasMessage( mips, "DDS mip count" ), std::string( "_c.dds with another mip count fails: " ) + CompareStatusName( mips.status ) + Messages( mips ) );
	}
}

// The DXT gate (S03 T07): dxt-tolerance.json, measured on shipped textures by
// `zig build measure-dxt-tolerance`, bounds the decoded deltas of a port
// _c.dds against its golden. A listed texture re-encoded by NDxt must pass it
// (the gate is the largest per-file measurement), and planted colour and alpha
// changes, a missing or malformed tolerance and a truncated file must fail.
static void DxtGate( const fs::path &data, const fs::path &scratch, const fs::path &fixtures )
{
	const fs::path dir = scratch / "dxt";
	SDxtTolerance tolerance;
	std::string szError;
	if ( !Check( LoadDxtTolerance( ( fixtures / "dxt-tolerance.json" ).string(), &tolerance, &szError ) && tolerance.Find( "DXT1" ) &&
	             tolerance.Find( "DXT3" ) && tolerance.Find( "DXT5" ),
	             "dxt-tolerance.json loads with DXT1/DXT3/DXT5 gates " + szError ) )
		return;
	for ( const auto &bad : { std::make_pair( std::string( "{ \"schema_version\": 1, \"formats\": {} }" ), std::string( "schema_version" ) ),
	                          std::make_pair( std::string( "{ \"schema_version\": 2, \"gate\": { \"DXT5\": { \"colour_max_delta\": 1, \"colour_p99\": 1, \"alpha_max_delta\": 1 } } }" ),
	                                          std::string( "alpha_p99" ) ),
	                          std::make_pair( std::string( "{ \"schema_version\": 2, \"gate\": { } }" ), std::string( "empty" ) ) } )
	{
		WriteBytes( dir / "bad-tolerance.json", bad.first );
		SDxtTolerance rejected;
		Check( !LoadDxtTolerance( ( dir / "bad-tolerance.json" ).string(), &rejected, &szError ) && !rejected.bLoaded && szError.find( bad.second ) != std::string::npos,
		       "a malformed tolerance is refused naming " + bad.second + ": " + szError );
	}
	SDxtTolerance missing;
	Check( !LoadDxtTolerance( ( dir / "missing.json" ).string(), &missing, &szError ) && szError.find( "cannot open" ) != std::string::npos,
	       "a missing tolerance file is refused: " + szError );

	// Listed textures, one per format, alpha in each: DXT1 punch-through, DXT3 explicit, DXT5 interpolated.
	static const char *const kListed[] = { "Terrain\\sets\\1\\tileset_c.dds", "Scenarios\\Chapters\\Allies\\Ardennes\\map_c.dds", "Units\\Humans\\Allies\\Bren\\1_c.dds" };
	for ( const char *pszListed : kListed )
	{
		szError.clear();
		const fs::path original = FindNoCase( data, pszListed );
		std::string bytes, reencoded;
		SDdsImage image;
		if ( !Check( !original.empty() && ReadBytes( original, &bytes ) && DecodeDds( bytes, &image, &szError ), std::string( "a listed _c.dds decodes: " ) + pszListed + " " + szError ) )
			continue;
		const std::string szName = image.szFourCC;
		const fs::path golden = dir / ( szName + "-golden_c.dds" );
		WriteBytes( golden, bytes );
		Check( EncodeDds( bytes, image, &reencoded, &szError ) && reencoded.size() == bytes.size() && WriteBytes( dir / ( szName + "-reencoded_c.dds" ), reencoded ),
		       szName + " re-encodes by NDxt to the same size " + szError );
		const SCompareResult within = CompareDxt( ( dir / ( szName + "-reencoded_c.dds" ) ).string(), golden.string(), tolerance );
		Check( within.status == ECompareStatus::EQUAL && HasMessage( within, "within the " + szName + " gate" ),
		       szName + " re-encoded by NDxt is within its gate: " + CompareStatusName( within.status ) + Messages( within ) );
		const SCompareResult unloaded = CompareDxt( ( dir / ( szName + "-reencoded_c.dds" ) ).string(), golden.string(), SDxtTolerance() );
		Check( reencoded == bytes || unloaded.status == ECompareStatus::UNREADABLE,
		       szName + " pixels that differ with no tolerance loaded are UNREADABLE, never EQUAL: " + CompareStatusName( unloaded.status ) );

		// A 16x16 region pushed to the far end of each channel: colour deltas of at least 128.
		SDdsImage planted = image;
		SDdsMip &mip = planted.mips[0];
		const bool bAlpha = szName != "DXT1";
		for ( int y = 0; y < std::min( 16, mip.nHeight ); ++y )
			for ( int x = 0; x < std::min( 16, mip.nWidth ); ++x )
			{
				unsigned &nPixel = mip.pixels[static_cast<size_t>( y ) * mip.nWidth + x];
				unsigned nFlipped = 0;
				for ( int nShift = 0; nShift < 32; nShift += 8 )
				{
					const unsigned nChannel = ( nPixel >> nShift ) & 0xff;
					const bool bFlip = nShift < 24 || bAlpha;
					nFlipped |= ( bFlip ? ( nChannel < 128 ? 255u : 0u ) : nChannel ) << nShift;
				}
				nPixel = nFlipped;
			}
		std::string szPlanted;
		EncodeDds( bytes, planted, &szPlanted, &szError );
		WriteBytes( dir / ( szName + "-planted_c.dds" ), szPlanted );
		const SCompareResult far = CompareDxt( ( dir / ( szName + "-planted_c.dds" ) ).string(), golden.string(), tolerance );
		Check( far.status == ECompareStatus::DIFFERENT && HasMessage( far, szName + " colour max delta" ) && HasMessage( far, "largest delta" ) &&
		           ( !bAlpha || HasMessage( far, szName + " alpha max delta" ) ),
		       szName + " with a planted 16x16 region fails the gate: " + CompareStatusName( far.status ) + Messages( far ) );

		const std::string truncated = bytes.substr( 0, 128 + ( bytes.size() - 128 ) / 2 );
		WriteBytes( dir / ( szName + "-truncated_c.dds" ), truncated );
		const SCompareResult cut = CompareDxt( ( dir / ( szName + "-truncated_c.dds" ) ).string(), golden.string(), tolerance );
		Check( cut.status == ECompareStatus::UNREADABLE && HasMessage( cut, "mip 0 needs" ), szName + " cut short is UNREADABLE: " + CompareStatusName( cut.status ) + Messages( cut ) );
	}

	// DXT1 punch-through survives NDxt: an alpha-0 pixel that turns opaque is an alpha difference.
	{
		const fs::path original = FindNoCase( data, "Terrain\\sets\\1\\tileset_c.dds" );
		std::string bytes, szOpaque;
		SDdsImage image;
		if ( ReadBytes( original, &bytes ) && DecodeDds( bytes, &image, &szError ) )
		{
			int nCleared = 0;
			for ( unsigned &nPixel : image.mips[0].pixels )
				if ( ( nPixel >> 24 ) == 0 )
				{
					nPixel |= 0xff000000u;
					++nCleared;
				}
			EncodeDds( bytes, image, &szOpaque, &szError );
			WriteBytes( dir / "DXT1-opaque_c.dds", szOpaque );
			const SCompareResult opaque = CompareDxt( ( dir / "DXT1-opaque_c.dds" ).string(), ( dir / "DXT1-golden_c.dds" ).string(), tolerance );
			Check( nCleared > 0 && opaque.status == ECompareStatus::DIFFERENT && HasMessage( opaque, "DXT1 alpha max delta 255 exceeds the gate 0" ),
			       "a DXT1 tileset that lost its punch-through fails the alpha gate: " + std::to_string( nCleared ) + " pixels, " + CompareStatusName( opaque.status ) + Messages( opaque ) );
		}
	}

	// An uncompressed DDS has no pixel tolerance: a changed byte is a difference.
	const fs::path rgb = FindNoCase( data, "UI\\container_c.dds" );
	std::string bytes;
	if ( Check( !rgb.empty() && ReadBytes( rgb, &bytes ) && bytes.size() > 200, "an uncompressed shipped _c.dds: " + rgb.generic_string() ) )
	{
		WriteBytes( dir / "rgb-golden_c.dds", bytes );
		bytes[bytes.size() - 1] ^= 1;
		WriteBytes( dir / "rgb-port_c.dds", bytes );
		const SCompareResult result = CompareDxt( ( dir / "rgb-port_c.dds" ).string(), ( dir / "rgb-golden_c.dds" ).string(), tolerance );
		Check( result.status == ECompareStatus::DIFFERENT && HasMessage( result, "uncompressed" ), std::string( "an uncompressed DDS with one byte changed fails: " ) + CompareStatusName( result.status ) + Messages( result ) );
	}
}

// The S06 exporters over their fixtures. The expected structs are the
// fixture projects' values put through the MFC frames' SaveRPGStats by hand
// (WeaponFrm.cpp, MineFrm.cpp, TrenchFrm.cpp, SquadFrm.cpp), written by the
// engine's own writer, so the comparison is the game's reader on both sides.

template <class TStats>
static bool WriteExpected( const fs::path &file, TStats &stats )
{
	std::error_code error;
	fs::create_directories( file.parent_path(), error );
	CPtr<IDataStorage> pStorage = CreateStorage( ( file.parent_path().string() + "/" ).c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
	CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->CreateStream( file.filename().string().c_str(), STREAM_ACCESS_WRITE ) : 0;
	if ( pStream == 0 )
		return false;
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::WRITE );
	if ( pDT == 0 )
		return false;
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &stats );
	return true;
}

// A copy of a fixture folder at <scratch>/<ext>/<ext>/, so the project's
// folder name, which names the exported file, is the extension.
static fs::path CopyFixture( const fs::path &fixtures, const fs::path &scratch, const std::string &szExt )
{
	const fs::path dir = scratch / szExt / szExt;
	std::error_code error;
	fs::remove_all( scratch / szExt, error );
	fs::create_directories( dir, error );
	for ( fs::directory_iterator it( fixtures / szExt, error ), end; !error && it != end; it.increment( error ) )
		if ( it->is_regular_file() )
			fs::copy_file( it->path(), dir / it->path().filename(), fs::copy_options::overwrite_existing, error );
	return dir / ( "project." + szExt );
}

static bool LoadProject( const fs::path &file, Project *pProject )
{
	std::string szBytes, szError;
	return ReadBytes( file, &szBytes ) && Load( szBytes, *pProject, szError );
}

// The objects database a squad export asks, as a fixture: the one infantry
// unit the scp fixture names, under its path and key in Data/objects.xml.
static bool FixtureUnitKey( const std::string &szPath, std::string &szKey )
{
	if ( szPath != "units\\humans\\ussr\\mosin" )
		return false;
	szKey = "USSR_Mosin";
	return true;
}

struct SExportRun
{
	bool bExported = false;
	SExportOutcome outcome;
	fs::path data;
};

static SExportRun RunExporter( const std::string &szExt, const fs::path &project, const fs::path &data, SExportContext context )
{
	SExportRun run;
	run.data = data;
	std::error_code error;
	fs::remove_all( data, error );
	fs::create_directories( data, error );
	Project loaded;
	const FExporter pfnExporter = FindExporter( szExt );
	if ( !Check( pfnExporter != nullptr, "export " + szExt + ": FindExporter has a production exporter" ) )
		return run;
	if ( !Check( LoadProject( project, &loaded ), "export " + szExt + ": the fixture loads" ) )
		return run;
	context.szProjectPath = project.string();
	context.szStagingRoot = data.string();
	run.bExported = pfnExporter( loaded, context, run.outcome );
	return run;
}

// The exported file read by the engine: readable, every node read, none of
// an older layout, then field-equal with the expected struct.
template <class TStats>
static void CheckExported( EExportKind kind, const std::string &szExt, const SExportRun &run, const std::string &szFile, TStats &expected, const fs::path &scratch )
{
	const fs::path file = run.data / szFile;
	if ( !Check( run.bExported && fs::is_regular_file( file ), "export " + szExt + ": writes " + szFile + " " + run.outcome.szError ) )
		return;
	const SExportRead read = ReadExport( kind, file.string() );
	Check( read.bReadable && read.unknown.empty() && read.stale.empty(),
	       "export " + szExt + ": " + GetExportKindInfo( kind ).pszReader + " reads every node of the export (" + std::to_string( read.unknown.size() ) +
	       " unknown, " + std::to_string( read.stale.size() ) + " stale) " + read.szError );
	const fs::path golden = scratch / szExt / "expected.xml";
	if ( !Check( WriteExpected( golden, expected ), "export " + szExt + ": the engine writes the hand-derived expectation" ) )
		return;
	const SCompareResult result = CompareStats( kind, file.string(), golden.string() );
	Check( result.status == ECompareStatus::EQUAL && result.nFieldsCompared > 0,
	       "export " + szExt + ": field-equal with the fixture's values as MFC's SaveRPGStats fills them (" + std::to_string( result.nFieldsCompared ) +
	       " fields): " + CompareStatusName( result.status ) + Messages( result ) );
}

static void Exporters( const fs::path &fixtures, const fs::path &data, const fs::path &scratchRoot )
{
	const fs::path scratch = scratchRoot / "export";
	SExportContext context;
	context.findUnitKey = &FixtureUnitKey;

	// Weapon: WeaponFrm.cpp FillRPGStats over the one shell of the fixture.
	{
		const fs::path project = CopyFixture( fixtures, scratch, "wpn" );
		const SExportRun run = RunExporter( "wpn", project, scratch / "wpn" / "data", context );
		SWeaponRPGStats expected;
		expected.szKeyName = "Unknown Weapon";
		expected.wDeltaAngle = 10;
		expected.nAmmoPerBurst = 1;
		expected.fDispersion = 1.0f;
		expected.fRangeMin = 1.0f;
		expected.fRangeMax = 100.0f;
		expected.nCeiling = 100;
		expected.fAimingTime = 100.0f;
		expected.fRevealRadius = 10.0f;
		SWeaponRPGStats::SShell &shell = expected.shells[0];
		shell.trajectory = SWeaponRPGStats::SShell::TRAJECTORY_LINE;
		shell.nPiercing = 0;
		shell.nPiercingRandom = 0;
		shell.fDamagePower = 5.0f;
		shell.nDamageRandom = 2;
		shell.fArea = 1.0f;
		shell.fArea2 = 2.0f;
		shell.fSpeed = 10.0f;
		shell.fDetonationPower = 0.0f;
		shell.fFireRate = 1.0f;
		shell.fRelaxTime = 100.0f;
		shell.eDamageType = SWeaponRPGStats::SShell::DAMAGE_HEALTH;
		shell.fTraceProbability = 10.0f / 100.0f;   // "Trace probability (%)"
		shell.fTraceSpeedCoeff = 1.0f;
		shell.fBrokeTrackProbability = 0.01f;
		shell.specials.RemoveData( 0 );
		shell.flashFire.nPower = 100;
		shell.flashFire.nDuration = 1000;
		shell.flashExplosion.nPower = 100;
		shell.flashExplosion.nDuration = 1000;
		CheckExported( EExportKind::WEAPON, "wpn", run, "weapons/wpn.xml", expected, scratch );

		// Stats only writes the same stats: a weapon has no graphics.
		SExportContext statsOnly = context;
		statsOnly.bStatsOnly = true;
		const SExportRun runStats = RunExporter( "wpn", project, scratch / "wpn" / "data-stats", statsOnly );
		std::string szFull, szStats;
		Check( runStats.bExported && ReadBytes( run.data / "weapons/wpn.xml", &szFull ) && ReadBytes( runStats.data / "weapons/wpn.xml", &szStats ) &&
		       szFull == szStats && runStats.outcome.nWritten == 1, "export wpn: stats only writes the same weapons/wpn.xml" );

		// own_data/export_file_name, as MFC's Export stored it, names the file.
		std::string szXml;
		ReadBytes( project, &szXml );
		const std::string::size_type nOpen = szXml.find( '>', szXml.find( "<Weapon_Composer_Project" ) );
		szXml.insert( nOpen + 1, "<own_data><export_file_name>custom\\named.xml</export_file_name></own_data>" );
		const fs::path named = project.parent_path() / "named.wpn";
		WriteBytes( named, szXml );
		const SExportRun runNamed = RunExporter( "wpn", named, scratch / "wpn" / "data-named", context );
		Check( runNamed.bExported && fs::is_regular_file( runNamed.data / "weapons/custom/named.xml" ),
		       "export wpn: own_data/export_file_name puts the file at weapons\\custom\\named.xml " + runNamed.outcome.szError );
	}

	// Mine: MineFrm.cpp FillRPGStats; the weapon prop names both fields.
	{
		const fs::path project = CopyFixture( fixtures, scratch, "mcp" );
		const SExportRun run = RunExporter( "mcp", project, scratch / "mcp" / "data", context );
		SMineRPGStats expected;
		expected.szKeyName = "";
		expected.fWeight = 10.0f;
		expected.szFlagModel = "1";
		expected.szWeapon = "";
		CheckExported( EExportKind::MINE, "mcp", run, "objects/simpleobjects/common/summer/mine/mcp/1.xml", expected, scratch );
		Check( run.outcome.szObjectName == "objects\\simpleobjects\\common\\summer\\mine\\mcp\\1",
		       "export mcp: names the composed sprite \"1\" beside the stats: " + run.outcome.szObjectName );
	}

	// Trench: TrenchFrm.cpp SaveRPGStats. The fixture's one segment has no
	// model file: MFC's "Cannot copy file" box, and no segment.
	{
		const fs::path project = CopyFixture( fixtures, scratch, "trc" );
		const SExportRun run = RunExporter( "trc", project, scratch / "trc" / "data", context );
		SEntrenchmentRPGStats expected;
		expected.szKeyName = "Unknown Trench";
		expected.fMaxHP = 100.0f;
		for ( int i = 0; i < 6; ++i )
		{
			expected.defences[i].nArmorMin = 300;
			expected.defences[i].nArmorMax = 300;
			expected.defences[i].fSilhouette = 1.0f;
		}
		CheckExported( EExportKind::ENTRENCHMENT, "trc", run, "units/technics/common/entrenchment/trc/1.xml", expected, scratch );
		Check( run.outcome.warnings.size() == 1 && run.outcome.warnings[0].find( "Cannot copy file" ) != std::string::npos,
		       "export trc: the empty segment source is MFC's \"Cannot copy file\" warning" );

		// A segment model the export can open: its box from chunk 4 of the
		// shipped .mod and its fire places from the context's mesh reader.
		std::string szXml;
		ReadBytes( project, &szXml );
		const std::string szEmpty = "<default_name>Source file</default_name>";
		const std::string::size_type nSource = szXml.find( "<string_value/>", szXml.find( szEmpty ) );
		if ( Check( nSource != std::string::npos, "export trc: the fixture has a segment source" ) )
		{
			szXml.replace( nSource, std::string( "<string_value/>" ).size(), "<string_value>models\\5.mod</string_value>" );
			const fs::path withModel = project.parent_path() / "model.trc";
			WriteBytes( withModel, szXml );
			std::string szMod;
			ReadBytes( FindNoCase( data, "Units\\Technics\\Common\\Entrenchment\\5.mod" ), &szMod );
			WriteBytes( project.parent_path() / "models" / "5.mod", szMod );

			const SExportRun runNoMesh = RunExporter( "trc", withModel, scratch / "trc" / "data-nomesh", context );
			Check( !runNoMesh.bExported && runNoMesh.outcome.szError.find( "5.mod" ) != std::string::npos &&
			       runNoMesh.outcome.szError.find( "mesh builder" ) != std::string::npos,
			       "export trc: without a mesh reader the export fails, naming the model: " + runNoMesh.outcome.szError );

			SExportContext withMesh = context;
			withMesh.meshFirePlaces = []( const std::string &szModFile, std::vector<std::pair<float, float>> &firePlaces, std::string & )
			{
				firePlaces.push_back( std::make_pair( 1.5f, -2.5f ) );
				return szModFile.find( "5.mod" ) != std::string::npos;
			};
			const SExportRun runMesh = RunExporter( "trc", withModel, scratch / "trc" / "data-mesh", withMesh );
			SEntrenchmentRPGStats read;
			bool bRead = false;
			if ( runMesh.bExported )
			{
				const fs::path exported = runMesh.data / "units/technics/common/entrenchment/trc";
				CPtr<IDataStorage> pStorage = OpenStorage( ( exported.string() + "/" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
				CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( "1.xml", STREAM_ACCESS_READ ) : 0;
				CPtr<IDataTree> pDT = pStream != 0 ? CreateDataTreeSaver( pStream, IDataTree::READ, "base" ) : 0;
				if ( pDT != 0 )
				{
					CTreeAccessor tree = pDT;
					tree.Add( "RPG", &read );
					bRead = true;
				}
			}
			Check( bRead && read.segments.size() == 1 && read.segments[0].szModel == "5" && read.lines.size() == 1 && read.lines[0] == 0 &&
			       read.segments[0].eType == SEntrenchmentRPGStats::EST_LINE && read.segments[0].fCoverage == 0.2f &&
			       read.segments[0].vAABBHalfSize.x > 1.0f && read.segments[0].fireplaces.size() == 1 && read.segments[0].fireplaces[0].x == 1.5f,
			       "export trc: a line segment with model 5, coverage 0.2, the .mod's box and the reader's fire place " + runMesh.outcome.szError );
		}
	}

	// Squad: SquadFrm.cpp SaveRPGStats. The member is a path, resolved by
	// the objects database; the soldier is placed relative to the formation's
	// zero point moved by the cross icon's half size on screen.
	{
		const fs::path project = CopyFixture( fixtures, scratch, "scp" );
		const SExportRun run = RunExporter( "scp", project, scratch / "scp" / "data", context );

		// The shift on screen (15.4, 15.4) in the world, under the squad
		// frame's camera (SetDefaultCamera: yaw 45, pitch -(90+30)) and one
		// world unit per pixel: x + y = 15.4 / cos45, x - y = 15.4 / (cos45 sin30).
		const float fCos45 = std::cos( ToRadian( 45.0f ) );
		const float fSin30 = std::sin( ToRadian( 30.0f ) );
		const float fSum = 15.4f / fCos45, fDiff = 15.4f / ( fCos45 * fSin30 );
		const float fShiftX = ( fSum + fDiff ) / 2, fShiftY = ( fSum - fDiff ) / 2;
		// The same shift through the engine's own view matrix: back on screen
		// it is 15.4 pixels right and 15.4 down (the viewport flips y).
		{
			SHMatrix view;
			CreateViewMatrixRH( &view, VNULL3, CQuat( ToRadian( 45.0f ), V3_AXIS_Z ) * CQuat( -ToRadian( 90.0f + 30.0f ), V3_AXIS_X ) );
			CVec3 vCamera;
			view.RotateHVector( &vCamera, CVec3( fShiftX, fShiftY, 0 ) );
			Check( std::fabs( vCamera.x - 15.4f ) < 1e-3f && std::fabs( -vCamera.y - 15.4f ) < 1e-3f,
			       "export scp: the zero point's shift is (15.4, 15.4) pixels on the squad frame's screen: (" + std::to_string( vCamera.x ) + ", " +
			       std::to_string( -vCamera.y ) + ")" );
		}
		SSquadRPGStats expected;
		expected.szIcon = "icon.tga";
		expected.type = SSquadRPGStats::RIFLEMANS;
		expected.memberNames.push_back( "USSR_Mosin" );
		SSquadRPGStats::SFormation form;
		form.type = SSquadRPGStats::SFormation::DEFAULT;
		form.changesByEvent.resize( 1 );
		form.changesByEvent[0] = -1;
		form.cLieFlag = 0;
		form.fSpeedBonus = form.fDispersionBonus = form.fFireRateBonus = form.fRelaxTimeBonus = form.fCoverBonus = 1.0f;
		SSquadRPGStats::SFormation::SEntry entry;
		entry.szSoldier = "USSR_Mosin";
		const CVec3 vRealZero( 724.077f + fShiftX, 362.039f + fShiftY, 0 );
		entry.vPos.x = 723.42f - vRealZero.x;
		entry.vPos.y = 361.71f - vRealZero.y;
		entry.fDir = ToDegree( 0.5f );
		form.order.push_back( entry );
		expected.formations.push_back( form );
		CheckExported( EExportKind::SQUAD, "scp", run, "squads/scp/1.xml", expected, scratch );

		SExportContext noLookup = context;
		noLookup.findUnitKey = nullptr;
		const SExportRun runNoLookup = RunExporter( "scp", project, scratch / "scp" / "data-nolookup", noLookup );
		Check( !runNoLookup.bExported && runNoLookup.outcome.szError.find( "USSR\\Mosin" ) != std::string::npos,
		       "export scp: without an objects database the member cannot be resolved, and the error names it: " + runNoLookup.outcome.szError );
		SExportContext unknown = context;
		unknown.findUnitKey = []( const std::string &, std::string & ) { return false; };
		const SExportRun runUnknown = RunExporter( "scp", project, scratch / "scp" / "data-unknown", unknown );
		Check( !runUnknown.bExported && runUnknown.outcome.szError.find( "Can't find stats for \"units\\humans\\ussr\\mosin\"" ) != std::string::npos,
		       "export scp: an unknown member is MFC's \"Can't find stats\": " + runUnknown.outcome.szError );
	}

	Check( FindExporter( "mdc" ) == nullptr, "export: a kind S06 did not port (mdc) still has no exporter" );
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
	DxtGate( data, scratch, fixtures );
	Exporters( fixtures, data, scratch );
	Goldens( fixtures );

	Log( g_nFailures == 0 ? "VERDICT=PASS (goldens pending)" : "VERDICT=FAIL failures=" + std::to_string( g_nFailures ) );
	std::ofstream log( fs::path( argv[2] ) / "resource_model" / "comparator.log", std::ios::binary | std::ios::trunc );
	log << g_szLog;
	return g_nFailures == 0 ? 0 : 1;
}
