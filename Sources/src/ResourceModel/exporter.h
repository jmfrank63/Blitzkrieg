#pragma once
// The per-kind export skeleton. MFC's CParentFrame::ExportProject asked the
// active frame to write its game data (SaveRPGStats + ExportFrameData); each
// sub-editor's slice ports its frame's half as one exporter function and
// registers it here under the project extension. The bridge owns everything
// around it (the export root from MOD settings, the staging folder, moving the
// files into place only when the whole export succeeded, the report), so an
// exporter only writes files below SExportContext::szStagingRoot.
//
// A kind with no registered exporter is not exported: BkResExport answers
// BK_EDITOR_REFUSED and says the exporter is not ported yet. Nothing pretends
// to have written game data it did not write. The table starts with the
// exporters ported so far (S06: wpn, mcp, trc, scp; items/stats_export.h).

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "grid_projection.h"
#include "project.h"

namespace NResourceModel
{

struct SExportContext
{
	std::string szProjectPath;   // the project file; sources are relative to its folder
	std::string szStagingRoot;   // the export root's data/ folder, staged: write below it
	bool bForce = false;         // MFC's -f: export even when the files are up to date
	bool bStatsOnly = false;     // D-13: write the stats, leave exported graphics untouched
	// The export root's data/ folder as it stands before this export, which
	// the up-to-date check of the graphics reads (MFC compared the source
	// files with the export it had already made). Empty for an export that
	// has nothing to compare with, such as the preview's: everything is
	// written.
	std::string szDataRoot;

	// D015: the objects database MFC's frames asked through IObjectsDB, which
	// an exporter does not own. Given a resource path as MFC builds it (lower
	// case, backslashes, e.g. "units\humans\ussr\mosin"), the key name of the
	// sprite unit stored there, or false when no such unit is known. The
	// bridge fills it from the engine's IObjectsDB; tests pass a fixture
	// table. Empty: a squad member given as a path cannot be resolved and the
	// squad export fails, naming the member.
	std::function<bool( const std::string &szPath, std::string &szKey )> findUnitKey;

	// The fire places of a trench segment model (a .mod file): the positions
	// of its locators with the mesh at the origin, as CTrenchFrame::SaveRPGStats
	// reads them from the mesh IVisObjBuilder builds. That needs the engine's
	// mesh builder, which the bridge has and a data-only host does not; empty
	// here, a trench with a segment model fails its export instead of writing
	// a segment without fire places. False with szError when the model
	// cannot be built (MFC's "Cannot create model": the segment is skipped).
	std::function<bool( const std::string &szModFile, std::vector<std::pair<float, float>> &firePlaces, std::string &szError )> meshFirePlaces;

	// The weapon lookup CMeshFrame::FillRPGStats made through IObjectsDB
	// (gun.RetrieveShortcuts): whether the weapon fires a howitzer or cannon
	// shell, which makes the platform's elevation that of the shoot point.
	// False when the weapon is not known. The bridge fills it from the engine;
	// tests pass a fixture table. Empty: every weapon is unknown, which the
	// export reports as a warning and treats as not ballistic.
	std::function<bool( const std::string &szWeapon, bool &bBallistic )> isBallisticWeapon;

	// The camera of the editor scene, which an object, fence or building
	// export reads where MFC called IScene::GetPos2 (the sprite's and the
	// zero cross's screen positions, and the grid origin on screen). The
	// bridge fills it from the engine's scene. Empty: the exporter uses
	// DefaultEditorCamera() and says so in a warning.
	std::function<bool( SGroundCamera &camera )> groundCamera;
};

struct SExportOutcome
{
	int nWritten = 0;                    // files written below the staging root
	int nSkipped = 0;                    // files left alone as up to date
	std::vector<std::string> warnings;   // what MFC's output pane collected
	std::string szError;                 // why the export failed, when it returns false
	// The data name of the visual the export wrote, as IVisObjBuilder::
	// BuildObject takes it (backslashes, no extension, relative to the staging
	// root, e.g. "units\technics\tiger\1"). BkResPreviewShow builds this
	// from the preview storage (D-16); empty for a kind with nothing to draw.
	std::string szObjectName;
};

// Exports project into context.szStagingRoot. False with outcome.szError on a
// failure; the bridge then discards the staging folder.
using FExporter = bool ( * )( const Project &project, const SExportContext &context, SExportOutcome &outcome );

// Registers (or, with a null function, removes) the exporter of an extension
// ("wpn", lower case, no dot). The last registration wins.
void RegisterExporter( const std::string &szExtension, FExporter pfnExporter );
// The exporter of an extension, or null when that kind is not ported yet.
FExporter FindExporter( const std::string &szExtension );

}
