#include "StdAfx.h"

#include "CallStack.h"

#include "Notification.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <Process.h>
#include <imagehlp.h>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define WORK_AROUND_SRCLINE_BUG 1
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NBugSlayer
{
	// bugslayer library handle (to pass to dialog)
	HINSTANCE hModuleHandle;
	// ignores for asserts engine
	SIgnoresList ignores;
	// emergency commands list
	typedef std::list< CPtr<IBaseCommand> > CEmergencyCommandsList;
	CEmergencyCommandsList emergencyCommands;
	//
	bool IsIgnore( const char *pszFileName, int nLineNumber );
	void AddIgnore( const char *pszFunctionName, const char *pszFileName, int nLineNumber, const char *pszCondition, DWORD dwFlag );
	void RemoveIgnore( SIgnoresEntry *pEntry );
	//
	void SetModuleHandle( HINSTANCE hInstance ) { hModuleHandle = hInstance; }
	HINSTANCE GetModuleHandle() { return hModuleHandle; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STDCALL NBugSlayer::AddEmergencyCommand( IBaseCommand *pCommand )
{
	emergencyCommands.push_back( pCommand );
}
void STDCALL NBugSlayer::RemoveAllEmergencyCommands()
{
	emergencyCommands.clear();
}
void STDCALL NBugSlayer::ExecuteEmergencyCommands()
{
	for ( CEmergencyCommandsList::iterator it = emergencyCommands.begin(); it != emergencyCommands.end(); ++it )
	{
		if ( (*it) != 0 )
			(*it)->Do();
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sets up the symbols for functions in the debug file.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetStackTraceInfo( EXCEPTION_POINTERS *pex, CCallStackEntryList &callstack )
{
	SCallStackEntry entry;
	bool bRetVal = GetFirstStackTrace( pex, entry );
	while ( bRetVal )
	{
		callstack.push_back( entry );
		bRetVal = GetNextStackTrace( pex, entry );
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int UpdateStack( EXCEPTION_POINTERS *pex, CCallStackEntryList &callstack )
{
	GetStackTraceInfo( pex, callstack );
  return EXCEPTION_CONTINUE_SEARCH;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// workaround WinXP bug...
void UpdateCallStackInformationLocal( CCallStackEntryList &callstack )
{
	__try    
	{        
		char *p = 0;
		*p = 1;
		//RaiseException( 0, 0, 0, 0 );    
	}    
	__except( UpdateStack( GetExceptionInformation(), callstack ) )
	{
	}
}
void UpdateCallStackInformation( CCallStackEntryList &callstack )
{
	try
	{
		UpdateCallStackInformationLocal( callstack );
	}
	catch ( ... )
	{
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ExceptionCodeToString( DWORD dwExceptionCode )
{
  switch ( dwExceptionCode )
  {
  case EXCEPTION_ACCESS_VIOLATION: 
    return "The thread tried to read from or write to a virtual address for which it does not have the appropriate access.";
  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: 
    return "The thread tried to access an array element that is out of bounds and the underlying hardware supports bounds checking.";
  case EXCEPTION_BREAKPOINT: 
    return "A breakpoint was encountered.";
  case EXCEPTION_DATATYPE_MISALIGNMENT: 
    return "The thread tried to read or write data that is misaligned on hardware that does not provide alignment. For example, 16-bit values must be aligned on 2-byte boundaries; 32-bit values on 4-byte boundaries, and so on.";
  case EXCEPTION_FLT_DENORMAL_OPERAND: 
    return "One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value.";
  case EXCEPTION_FLT_DIVIDE_BY_ZERO: 
    return "The thread tried to divide a floating-point value by a floating-point divisor of zero.";
  case EXCEPTION_FLT_INEXACT_RESULT: 
    return "The result of a floating-point operation cannot be represented exactly as a decimal fraction.";
  case EXCEPTION_FLT_INVALID_OPERATION: 
    return "This exception represents any floating-point exception not included in this list.";
  case EXCEPTION_FLT_OVERFLOW: 
    return "The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.";
  case EXCEPTION_FLT_STACK_CHECK: 
    return "The stack overflowed or underflowed as the result of a floating-point operation.";
  case EXCEPTION_FLT_UNDERFLOW: 
    return "The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.";
  case EXCEPTION_ILLEGAL_INSTRUCTION: 
    return "The thread tried to execute an invalid instruction.";
  case EXCEPTION_IN_PAGE_ERROR: 
    return "The thread tried to access a page that was not present, and the system was unable to load the page. For example, this exception might occur if a network connection is lost while running a program over the network.";
  case EXCEPTION_INT_DIVIDE_BY_ZERO: 
    return "The thread tried to divide an integer value by an integer divisor of zero.";
  case EXCEPTION_INT_OVERFLOW: 
    return "The result of an integer operation caused a carry out of the most significant bit of the result.";
  case EXCEPTION_INVALID_DISPOSITION: 
    return "An exception handler returned an invalid disposition to the exception dispatcher.";
  case EXCEPTION_NONCONTINUABLE_EXCEPTION: 
    return "The thread tried to continue execution after a noncontinuable exception occurred.";
  case EXCEPTION_PRIV_INSTRUCTION: 
    return "The thread tried to execute an instruction whose operation is not allowed in the current machine mode.";
  case EXCEPTION_SINGLE_STEP: 
    return "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.";
  case EXCEPTION_STACK_OVERFLOW: 
    return "The thread used up its stack.";
  default:
    return "Unknown exception.";
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool NBugSlayer::IsIgnore( const char *pszFileName, int nLineNumber )
{
	for ( SIgnoresList::const_iterator pos = ignores.begin(); pos != ignores.end(); ++pos )
	{
		if ( pos->dwFlags & IGNORE_THIS )
		{
			if ( (pos->szFileName == pszFileName) && (pos->nLineNumber == nLineNumber) )
				return true;
		}
		else if ( pos->dwFlags & IGNORE_NON_THIS )
		{
			if ( (pos->szFileName != pszFileName) || (pos->nLineNumber != nLineNumber) )
				return true;
		}
		else if ( pos->dwFlags & IGNORE_FILE )
		{
			if ( pos->szFileName == pszFileName )
				return true;
		}
		else if ( pos->dwFlags & IGNORE_NON_FILE )
		{
			if ( pos->szFileName != pszFileName )
				return true;
		}
		else if ( pos->dwFlags & IGNORE_ALL )
			return true;
	}
	//
	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void NBugSlayer::AddIgnore( const char *pszFunctionName, const char *pszFileName, int nLineNumber, const char *pszCondition, DWORD dwFlag )
{
	SIgnoresEntry ignore;
	ignore.szFunctionName = pszFunctionName;
	ignore.szFileName = pszFileName;
	ignore.nLineNumber = nLineNumber;
	ignore.szCondition = pszCondition;
	ignore.dwFlags = dwFlag;
	ignores.push_back( ignore );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void NBugSlayer::RemoveIgnore( SIgnoresEntry *pEntry )
{
	ignores.remove( *pEntry );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EBSUReport STDCALL NBugSlayer::ReportAssert( const char *pszCondition, const char *pszDescription, 
	                                                           const char *pszFileName, int nLineNumber, bool bForceMode )
{
	// first, check for ignore
	if ( IsIgnore(pszFileName, nLineNumber) )
		return BSU_IGNORE;
	//
  CCallStackEntryList entries;
	UpdateCallStackInformation( entries );
	// pop 3 entries - this function, UpdateCallStackInformation and RaiseException
	/*
	if ( !entries.empty() )
		entries.pop_front();
		*/
	if ( !entries.empty() )
		entries.pop_front();
	if ( !entries.empty() )
		entries.pop_front();
	if ( !entries.empty() )
		entries.pop_front();
	// set filename and line number for the top-level entry
	if ( !entries.empty() )
	{
		entries.front().szFileName = pszFileName;
		entries.front().nLineNumber = nLineNumber;
	}
	// trace debug info
	OutputDebugString( "*********************************************************************************************************\n" );
	OutputDebugString( pszCondition );
	OutputDebugString( "\n" );
	OutputDebugString( pszDescription );
	OutputDebugString( "\n" );
	OutputDebugString( "CallStack entries dump:\n" );
	for ( CCallStackEntryList::const_iterator pos = entries.begin(); pos != entries.end(); ++pos )
	{
		char buff[1024];
		sprintf_s( buff, sizeof(buff), "%s(%d): %s\n", pos->szFileName.c_str(), pos->nLineNumber, pos->szFunctionName.c_str() );
		OutputDebugString( buff );
	}
	OutputDebugString( "CallStack entries dump done\n" );
	OutputDebugString( "*********************************************************************************************************\n" );
	// call report dialog
	EBSUReport eRetCode = ShowAssertionDlg( NBugSlayer::GetModuleHandle(), 0, 
		                                      pszFileName, nLineNumber,
		                                      pszCondition, pszDescription, 
																		      entries, ignores, 0 );
	return eRetCode;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
EBSUReport STDCALL NBugSlayer::ReportAssertHR( HRESULT dxrval, const char *pszDescription, 
	                                                             const char *pszFileName, int nLineNumber, bool bForceMode )
{
	char buff[1024];
	sprintf_s( buff, sizeof(buff), "(0x%08X) %s", static_cast<unsigned int>(dxrval), DXErrorToString(dxrval) );
	return ReportAssert( buff, pszDescription, pszFileName, nLineNumber, bForceMode );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LONG STDCALL CrashHandlerFilter( EXCEPTION_POINTERS *pExPtrs )
{
	// first, get stack trace...
  CCallStackEntryList entries;
	GetStackTraceInfo( pExPtrs, entries );
	// form <description> for unhandled exception
	char buff[128];
	if ( !entries.empty() )
		sprintf_s( buff, sizeof(buff), "First-chance exception in %s: 0x%.8X: %s.", entries.front().szModuleName.c_str(), pExPtrs->ExceptionRecord->ExceptionCode, ConvertSimpleException(pExPtrs->ExceptionRecord->ExceptionCode) );
	else
		sprintf_s( buff, sizeof(buff), "Unhandled Exception (0x%08X) at 0x%p", pExPtrs->ExceptionRecord->ExceptionCode, pExPtrs->ExceptionRecord->ExceptionAddress );
	// trace debug info
	OutputDebugString( "*********************************************************************************************************\n" );
	OutputDebugString( buff );
	OutputDebugString( "\n" );
	OutputDebugString( ExceptionCodeToString( pExPtrs->ExceptionRecord->ExceptionCode ) );
	OutputDebugString( "\n" );
	OutputDebugString( "CallStack entries dump:\n" );
	for ( CCallStackEntryList::const_iterator pos = entries.begin(); pos != entries.end(); ++pos )
	{
		char buff[1024];
		sprintf_s( buff, sizeof(buff), "%s(%d): %s\n", pos->szFileName.c_str(), pos->nLineNumber, pos->szFunctionName.c_str() );
		OutputDebugString( buff );
	}
	OutputDebugString( "CallStack entries dump done\n" );
	OutputDebugString( "*********************************************************************************************************\n" );
	//
	EBSUReport eRetCode = ShowExceptionDlg( NBugSlayer::GetModuleHandle(), 0, buff, 
		                                      ExceptionCodeToString( pExPtrs->ExceptionRecord->ExceptionCode ), 
																		      entries, GetRegisterString( pExPtrs ) );
	// pExceptionRecord->ExceptionFlags != EXCEPTION_NONCONTINUABLE
  //
	switch ( eRetCode )
	{
	case BSU_DEBUG:
		return EXCEPTION_CONTINUE_SEARCH;
	case BSU_ABORT:
		SetCrashHandlerFilter( 0 );					// reset crash handler filter
		abort();
		break;
	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
