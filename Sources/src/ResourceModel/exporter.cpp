#include "exporter.h"

#include <map>

#include "items/stats_export.h"

namespace NResourceModel
{

namespace
{

// A function-local map so a registration from another translation unit's
// static initialiser never meets an unconstructed table. It starts with the
// exporters the sub-editor slices have ported, named here rather than
// registered by their own static initialisers: those would sit in a static
// archive that nothing references, and the linker would drop them.
std::map<std::string, FExporter> &Exporters()
{
	static std::map<std::string, FExporter> exporters = {
		{ "wpn", &ExportWeapon },
		{ "mcp", &ExportMine },
		{ "trc", &ExportTrench },
		{ "scp", &ExportSquad },
		{ "spt", &ExportSprite },
		{ "unt", &ExportInfantry },
		{ "msh", &ExportMesh },
		{ "obt", &ExportObject },
		{ "fnc", &ExportFence },
		{ "bld", &ExportBuilding },
		{ "bdg", &ExportBridge },
		{ "pcp", &ExportParticle },
		{ "eff", &ExportEffect },
		{ "til", &ExportTileSet },
		{ "3rd", &ExportRoad3D },
		{ "3rv", &ExportRiver3D },
		{ "mdc", &ExportMedal },
	};
	return exporters;
}

}

void RegisterExporter( const std::string &szExtension, FExporter pfnExporter )
{
	if ( pfnExporter == nullptr )
		Exporters().erase( szExtension );
	else
		Exporters()[ szExtension ] = pfnExporter;
}

FExporter FindExporter( const std::string &szExtension )
{
	const auto it = Exporters().find( szExtension );
	return it == Exporters().end() ? nullptr : it->second;
}

}
