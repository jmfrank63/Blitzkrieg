#include "StdAfx.h"
static const char LOCAL_FILE[] = __FILE__;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Notification.h"

#include <string>
#include <list>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* DXErrorToString( HRESULT hErrorCode );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCommonException : public ICommonException
{
private:
	const std::string szErrorString;
	const DWORD dwErrorCode;
public:
	CCommonException( const char *pszString, DWORD dwCode )
		: szErrorString( pszString ), dwErrorCode( dwCode ) {  }
	CCommonException( const char *pszString )
		: szErrorString( pszString ), dwErrorCode( 0 ) {  }
	CCommonException( DWORD dwCode )
		: dwErrorCode( dwCode ) {  }

	virtual const char* GetString() const { return szErrorString.c_str(); }
	virtual DWORD GetCode() const { return dwErrorCode; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGuardException : public IGuardException
{
private:
  std::list<std::string> szErrors;
  mutable std::string szErrorString;
  mutable bool bStringChanged;
public:
  CGuardException( const char *pszString )
    : bStringChanged( true ) { szErrors.push_back( pszString ); }

	virtual const char* GetString() const 
  { 
    if ( bStringChanged )
    {
			char buff[2048];
			buff[0] = 0;
      for ( std::list<std::string>::const_iterator pos = szErrors.begin(); pos != szErrors.end(); ++pos )
			{
				strcat_s( buff, sizeof(buff), pos->c_str() );
				strcat_s( buff, sizeof(buff), " <= " );
			}
			strcat_s( buff, sizeof(buff), "WinMain" );
      szErrorString = buff;
      bStringChanged = false;
    }
    return szErrorString.c_str(); 
  }
  virtual void Append( const char *pszFormat, ... )
  {
    char buffer[512];
    va_list va;
	  // compose error string
    va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
    va_end( va );
    //
    szErrors.push_back( buffer );
    bStringChanged = true;
  }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static std::list<NotificationShowMBox> notifyError;
static std::list<NotificationShowMBox> notifyWarning;
static std::list<NotificationShowMBox> notifyReport;
void AddNotifyErrorShowMBoxFunction( NotificationShowMBox funct )
{
	notifyError.push_back( funct );
}
void AddNotifyWarningShowMBoxFunction( NotificationShowMBox funct )
{
	notifyWarning.push_back( funct );
}
void AddNotifyReportShowMBoxFunction( NotificationShowMBox funct )
{
	notifyReport.push_back( funct );
}
void RemoveNotifyErrorShowMBoxFunction( NotificationShowMBox funct )
{
	notifyError.remove( funct );
}
void RemoveNotifyWarningShowMBoxFunction( NotificationShowMBox funct )
{
	notifyWarning.remove( funct );
}
void RemoveNotifyReportShowMBoxFunction( NotificationShowMBox funct )
{
	notifyReport.remove( funct );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BreakHere()
{
#ifdef _DEBUG
	DebugBreak();
#endif // _DEBUG
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ShowError( const char *pszString )
{
	OutputDebugString( pszString );
	OutputDebugString( "\n" );
	int nRetVal = -1000000000;
	for ( std::list<NotificationShowMBox>::iterator pos = notifyError.begin(); pos != notifyError.end(); ++pos )
		nRetVal = Max( nRetVal, (*(*pos))( "ERROR!", pszString, MB_ABORTRETRYIGNORE | MB_ICONERROR ) );
	return nRetVal;
}
int ShowWarning( const char *pszString )
{
	OutputDebugString( pszString );
	OutputDebugString( "\n" );
	int nRetVal = -1000000000;
	for ( std::list<NotificationShowMBox>::iterator pos = notifyWarning.begin(); pos != notifyWarning.end(); ++pos )
		nRetVal = Max( nRetVal, (*(*pos))( "Warning!", pszString, MB_ABORTRETRYIGNORE | MB_ICONEXCLAMATION ) );
	return nRetVal;
}
int ShowReport( const char *pszString )
{
	OutputDebugString( pszString );
	OutputDebugString( "\n" );
	int nRetVal = -1000000000;
	for ( std::list<NotificationShowMBox>::iterator pos = notifyReport.begin(); pos != notifyReport.end(); ++pos )
		nRetVal = Max( nRetVal, (*(*pos))( "Report", pszString, MB_OK ) );
	return nRetVal;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThrowExceptionHR( HRESULT dxrval, const char *pszFormat, ... ) throw ( ICommonException* )
{
  char buffer[512], buff1[32];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
  va_end( va );
	//
	sprintf_s( buff1, sizeof(buff1), "(0x%08p) ", reinterpret_cast<void*>(static_cast<uintptr_t>(dxrval)) );
	strcat_s( buffer, sizeof(buffer), "\n" );
	strcat_s( buffer, sizeof(buffer), buff1 );
	strcat_s( buffer, sizeof(buffer), DXErrorToString(dxrval) );
	//
	CCommonException* pException = new CCommonException( buffer, dxrval );
	int nRetCode = ShowError( buffer );
	if ( nRetCode == IDRETRY )
		BreakHere();
	else if ( nRetCode == IDABORT )
		exit( 0xDEAD );

	throw pException;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThrowException( const char *pszFormat, ... ) throw ( ICommonException* )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
  va_end( va );
	//
	CCommonException* pException = new CCommonException( buffer );
	int nRetCode = ShowError( buffer );
	if ( nRetCode == IDRETRY )
		BreakHere();
	else if ( nRetCode == IDABORT )
		exit( 0xDEAD );

	throw pException;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ThrowGuardException( const char *pszFormat, ... ) throw ( IGuardException* )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
  va_end( va );
	//
	CGuardException* pException = new CGuardException( buffer );
	throw pException;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// error notification
bool ReportErrorHR( HRESULT dxrval, const char *pszFormat, ... )
{
  char buffer[512], buff1[32];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
  va_end( va );
	//
	sprintf_s( buff1, sizeof(buff1), "(0x%08p) ", reinterpret_cast<void*>(static_cast<uintptr_t>(dxrval)) );
	strcat_s( buffer, sizeof(buffer), "\n" );
	strcat_s( buffer, sizeof(buffer), buff1 );
	strcat_s( buffer, sizeof(buffer), DXErrorToString(dxrval) );
	//
	int nRetCode = ShowError( buffer );
	if ( nRetCode == IDRETRY )
		BreakHere();
	else if ( nRetCode == IDABORT )
		exit( 0xDEAD );

	return false;
}
bool ReportError( const char *pszFormat, ... )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
  va_end( va );
	//
	int nRetCode = ShowError( buffer );
	if ( nRetCode == IDRETRY )
		BreakHere();
	else if ( nRetCode == IDABORT )
		exit( 0xDEAD );

	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// warning notification
bool ReportWarningHR( HRESULT dxrval, const char *pszFormat, ... )
{
  char buffer[512], buff1[32];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
  va_end( va );
	//
	sprintf_s( buff1, sizeof(buff1), "(0x%08p) ", reinterpret_cast<void*>(static_cast<uintptr_t>(dxrval)) );
	strcat_s( buffer, sizeof(buffer), "\n" );
	strcat_s( buffer, sizeof(buffer), buff1 );
	strcat_s( buffer, sizeof(buffer), DXErrorToString(dxrval) );
	//
	ShowWarning( buffer );

	return false;
}
bool ReportWarning( const char *pszFormat, ... )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
  va_end( va );
	//
	ShowWarning( buffer );

	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// just plain notification
bool ReportInfoHR( HRESULT dxrval, const char *pszFormat, ... )
{
  char buffer[512], buff1[32];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
  va_end( va );
	//
	sprintf_s( buff1, sizeof(buff1), "(0x%08p) ", reinterpret_cast<void*>(static_cast<uintptr_t>(dxrval)) );
	strcat_s( buffer, sizeof(buffer), "\n" );
	strcat_s( buffer, sizeof(buffer), buff1 );
	strcat_s( buffer, sizeof(buffer), DXErrorToString(dxrval) );
	//
	ShowReport( buffer );

	return false;
}
bool ReportInfo( const char *pszFormat, ... )
{
  char buffer[512];
  va_list va;
	// compose error string
  va_start( va, pszFormat );
	vsprintf_s( buffer, sizeof(buffer), pszFormat, va );
  va_end( va );
	//
	ShowReport( buffer );

	return false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// converts DirectX error code to the string
const char* DXErrorToString( HRESULT hErrorCode )
{
	switch( hErrorCode )
	{
		case D3D_OK:
		  return "No error occurred.";
		case D3DERR_CONFLICTINGRENDERSTATE:
		  return "The currently set render states cannot be used together.";
		case D3DERR_CONFLICTINGTEXTUREFILTER:
		  return "The current texture filters cannot be used together.";
		case D3DERR_CONFLICTINGTEXTUREPALETTE: 
		  return "The current textures cannot be used simultaneously.\nThis generally occurs when a multitexture device requires that all palletized textures simultaneously enabled also share the same palette.";
		case D3DERR_DEVICELOST:
		  return "The device is lost and cannot be restored at the current time, so rendering is not possible.";
		case D3DERR_DEVICENOTRESET:
		  return "The device cannot be reset.";
		case D3DERR_DRIVERINTERNALERROR:
		  return "Internal driver error.";
		case D3DERR_INVALIDCALL:
		  return "The method call is invalid. For example, a method's parameter may have an invalid value.";
		case D3DERR_INVALIDDEVICE:
		  return "The requested device type is not valid.";
		case D3DERR_MOREDATA:
		  return "There is more data available than the specified buffer size can hold.";
		case D3DERR_NOTAVAILABLE:
		  return "The queried technique is not supported by this device.";
		case D3DERR_NOTFOUND:
		  return "The requested item was not found.";
		case D3DERR_OUTOFVIDEOMEMORY:
		  return "Direct3D does not have enough display memory to perform the operation.";
		case D3DERR_TOOMANYOPERATIONS: 
		  return "The application is requesting more texture-filtering operations than the device supports.";
		case D3DERR_UNSUPPORTEDALPHAARG:
		  return "The device does not support a specified texture-blending arguments for the alpha channel.";
		case D3DERR_UNSUPPORTEDALPHAOPERATION:
		  return "The device does not support a specified texture-blending operations for the alpha channel.";
		case D3DERR_UNSUPPORTEDCOLORARG:
		  return "The device does not support a specified texture-blending arguments for color values.";
		case D3DERR_UNSUPPORTEDCOLOROPERATION:
		  return "The device does not support a specified texture-blending operations for color values.";
		case D3DERR_UNSUPPORTEDFACTORVALUE:
		  return "The specified texture factor value is not supported by the device.";
		case D3DERR_UNSUPPORTEDTEXTUREFILTER: 
		  return "The specified texture filter is not supported by the device.";
		case D3DERR_WRONGTEXTUREFORMAT:
		  return "The pixel format of the texture surface is not valid.";
  	default:
     	return "Unrecognized error value.";
  }
 	return "Unrecognized error value.";
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
