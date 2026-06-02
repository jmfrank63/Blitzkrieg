#ifndef __CONSOLE_BUFFER_H__
#define __CONSOLE_BUFFER_H__
#pragma ONCE
class CConsoleBuffer : public IConsoleBuffer
{
	OBJECT_NORMAL_METHODS( CConsoleBuffer );
	typedef std::list< std::pair<std::wstring, DWORD> > CStringsList;
	typedef std::unordered_map<int, CStringsList> CStreamsMap;
	typedef std::unordered_map<int, std::list<int> > CDublicateMap;
	typedef std::unordered_map<int, std::string> CStreamNamesMap;
	CStreamsMap streams;									// all stream channels map
	CStreamNamesMap names;								// some channel names
	CStreamsMap logs;											// channel logs
	CDublicateMap dublicates;							// dublite channels info
	std::string szDumpFileName;						// dump file name
	std::wstring szTempString;						// temp string to return values
	std::string szTempStringASCII;				// ASCII temp string
	std::string szLogFileName;						// log file name
	void WriteLocal( int nStreamID, const wchar_t *pszString, DWORD color, bool bBackupLog );
	virtual ~CConsoleBuffer();
public:
	virtual bool STDCALL Configure( const char *pszConfigure );
	virtual void STDCALL Write( int nStreamID, const wchar_t *pszString, DWORD color = 0xffffffff, bool bBackupLog = false );
	virtual void STDCALL WriteASCII( int nStreamID, const char *pszString, DWORD color = 0xffffffff, bool bBackupLog = false );
	virtual const wchar_t* STDCALL Read( int nStreamID, DWORD *pColor = 0 );
	virtual const char* STDCALL ReadASCII( int nStreamID, DWORD *pColor = 0 );
	virtual bool STDCALL DumpLog( int nStreamID );
};
#endif // __CONSOLE_BUFFER_H__
