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
// to have written game data it did not write.

#include <string>
#include <vector>

#include "project.h"

namespace NResourceModel
{

struct SExportContext
{
	std::string szProjectPath;   // the project file; sources are relative to its folder
	std::string szStagingRoot;   // the export root's data/ folder, staged: write below it
	bool bForce = false;         // MFC's -f: export even when the files are up to date
	bool bStatsOnly = false;     // D-13: write the stats, leave exported graphics untouched
};

struct SExportOutcome
{
	int nWritten = 0;                    // files written below the staging root
	int nSkipped = 0;                    // files left alone as up to date
	std::vector<std::string> warnings;   // what MFC's output pane collected
	std::string szError;                 // why the export failed, when it returns false
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
