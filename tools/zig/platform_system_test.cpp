#include "Platform/System.h"

#include <string>
#include <vector>

static std::string capturedTitle;
static std::string capturedText;
static std::string capturedUrl;

static bool CaptureError( const char *title, const char *text )
{
	capturedTitle = title != nullptr ? title : "";
	capturedText = text != nullptr ? text : "";
	return true;
}

static bool CaptureOpen( const char *url, const char * )
{
	capturedUrl = url != nullptr ? url : "";
	return true;
}

int main( int argc, char **argv )
{
	if ( argc > 1 && std::string( argv[1] ) == "--child" ) return 17;
	if ( NPlatform::ExecutablePath().empty() ) return 1;
	if ( !NPlatform::SetEnvironment( "BK_PLATFORM_SYSTEM_TEST", "ok" ) || NPlatform::GetEnvironment( "BK_PLATFORM_SYSTEM_TEST" ) != "ok" ) return 2;
	NPlatform::SetUiHandlers( CaptureError, CaptureOpen );
	if ( !NPlatform::ShowError( "Test title", "Test text" ) || capturedTitle != "Test title" || capturedText != "Test text" ) return 3;
	if ( !NPlatform::OpenUrl( "https://example.invalid/test" ) || capturedUrl != "https://example.invalid/test" ) return 4;
	if ( !NPlatform::OpenFile( "C:\\temp\\test.txt" ) || capturedUrl != "file:///C:/temp/test.txt" ) return 5;
	int exitCode = -1;
	const std::string childName =
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
		"platform-system-test.exe";
#else
		"platform-system-test";
#endif
	if ( !NPlatform::RunProcess( { NPlatform::ExecutablePath() + childName, "--child" }, std::string(), &exitCode ) || exitCode != 17 ) return 6;
	return 0;
}
