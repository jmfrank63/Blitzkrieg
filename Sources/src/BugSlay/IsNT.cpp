/*----------------------------------------------------------------------
       John Robbins - Microsoft Systems Journal Bugslayer Column

A simpler way to figure out the OS.
----------------------------------------------------------------------*/

#include "StdAfx.h"
#include "BugSlayer.h"
#include "Internal.h"

/*//////////////////////////////////////////////////////////////////////
                           File Scope Globals
//////////////////////////////////////////////////////////////////////*/
// Indicates that the version information is valid.
static BOOL g_bHasVersion = FALSE ;
// Indicates NT or 95/98.
static BOOL g_bIsNT = TRUE ;

BOOL STDCALL IsNT()
{
  typedef BOOL (WINAPI *GET_VERSION_EX_A_PROC)( LPOSVERSIONINFOA );
  if ( TRUE == g_bHasVersion )
    return ( TRUE == g_bIsNT );

  OSVERSIONINFO stOSVI;

  memset( &stOSVI, 0, sizeof(OSVERSIONINFO) );
  stOSVI.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );

  GET_VERSION_EX_A_PROC pGetVersionExA = reinterpret_cast<GET_VERSION_EX_A_PROC>( GetProcAddress( GetModuleHandleA( "kernel32.dll" ), "GetVersionExA" ) );
  BOOL bRet = ( pGetVersionExA != 0 ) ? pGetVersionExA( &stOSVI ) : FALSE;
  ASSERT( TRUE == bRet );
  if ( FALSE == bRet )
  {
    TRACE0( "GetVersionEx failed!\n" );
    return FALSE;
  }

  // Check the version and call the appropriate thing.
  if ( VER_PLATFORM_WIN32_NT == stOSVI.dwPlatformId )
    g_bIsNT = TRUE;
  else
    g_bIsNT = FALSE;
  g_bHasVersion = TRUE;
  return ( TRUE == g_bIsNT );
}


