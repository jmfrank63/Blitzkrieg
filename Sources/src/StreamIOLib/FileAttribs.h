#ifndef __FILEATTRIBS_H__
#define __FILEATTRIBS_H__
#pragma ONCE
class CFileAttribs : public CBasicAccessor<DWORD>
{
	enum
	{
		FILE_ATTRIB_READONLY            = 0x00000001,
		FILE_ATTRIB_HIDDEN              = 0x00000002,
		FILE_ATTRIB_SYSTEM              = 0x00000004,
		FILE_ATTRIB_DIRECTORY           = 0x00000010,
		FILE_ATTRIB_ARCHIVE             = 0x00000020,
		FILE_ATTRIB_NORMAL              = 0x00000080,
		FILE_ATTRIB_TEMPORARY           = 0x00000100,
		FILE_ATTRIB_SPARSE_FILE         = 0x00000200,
		FILE_ATTRIB_REPARSE_POINT       = 0x00000400,
		FILE_ATTRIB_COMPRESSED          = 0x00000800,
		FILE_ATTRIB_OFFLINE             = 0x00001000,
		FILE_ATTRIB_NOT_CONTENT_INDEXED = 0x00002000,

		FILE_ATTRIB_FORCE_DWORD = 0x7fffffff
	};
	DWORD dwAttribs;
public:
	CFileAttribs() : dwAttribs( 0 ) {  }
	CFileAttribs( DWORD _dwAttribs ) : CBasicAccessor<DWORD>( _dwAttribs ) {  }
	CFileAttribs( const CFileAttribs &attribs ) : CBasicAccessor<DWORD>( attribs.dwAttribs ) {  }
	bool IsReadOnly() const { return ( dwAttribs & FILE_ATTRIB_READONLY ) != 0; }
	bool IsSystem() const { return ( dwAttribs & FILE_ATTRIB_SYSTEM ) != 0; }
	bool IsHidden() const { return ( dwAttribs & FILE_ATTRIB_HIDDEN ) != 0; }
	bool IsTemporary() const { return ( dwAttribs & FILE_ATTRIB_TEMPORARY ) != 0; }
	bool IsNormal() const { return ( dwAttribs & FILE_ATTRIB_NORMAL ) != 0; }
	bool IsArchive() const { return ( dwAttribs & FILE_ATTRIB_ARCHIVE ) != 0; }
	bool IsCompressed() const { return ( dwAttribs & FILE_ATTRIB_COMPRESSED ) != 0; }
	bool IsDirectory() const { return ( dwAttribs & FILE_ATTRIB_DIRECTORY ) != 0; }
	bool IsDots( const char *pszName ) const { return IsDirectory() && ( (pszName[0] == '.') ) && ( (pszName[1] == '\0') || ((pszName[1] == '.') && (pszName[2] == '\0')) ); }
	bool IsMatchesMask( DWORD dwMask ) const { return (dwAttribs & dwMask) == dwMask; }
};
inline DWORD DOSToWin32DateTime( time_t time )
{
	tm *pTime = localtime( &time );
	SWin32Time filetime;
	filetime.year    = pTime->tm_year - 80;	// due to 'tm' year relative to 1900 year, but we need relative to 1980
	filetime.month   = pTime->tm_mon + 1;		// due to the month represented in the '0..11' format, but we need in '1..12'
	filetime.day     = pTime->tm_mday;			// day in '1..31' format
	filetime.hours   = pTime->tm_hour;			// hours in '0..23' format
	filetime.minutes = pTime->tm_min;				// minutes in '0..59' format
	filetime.seconds = pTime->tm_sec / 2;		// due to win32 seconds resolution are 2 sec. (i.e. seconds represented in '0..29' format)

	return bit_cast<DWORD>( filetime );
}
inline DWORD FILETIMEToWin32DateTime( const FILETIME &filetime )
{
	FILETIME localfiletime;
	FileTimeToLocalFileTime( &filetime, &localfiletime );
	SWin32Time win32time;
	FileTimeToDosDateTime( &localfiletime, &win32time.wDate, &win32time.wTime );
	return bit_cast<DWORD>( win32time );
}
inline time_t Win32ToDOSDateTime( const SWin32Time &time )
{
	tm tmTime;
	Zero( tmTime );
	tmTime.tm_year = int( time.year ) + 80;
	tmTime.tm_mon  = int( time.month ) - 1;
	tmTime.tm_mday = int( time.day);
	tmTime.tm_hour = int( time.hours);
	tmTime.tm_min  = int( time.minutes);
	tmTime.tm_sec  = int( time.seconds ) * 2;
	time_t result = mktime( &tmTime );
	return result;
}
#endif // __FILEATTRIBS_H__
