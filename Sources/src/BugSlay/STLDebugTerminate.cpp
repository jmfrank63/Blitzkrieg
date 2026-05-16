#include "StdAfx.h"

#define _DO_ASSERT_SLOW 1
#include "BugSlayer.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static std::list<std::string> szSTLDebugMessages;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void __stl_debug_message( const char *pszFormat, ... )
{
  static char buff[2048];
  va_list va;
	// 
  va_start( va, pszFormat );
	vsprintf_s( buff, sizeof(buff), pszFormat, va );
  va_end( va );

	szSTLDebugMessages.push_back( buff );
	//
	OutputDebugString( buff );
	OutputDebugString( "\n" );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void __stl_debug_terminate()
{
	std::string szSTLError;
	for ( std::list<std::string>::const_iterator it = szSTLDebugMessages.begin(); it != szSTLDebugMessages.end(); ++it )
	{
		szSTLError += *it;
		szSTLError += "\n";
	}
	//
	switch ( NBugSlayer::ReportAssert( "__stl_debug_terminate", szSTLError.c_str(), __FILE__, __LINE__, false ) )
	{
	case BSU_DEBUG:
		DEBUG_BREAK;
		break;
	case BSU_IGNORE:
		break;
	case BSU_ABORT:
		SetCrashHandlerFilter( 0 );
		abort();
		break;
	}
	szSTLDebugMessages.clear();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
