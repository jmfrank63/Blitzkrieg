#include "exporter.h"

#include <map>

namespace NResourceModel
{

namespace
{

// A function-local map so a registration from another translation unit's
// static initialiser never meets an unconstructed table.
std::map<std::string, FExporter> &Exporters()
{
	static std::map<std::string, FExporter> exporters;
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
