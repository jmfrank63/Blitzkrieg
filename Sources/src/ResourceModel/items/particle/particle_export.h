#pragma once
// The Particle exporter and importer: CParticleFrame::SaveRPGStats,
// FillRPGStats, FillRPGStats2, GetRPGStats, GetRPGStats2 and LoadRPGStats
// (Sources/src/editor/ParticleFrm.cpp:274-498). ExportParticle is declared in
// stats_export.h with the other exporters; the functions here are the
// importer's halves, which the particle's BkResImportFromGame calls with the
// struct the engine's reader found.
//
// Simple or complex. MFC kept that in the frame (bComplexSource, flipped by the
// toolbar's Particle source button), not in the project file: LoadRPGStats set
// it from KeyData/ComplexParticleSource and nothing else read it back. The
// port has no frame, so a project says it with the one value only a complex
// source uses: it is complex when the "Particle reference" of the complex
// source item names an effect, and simple otherwise. MFC's own
// FillRPGStats2 refused an empty reference (it substituted
// Effects\particles\flame after a message box), so a complex project with no
// reference never reached a file there either. The importer follows the
// same rule: a complex file fills the reference, a simple one leaves it empty.
//
// MFC's GetRPGStats put only the texture name (simple) or the effect name
// (complex) back into the tree, because its project held the rest. The
// importer fills the whole tree from the stats, so a shipped source opens as a
// project: the basic info, the texture sizes and every curve. That is the
// port's addition; the exporter is MFC's.

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../../tree_item.h"

struct IDataTree;
struct SParticleSourceData;
struct SSmokinParticleSourceData;

namespace NResourceModel
{

// The tracks of a KeyData chunk as the file has them. The engine's reader
// is not the file: CTrack::operator& adds a key at 0 and at the end of the
// life, SParticleSourceData::Init gives the Speed track a midpoint key and a
// first key of (0, 1) and normalises the direction, and the comparator holds
// the exporter to the keys the file lists. The importer therefore reads the
// chunk a second time into this, and the curves take the file's keys.
// Times are the file's (thousandths of the life); a curve's x is the fraction.
struct SParticleRawFile
{
	// Keyed by the chunk name the engine reads (Density, Wight, ...); a chunk the file lacks has no entry.
	std::map<std::string, std::vector<std::pair<float, float>>> tracks;
	bool bFoundDirection = false;
	float vDirection[3] = { 0, 0, 0 };
	int operator&( IDataTree &ss );
};

// The one place that says whether a project is complex: the "Particle
// reference" (value 0) of the complex source item is non-empty. The exporter
// and the editor's source toggle both call it, so what the toolbar shows is
// what the file gets.
bool IsComplexSource( const CTreeItem &root );

// The tree half of LoadRPGStats for a simple source. root is a default tree
// (CreateDefaultChilds has run); szName is the source's name (the file's).
void ParticleStatsToTree( const SParticleSourceData &stats, const SParticleRawFile &raw, CTreeItem &root, const std::string &szName );
// The same for a complex source: the effect it scatters and the curves of the
// complex source and complex props items.
void ParticleStatsToTree( const SSmokinParticleSourceData &stats, const SParticleRawFile &raw, CTreeItem &root, const std::string &szName );

}
