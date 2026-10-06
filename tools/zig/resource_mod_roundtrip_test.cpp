// test-resource-mod-roundtrip: a third-party mod's game data through the Resource Editor's
// import and export (D053, replacing the unreachable GOG INTEX2 rows B-09.14 and B-14.5).
//
// For every resource of each kind BkResImportFromGame ports, the mod's stats file is copied
// (the mod is only ever read) into the scratch root, imported into a new project, saved,
// exported stats-only into a scratch export root, and the export is compared with the copy
// field by field through the engine's own readers (the D-11 comparator). A difference is a
// FAIL unless the accept table below names a rule and a reason for it.
//
// One line per resource:
//   MOD <kind> <path below the mod root> EQUAL | ACCEPTED <reason> | FAIL <field>: <mod value> vs <port value>
// then one summary line per kind:
//   MOD SUMMARY <kind> found= imported= equal= accepted= failed=
//
// The tier always runs over the tracked mini-mod first (tools/zig/fixtures/mod-roundtrip, built
// from Data/ files), so CI exercises the whole path. The real mod comes from BK_MOD_ROOT (the
// build option -Dmod-root exports it); without one the tier prints "skipped: no mod".
// BK_MOD_KINDS=unt,msh limits both runs to those kinds, to keep a run inside the tier budget.
//
// A host without a GPU skips as test-resource-bridge does; BK_REQUIRE_ENGINE=1 turns the skip
// into a failure on the runners that have a device.
//
// argv:
//   [1] staged install root (contains Data/consts.xml)
//   [2] the mini-mod's data folder: tools/zig/fixtures/mod-roundtrip/data
//   [3] scratch root: zig-out/local-test/mod-roundtrip
#include "StdAfx.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "resource_bridge.h"
#include "bridge_session.h"
#include "../ResourceModel/comparator.h"

namespace fs = std::filesystem;
using NResourceModel::EExportKind;

static int g_nFailures = 0;

static bool Check( bool bCondition, const std::string &szWhat )
{
	if ( !bCondition )
	{
		std::printf( "FAIL: %s\n", szWhat.c_str() );
		++g_nFailures;
	}
	return bCondition;
}

static std::string Fold( std::string sz )
{
	for ( char &c : sz )
		c = char( std::tolower( (unsigned char)c ) );
	return sz;
}

static std::string DirectoryOf( const char *pszPath )
{
	const std::string szPath( pszPath );
	const std::string::size_type nCut = szPath.find_last_of( "/\\" );
	return nCut == std::string::npos ? std::string( "." ) : szPath.substr( 0, nCut );
}

static int SkipOrFail( const std::string &szWhy )
{
	const char *pszRequire = std::getenv( "BK_REQUIRE_ENGINE" );
	if ( pszRequire != 0 && *pszRequire != 0 && std::strcmp( pszRequire, "0" ) != 0 )
	{
		std::printf( "FAIL: resource-mod-roundtrip: %s, and BK_REQUIRE_ENGINE is set\n", szWhy.c_str() );
		return 1;
	}
	std::printf( "resource-mod-roundtrip: skipped: %s\n", szWhy.c_str() );
	return 0;
}

// ---- The kinds BkResImportFromGame ports ---------------------------------

struct SKind
{
	const char *pszExt;
	BkResKind kind;                 // the bridge's ordinal
	EExportKind exportKind;         // the struct and chunk the engine reads it as
	bool bFlatFile;                 // the import takes the .xml itself, not a folder holding 1.xml
};

// The order of the summary lines. The code is the authority for what is importable (kind <= 3
// and the list at the top of BkResImportFromGame), not the header's comment.
static const SKind kKinds[] =
{
	{ "wpn", 0,  EExportKind::WEAPON,       true  },
	{ "mcp", 1,  EExportKind::MINE,         false },
	{ "trc", 2,  EExportKind::ENTRENCHMENT, false },
	{ "scp", 3,  EExportKind::SQUAD,        false },
	{ "unt", 5,  EExportKind::INFANTRY,     false },
	{ "msh", 6,  EExportKind::MECH_UNIT,    false },
	{ "obt", 7,  EExportKind::OBJECT,       false },
	{ "fnc", 8,  EExportKind::FENCE,        false },
	{ "bld", 9,  EExportKind::BUILDING,     false },
	{ "bdg", 10, EExportKind::BRIDGE,       false },
	{ "pcp", 11, EExportKind::PARTICLE,     true  },
	{ "3rd", 14, EExportKind::VSO,          true  },
	{ "3rv", 15, EExportKind::VSO,          true  },
	{ "mip", 16, EExportKind::MISSION,      false },
	{ "chc", 17, EExportKind::CHAPTER,      false },
	{ "cgc", 18, EExportKind::CAMPAIGN,     true  },
	{ "mdc", 19, EExportKind::MEDAL,        false },
};
static const int kKindCount = int( sizeof( kKinds ) / sizeof( kKinds[0] ) );

static int KindIndex( const char *pszExt )
{
	for ( int i = 0; i < kKindCount; ++i )
		if ( std::strcmp( kKinds[i].pszExt, pszExt ) == 0 )
			return i;
	return -1;
}

// ---- The accept table ----------------------------------------------------

// A difference the tier accepts, with the reason it is not a defect of the port. Everything
// else is a FAIL. One table, so the real mod run can extend it: add a row with the kind it
// concerns ("*" for any), a substring of the field path as the comparator prints it, and why.
struct SAcceptRule
{
	const char *pszExt;
	const char *pszPath;
	const char *pszReason;
	// Only when the mod ships none of the sibling files (.mod models, .dds pictures) the kind needs
	// beside its stats: a file the mod references but does not ship.
	bool bOnlyWithoutSiblings = false;
	// When set, the rule covers only a float field whose two values differ by at most this much.
	double fMaxDelta = 0;
};

static const SAcceptRule kAcceptRules[] =
{
	// Graphics-source fields stay empty on import (the BkResImportFromGame contract): MFC's
	// frame did not carry the editor-only state back either.
	{ "msh", "RPG/Platforms/item[", "MeshFrm.cpp:1274-1298 sets only the two rotation speeds of a platform; the part, gun carriage 1 and 2 combos stay \"NA\" (06-PARITY: not carried back by GetRPGStats)" },
	{ "msh", "RPG/Guns/item[", "MeshFrm.cpp:1285-1295 sets weapon, priority, recoil and ammo of a gun; the shoot point and shoot part combos stay \"NA\" (06-PARITY: not carried back by GetRPGStats)" },
	// What the export adds from the export root rather than from the project.
	{ "mip", "RPG/MODName", "the export writes the export root's mod name, which the mod's own stats do not have" },
	{ "mip", "RPG/MODVersion", "the export writes the export root's mod version, which the mod's own stats do not have" },
	{ "chc", "RPG/MODName", "the export writes the export root's mod name, which the mod's own stats do not have" },
	{ "chc", "RPG/MODVersion", "the export writes the export root's mod version, which the mod's own stats do not have" },
	{ "cgc", "RPG/MODName", "the export writes the export root's mod name, which the mod's own stats do not have" },
	{ "cgc", "RPG/MODVersion", "the export writes the export root's mod version, which the mod's own stats do not have" },
	// Files the mod references but does not ship (the tracked mini-mod ships none of these).
	{ "msh", "Can not load combat mechanics file", "the mod ships no .mod model beside the stats, which the export builds the unit from", true },
	{ "mdc", "RPG/ImageRect", "the mod ships no picture to measure the rectangle from", true },
	{ "mip", "RPG/MapImageRect", "the mod ships no map picture to measure the rectangle from", true },
	{ "chc", "RPG/MapImageRect", "the mod ships no map picture to measure the rectangle from", true },
	{ "cgc", "RPG/MapImageRect", "the mod ships no map picture to measure the rectangle from", true },
	// What the real mod run found (AchtungPanzer2, D053). Each is a proven difference, not a port defect.
	// The mod ships compiled textures (1_h.dds), not the source .tga the export measures, and a stats-only
	// export keeps MFC's all-zero rectangle (medal_export.cpp, the same as for the missing picture).
	{ "mdc", "RPG/ImageRect", "the mod ships the compiled texture (1_h.dds), not the source .tga the export measures; a stats-only export keeps MFC's all-zero rectangle" },
	// The campaign's stats were exported from a project under scenarios\custom, so MapImage carries that folder; the
	// tier places the project at the mod-relative path, and the import keeps a value that lacks the project's prefix whole.
	{ "cgc", "RPG/MapImage:", "the mod's MapImage names a texture under scenarios\\custom, not under the folder the tier places the project in; the import keeps a value without the project's prefix whole (WithoutPrefix), so the export prefixes it again" },
	// MiniMapBorderColor of a river: the river frame holds none (C3DRiverFrame::FillRPGStats sets none, GetRPGStats imports none).
	{ "3rv", "VSODescription/MiniMapBorderColor", "the river editor frame neither holds nor writes the minimap border colour (see the VSO losses in the comparator), so a river the mod authored with one exports 00000000" },
	// Older object stubs: no Defence nodes and the effect still written as a struct.
	{ "obt", "stale field desc/EffectExplosion/", "the mod ships the object in the older form that wrote the effect as a struct (Effect, Sound, MinDist, MaxDist); the struct reads the element's text, and the effect is empty" },
	{ "obt", "stale field desc/EffectDeath/", "as desc/EffectExplosion/" },
	{ "obt", "desc/Defence", "the mod ships a stub object without the Defence nodes; the reader gives the struct its defaults, while the project holds the object frame's defaults, which the export writes" },
	{ "obt", "extra field desc/CycledSound", "the mod ships a stub object without sounds; the reader gives the struct empty ones and the export writes the empty elements" },
	{ "obt", "extra field desc/AmbientSound", "as desc/CycledSound" },
	// The weapon exporter of the mod's time wrote only some per-shell fields; the reader gives an absent one its default.
	{ "wpn", "extra field RPG/Shells/item[", "the mod's weapon omits a per-shell field (BrokeTrackProbability, TraceProbability, TraceSpeedCoeff) that SWeaponRPGStats::SShell::operator& reads as its default; the export writes the attribute (a value difference would be a separate field line)" },
	// A bridge origin is the sprite position minus a grid corner; the project stores the position with six digits.
	{ "bdg", "/Origin/", "a segment's origin is the sprite position minus the center cross, and the project stores the sprite position with six digits, so an imported origin differs from the shipped one by up to 5e-4 and more where the position is large (as for fences; the mod run saw 5.5e-4)", false, 1e-3 },
	{ "bdg", "/VisOrigin/", "as the segment's origin", false, 1e-3 },
	// The mesh export builds the unit from the .mod model beside the stats (mesh_export.cpp, locators by node name),
	// so a mod whose stats were made against another model, or edited by hand, differs there. G_vt_Opel_Blitz_41x/1.mod
	// holds no LExhaust node (strings of the file) while its stats list ExhaustPoints 10.
	{ "msh", "RPG/ExhaustPoints", "the export takes the exhaust points from the LExhaust nodes of the .mod model beside the stats, and the mod's stats list points the shipped model lacks (G_vt_Opel_Blitz_41x/1.mod has no LExhaust node)" },
	{ "msh", "RPG/EntrancePoint", "the export takes the entrance point from the LPeople node of the .mod model beside the stats, and the mod's stats name another node" },
	{ "msh", "RPG/AnimDescs", "the export takes the animation list from the .mod model (and its install and transport variants) beside the stats, and the mod's stats list another set" },
	{ "msh", "RPG/DamagePoints", "the export takes the damage points from the .mod model beside the stats, and the mod's stats hold others" },
	{ "msh", "RPG/AABB_As", "the export takes the attack boxes from the .mod model beside the stats, and the mod's stats hold others" },
	{ "msh", "RPG/AABB_Ds", "the export takes the defence boxes from the .mod model beside the stats, and the mod's stats hold others" },
	{ "msh", "RPG/AABBHalfSize", "the export takes the box from the .mod model beside the stats, and the mod's stats hold others" },
	{ "msh", "RPG/Gunners", "the export takes the gunner points from the .mod model beside the stats, and the mod's stats hold others" },
	{ "msh", "RPG/FatalitySmokePoint", "the export takes the fatality smoke point from the .mod model beside the stats, and the mod's stats hold others" },
	{ "msh", "RPG/TowPoint", "the export takes the point from the locator nodes of the .mod model beside the stats, and the mod's stats name others" },
	{ "msh", "RPG/PeoplePoints", "the export takes the point from the locator nodes of the .mod model beside the stats, and the mod's stats name others" },
	{ "msh", "RPG/HookPoint", "the export takes the point from the locator nodes of the .mod model beside the stats, and the mod's stats name others" },
	{ "msh", "RPG/BackWheel", "the export takes the point from the locator nodes of the .mod model beside the stats, and the mod's stats name others" },
	{ "msh", "RPG/FrontWheel", "the export takes the point from the locator nodes of the .mod model beside the stats, and the mod's stats name others" },
	{ "msh", "RPG/ShootDustPoint", "the export takes the point from the locator nodes of the .mod model beside the stats, and the mod's stats name others" },
	{ "msh", "RPG/AmmoPoint", "the export takes the point from the locator nodes of the .mod model beside the stats, and the mod's stats name others" },
	{ "msh", "RPG/AABBCenter", "the export takes the box from the .mod model beside the stats, and the mod's stats hold another box" },
	{ "msh", "RPG/UninstallRotate", "the mod's stats hold 1.4013e-045 (the integer 1 read as a float, MSVC's text for it), an uninitialised value; ToAIUnits turns it into nUninstallRotate = int( 1.4e-45 * 1000 ) = 0, the same as the absent field the export leaves" },
	{ "msh", "RPG/UninstallTransport", "as UninstallRotate" },
	// A stub no editor saved: the KeyName is empty, so the engine's reader finds no stats and the import refuses it (the
	// importer's contract, as MFC's LoadRPGStats treats an empty KeyName).
	{ "*", " RPG stats", "an empty KeyName marks a stub no editor saved; the import's contract refuses a file the engine's reader finds no stats in" },
	// MFC's own export check (ExportFrameData) refuses a project that lacks a field, and a shipped
	// file may lack it: the port keeps the refusal, so there is no export to compare.
	{ "*", "You should specify", "MFC's own export check refuses a project the mod's file leaves a required field out of; the port keeps that refusal (06-PARITY)" },
};

// The mod's XML prints a float to six digits, so a port value that parses to the same six
// digits is the same number.
static bool ParseFloatField( const char *pszText, const char *pszLimit, double &fValue )
{
	char *pszEnd = 0;
	fValue = std::strtod( pszText, &pszEnd );
	// The comparator prints a float's bits after it: " (float 0x3f800000)".
	return pszEnd != pszText && ( pszEnd == pszLimit || std::strncmp( pszEnd, " (float 0x", 10 ) == 0 );
}

// The two values of a "field F: port A, golden B" message.
static bool FloatValues( const std::string &szMessage, double &fPort, double &fMod )
{
	const std::string::size_type nPort = szMessage.find( ": port " ), nMod = szMessage.find( ", golden " );
	if ( nPort == std::string::npos || nMod == std::string::npos || nMod < nPort )
		return false;
	return ParseFloatField( szMessage.c_str() + nPort + 7, szMessage.c_str() + nMod, fPort ) &&
	       ParseFloatField( szMessage.c_str() + nMod + 9, szMessage.c_str() + szMessage.size(), fMod );
}

static bool IsPrintRounding( const std::string &szMessage )
{
	double fPort = 0, fMod = 0;
	if ( !FloatValues( szMessage, fPort, fMod ) )
		return false;
	return std::fabs( fPort - fMod ) <= 2e-5 * std::max( 1.0, std::fabs( fMod ) );
}

static bool WithinDelta( const std::string &szMessage, double fMaxDelta )
{
	double fPort = 0, fMod = 0;
	return FloatValues( szMessage, fPort, fMod ) && std::fabs( fPort - fMod ) <= fMaxDelta;
}

static const char *AcceptReason( const char *pszExt, const std::string &szMessage, bool bNoSiblings )
{
	for ( const SAcceptRule &rule : kAcceptRules )
		if ( ( std::strcmp( rule.pszExt, "*" ) == 0 || std::strcmp( rule.pszExt, pszExt ) == 0 ) && szMessage.find( rule.pszPath ) != std::string::npos && ( !rule.bOnlyWithoutSiblings || bNoSiblings ) &&
		     ( rule.fMaxDelta == 0 || WithinDelta( szMessage, rule.fMaxDelta ) ) )
			return rule.pszReason;
	if ( IsPrintRounding( szMessage ) )
		return "a float the mod's XML prints to six digits";
	return 0;
}

// "field F: port A, golden B" as "F: B vs A" (the mod's value first); anything else as it is.
static std::string AsModVsPort( const std::string &szMessage )
{
	if ( szMessage.compare( 0, 6, "field " ) != 0 )
		return szMessage;
	const std::string::size_type nPort = szMessage.find( ": port " );
	if ( nPort == std::string::npos )
		return szMessage;
	const std::string szField = szMessage.substr( 6, nPort - 6 );
	const std::string szRest = szMessage.substr( nPort + 7 );
	if ( szRest.size() > 21 && szRest.compare( szRest.size() - 21, 21, ", golden has no such field" ) == 0 )
		return szField + ": <absent> vs " + szRest.substr( 0, szRest.size() - 21 );
	const std::string::size_type nMod = szRest.find( ", golden " );
	if ( nMod == std::string::npos )
		return szField + ": " + szRest;
	return szField + ": " + szRest.substr( nMod + 9 ) + " vs " + szRest.substr( 0, nMod );
}

// ---- Discovery -----------------------------------------------------------

struct SResource
{
	std::string szRel;              // below the mod root, '/' separated, as the mod spells it
	fs::path file;
	std::vector<int> candidates;    // indices into kKinds, in the order the folder suggests
};

static std::vector<std::string> Components( const std::string &szRel )
{
	std::vector<std::string> parts;
	std::string szPart;
	for ( char c : szRel )
	{
		if ( c == '/' )
		{
			parts.push_back( szPart );
			szPart.clear();
		}
		else
			szPart += c;
	}
	parts.push_back( szPart );
	return parts;
}

static std::vector<int> Kinds( std::initializer_list<const char *> exts )
{
	std::vector<int> indices;
	for ( const char *pszExt : exts )
		indices.push_back( KindIndex( pszExt ) );
	return indices;
}

// The kinds that could own a file, by where the engine finds it. The folder only suggests: the
// kind a file is counted under is the one whose reader accepts it (Classify).
static std::vector<int> CandidatesFor( const std::string &szRel )
{
	const std::vector<std::string> parts = Components( Fold( szRel ) );
	const std::string &szFile = parts.back();
	const std::string &szTop = parts[0];
	if ( parts.size() == 2 && szTop == "weapons" )
		return Kinds( { "wpn" } );
	if ( szTop == "effects" && parts.size() >= 3 && parts[1] == "particles" )
		return Kinds( { "pcp" } );
	if ( szTop == "terrain" && parts.size() >= 2 )
	{
		const std::string &szParent = parts[parts.size() - 2];
		if ( szParent == "roads3d" || szParent == "roads" )
			return Kinds( { "3rd", "3rv" } );
		if ( szParent == "rivers3d" || szParent == "rivers" )
			return Kinds( { "3rv", "3rd" } );
		return std::vector<int>();
	}
	if ( szTop == "scenarios" && parts.size() >= 3 && parts[1] == "campaigns" )
		return Kinds( { "cgc" } );
	if ( szFile != "1.xml" )
		return std::vector<int>();
	if ( szTop == "units" )
		return Kinds( { "unt", "msh", "trc" } );
	if ( szTop == "objects" )
		return Kinds( { "obt", "mcp" } );
	if ( szTop == "buildings" )
		return Kinds( { "bld" } );
	if ( szTop == "bridges" )
		return Kinds( { "bdg" } );
	if ( szTop == "fences" )
		return Kinds( { "fnc" } );
	if ( szTop == "squads" )
		return Kinds( { "scp" } );
	if ( szTop == "medals" )
		return Kinds( { "mdc" } );
	if ( szTop == "scenarios" )
		return Kinds( { "mip", "chc" } );
	if ( szTop == "effects" )
		return std::vector<int>();     // an effect (eff): its import is refused by design
	return Kinds( { "mcp", "trc", "scp", "unt", "msh", "obt", "fnc", "bld", "bdg", "mdc", "mip", "chc" } );
}

// The kind the engine's reader accepts: it opens the file's chunk and asks for no node the file
// lacks, and no node of the file is one it never asks for. Of several that do, the one that read
// the most chunks (a mine's file is a squad's with less) wins; a tie goes to the folder's order.
// -1 with the reason when none accepts.
static int Classify( const SResource &res, std::string &szWhy )
{
	int nBest = -1;
	std::size_t nBestChunks = 0;
	for ( int nCandidate : res.candidates )
	{
		const NResourceModel::SExportRead read = NResourceModel::ReadExport( kKinds[nCandidate].exportKind, res.file.string() );
		if ( !read.bReadable )
		{
			if ( szWhy.empty() )
				szWhy = std::string( kKinds[nCandidate].pszExt ) + ": " + read.szError;
			continue;
		}
		if ( !read.unknown.empty() )
		{
			if ( szWhy.empty() )
				szWhy = std::string( kKinds[nCandidate].pszExt ) + " reader leaves " + read.unknown[0] + " unread";
			continue;
		}
		if ( nBest < 0 || read.present.size() > nBestChunks )
		{
			nBest = nCandidate;
			nBestChunks = read.present.size();
		}
	}
	return nBest;
}

static std::vector<SResource> Discover( const fs::path &modRoot )
{
	std::vector<SResource> found;
	std::error_code ec;
	for ( fs::recursive_directory_iterator it( modRoot, ec ), end; !ec && it != end; it.increment( ec ) )
	{
		if ( !it->is_regular_file( ec ) || Fold( it->path().extension().string() ) != ".xml" )
			continue;
		SResource res;
		res.file = it->path();
		res.szRel = fs::relative( it->path(), modRoot, ec ).generic_string();
		res.candidates = CandidatesFor( res.szRel );
		if ( !res.candidates.empty() )
			found.push_back( res );
	}
	std::sort( found.begin(), found.end(), []( const SResource &a, const SResource &b ) { return a.szRel < b.szRel; } );
	return found;
}

// ---- One resource --------------------------------------------------------

struct SCounts
{
	int nFound = 0, nImported = 0, nEqual = 0, nAccepted = 0, nFailed = 0;
};

static void Fail( SCounts &counts, const char *pszExt, const std::string &szRel, const std::string &szWhat )
{
	++counts.nFailed;
	++g_nFailures;
	std::printf( "MOD %s %s FAIL %s\n", pszExt, szRel.c_str(), szWhat.c_str() );
}

// The exported stats file: the one .xml the export wrote beside mod.xml and modobjects.xml,
// or the one named as the source if it wrote several.
static fs::path ExportedFile( const fs::path &modDir, const fs::path &source )
{
	std::vector<fs::path> files;
	std::error_code ec;
	for ( fs::recursive_directory_iterator it( modDir / "data", ec ), end; !ec && it != end; it.increment( ec ) )
	{
		if ( !it->is_regular_file( ec ) || Fold( it->path().extension().string() ) != ".xml" )
			continue;
		const std::string szName = Fold( it->path().filename().string() );
		if ( szName == "mod.xml" || szName == "modobjects.xml" )
			continue;
		files.push_back( it->path() );
	}
	if ( files.size() == 1 )
		return files[0];
	for ( const fs::path &file : files )
		if ( Fold( file.filename().string() ) == Fold( source.filename().string() ) )
			return file;
	return fs::path();
}

static void RoundTripOne( BkResSession *pSession, int nKind, const SResource &res, const fs::path &scratch, int nIndex, SCounts *pAllCounts )
{
	SCounts *pCounts = &pAllCounts[nKind];

	std::error_code ec;
	const SKind *pKind = &kKinds[nKind];
	const fs::path dir = scratch / pKind->pszExt / std::to_string( nIndex );
	fs::remove_all( dir, ec );
	// The copy keeps the path below the mod root: a medal, mission, chapter and campaign keep
	// their place below medals\ and scenarios\ as the project's export file name.
	const fs::path copyDir = dir / "src" / fs::path( res.szRel ).parent_path();
	fs::create_directories( copyDir, ec );
	const fs::path copy = copyDir / res.file.filename();
	fs::copy_file( res.file, copy, fs::copy_options::overwrite_existing, ec );
	// The files the import and the export read beside the stats: a mesh's models, the pictures a
	// medal, mission, chapter or campaign takes its picture rectangle from.
	const char *pszSiblings = std::strcmp( pKind->pszExt, "msh" ) == 0 ? ".mod" : ( std::strcmp( pKind->pszExt, "mdc" ) == 0 || std::strcmp( pKind->pszExt, "mip" ) == 0 ||
	                          std::strcmp( pKind->pszExt, "chc" ) == 0 || std::strcmp( pKind->pszExt, "cgc" ) == 0 ) ? ".dds" : 0;
	int nSiblings = 0;
	for ( fs::directory_iterator it( res.file.parent_path(), ec ), end; pszSiblings != 0 && !ec && it != end; it.increment( ec ) )
		if ( it->is_regular_file( ec ) && Fold( it->path().extension().string() ) == pszSiblings )
		{
			fs::copy_file( it->path(), copyDir / it->path().filename(), fs::copy_options::overwrite_existing, ec );
			++nSiblings;
		}
	if ( ec )
	{
		Fail( *pCounts, pKind->pszExt, res.szRel, "copy: " + ec.message() );
		return;
	}

	BkEditorStatus status = BkResImportFromGame( pSession, pKind->kind, ( pKind->bFlatFile ? copy : copyDir ).string().c_str() );
	// The folder only suggested a road or a river; the engine's type decides.
	if ( status == BK_EDITOR_REFUSED && ( std::strcmp( pKind->pszExt, "3rd" ) == 0 || std::strcmp( pKind->pszExt, "3rv" ) == 0 ) )
	{
		const std::string szMessage = BkEditorLastMessage( pSession );
		const bool bIsRiver = szMessage.find( " is a river" ) != std::string::npos, bIsRoad = szMessage.find( " is a road" ) != std::string::npos;
		if ( bIsRiver || bIsRoad )
		{
			--pCounts->nFound;
			nKind = KindIndex( bIsRiver ? "3rv" : "3rd" );
			pCounts = &pAllCounts[nKind];
			++pCounts->nFound;
			pKind = &kKinds[nKind];
			status = BkResImportFromGame( pSession, pKind->kind, copy.string().c_str() );
		}
	}
	if ( status != BK_EDITOR_OK )
	{
		const std::string szImportMessage = BkEditorLastMessage( pSession );
		if ( const char *pszReason = AcceptReason( pKind->pszExt, szImportMessage, nSiblings == 0 ) )
		{
			++pCounts->nAccepted;
			std::printf( "MOD %s %s ACCEPTED %s [not imported]\n", pKind->pszExt, res.szRel.c_str(), pszReason );
			return;
		}
		Fail( *pCounts, pKind->pszExt, res.szRel, std::string( "import: " ) + BkEditorLastMessage( pSession ) + " vs imported" );
		return;
	}
	++pCounts->nImported;

	// The project is saved beside the copy, where the engine finds the models and pictures.
	const fs::path project = copyDir / ( std::string( "project." ) + pKind->pszExt );
	if ( BkResSave( pSession, project.string().c_str() ) != BK_EDITOR_OK )
	{
		const std::string szMessage = BkEditorLastMessage( pSession );
		BkResClose( pSession );
		Fail( *pCounts, pKind->pszExt, res.szRel, "save: " + szMessage + " vs saved" );
		return;
	}
	const fs::path modDir = dir / "mod";
	BkResModSettings mod = {};
	std::snprintf( mod.export_dir, sizeof( mod.export_dir ), "%s", modDir.string().c_str() );
	BkResModSettingsSet( pSession, &mod );
	BkResExportReport report = {};
	BkResWarning warnings[16] = {};
	report.warnings = warnings;
	report.warnings_capacity = 16;
	status = BkResExportStatsOnly( pSession, BK_RES_EXPORT_FORCE, &report );
	const std::string szExportMessage = BkEditorLastMessage( pSession );
	BkResClose( pSession );
	if ( status != BK_EDITOR_OK || report.written < 1 )
	{
		if ( const char *pszReason = AcceptReason( pKind->pszExt, szExportMessage, nSiblings == 0 ) )
		{
			++pCounts->nAccepted;
			std::printf( "MOD %s %s ACCEPTED %s [%s]\n", pKind->pszExt, res.szRel.c_str(), pszReason, szExportMessage.c_str() );
			return;
		}
		Fail( *pCounts, pKind->pszExt, res.szRel, "export: " + szExportMessage + " vs exported" );
		return;
	}
	const fs::path exported = ExportedFile( modDir, copy );
	if ( exported.empty() )
	{
		Fail( *pCounts, pKind->pszExt, res.szRel, "export: no single stats file below the export root vs exported" );
		return;
	}

	// The export is the port's, the copy is the mod's; the comparator calls them port and golden.
	const NResourceModel::SCompareResult result = NResourceModel::CompareRoundTrip( pKind->exportKind, exported.string(), copy.string() );
	std::vector<std::string> failed;
	std::set<std::string> reasons;
	int nAcceptedFields = 0;
	for ( const std::string &szExcused : result.excused )
	{
		// "<message> [<reason>]": the reason is what the table says.
		const std::string::size_type nReason = szExcused.find( " [" );
		reasons.insert( nReason == std::string::npos ? szExcused : szExcused.substr( nReason + 2, szExcused.size() - nReason - 3 ) );
		++nAcceptedFields;
	}
	for ( const std::string &szMessage : result.messages )
	{
		if ( const char *pszReason = AcceptReason( pKind->pszExt, szMessage, nSiblings == 0 ) )
		{
			reasons.insert( pszReason );
			++nAcceptedFields;
		}
		else
			failed.push_back( szMessage );
	}
	if ( result.status != NResourceModel::ECompareStatus::EQUAL && result.status != NResourceModel::ECompareStatus::DIFFERENT && failed.empty() )
		failed.push_back( std::string( NResourceModel::CompareStatusName( result.status ) ) + ": the engine cannot read one side" );

	if ( !failed.empty() )
	{
		std::string szWhat = AsModVsPort( failed[0] );
		if ( failed.size() > 1 )
			szWhat += " (+" + std::to_string( failed.size() - 1 ) + " more)";
		Fail( *pCounts, pKind->pszExt, res.szRel, szWhat );
		for ( std::size_t i = 1; i < failed.size() && i < 20; ++i )
			std::printf( "   %s\n", AsModVsPort( failed[i] ).c_str() );
		return;
	}
	if ( nAcceptedFields == 0 )
	{
		++pCounts->nEqual;
		std::printf( "MOD %s %s EQUAL\n", pKind->pszExt, res.szRel.c_str() );
		return;
	}
	++pCounts->nAccepted;
	std::string szReasons;
	for ( const std::string &szReason : reasons )
		szReasons += ( szReasons.empty() ? "" : "; " ) + szReason;
	std::printf( "MOD %s %s ACCEPTED %s (%d fields)\n", pKind->pszExt, res.szRel.c_str(), szReasons.c_str(), nAcceptedFields );
}

// ---- One mod root --------------------------------------------------------

typedef std::map<std::string, std::pair<std::uintmax_t, fs::file_time_type>> TFileStates;

static TFileStates Snapshot( const fs::path &root )
{
	TFileStates states;
	std::error_code ec;
	for ( fs::recursive_directory_iterator it( root, ec ), end; !ec && it != end; it.increment( ec ) )
	{
		std::error_code stat;
		if ( it->is_regular_file( stat ) )
			states[it->path().generic_string()] = std::make_pair( it->file_size( stat ), it->last_write_time( stat ) );
	}
	return states;
}

static bool KindWanted( const char *pszExt )
{
	const char *pszList = std::getenv( "BK_MOD_KINDS" );
	if ( pszList == 0 || *pszList == 0 )
		return true;
	const std::string szList = "," + Fold( pszList ) + ",";
	return szList.find( std::string( "," ) + pszExt + "," ) != std::string::npos;
}

static void RunMod( BkResSession *pSession, const char *pszName, const fs::path &modRoot, const fs::path &scratch )
{
	std::printf( "MOD ROOT %s %s\n", pszName, modRoot.string().c_str() );
	const TFileStates before = Snapshot( modRoot );
	const std::vector<SResource> found = Discover( modRoot );
	SCounts counts[kKindCount];
	int nIndex = 0, nUnclassified = 0;
	for ( const SResource &res : found )
	{
		std::string szWhy;
		int nKind = Classify( res, szWhy );
		if ( nKind < 0 )
		{
			// Not a stats file of any kind the importer ports: a note, not a failure, unless the
			// folder said it should have been one of them (a file that no reader accepts).
			++nUnclassified;
			std::printf( "MOD ? %s UNCLASSIFIED %s\n", res.szRel.c_str(), szWhy.c_str() );
			continue;
		}
		if ( !KindWanted( kKinds[nKind].pszExt ) )
			continue;
		++counts[nKind].nFound;
		RoundTripOne( pSession, nKind, res, scratch, nIndex++, counts );
	}
	for ( int i = 0; i < kKindCount; ++i )
		if ( KindWanted( kKinds[i].pszExt ) )
			std::printf( "MOD SUMMARY %s found=%d imported=%d equal=%d accepted=%d failed=%d\n", kKinds[i].pszExt, counts[i].nFound, counts[i].nImported, counts[i].nEqual, counts[i].nAccepted, counts[i].nFailed );
	std::printf( "MOD NOTE %s: %d .xml files that no importable kind's reader accepts (an effect, a tileset, a context file...)\n", pszName, nUnclassified );

	// The mod is read-only: nothing in it may have been written, created or removed.
	const TFileStates after = Snapshot( modRoot );
	Check( before == after, std::string( pszName ) + ": the mod's files (path, size, mtime) are the same after the run as before" );
	std::printf( "MOD GUARD %s: %d files, %s\n", pszName, int( before.size() ), before == after ? "unchanged" : "CHANGED" );
}

int main( int argc, char **argv )
{
#if defined(_WIN32) || defined(_WIN64)
	_set_error_mode( _OUT_TO_STDERR );
	_set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
#endif
	const std::string szSelfDir = DirectoryOf( argv[0] != 0 ? argv[0] : "." );
	const char *pszRoot = argc > 1 ? argv[1] : szSelfDir.c_str();
	const fs::path fixtureMod = argc > 2 ? argv[2] : ( fs::path( szSelfDir ) / "fixtures/mod-roundtrip/data" );
	const fs::path scratch = argc > 3 ? argv[3] : ( fs::path( szSelfDir ) / "local-test/mod-roundtrip" );
	const char *pszModRoot = std::getenv( "BK_MOD_ROOT" );

	if ( !SDL_Init( SDL_INIT_VIDEO ) )
	{
		const char *pszError = SDL_GetError();
		if ( std::strstr( pszError, "video driver" ) != 0 || std::strstr( pszError, "No available" ) != 0 )
			return SkipOrFail( std::string( "no video driver (" ) + pszError + ")" );
		std::printf( "FAIL: SDL_Init: %s\n", pszError );
		return 1;
	}
	// A hidden, unfocusable window: the Linux pitfall the Map Editor tiers already solved.
	SDL_Window *pWindow = SDL_CreateWindow( "resource-mod-roundtrip", 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE );
	if ( pWindow == 0 )
	{
		std::printf( "FAIL: SDL_CreateWindow: %s\n", SDL_GetError() );
		SDL_Quit();
		return 1;
	}
	std::error_code ec;
	if ( !fs::is_regular_file( fs::path( pszRoot ) / "Data/consts.xml", ec ) )
	{
		const int nSkipped = SkipOrFail( std::string( "no staged game at " ) + pszRoot + " (run: zig build install-game)" );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	if ( !fs::is_directory( fixtureMod, ec ) )
	{
		std::printf( "FAIL: resource-mod-roundtrip: the tracked mini-mod %s is missing\n", fixtureMod.string().c_str() );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 1;
	}

	BkEditorSession *pSession = 0;
	const BkEditorStatus start = BkEditorStart( pWindow, pszRoot, &pSession );
	if ( start == BK_EDITOR_NO_DEVICE )
	{
		const int nSkipped = SkipOrFail( std::string( "no GPU device (" ) + BkEditorLastMessage( pSession ) + ")" );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return nSkipped;
	}
	if ( !Check( start == BK_EDITOR_OK, "the bridge starts" ) )
	{
		std::printf( "resource-mod-roundtrip: %s\n", BkEditorLastMessage( pSession ) );
		BkEditorStop( pSession );
		SDL_DestroyWindow( pWindow );
		SDL_Quit();
		return 1;
	}
	std::string szError;
	if ( Check( NResourceModel::StartEngineReaders( &szError ), "the comparator's engine readers start " + szError ) )
	{
		// The tracked mini-mod first, so CI runs the whole path with or without a real mod.
		RunMod( pSession, "fixture", fixtureMod, scratch / "fixture" );
		if ( pszModRoot != 0 && *pszModRoot != 0 )
		{
			if ( Check( fs::is_directory( pszModRoot, ec ), std::string( "BK_MOD_ROOT " ) + pszModRoot + " is a folder" ) )
				RunMod( pSession, "mod", pszModRoot, scratch / "mod" );
		}
		else
			std::printf( "resource-mod-roundtrip: skipped: no mod (set BK_MOD_ROOT or -Dmod-root to the mod's data folder)\n" );
	}

	Check( BkEditorStop( pSession ) == BK_EDITOR_OK, "the bridge stops" );
	SDL_DestroyWindow( pWindow );
	SDL_Quit();
	if ( g_nFailures == 0 )
		std::printf( "resource-mod-roundtrip: OK\n" );
	else
		std::printf( "resource-mod-roundtrip: %d failures\n", g_nFailures );
	return g_nFailures == 0 ? 0 : 1;
}
