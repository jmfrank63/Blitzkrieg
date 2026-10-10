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
// BK_MOD_DETAIL=1 lists under each ACCEPTED resource every accepted field as "<field>: <mod value> vs <port value>
// [reason]", which is the per-file justification of the accept table.
//
// What a difference needs to be accepted (D054, "port bugs are fixed, not accepted"): a rule of the table below whose
// proof holds for that very file (bytes of the mod's stats or its models), a float within the six-digit print or a
// bound the source gives, or a field the engine's writer emits that an older export left out (the comparator names
// the file as the one lacking it, and a differing value would be a line of its own). The comparator's own excuses,
// which stand on source alone for the goldens' sake, are held to the same bounds for a value.
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
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "../../Sources/src/BkMemory/bk_memory_sdl.h"
#include "resource_bridge.h"
#include "bridge_session.h"
#include "../ResourceModel/comparator.h"

namespace fs = std::filesystem;
using NResourceModel::EExportKind;

static int g_nFailures = 0;
// The mod's own mod.xml, which the scratch export root takes; empty for a mod that has none.
static fs::path g_modXml;

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

// The evidence a rule needs from the mod's own files before it excuses a difference. A rule without
// one is a source rule: the difference follows from the engine's or MFC's source alone.
enum EProof
{
	PROOF_NONE,
	// The mod's stats file holds pszNeedle (a byte string of that very file).
	PROOF_SOURCE_HAS,
	// The mod's stats file does not hold pszNeedle.
	PROOF_SOURCE_LACKS,
	// No node of the combat model beside the stats (1.mod, and the install and transportable variants 2.mod
	// and 3.mod when the mod ships them) has a name that starts with pszNeedle: the export finds no such locator.
	PROOF_MODEL_LACKS,
	// The same for the combat model (1.mod) alone, which the entrance, tow, fatality smoke and damage points come from.
	PROOF_COMBAT_MODEL_LACKS,
};

// A difference the tier accepts, with the reason it is not a defect of the port. Everything
// else is a FAIL. One table, so the real mod run can extend it: add a row with the kind it
// concerns, a substring of the field path as the comparator prints it, and why. A rule that
// names a proof applies to a file only when the proof holds for that file; a rule with a bound
// applies to a float field only while the two values are within it.
struct SAcceptRule
{
	const char *pszExt;
	const char *pszPath;
	const char *pszReason;
	EProof proof = PROOF_NONE;
	const char *pszNeedle = 0;
	// Only when the mod ships none of the sibling files (.mod models, .dds pictures) the kind needs
	// beside its stats: a file the mod references but does not ship.
	bool bOnlyWithoutSiblings = false;
	// When set, the rule covers only a float field whose two values differ by at most this much.
	double fMaxDelta = 0;
};

// What the proofs read: the mod's own stats file and the models copied beside the project.
struct SProofContext
{
	fs::path source;
	fs::path copyDir;
};

static const SAcceptRule kAcceptRules[] =
{
	// Files the mod references but does not ship (the tracked mini-mod ships none of these).
	{ "msh", "Can not load combat mechanics file", "the mod ships no .mod model beside the stats, which the export builds the unit from", PROOF_NONE, 0, true },

	// The mod's object stubs of the older form (all seven, below objects\terraobjects): the effects are elements with
	// the struct's children and attributes, no sounds, no Defence nodes. SObjectRPGStats reads an effect as the
	// element's text only, so the children are not read, and the struct's defaults (SDefenseRPGStats(): 40, 90, 1.0)
	// are the six defences the import and the export keep (a value difference would be a line of its own). The three rules after the effects name what the stub lacks.
	{ "obt", "stale field desc/EffectExplosion/", "the mod's stub spells the effect as a struct (<EffectExplosion MinDist=...><Effect/><Sound/>); the reader takes the element's text, which is empty", PROOF_SOURCE_HAS, "<EffectExplosion MinDist=" },
	{ "obt", "stale field desc/EffectDeath/", "the mod's stub spells the effect as a struct (<EffectDeath MinDist=...><Effect/><Sound/>); the reader takes the element's text, which is empty", PROOF_SOURCE_HAS, "<EffectDeath MinDist=" },
	{ "obt", "extra field desc/Defence", "the mod's stub has no Defence nodes, so the reader gives the struct's defaults (SDefenseRPGStats(): 40, 90, 1.0), which the import keeps and the export writes", PROOF_SOURCE_LACKS, "Defence" },
	{ "obt", "extra field desc/CycledSound", "the mod's stub has no sounds, so the reader gives the struct empty ones and the export writes the empty elements", PROOF_SOURCE_LACKS, "CycledSound" },
	{ "obt", "extra field desc/AmbientSound", "the mod's stub has no sounds, so the reader gives the struct empty ones and the export writes the empty elements", PROOF_SOURCE_LACKS, "AmbientSound" },

	// The weapon exporter of the mod's time wrote only some per-shell fields; the reader gives an absent one its
	// default (SWeaponRPGStats::SShell::operator&), and the project holds that default, so the export writes the attribute.
	{ "wpn", "extra field RPG/Shells/item[", "the mod's weapon omits a per-shell field (BrokeTrackProbability, TraceProbability, TraceSpeedCoeff) that SWeaponRPGStats::SShell::operator& reads as its default; the export writes the attribute (a value difference would be a separate field line)" },

	// A bridge origin is the sprite position minus a grid corner; the project stores the sprite position with six
	// digits (an absolute error of at most 5e-4 below 1000), so an imported origin differs from the shipped one by
	// that and a few float ulps.
	{ "bdg", "/Origin/", "a segment's origin is the sprite position minus the center cross, and the project stores the sprite position with six digits (5e-4 below 1000)", PROOF_NONE, 0, false, 6e-4 },
	{ "bdg", "/VisOrigin/", "as the segment's origin", PROOF_NONE, 0, false, 6e-4 },

	// The mesh export builds the unit from the .mod model beside the stats (mesh_export.cpp, locators by node name),
	// so a mod whose stats were made against another model, or edited by hand, differs there. The rules below name
	// the node prefix the export looks for and apply to a file only while its model has no such node.
	{ "msh", "RPG/ExhaustPoints", "the export takes the exhaust points from the LExhaust nodes of the models beside the stats; they have none, and the mod's stats list points", PROOF_MODEL_LACKS, "LExhaust" },
	{ "msh", "RPG/EntrancePoint", "the export takes the entrance point from the LPeople node of the model beside the stats; it has none (-1, point 0 0), and the mod's stats name one", PROOF_COMBAT_MODEL_LACKS, "LPeople" },
	{ "msh", "RPG/FatalitySmokePoint", "the export takes the fatality smoke point from the LFatalitySmoke node of the model beside the stats; it has none (-1), and the mod's stats name one", PROOF_COMBAT_MODEL_LACKS, "LFatalitySmoke" },
	{ "msh", "RPG/TowPoint", "the export takes the tow point from the LTowingPoint node of the model beside the stats; it has none (-1, point 0 0), and the mod's stats name one", PROOF_COMBAT_MODEL_LACKS, "LTowingPoint" },
	{ "msh", "RPG/DamagePoints", "the export takes the damage points from the LSmoke nodes of the model beside the stats; it has none, and the mod's stats list points", PROOF_COMBAT_MODEL_LACKS, "LSmoke" },
	{ "msh", "RPG/UninstallRotate", "the mod's stats hold 1.4013e-045 (the integer 1 read as a float, MSVC's text for it); ToAIUnits turns it into nUninstallRotate = int( 1.4e-45 * 1000 ) = 0, the same as the absent field the export leaves", PROOF_SOURCE_HAS, "UninstallRotate=\"1.4013e-045\"" },
	{ "msh", "RPG/UninstallTransport", "as UninstallRotate", PROOF_SOURCE_HAS, "UninstallTransport=\"1.4013e-045\"" },

	// A mod without a mod.xml (the tracked mini-mod) gives the export root no mod name, so a mission, chapter or
	// campaign gets empty MODName and MODVersion elements, which the reader of a file that lacks them also gives.
	{ "mip", "extra field RPG/MODName", "the file has no MODName and the export root no mod.xml: the empty element reads as the absent one", PROOF_SOURCE_LACKS, "MODName" },
	{ "mip", "extra field RPG/MODVersion", "as MODName", PROOF_SOURCE_LACKS, "MODVersion" },
	{ "chc", "extra field RPG/MODName", "as for a mission", PROOF_SOURCE_LACKS, "MODName" },
	{ "chc", "extra field RPG/MODVersion", "as for a mission", PROOF_SOURCE_LACKS, "MODVersion" },
	{ "cgc", "extra field RPG/MODName", "as for a mission", PROOF_SOURCE_LACKS, "MODName" },
	{ "cgc", "extra field RPG/MODVersion", "as for a mission", PROOF_SOURCE_LACKS, "MODVersion" },

	// MFC's own export check (ChapterFrm.cpp ExportFrameData, ported as Validate in chapter_export.cpp) refuses a chapter
	// without placeholders or without a player side; the port keeps the refusal (06-PARITY), so there is no export to
	// compare. The proof is the mod's own file: its chapter holds no placeholders or names no side.
	{ "chc", "You should specify some placeholders", "MFC's export check refuses a chapter without placeholders, and the mod's chapter has none (<PlaceHolders/>); the port keeps that refusal (06-PARITY)", PROOF_SOURCE_HAS, "<PlaceHolders/>" },
	{ "chc", "You should specify player side", "MFC's export check refuses a chapter without a player side, and the mod's chapter names none; the port keeps that refusal (06-PARITY)", PROOF_SOURCE_HAS, "<PlayerSide></PlayerSide>" },
	{ "chc", "You should specify player side", "MFC's export check refuses a chapter without a player side, and the mod's chapter names none; the port keeps that refusal (06-PARITY)", PROOF_SOURCE_HAS, "<PlayerSide/>" },
};

static bool ReadBytes( const fs::path &file, std::string &szBytes )
{
	std::ifstream in( file, std::ios::binary );
	if ( !in )
		return false;
	szBytes.assign( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
	return true;
}

// Whether any node-name string of the model files beside the project starts with pszPrefix. A model keeps each
// node name once as plain text, so the prefix as a byte string tells.
static bool ModelsHavePrefix( const fs::path &copyDir, const char *pszPrefix, bool bCombatOnly )
{
	for ( const char *pszModel : { "1.mod", "2.mod", "3.mod" } )
	{
		if ( bCombatOnly && std::strcmp( pszModel, "1.mod" ) != 0 )
			continue;
		std::string szBytes;
		std::error_code ec;
		fs::path model;
		for ( fs::directory_iterator it( copyDir, ec ), end; !ec && it != end; it.increment( ec ) )
			if ( Fold( it->path().filename().string() ) == pszModel )
				model = it->path();
		if ( !model.empty() && ReadBytes( model, szBytes ) && szBytes.find( pszPrefix ) != std::string::npos )
			return true;
	}
	return false;
}

static bool ProofHolds( const SAcceptRule &rule, const SProofContext &ctx )
{
	switch ( rule.proof )
	{
		case PROOF_SOURCE_HAS:
		{
			std::string szBytes;
			return ReadBytes( ctx.source, szBytes ) && szBytes.find( rule.pszNeedle ) != std::string::npos;
		}
		case PROOF_SOURCE_LACKS:
		{
			std::string szBytes;
			return ReadBytes( ctx.source, szBytes ) && szBytes.find( rule.pszNeedle ) == std::string::npos;
		}
		case PROOF_MODEL_LACKS:
			return !ModelsHavePrefix( ctx.copyDir, rule.pszNeedle, false );
		case PROOF_COMBAT_MODEL_LACKS:
			return !ModelsHavePrefix( ctx.copyDir, rule.pszNeedle, true );
		default:
			return true;
	}
}

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
	return std::fabs( fPort - fMod ) <= 2e-5 * (std::max)( 1.0, std::fabs( fMod ) );
}

static bool WithinDelta( const std::string &szMessage, double fMaxDelta )
{
	double fPort = 0, fMod = 0;
	return FloatValues( szMessage, fPort, fMod ) && std::fabs( fPort - fMod ) <= fMaxDelta;
}

// The comparator excuses a few float differences by their source alone, for the goldens' sake, with no bound. The
// real mod run holds them to the bound the source gives: a squad slot is rebuilt with float arithmetic from slots
// printed with six digits (a few 1e-5), a fence origin is a six-digit sprite position minus a corner (5e-4 below 1000).
struct SExcuseBound
{
	const char *pszExt;
	const char *pszPath;
	double fMaxDelta;
};

static const SExcuseBound kExcuseBounds[] =
{
	{ "scp", "/Pos/", 1e-4 },
	{ "scp", "/Dir", 1e-4 },
	{ "fnc", "/Origin/", 6e-4 },
	{ "fnc", "/VisOrigin/", 6e-4 },
};

// A comparator excuse for a missing or extra field stands on the source the comparator names (the engine's writer
// emits every field, an older export omitted some); one for a value is held to the six-digit print or to a bound above.
// The comparator excuses a value by its source alone for the goldens' sake, which could hide a lost value.
static bool ExcuseWithinBound( const char *pszExt, const std::string &szExcused )
{
	if ( szExcused.compare( 0, 6, "field " ) != 0 || szExcused.find( "no such field" ) != std::string::npos )
		return true;
	for ( const SExcuseBound &bound : kExcuseBounds )
		if ( std::strcmp( bound.pszExt, pszExt ) == 0 && szExcused.find( bound.pszPath ) != std::string::npos )
			return WithinDelta( szExcused, bound.fMaxDelta );
	return IsPrintRounding( szExcused );
}

static const char *AcceptReason( const char *pszExt, const std::string &szMessage, bool bNoSiblings, const SProofContext &ctx )
{
	for ( const SAcceptRule &rule : kAcceptRules )
		if ( ( std::strcmp( rule.pszExt, "*" ) == 0 || std::strcmp( rule.pszExt, pszExt ) == 0 ) && szMessage.find( rule.pszPath ) != std::string::npos && ( !rule.bOnlyWithoutSiblings || bNoSiblings ) &&
		     ( rule.fMaxDelta == 0 || WithinDelta( szMessage, rule.fMaxDelta ) ) && ProofHolds( rule, ctx ) )
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
	const SProofContext proofContext = { res.file, copyDir };
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
		if ( const char *pszReason = AcceptReason( pKind->pszExt, szImportMessage, nSiblings == 0, proofContext ) )
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
	// The export root is the mod's own: its settings name the mod, as the editor's export root does, so the MODName and
	// MODVersion a mission, chapter or campaign carries are the mod's (the settings write the root's mod.xml).
	std::string szModXml;
	if ( !g_modXml.empty() && ReadBytes( g_modXml, szModXml ) )
	{
		const auto Tag = [&]( const char *pszTag, char *pszOut, std::size_t nSize )
		{
			const std::string szOpen = std::string( "<" ) + pszTag + ">", szClose = std::string( "</" ) + pszTag + ">";
			const std::string::size_type nFrom = szModXml.find( szOpen ), nTo = szModXml.find( szClose );
			if ( nFrom != std::string::npos && nTo != std::string::npos && nTo > nFrom )
				std::snprintf( pszOut, nSize, "%s", szModXml.substr( nFrom + szOpen.size(), nTo - nFrom - szOpen.size() ).c_str() );
		};
		Tag( "MODName", mod.name, sizeof( mod.name ) );
		Tag( "MODVersion", mod.version, sizeof( mod.version ) );
		Tag( "MODDesc", mod.desc, sizeof( mod.desc ) );
	}
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
		if ( const char *pszReason = AcceptReason( pKind->pszExt, szExportMessage, nSiblings == 0, proofContext ) )
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
	std::vector<std::string> accepted;     // "<field: mod vs port> [<reason>]", printed under BK_MOD_DETAIL
	int nAcceptedFields = 0;
	for ( const std::string &szExcused : result.excused )
	{
		if ( !ExcuseWithinBound( pKind->pszExt, szExcused ) )
		{
			failed.push_back( szExcused );
			continue;
		}
		// "<message> [<reason>]": the reason is what the table says.
		const std::string::size_type nReason = szExcused.find( " [" );
		reasons.insert( nReason == std::string::npos ? szExcused : szExcused.substr( nReason + 2, szExcused.size() - nReason - 3 ) );
		accepted.push_back( AsModVsPort( nReason == std::string::npos ? szExcused : szExcused.substr( 0, nReason ) ) + ( nReason == std::string::npos ? std::string() : " [comparator] " + szExcused.substr( nReason + 2, 60 ) ) );
		++nAcceptedFields;
	}
	for ( const std::string &szMessage : result.messages )
	{
		if ( const char *pszReason = AcceptReason( pKind->pszExt, szMessage, nSiblings == 0, proofContext ) )
		{
			reasons.insert( pszReason );
			accepted.push_back( AsModVsPort( szMessage ) + " [" + std::string( pszReason ).substr( 0, 60 ) + "]" );
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
	const char *pszDetail = std::getenv( "BK_MOD_DETAIL" );
	if ( pszDetail != 0 && *pszDetail != 0 && std::strcmp( pszDetail, "0" ) != 0 )
		for ( const std::string &szField : accepted )
			std::printf( "   ~ %s\n", szField.c_str() );
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
	g_modXml.clear();
	{
		std::error_code ec;
		for ( fs::directory_iterator it( modRoot, ec ), end; !ec && it != end; it.increment( ec ) )
			if ( Fold( it->path().filename().string() ) == "mod.xml" )
				g_modXml = it->path();
	}
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

	// Hooks first: the first SDL call of the process must already allocate through BkMemory.
	BkMemoryInstallSdlFunctions();
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
