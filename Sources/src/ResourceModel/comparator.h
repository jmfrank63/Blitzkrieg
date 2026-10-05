#pragma once
// The D-11 comparator: does the game read the port's export the same as the
// MFC editor's export of the same project (the golden)?
//
// It compares exported game data, not project XML. A stats file is read by
// the engine itself: the engine's StreamIO opens it as a CDataTreeXML, and
// the stats struct's own operator&( IDataTree & ) reads it, the same calls
// GameDB::ReadRPGStats, GetAddStats, GetGameStats, SParticleSourceData::Load
// and the terrain and VSO loaders make. Two things are recorded on the way:
//
//   - every chunk the reader asks for and finds (a delegating IDataTree in
//     front of the engine's tree), so a node in the file that the reader never
//     asks for is an unknown field, and a node present on one side only is a
//     dropped or an extra field;
//   - every field of the struct as read, by writing the struct back through
//     the same operator& into a recording IDataTree, so values are compared
//     field by field and floats bit for bit, as the struct holds them.
//
// Files the game reads as bytes (_h.dds, .san, sprite packs, icons, copied
// files) are compared byte for byte. A DXT texture (_c.dds) is compared by
// size, format and mip count; its pixels go through the tolerance S03 T07
// measures, and until that gate exists a pixel difference reports
// PENDING_DXT_GATE, never EQUAL.
//
// The comparator needs the engine's StreamIO module, so it runs in an
// engine-hosted, data-only executable (no window, no GPU): call
// StartEngineReaders() once before the first comparison.

#include <string>
#include <vector>

namespace NResourceModel
{

// One stats type, named by the engine struct its file is read into.
enum class EExportKind
{
	MECH_UNIT,       // SMechUnitRPGStats, "RPG"
	INFANTRY,        // SInfantryRPGStats, "RPG"
	WEAPON,          // SWeaponRPGStats, "RPG"
	SQUAD,           // SSquadRPGStats, "RPG"
	MINE,            // SMineRPGStats, "RPG"
	ENTRENCHMENT,    // SEntrenchmentRPGStats, "RPG"
	OBJECT,          // SObjectRPGStats, "desc"
	FENCE,           // SFenceRPGStats, "RPG"
	BUILDING,        // SBuildingRPGStats, "desc"
	BRIDGE,          // SBridgeRPGStats, "RPG"
	PARTICLE,        // SParticleSourceData or SSmokinParticleSourceData, "KeyData"
	EFFECT,          // SEffectDesc, "effect"
	TILESET,         // STilesetDesc, "tileset"
	CROSSET,         // SCrossetDesc, "crosset"
	VSO,             // SVectorStripeObjectDesc, "VSODescription"
	MISSION,         // SMissionStats, "RPG"
	CHAPTER,         // SChapterStats, "RPG"
	CAMPAIGN,        // SCampaignStats, "RPG"
	MEDAL,           // SMedalStats, "RPG"
};

struct SExportKindInfo
{
	EExportKind kind;
	const char *pszName;      // "SWeaponRPGStats"
	const char *pszReader;    // "GetAddStats<SWeaponRPGStats>"
	const char *pszRoot;      // the chunk the reader opens under the base node
	const char *pszBase;      // the document element CreateDataTreeSaver opens: "base", "effect" for effects
};

const SExportKindInfo &GetExportKindInfo( EExportKind kind );
std::vector<EExportKind> AllExportKinds();

enum class ECompareStatus
{
	EQUAL,
	DIFFERENT,          // values, dropped or extra fields, or bytes differ
	UNKNOWN_FIELD,      // a side has a node its reader does not read
	UNREADABLE,         // a side is missing or the engine cannot parse it
	PENDING_DXT_GATE,   // DXT headers equal, pixels differ, T07's gate not there yet
};
const char *CompareStatusName( ECompareStatus status );

struct SCompareResult
{
	ECompareStatus status = ECompareStatus::EQUAL;
	int nFieldsCompared = 0;
	int nStale = 0;     // stale nodes on the port side (SExportRead::stale), reported, not ignored
	// One line per difference, precise enough to fix it from: the field path
	// (chunk names, item[i] for container items), both values, and for floats
	// the bits.
	std::vector<std::string> messages;
};

// Loads the engine's StreamIO module and its globals beside the running
// executable, as the editor bridge's start does. False with a reason when it
// cannot.
bool StartEngineReaders( std::string *pszError );

// What the engine's reader makes of one stats file.
struct SExportRead
{
	bool bReadable = false;
	std::string szError;
	std::vector<std::pair<std::string, std::string>> fields; // struct fields as read, in operator& order
	std::vector<std::string> present;                       // chunk paths the reader asked for and found
	std::vector<std::string> unknown;                       // file nodes the reader never asked for
	std::vector<std::string> stale;                         // unread nodes of an older exporter's layout (kStaleFields)
	std::string szVariant;                                   // the struct a PARTICLE file was read into
};
SExportRead ReadExport( EExportKind kind, const std::string &szFile );

// Port export against the golden, both stats files of the given kind.
SCompareResult CompareStats( EExportKind kind, const std::string &szPortFile, const std::string &szGoldenFile );
// Files the game reads as bytes.
SCompareResult CompareBytes( const std::string &szPortFile, const std::string &szGoldenFile );
// A DXT texture: header equality, then bytes; see the note at the top.
SCompareResult CompareDxt( const std::string &szPortFile, const std::string &szGoldenFile );

}
