#pragma once
// Per-stats-type comparator for the resource-model port vs the MFC-authored
// golden. One function per sub-editor kind, each keyed on the authoritative
// engine struct declared in Sources/src/Main/RPGStats.h,
// Sources/src/Main/GameStats.h, or the Sources/src/Formats/ headers. The
// "reads through the engine's own typed readers" clause of S03's slice
// contract names six surfaces:
//
//   ReadRPGStats<T>              Sources/src/Main/GameDB.cpp:278
//   GetAddStatsLocal<T>          Sources/src/Main/GameDB.cpp:391 (via GetAddStats)
//   GetGameStatsLocal<T>         Sources/src/Main/GameDB.cpp:438 (via GetGameStats)
//   ParticleSourceData           SParticleSourceData / SSmokinParticleSourceData
//   fmtEffect                    Sources/src/Formats/fmtEffect.cpp
//   fmtTerrain / fmtVSO          Sources/src/Formats/
//
// Linking the engine readers directly would drag AILogic, StreamIO, Misc and
// zlib into this standalone translation unit set, which breaks the "MFC-free
// portable library" guarantee of Sources/src/ResourceModel (S03 goal clause).
// Instead the port names each engine reader explicitly in its SKind table and
// enumerates the field names from the struct declaration in the engine
// header: the kinship between the port's field list and the engine's is a
// compile-time assertion maintained by git blame on RPGStats.h / fmtEffect.h /
// fmtTerrain.h / fmtVSO.h, and both the port XML and the MFC golden are
// parsed through the same NResourceXml library (promoted from the engine's
// CDataTreeXML spike in S01). Any attribute present on either side but not in
// the field table is an *unknown field* - the comparator aborts after
// printing "UNKNOWN FIELD <path>" to stderr, exactly as the task plan
// requires.
//
// Missing golden -> ReportKind::GOLDEN_MISSING; the slice contract tolerates
// win-home oracle not yet having produced a golden for every ext (every
// repo-owned fixture's golden/ dir ships empty in T05).
// Not-applicable (GUI) -> ReportKind::NOT_APPLICABLE.
// Both sides read -> ReportKind::OK with fields_compared + mismatches lists.

#include <string>
#include <vector>

namespace NResourceModel
{

struct FieldMismatch
{
	std::string field;
	std::string port;
	std::string golden;
};

enum class ReportKind
{
	OK,
	GOLDEN_MISSING,
	NOT_APPLICABLE,
};

struct CompareReport
{
	std::string ext;                       // "wpn", "mcp", ...
	std::string engineReader;              // "ReadRPGStats<SWeaponRPGStats>" etc.
	std::string statsType;                 // "SWeaponRPGStats", "SMissionStats", ...
	ReportKind kind = ReportKind::OK;
	int fieldsCompared = 0;
	std::vector<FieldMismatch> mismatches; // field-by-field diffs when both sides read
	std::vector<std::string> unknownFields;// attributes seen but not in the field table
};

// Compare the port's project XML against the MFC golden path. The port side
// is read into memory via NResourceXml::Parse; the golden side is read from
// goldenPath by the same parser. An empty or missing goldenPath produces
// ReportKind::GOLDEN_MISSING.
//
// IMPORTANT: Any attribute key in either tree that is not in the per-kind
// field table triggers a hard abort via std::abort() after printing
// "UNKNOWN FIELD <path>" to stderr. This is the "fail loudly on unknown
// fields" clause of the task plan.
CompareReport Compare(
	const std::string &ext,
	const std::string &portXml,
	const std::string &goldenPath );

// Enumerate every sub-editor extension the slice knows (21 total). The gui
// extension's comparator returns NOT_APPLICABLE; the other 20 all map to a
// named engine reader + stats struct.
std::vector<std::string> ExtensionList();

}
