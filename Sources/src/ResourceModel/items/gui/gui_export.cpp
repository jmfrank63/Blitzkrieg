// The GUI exporter (see gui_export.h). The screen is not a project tree: the bridge hands its
// current text in SExportContext::szScreenText, so what is exported is what a save would write.
#include "gui_export.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#include "../../ui_screen.h"

namespace NResourceModel
{

namespace
{

std::string Folded( const std::filesystem::path &path )
{
	std::error_code ec;
	std::string sz = std::filesystem::weakly_canonical( path, ec ).generic_string();
	std::transform( sz.begin(), sz.end(), sz.begin(), []( unsigned char c ) { return char( std::tolower( c ) ); } );
	return sz;
}

}

bool ExportGui( const Project &, const SExportContext &context, SExportOutcome &outcome )
{
	if ( context.szScreenName.empty() || context.szScreenText.empty() )
	{
		outcome.szError = "gui export refused: the project has no screen name, save it as <Screen>.gui or <Screen>.xml first (the game finds the screen by that name)";
		return false;
	}
	const std::filesystem::path target = std::filesystem::path( context.szStagingRoot ) / "ui" / ( context.szScreenName + ".xml" );
	// The bridge refuses a shipped-Data export root before it gets here; this check is for a
	// caller that stages straight into the folder the editor reads its own art from.
	if ( !context.szEditorDataDir.empty() && !context.szDataRoot.empty() && Folded( context.szDataRoot ) == Folded( context.szEditorDataDir ) )
	{
		outcome.szError = "gui export refused: " + target.generic_string() + " would be written into the shipped Data folder, which is never written; export into a mod folder";
		return false;
	}
	CUiScreen screen;
	std::string szError;
	if ( !screen.Open( context.szScreenText, context.szScreenName + ".xml", szError ) || !screen.Validate( szError ) )
	{
		outcome.szError = "gui export refused: " + szError;
		return false;
	}
	std::error_code ec;
	std::filesystem::create_directories( target.parent_path(), ec );
	std::ofstream file( target, std::ios::binary | std::ios::trunc );
	const std::string szBase = screen.SaveAsBase();
	if ( !file.write( szBase.data(), std::streamsize( szBase.size() ) ).good() )
	{
		outcome.szError = "gui export failed: cannot write " + target.generic_string();
		return false;
	}
	file.close();
	++outcome.nWritten;
	return true;
}

}
