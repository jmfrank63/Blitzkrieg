/*----------------------------------------------------------------------
 John Robbins - Microsoft Systems Journal Bugslayer Column - August '98

CONDITIONAL COMPILATION :
    WORK_AROUND_SRCLINE_BUG - Define this to work around the
                              SymGetLineFromAddr bug where PDB file
                              lookups fail after the first lookup.
----------------------------------------------------------------------*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "CallStack.h"
// The project internal header file.
#include "Internal.h"

#define WORK_AROUND_SRCLINE_BUG 1
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*//////////////////////////////////////////////////////////////////////
                      File Scope Global Variables
//////////////////////////////////////////////////////////////////////*/
// The filter function.
static PFNCHFILTFN g_pfnCallBack = 0;

// The original exception filter.
static LPTOP_LEVEL_EXCEPTION_FILTER g_pfnOrigFilt = 0;

// The array of modules to limit Crash Handler to.
static HMODULE *g_ahMod = 0;
// The size, in items, of g_ahMod.
static UINT g_uiModCount = 0;

// The static buffer returned by various functions.  This avoids putting
//  things on the stack.
static const DWORD BUFF_SIZE = 1024;
static TCHAR g_szBuff[BUFF_SIZE];

// The static symbol lookup buffer.  This gets casted to make it work.
static const DWORD SYM_BUFF_SIZE = 512;
static BYTE g_stSymbol[SYM_BUFF_SIZE];

// The static source and line structure.
static IMAGEHLP_LINE g_stLine;

// The stack frame used in walking the stack.
static STACKFRAME g_stFrame;

// The pointer to the SymGetLineFromAddr function I GetProcAddress out
//  of IMAGEHLP.DLL in case the user has an older version that does not
//  support the new extensions.
static PFNSYMGETLINEFROMADDR g_pfnSymGetLineFromAddr = 0;

// The flag that says if I have already done the GetProcAddress on
//  g_pfnSymGetLineFromAddr.
static BOOL g_bLookedForSymFuncs = FALSE;

// The flag that indicates that the symbol engine as been initialized.
static BOOL g_bSymEngInit = FALSE;
//
static std::list<std::string> ignoremodules;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline int MSVCMustDie_tolower( int a ) { return tolower(a); } 
void STDCALL AddIgnoreModule( const char *pszModuleName )
{
	ignoremodules.push_back( pszModuleName );
	std::string &szString = ignoremodules.back();
    std::transform( szString.begin(), szString.end(), szString.begin(), MSVCMustDie_tolower );
}
/*//////////////////////////////////////////////////////////////////////
                    File Scope Function Declarations
//////////////////////////////////////////////////////////////////////*/
// The exception handler.
LONG STDCALL CrashHandlerExceptionFilter( EXCEPTION_POINTERS *pExPtrs );

// Converts a simple exception to a string value.
LPCTSTR ConvertSimpleException( DWORD dwExcept );

// The internal function that does all the stack walking.
LPCTSTR STDCALL InternalGetStackTraceString( DWORD dwOpts, EXCEPTION_POINTERS *pExPtrs );

// The internal SymGetLineFromAddr function.
BOOL InternalSymGetLineFromAddr( IN  HANDLE          hProcess,
                                 IN  DWORD           dwAddr,
                                 OUT PDWORD          pdwDisplacement,
                                 OUT PIMAGEHLP_LINE  Line );

// Initializes the symbol engine if needed.
void InitSymEng();

// Cleans up the symbol engine if needed.
void CleanupSymEng();

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*//////////////////////////////////////////////////////////////////////
                            Destructor Class
//////////////////////////////////////////////////////////////////////*/
// See the note in MemDumpValidator.cpp about automatic classes.
#pragma warning (disable : 4073)
#pragma init_seg(lib)
class CleanUpCrashHandler
{
public:
  CleanUpCrashHandler()
  {
  }
  ~CleanUpCrashHandler()
  {
    // Is there any outstanding memory allocations?
    if ( 0 != g_ahMod )
    {
      VERIFY( HeapFree(GetProcessHeap(), 0, g_ahMod) ) ;
      g_ahMod = 0 ;
    }
    // Set the handler back to what it originally was.
    if ( 0 != g_pfnOrigFilt )
      SetUnhandledExceptionFilter( g_pfnOrigFilt );
  }
};
// The static class.
static CleanUpCrashHandler g_cBeforeAndAfter ;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*//////////////////////////////////////////////////////////////////////
                 Crash Handler Function Implementation
//////////////////////////////////////////////////////////////////////*/
BOOL STDCALL SetCrashHandlerFilter( PFNCHFILTFN pFn )
{
  // It's OK to have a 0 parameter because this will unhook the
  //  callback.
  if ( 0 == pFn )
  {
    if ( 0 != g_pfnOrigFilt )
    {
      // Put the original one back.
      SetUnhandledExceptionFilter( g_pfnOrigFilt );
      g_pfnOrigFilt = 0;
      if ( 0 != g_ahMod )
      {
        // FIXED BUG: Previously, I called "free" instead of "HeapFree."
        VERIFY( HeapFree(GetProcessHeap(), 0, g_ahMod ) );
        g_ahMod = 0;
      }
      g_pfnCallBack = 0;
    }
  }
  else
  {
    g_pfnCallBack = pFn;

    // If this is the first time that CrashHandler has been called
    //  set the exception filter and save off the previous handler.
    if ( 0 == g_pfnOrigFilt )
      g_pfnOrigFilt = SetUnhandledExceptionFilter( CrashHandlerExceptionFilter );
  }
  return TRUE;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL STDCALL AddCrashHandlerLimitModule( HMODULE hMod )
{
    // Check the obvious cases.
    ASSERT ( 0 != hMod );
    if ( 0 == hMod )
			return FALSE;

    // TODO TODO
    //  Do the check that hMod really is a PE module.

    // Allocate a temporary version.  This must be allocated into memory
    //  that is guaranteed to be around even if the process is toasting.
    //  This means the RTL heap is probably already gone so I do it out
    //  of the process heap.
    HMODULE *phTemp = (HMODULE*)HeapAlloc( GetProcessHeap(), 
			                                     HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,
                                           sizeof(HMODULE)*(g_uiModCount + 1) );
    ASSERT( 0 != phTemp );
    if ( 0 == phTemp )
    {
      TRACE0( "Serious trouble in the house! - malloc failed!!!\n" );
      return FALSE;
    }

    if ( 0 == g_ahMod )
    {
      g_ahMod = phTemp ;
      g_ahMod[0] = hMod ;
      g_uiModCount++ ;
    }
    else
    {
      // Copy the old values.
      CopyMemory( phTemp, g_ahMod, sizeof(HMODULE) * g_uiModCount );
      g_ahMod = phTemp;
      g_ahMod[ g_uiModCount ] = hMod;
      g_uiModCount++;
    }
    return TRUE;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UINT STDCALL GetLimitModuleCount()
{
  return g_uiModCount;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STDCALL GetLimitModulesArray( HMODULE *pahMod, UINT uiSize )
{
  int iRet;

  __try
  {
    ASSERT( FALSE == IsBadWritePtr(pahMod, uiSize*sizeof(HMODULE)) );
    if ( TRUE == IsBadWritePtr(pahMod, uiSize*sizeof(HMODULE)) )
    {
      iRet = GLMA_BADPARAM;
      __leave;
    }

    if ( uiSize < g_uiModCount )
    {
      iRet = GLMA_BUFFTOOSMALL;
      __leave;
    }

    CopyMemory( pahMod, g_ahMod, sizeof(HMODULE)*g_uiModCount );

    iRet = GLMA_SUCCESS;
  }
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
    iRet = GLMA_FAILURE ;
  }
  return iRet;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LONG STDCALL CrashHandlerExceptionFilter( EXCEPTION_POINTERS *pExPtrs )
{
  LONG lRet = EXCEPTION_CONTINUE_SEARCH ;

  __try
  {
    if ( 0 != g_pfnCallBack )
    {
      // The symbol engine has to be initialized here so that
      //  I can look up the base module information for the
      //  crash address as well as just get the symbol engine
      //  ready.
      InitSymEng();

      // Check the g_ahMod list.
      BOOL bCallIt = FALSE;
      if ( 0 == g_uiModCount )
        bCallIt = TRUE;
      else
      {
        HINSTANCE hBaseAddr = (HINSTANCE)SymGetModuleBase( (HANDLE)GetCurrentProcessId(),
                                                           (DWORD)pExPtrs->ExceptionRecord->ExceptionAddress );
        if ( 0 != hBaseAddr )
        {
          for ( UINT i = 0; i < g_uiModCount; i++ )
          {
            if ( hBaseAddr == g_ahMod[i] )
            {
              bCallIt = TRUE;
              break;
            }
          }
        }
      }
      if ( TRUE == bCallIt )
      {
          // Check that the filter function still exists in memory
          //  before I call it.  The user might have forgotten to
          //  unregister and the filter function is invalid
          //  because it got unloaded.  Of course, if something
          //  loaded back into the same address, there is not much
          //  I can do.
          if ( FALSE == IsBadCodePtr((FARPROC)g_pfnCallBack) )
            lRet = (*g_pfnCallBack)( pExPtrs );
      }
      else
      {
        // Call the previous filter but only after it checks
        //  out.  I am just being a little paranoid.
        if ( FALSE == IsBadCodePtr((FARPROC)g_pfnOrigFilt) )
          lRet = (*g_pfnOrigFilt)( pExPtrs );
      }
      CleanupSymEng();
    }
  }
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
    lRet = EXCEPTION_CONTINUE_SEARCH ;
  }
  return lRet;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*//////////////////////////////////////////////////////////////////////
         EXCEPTION_POINTER Translation Functions Implementation
//////////////////////////////////////////////////////////////////////*/

LPCTSTR STDCALL GetFaultReason( EXCEPTION_POINTERS  *pExPtrs )
{
  ASSERT( FALSE == IsBadReadPtr( pExPtrs, sizeof(EXCEPTION_POINTERS) ) ) ;
  if ( TRUE == IsBadReadPtr( pExPtrs, sizeof(EXCEPTION_POINTERS) ) )
  {
    TRACE0( "Bad parameter to GetFaultReasonA\n" ) ;
    return 0;
  }

  // The value that holds the return.
  LPCTSTR szRet = 0;
  __try
  {
    // Initialize the symbol engine in case it is not initialized.
    InitSymEng();

    // The current position in the buffer.
    int iCurr = 0;
    // A temp value holder.  This is to keep the stack usage to a
    //  minimum.
    iCurr += BSUGetModuleBaseName( GetCurrentProcess(), 0, g_szBuff, BUFF_SIZE );
    iCurr += wsprintf ( g_szBuff + iCurr, _T(" caused a ") );

    DWORD dwTemp = (DWORD)ConvertSimpleException( pExPtrs->ExceptionRecord->ExceptionCode );

    if ( 0 != dwTemp )
      iCurr += wsprintf( g_szBuff + iCurr, _T("%s"), dwTemp );
    else
    {
      iCurr += ( FormatMessage( FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                                GetModuleHandle( _T("NTDLL.DLL") ),
                                pExPtrs->ExceptionRecord->ExceptionCode,
                                0,
                                g_szBuff + iCurr,
                                BUFF_SIZE,
                                0 ) * sizeof(TCHAR) );
    }

    ASSERT( iCurr < BUFF_SIZE );

    iCurr += wsprintf( g_szBuff + iCurr, _T(" in module ") );

    dwTemp = SymGetModuleBase( (HANDLE)GetCurrentProcessId(), (DWORD)pExPtrs->ExceptionRecord->ExceptionAddress );
    ASSERT( 0 != dwTemp ) ;

    if ( 0 == dwTemp )
      iCurr += wsprintf( g_szBuff + iCurr, _T("<UNKNOWN>") );
    else
      iCurr += BSUGetModuleBaseName( GetCurrentProcess(), (HINSTANCE)dwTemp, g_szBuff + iCurr, BUFF_SIZE - iCurr );

#ifdef _ALPHA_
    iCurr += wsprintf( g_szBuff + iCurr, _T(" at %08X"), pExPtrs->ExceptionRecord->ExceptionAddress );
#else
    iCurr += wsprintf( g_szBuff + iCurr, _T(" at %04X:%08X" ), pExPtrs->ContextRecord->SegCs, pExPtrs->ExceptionRecord->ExceptionAddress );
#endif

    ASSERT( iCurr < BUFF_SIZE );

    // Start looking up the exception address.
    //lint -e545
    PIMAGEHLP_SYMBOL pSym = (PIMAGEHLP_SYMBOL)&g_stSymbol;
    //lint +e545
    FillMemory( pSym , 0 , SYM_BUFF_SIZE );
    pSym->SizeOfStruct = sizeof( IMAGEHLP_SYMBOL );
    pSym->MaxNameLength = SYM_BUFF_SIZE - sizeof( IMAGEHLP_SYMBOL);

    DWORD dwDisp ;
    if ( TRUE == SymGetSymFromAddr((HANDLE)GetCurrentProcessId(), (DWORD)pExPtrs->ExceptionRecord->ExceptionAddress, &dwDisp, pSym) )
    {
      iCurr += wsprintf( g_szBuff + iCurr, _T(", ") );
      // Copy no more than there is room for.
      dwTemp = lstrlen( pSym->Name );
      if ( (int)dwTemp > ( BUFF_SIZE - iCurr - 20 ) )
      {
        lstrcpyn( g_szBuff + iCurr, pSym->Name, BUFF_SIZE - iCurr - 1 );
        // Gotta leave now.
        szRet = g_szBuff;
        __leave ;
      }
      else
      {
        if ( dwDisp > 0 )
          iCurr += wsprintf( g_szBuff + iCurr, _T("%s()+%d byte(s)"), pSym->Name, dwDisp );
        else
          iCurr += wsprintf( g_szBuff + iCurr, _T("%s "), pSym->Name );
      }
    }
    else
    {
      // If the symbol was not found, the source and line will not
      //  be found either so leave now.
      szRet = g_szBuff;
      __leave;
    }

    ASSERT( iCurr < BUFF_SIZE );

    // Do the source and line lookup.
    ZeroMemory( &g_stLine , sizeof( IMAGEHLP_LINE ) );
    g_stLine.SizeOfStruct = sizeof( IMAGEHLP_LINE );

    if ( TRUE == InternalSymGetLineFromAddr((HANDLE)GetCurrentProcessId(), (DWORD)pExPtrs->ExceptionRecord->ExceptionAddress, &dwDisp, &g_stLine) )
    {
      iCurr += wsprintf( g_szBuff + iCurr, _T(", ") );

      // Copy no more than there is room for.
      dwTemp = lstrlen( g_stLine.FileName );
      if ( (int)dwTemp > ( BUFF_SIZE - iCurr - 25 ) )
      {
        lstrcpyn ( g_szBuff + iCurr, g_stLine.FileName, BUFF_SIZE - iCurr - 1 );
        // Gotta leave now.
        szRet = g_szBuff ;
        __leave ;
      }
      else
      {
        if ( dwDisp > 0 )
          iCurr += wsprintf( g_szBuff + iCurr, _T("%s, line %d+%d byte(s)"), g_stLine.FileName, g_stLine.LineNumber, dwDisp );
        else
          iCurr += wsprintf( g_szBuff + iCurr, _T("%s, line %d"), g_stLine.FileName, g_stLine.LineNumber );
      }
    }
    szRet = g_szBuff ;
  }
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
    ASSERT( FALSE );
    szRet = 0 ;
  }
  return szRet;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL STDCALL GetFaultReasonVB( EXCEPTION_POINTERS * pExPtrs, LPTSTR szBuff, UINT uiSize )
{
  ASSERT ( FALSE == IsBadWritePtr(szBuff, uiSize) );
  if ( TRUE == IsBadWritePtr(szBuff, uiSize) )
    return FALSE;

  LPCTSTR szRet;

  __try
  {

    szRet = GetFaultReason( pExPtrs );

    ASSERT( 0 != szRet );
    if ( 0 == szRet )
      __leave ;
    lstrcpyn( szBuff, szRet, Min( (UINT)lstrlen(szRet) + 1, uiSize ) );
  }
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
    szRet = 0;
  }
  return ( 0 != szRet );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LPCTSTR STDCALL GetFirstStackTraceString( DWORD dwOpts, EXCEPTION_POINTERS *pExPtrs )
{
  // All of the error checking is in the InternalGetStackTraceString
  //  function.

  // Initialize the STACKFRAME structure.
  ZeroMemory( &g_stFrame, sizeof(STACKFRAME) );

#ifdef _X86_
  g_stFrame.AddrPC.Offset     = pExPtrs->ContextRecord->Eip;
  g_stFrame.AddrPC.Mode       = AddrModeFlat;
  g_stFrame.AddrStack.Offset  = pExPtrs->ContextRecord->Esp;
  g_stFrame.AddrStack.Mode    = AddrModeFlat;
  g_stFrame.AddrFrame.Offset  = pExPtrs->ContextRecord->Ebp;
  g_stFrame.AddrFrame.Mode    = AddrModeFlat;
#else
  g_stFrame.AddrPC.Offset     = (DWORD)pExPtrs->ContextRecord->Fir ;
  g_stFrame.AddrPC.Mode       = AddrModeFlat;
  g_stFrame.AddrReturn.Offset = (DWORD)pExPtrs->ContextRecord->IntRa;
  g_stFrame.AddrReturn.Mode   = AddrModeFlat;
  g_stFrame.AddrStack.Offset  = (DWORD)pExPtrs->ContextRecord->IntSp;
  g_stFrame.AddrStack.Mode    = AddrModeFlat;
  g_stFrame.AddrFrame.Offset  = (DWORD)pExPtrs->ContextRecord->IntFp;
  g_stFrame.AddrFrame.Mode    = AddrModeFlat;
#endif

  return InternalGetStackTraceString( dwOpts, pExPtrs );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LPCTSTR STDCALL GetNextStackTraceString( DWORD dwOpts, EXCEPTION_POINTERS *pExPtrs )
{
  // All error checking is in InternalGetStackTraceString.
  // Assume that GetFirstStackTraceString has already initialized the
  //  stack frame information.
  return InternalGetStackTraceString( dwOpts, pExPtrs );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL STDCALL CH_ReadProcessMemory( HANDLE , LPCVOID lpBaseAddress, LPVOID lpBuffer, 
																		  DWORD nSize, LPDWORD lpNumberOfBytesRead )
{
  return ReadProcessMemory( GetCurrentProcess(), lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The internal function that does all the stack walking.
bool STDCALL GetStackTrace( EXCEPTION_POINTERS *pExPtrs, SCallStackEntry &entry )
{
  ASSERT ( FALSE == IsBadReadPtr(pExPtrs, sizeof(EXCEPTION_POINTERS)) );
  if ( TRUE == IsBadReadPtr(pExPtrs, sizeof(EXCEPTION_POINTERS)) )
  {
    TRACE0( "GetStackTrace - invalid pExPtrs!\n" );
    return false;
  }
  // A temporary for all to use.  This saves stack space.
  DWORD dwTemp = 0;

  __try
  {
    // Initialize the symbol engine in case it is not initialized.
    InitSymEng();

#ifdef _ALPHA_
#define CH_MACHINE IMAGE_FILE_MACHINE_ALPHA
#else
#define CH_MACHINE IMAGE_FILE_MACHINE_I386
#endif
    // Note:  If the source and line functions are used, then
    //        StackWalk can access violate.
    BOOL bSWRet = StackWalk( CH_MACHINE,
                             (HANDLE)GetCurrentProcessId(),
                             GetCurrentThread(),
                             &g_stFrame,
                             pExPtrs->ContextRecord,
                             (PREAD_PROCESS_MEMORY_ROUTINE)CH_ReadProcessMemory,
                             SymFunctionTableAccess,
                             SymGetModuleBase,
                             0 );
    if ( (FALSE == bSWRet) || (0 == g_stFrame.AddrFrame.Offset) )
			return false;

    int iCurr = 0 ;

    // Do the parameters?
    {
			entry.params[0] = g_stFrame.Params[0];
			entry.params[1] = g_stFrame.Params[1];
			entry.params[2] = g_stFrame.Params[2];
			entry.params[3] = g_stFrame.Params[3];
    }

    {
      iCurr += wsprintf( g_szBuff + iCurr, _T(" ") );

      dwTemp = SymGetModuleBase( (HANDLE)GetCurrentProcessId(), g_stFrame.AddrPC.Offset );

      ASSERT( 0 != dwTemp );

      if ( 0 == dwTemp )
				entry.szModuleName = _T( "<UNKNOWN>" );
      else
			{
        BSUGetModuleBaseName( GetCurrentProcess(), (HINSTANCE)dwTemp, g_szBuff, BUFF_SIZE );
				entry.szModuleName = g_szBuff;
			}
    }

    DWORD dwDisp;

    {
      // Start looking up the exception address.
      //lint -e545
      PIMAGEHLP_SYMBOL pSym = (PIMAGEHLP_SYMBOL)&g_stSymbol;
      //lint +e545
      ZeroMemory( pSym, SYM_BUFF_SIZE );
      pSym->SizeOfStruct = sizeof( IMAGEHLP_SYMBOL );
      pSym->MaxNameLength = SYM_BUFF_SIZE - sizeof( IMAGEHLP_SYMBOL );

      if ( TRUE == SymGetSymFromAddr((HANDLE)GetCurrentProcessId(), g_stFrame.AddrPC.Offset, &dwDisp, pSym) )
      {
				entry.szFunctionName = pSym->Name;
				entry.dwFunctionDisp = dwDisp;
      }
      else
      {
				entry.szFunctionName = _T( "<UNKNOWN>" );
				entry.dwFunctionDisp = 0;
        // If the symbol was not found, the source and line will
        //  not be found either so leave now.
				entry.szFileName = _T( "<UNKNOWN>" );
				entry.nLineNumber = 0;
				entry.dwLineDisp = 0;
				return true;
      }

    }

    {
      ZeroMemory ( &g_stLine , sizeof(IMAGEHLP_LINE) );
      g_stLine.SizeOfStruct = sizeof( IMAGEHLP_LINE );

      if ( TRUE == InternalSymGetLineFromAddr((HANDLE)GetCurrentProcessId(), g_stFrame.AddrPC.Offset, &dwDisp, &g_stLine) )
      {
				entry.szFileName = g_stLine.FileName;
				entry.nLineNumber = g_stLine.LineNumber;
				entry.dwLineDisp = dwDisp;
			}
			else
			{
				entry.szFileName = _T( "<UNKNOWN>" );
				entry.nLineNumber = 0;
				entry.dwLineDisp = 0;
			}
		}
	}
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
		return false;
  }
  return true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool STDCALL GetFirstStackTrace( EXCEPTION_POINTERS *pExPtrs, SCallStackEntry &entry )
{
  // All of the error checking is in the GetStackTrace function.
  // Initialize the STACKFRAME structure.
  ZeroMemory( &g_stFrame, sizeof(STACKFRAME) );
#ifdef _X86_
  g_stFrame.AddrPC.Offset     = pExPtrs->ContextRecord->Eip;
  g_stFrame.AddrPC.Mode       = AddrModeFlat;
  g_stFrame.AddrStack.Offset  = pExPtrs->ContextRecord->Esp;
  g_stFrame.AddrStack.Mode    = AddrModeFlat;
  g_stFrame.AddrFrame.Offset  = pExPtrs->ContextRecord->Ebp;
  g_stFrame.AddrFrame.Mode    = AddrModeFlat;
#else
  g_stFrame.AddrPC.Offset     = (DWORD)pExPtrs->ContextRecord->Fir ;
  g_stFrame.AddrPC.Mode       = AddrModeFlat;
  g_stFrame.AddrReturn.Offset = (DWORD)pExPtrs->ContextRecord->IntRa;
  g_stFrame.AddrReturn.Mode   = AddrModeFlat;
  g_stFrame.AddrStack.Offset  = (DWORD)pExPtrs->ContextRecord->IntSp;
  g_stFrame.AddrStack.Mode    = AddrModeFlat;
  g_stFrame.AddrFrame.Offset  = (DWORD)pExPtrs->ContextRecord->IntFp;
  g_stFrame.AddrFrame.Mode    = AddrModeFlat;
#endif

  return GetStackTrace( pExPtrs, entry );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool STDCALL GetNextStackTrace( EXCEPTION_POINTERS *pExPtrs, SCallStackEntry &entry )
{
  // All error checking is in GetStackTrace.
  // Assume that GetFirstStackTrace has already initialized the stack frame information.
  return GetStackTrace( pExPtrs, entry );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LPCTSTR STDCALL InternalGetStackTraceString( DWORD dwOpts, EXCEPTION_POINTERS *pExPtrs )
{
  ASSERT ( FALSE == IsBadReadPtr(pExPtrs, sizeof(EXCEPTION_POINTERS)) );
  if ( TRUE == IsBadReadPtr(pExPtrs, sizeof(EXCEPTION_POINTERS)) )
  {
    TRACE0( "GetStackTraceString - invalid pExPtrs!\n" );
    return 0;
  }

  // The value that is returned.
  LPCTSTR szRet;
  // A temporary for all to use.  This saves stack space.
  DWORD dwTemp;

  __try
  {
    // Initialize the symbol engine in case it is not initialized.
    InitSymEng();

#ifdef _ALPHA_
#define CH_MACHINE IMAGE_FILE_MACHINE_ALPHA
#else
#define CH_MACHINE IMAGE_FILE_MACHINE_I386
#endif
    // Note:  If the source and line functions are used, then
    //        StackWalk can access violate.
    BOOL bSWRet = StackWalk( CH_MACHINE,
                             (HANDLE)GetCurrentProcessId(),
                             GetCurrentThread(),
                             &g_stFrame,
                             pExPtrs->ContextRecord,
                             (PREAD_PROCESS_MEMORY_ROUTINE)CH_ReadProcessMemory,
                             SymFunctionTableAccess,
                             SymGetModuleBase,
                             0 );
    if ( (FALSE == bSWRet) || (0 == g_stFrame.AddrFrame.Offset) )
    {
      szRet = 0;
      __leave;
    }

    int iCurr = 0 ;
    // At a minimum, put the address in.
#ifdef _ALPHA_
    iCurr += wsprintf( g_szBuff + iCurr, _T("0x%08X"), g_stFrame.AddrPC.Offset );
#else
    iCurr += wsprintf( g_szBuff + iCurr, _T("%04X:%08X"), pExPtrs->ContextRecord->SegCs, g_stFrame.AddrPC.Offset );
#endif

    // Do the parameters?
    if ( GSTSO_PARAMS == (dwOpts & GSTSO_PARAMS) )
    {
      iCurr += wsprintf( g_szBuff + iCurr, 
				                 _T( " (0x%08X 0x%08X 0x%08X 0x%08X)" ), 
                         g_stFrame.Params[0],
                         g_stFrame.Params[1],
                         g_stFrame.Params[2],
                         g_stFrame.Params[3] );
    }

    if ( GSTSO_MODULE == (dwOpts & GSTSO_MODULE) )
    {
      iCurr += wsprintf( g_szBuff + iCurr, _T(" ") );

      dwTemp = SymGetModuleBase( (HANDLE)GetCurrentProcessId(), g_stFrame.AddrPC.Offset );

      ASSERT( 0 != dwTemp );

      if ( 0 == dwTemp )
        iCurr += wsprintf( g_szBuff + iCurr, _T("<UNKNOWN>") );
      else
        iCurr += BSUGetModuleBaseName( GetCurrentProcess(), (HINSTANCE)dwTemp, g_szBuff + iCurr, BUFF_SIZE - iCurr );
    }

    ASSERT( iCurr < BUFF_SIZE ) ;
    DWORD dwDisp;

    if ( GSTSO_SYMBOL == (dwOpts & GSTSO_SYMBOL) )
    {
      // Start looking up the exception address.
      //lint -e545
      PIMAGEHLP_SYMBOL pSym = (PIMAGEHLP_SYMBOL)&g_stSymbol;
      //lint +e545
      ZeroMemory( pSym, SYM_BUFF_SIZE );
      pSym->SizeOfStruct = sizeof( IMAGEHLP_SYMBOL );
      pSym->MaxNameLength = SYM_BUFF_SIZE - sizeof( IMAGEHLP_SYMBOL );

      if ( TRUE == SymGetSymFromAddr((HANDLE)GetCurrentProcessId(), g_stFrame.AddrPC.Offset, &dwDisp, pSym) )
      {
        iCurr += wsprintf( g_szBuff + iCurr, _T(", ") );

        // Copy no more than there is room for.
        dwTemp = lstrlen ( pSym->Name );
        if ( dwTemp > (DWORD)(BUFF_SIZE - iCurr - 20) )
        {
          lstrcpyn( g_szBuff + iCurr, pSym->Name, BUFF_SIZE - iCurr - 1 );
          // Gotta leave now.
          szRet = g_szBuff ;
          __leave ;
        }
        else
        {
          if ( dwDisp > 0 )
            iCurr += wsprintf( g_szBuff + iCurr, _T("%s()+%d byte(s)"), pSym->Name, dwDisp );
          else
            iCurr += wsprintf( g_szBuff + iCurr, _T("%s"), pSym->Name );
        }
      }
      else
      {
        // If the symbol was not found, the source and line will
        //  not be found either so leave now.
        szRet = g_szBuff;
        __leave;
      }

    }

    if ( GSTSO_SRCLINE == ( dwOpts & GSTSO_SRCLINE ) )
    {
      ZeroMemory ( &g_stLine , sizeof ( IMAGEHLP_LINE ) ) ;
      g_stLine.SizeOfStruct = sizeof ( IMAGEHLP_LINE ) ;

      if ( TRUE == InternalSymGetLineFromAddr((HANDLE)GetCurrentProcessId(), g_stFrame.AddrPC.Offset, &dwDisp, &g_stLine) )
      {
        iCurr += wsprintf( g_szBuff + iCurr, _T(", ") );

        // Copy no more than there is room for.
        dwTemp = lstrlen( g_stLine.FileName );
        if ( dwTemp > (DWORD)( BUFF_SIZE - iCurr - 25 ) )
        {
          lstrcpyn( g_szBuff + iCurr, g_stLine.FileName, BUFF_SIZE - iCurr - 1 );
          // Gotta leave now.
          szRet = g_szBuff;
          __leave;
        }
        else
        {
          if ( dwDisp > 0 )
            iCurr += wsprintf( g_szBuff + iCurr, _T("%s, line %d+%d byte(s)"), g_stLine.FileName, g_stLine.LineNumber, dwDisp );
          else
            iCurr += wsprintf( g_szBuff + iCurr, _T("%s, line %d"), g_stLine.FileName, g_stLine.LineNumber );
        }
			}
		}
		szRet = g_szBuff ;
	}
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
    ASSERT( FALSE );
    szRet = 0;
  }
  return szRet;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool STDCALL GetSourceLine( DWORD pointer, const char* &pszFileName, int &nLineNumber )
{
  ASSERT ( FALSE == IsBadCodePtr(FARPROC(pointer)) );

  // A temporary for all to use.  This saves stack space.
  __try
  {
    // Initialize the symbol engine in case it is not initialized.
    InitSymEng();

    ZeroMemory ( &g_stLine , sizeof(IMAGEHLP_LINE) );
    g_stLine.SizeOfStruct = sizeof( IMAGEHLP_LINE );

    DWORD dwDisp = 0;
    if ( TRUE == InternalSymGetLineFromAddr((HANDLE)GetCurrentProcessId(), pointer, &dwDisp, &g_stLine) )
    {
			pszFileName = g_stLine.FileName;
			nLineNumber = g_stLine.LineNumber;
		}
	}
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
    ASSERT( FALSE );
		return false;
  }

  return true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL STDCALL GetFirstStackTraceStringVB( DWORD                dwOpts  ,
                                           EXCEPTION_POINTERS * pExPtrs ,
                                           LPTSTR               szBuff  ,
                                           UINT                 uiSize   )
{
  ASSERT( FALSE == IsBadWritePtr(szBuff, uiSize) ) ;
  if ( TRUE == IsBadWritePtr(szBuff, uiSize) )
    return FALSE;

  LPCTSTR szRet = 0;

  __try
  {
    szRet = GetFirstStackTraceString( dwOpts, pExPtrs );
    if ( 0 == szRet )
      __leave;
    lstrcpyn( szBuff, szRet, Min( (UINT)lstrlen( szRet ) + 1, uiSize ) );
  }
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
    szRet = 0;
  }
  return ( 0 != szRet );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL STDCALL GetNextStackTraceStringVB( DWORD                dwOpts  ,
                                          EXCEPTION_POINTERS * pExPtrs ,
                                          LPTSTR               szBuff  ,
                                          UINT                 uiSize   )
{
  ASSERT( FALSE == IsBadWritePtr(szBuff, uiSize) );
  if ( TRUE == IsBadWritePtr(szBuff, uiSize) )
    return FALSE;

  LPCTSTR szRet = 0;

  __try
  {
    szRet = GetNextStackTraceString( dwOpts, pExPtrs );
    if ( 0 == szRet )
      __leave;
    lstrcpyn( szBuff, szRet, Min( (UINT)lstrlen( szRet ) + 1, uiSize ) );
  }
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
    szRet = 0;
  }
  return ( 0 != szRet );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LPCTSTR STDCALL GetRegisterString( EXCEPTION_POINTERS *pExPtrs )
{
  // Check the parameter.
  ASSERT( FALSE == IsBadReadPtr(pExPtrs, sizeof(EXCEPTION_POINTERS)) );
  if ( TRUE == IsBadReadPtr(pExPtrs, sizeof(EXCEPTION_POINTERS)) )
  {
    TRACE0( "GetRegisterString - invalid pExPtrs!\n" );
    return 0;
  }

#ifdef _ALPHA_
  // Do the ALPHA ones if needed.
  ASSERT( FALSE ) ;
#else
  // This puts 48 bytes on the stack.  This could be a problem when
  //  the stack is blown.
  wsprintf( g_szBuff,
            _T ("EAX=%08X  EBX=%08X  ECX=%08X  EDX=%08X  ESI=%08X\n"\
                "EDI=%08X  EBP=%08X  ESP=%08X  EIP=%08X  FLG=%08X\n"\
                "CS=%04X   DS=%04X  SS=%04X  ES=%04X   "\
                "FS=%04X  GS=%04X" ) ,
                pExPtrs->ContextRecord->Eax,
                pExPtrs->ContextRecord->Ebx,
                pExPtrs->ContextRecord->Ecx,
                pExPtrs->ContextRecord->Edx,
                pExPtrs->ContextRecord->Esi,
                pExPtrs->ContextRecord->Edi,
                pExPtrs->ContextRecord->Ebp,
                pExPtrs->ContextRecord->Esp,
                pExPtrs->ContextRecord->Eip,
                pExPtrs->ContextRecord->EFlags,
                pExPtrs->ContextRecord->SegCs,
                pExPtrs->ContextRecord->SegDs,
                pExPtrs->ContextRecord->SegSs,
                pExPtrs->ContextRecord->SegEs,
                pExPtrs->ContextRecord->SegFs,
                pExPtrs->ContextRecord->SegGs );

#endif
  return g_szBuff;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL STDCALL GetRegisterStringVB( EXCEPTION_POINTERS * pExPtrs ,
                                    LPTSTR               szBuff  ,
                                    UINT                 uiSize   )
{
  ASSERT( FALSE == IsBadWritePtr(szBuff, uiSize) );
  if ( TRUE == IsBadWritePtr(szBuff, uiSize) )
    return FALSE;

  LPCTSTR szRet;

  __try
  {
    szRet = GetRegisterString( pExPtrs );
    if ( 0 == szRet )
      __leave;
    lstrcpyn( szBuff, szRet, Min( (UINT)lstrlen(szRet) + 1, uiSize) );
  }
  __except ( EXCEPTION_EXECUTE_HANDLER )
  {
    szRet = 0;
  }
  return ( 0 != szRet );

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//lint -e527
LPCTSTR ConvertSimpleException( DWORD dwExcept )
{
  switch ( dwExcept )
  {
		case EXCEPTION_ACCESS_VIOLATION:
			return _T( "Access Violation" );
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			return _T( "Datatype Misalignment" );
		case EXCEPTION_BREAKPOINT:
			return _T( "Breakpoint" );
		case EXCEPTION_SINGLE_STEP:
			return _T( "Single Step" );
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			return _T( "Array Bounds Exceeded" );
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			return _T( "Flt Denormal Operand" );
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			return _T( "Flt Divide by Zero" );
		case EXCEPTION_FLT_INEXACT_RESULT:
			return _T( "Flt Inexact Result" );
		case EXCEPTION_FLT_INVALID_OPERATION:
			return _T( "Flt Invalid Operation" );
		case EXCEPTION_FLT_OVERFLOW:
			return _T( "Flt Overflow" );
		case EXCEPTION_FLT_STACK_CHECK:
			return _T( "Flt Stack Check" );
		case EXCEPTION_FLT_UNDERFLOW:
			return _T( "Flt Underflow" );
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			return _T( "Int Divide by Zero" );
		case EXCEPTION_INT_OVERFLOW:
			return _T( "Int Overflow" );
		case EXCEPTION_PRIV_INSTRUCTION:
			return _T( "Priv Instruction" );
		case EXCEPTION_IN_PAGE_ERROR:
			return _T( "In Page Error" );
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			return _T( "Illegal Instruction" );
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			return _T( "Noncontinuable Exception" );
		case EXCEPTION_STACK_OVERFLOW:
			return _T( "Stack Overflow" );
		case EXCEPTION_INVALID_DISPOSITION:
			return _T( "Invalid Disposition" );
		case EXCEPTION_GUARD_PAGE:
			return _T( "Guard Page" );
		case EXCEPTION_INVALID_HANDLE:
			return _T( "Invalid Handle" );
		default:
			return 0;
  }
}
//lint +e527
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL InternalSymGetLineFromAddr( IN  HANDLE          hProcess,
                                 IN  DWORD           dwAddr,
                                 OUT PDWORD          pdwDisplacement,
                                 OUT PIMAGEHLP_LINE  Line )
{
  // Have I already done the GetProcAddress?
  if ( FALSE == g_bLookedForSymFuncs )
  {
    g_bLookedForSymFuncs = TRUE;
    g_pfnSymGetLineFromAddr = (PFNSYMGETLINEFROMADDR) GetProcAddress( GetModuleHandle(_T("IMAGEHLP.DLL")), "SymGetLineFromAddr" );
  }
  if ( 0 != g_pfnSymGetLineFromAddr )
  {
#ifdef WORK_AROUND_SRCLINE_BUG

    // The problem is that the symbol engine only finds those source
    //  line addresses (after the first lookup) that fall exactly on
    //  a zero displacement.  I will walk backwards 100 bytes to
    //  find the line and return the proper displacement.
    DWORD dwTempDis = 0 ;
    while ( FALSE == (*g_pfnSymGetLineFromAddr)(hProcess, dwAddr - dwTempDis, pdwDisplacement, Line) )
    {
      dwTempDis += 1;
      if ( 100 == dwTempDis )
				return FALSE;
    }

    // It was found and the source line information is correct so
    //  change the displacement if it was looked up multiple times.
    if ( 0 != dwTempDis )
      *pdwDisplacement = dwTempDis;
    return TRUE;

#else  // WORK_AROUND_SRCLINE_BUG
    return (*g_pfnSymGetLineFromAddr)( hProcess, dwAddr, pdwDisplacement, Line );
#endif
  }
  return FALSE;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initializes the symbol engine if needed.
void InitSymEng()
{
  if ( FALSE == g_bSymEngInit )
  {
    // Set up the symbol engine.
    DWORD dwOpts = SymGetOptions();

    // Turn on load lines.
    SymSetOptions( dwOpts | SYMOPT_LOAD_LINES );

    // Initialize the symbol engine.
    VERIFY( SymInitialize((HANDLE)GetCurrentProcessId(), 0, FALSE) );
    UINT uiCount;
    // Find out how many modules there are.
    VERIFY( GetLoadedModules(GetCurrentProcessId(), 0, 0, &uiCount) );
    // Allocate something big enough to hold the list.
    HMODULE *paMods = new HMODULE[uiCount];

    // Get the list for real.
    if ( FALSE == GetLoadedModules(GetCurrentProcessId(), uiCount, paMods, &uiCount) )
    {
      ASSERT( FALSE );
      // Free the memory that I allocated earlier.
      delete []paMods;
      // There's not much I can do here...
      g_bSymEngInit = FALSE;
      return;
    }
    // The module filename.
    TCHAR szModName[MAX_PATH];
    for ( UINT uiCurr = 0; uiCurr < uiCount; uiCurr++ )
    {
      // Get the module's filename.
      VERIFY( GetModuleFileName( paMods[uiCurr], szModName, sizeof(szModName)) );
			//
			{
				std::string szName = szModName;
				szName = szName.substr( szName.rfind( '\\' ) + 1 );
                std::transform( szName.begin(), szName.end(), szName.begin(), MSVCMustDie_tolower );
				if ( std::find( ignoremodules.begin(), ignoremodules.end(), szName ) != ignoremodules.end() )
					continue;
			}

      // In order to get the symbol engine to work outside a
      //  debugger, it needs a handle to the image.  Yes, this
      //  will leak but the OS will close it down when the process
      //  ends.
      HANDLE hFile = CreateFile( szModName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );

      BOOL bLoaded = SymLoadModule( (HANDLE)GetCurrentProcessId(), hFile, szModName, 0, (DWORD)paMods[uiCurr], 0 );
			if ( !bLoaded )
			{
				OutputDebugString( "Can't load symbols for module " );
				OutputDebugString( szModName );
				OutputDebugString( "\n" );
			}
    }
    delete []paMods;
  }
  g_bSymEngInit = TRUE;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cleans up the symbol engine if needed.
void CleanupSymEng()
{
  if ( TRUE == g_bSymEngInit )
  {
    VERIFY( SymCleanup((HANDLE)GetCurrentProcessId()) );
  }
  g_bSymEngInit = FALSE;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
