#include "StdAfx.h"

#include "BuildVersion.h"
#include "StringTokenizer.h"

#include <shellapi.h>
#include <io.h>
#include <fstream>
#include <algorithm>
#include <functional>
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** helper functions
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static std::string szSS;
static std::string szCheckOut;
static std::string szCheckIn;
static std::string szGetVersion;
static std::string szNMake;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const std::string GetSourceControlName( const SProject *pProject, const std::string &szFilePath )
{
	std::string szSourceName;
	if ( szFilePath.compare(0, pProject->szPathName.size(), pProject->szPathName) != 0 ) 
		szSourceName = pProject->szSourceControl + szFilePath.substr( szFilePath.rfind('\\') + 1 );
	else
		szSourceName = pProject->szSourceControl + szFilePath.substr( pProject->szPathName.size(), std::string::npos );
	std::replace_if( szSourceName.begin(), szSourceName.end(), [](char c){ return c == '\\'; }, '/' );
	return szSourceName;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ExecuteCommand( const std::string &szCommand, const std::string &szCmdLine, const std::string &szDirectory )
{
	char pszCommandLine[2048];
	strcpy( pszCommandLine, szCmdLine.c_str() );
	//
	STARTUPINFO startinfo;
	PROCESS_INFORMATION procinfo;
	Zero( startinfo );
	Zero( procinfo );
	startinfo.cb = sizeof( startinfo );
	BOOL bRetVal = CreateProcess( szCommand.c_str(), pszCommandLine, 0, 0, FALSE, 0, 0, szDirectory.c_str(), &startinfo, &procinfo );
	if ( bRetVal == FALSE ) 
		return false;
	WaitForSingleObject( procinfo.hProcess, INFINITE );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ExecuteSourceCommand( const std::string &szCommand, const std::string &szSourceName, const std::string &szDirectory, const char *pszOptions )
{
	char pszCommandLine[2048];
	strcpy( pszCommandLine, " " );
	strcat( pszCommandLine, szCommand.c_str() );
	strcat( pszCommandLine, " " );
	strcat( pszCommandLine, szSourceName.c_str() );
	strcat( pszCommandLine, pszOptions );
	//
	return ExecuteCommand( szSS, pszCommandLine, szDirectory );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CheckOut( const SProject *pProject, const std::string &szFilePath )
{
	if ( pProject->szSourceControl.empty() ) 
		return false;
	// don't check out file, which was already checked out
	DWORD dwAttributes = GetFileAttributes( szFilePath.c_str() );
	if ( (dwAttributes & FILE_ATTRIBUTE_READONLY) == 0 )
		return false;
	//
	const std::string szDirectory = szFilePath.substr( 0, szFilePath.rfind('\\') + 1 );
	const std::string szSourceName = GetSourceControlName( pProject, szFilePath );
	return ExecuteSourceCommand( szCheckOut, szSourceName, szDirectory, " -I-" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CheckIn( const SProject *pProject, const std::string &szFilePath )
{
	if ( pProject->szSourceControl.empty() ) 
		return false;
	const std::string szDirectory = szFilePath.substr( 0, szFilePath.rfind('\\') + 1 );
	const std::string szSourceName = GetSourceControlName( pProject, szFilePath );
	return ExecuteSourceCommand( szCheckIn, szSourceName, szDirectory, " -I-" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline std::string GetFullName( const std::string &szPath )
{
	const DWORD BUFFER_SIZE = 1024;
	char buffer[BUFFER_SIZE];
	char *pszBufferFileName = 0;
	GetFullPathName( szPath.c_str(), 1024, buffer, &pszBufferFileName );
	return buffer;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline std::string ExtractPath( const SProject *pProject )
{
	const int nPos = pProject->szFileName.rfind( '\\' );
	return nPos != std::string::npos ? pProject->szFileName.substr( 0, nPos + 1 ) : ".\\";
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char *pszVersionTag[] = 
{
	"FILEVERSION ",
	"PRODUCTVERSION ",
	0
};
static const char *pszVersion2Tag[] = 
{
	"VALUE \"FileVersion\", ",
	"VALUE \"ProductVersion\", ",
	0
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ReplaceVersion( std::string *pString, const std::string &szVersion, const char *pszTags[] )
{
	for ( int i = 0; pszTags[i] != 0; ++i )
	{
		const int nPos = pString->find( pszTags[i] );
		if ( nPos != std::string::npos ) 
		{
			pString->resize( nPos + NStr::GetStrLen(pszTags[i]) - 1 );
			*pString += szVersion;
			return true;
		}
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool GetFileTime( HANDLE hFile, FILETIME *pTime )
{
	FILETIME ctime, atime, wtime;
	if ( ::GetFileTime(hFile, &ctime, &atime, &wtime) != FALSE )
	{
		*pTime = wtime;
		return true;
	}
	return false;
}
bool GetFileTime( const std::string &szFileName, FILETIME *pTime )
{
	HANDLE hFile = CreateFile( szFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
	if ( hFile == INVALID_HANDLE_VALUE ) 
		return false;
	const bool bRetVal = ::GetFileTime( hFile, pTime );
	CloseHandle( hFile );
	return bRetVal;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool IsBeginSourceFile( const std::list<std::string> &history )
{
	std::list<std::string>::const_iterator pos = history.begin();
	if ( *pos != "Begin" ) 
		return false;
	++pos;
	if ( *pos != "Source" ) 
		return false;
	++pos;
	if ( *pos != "File" ) 
		return false;
	++pos;
	if ( *pos != "SOURCE" ) 
		return false;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool IsResourceFile( const std::string &szFileName )
{
	return szFileName.compare( szFileName.rfind('.'), std::string::npos, ".rc" ) == 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsModified( const SProject *pProject, const FILETIME &timeVersion )
{
	for ( std::list<std::string>::const_iterator it = pProject->sources.begin(); it != pProject->sources.end(); ++it )
	{
		FILETIME timeSource;
		GetFileTime( *it, &timeSource );
		if ( CompareFileTime(&timeSource, &timeVersion) == 1 )
			return true;
	}
	return false;
}
bool IsModified( const std::list<SProject*> &projects, const FILETIME &timeVersion )
{
	for ( std::list<SProject*>::const_iterator it = projects.begin(); it != projects.end(); ++it )
	{
		if ( IsModified(*it, timeVersion) )
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool IsSourceCodeControl( const std::list<std::string> &history )
{
	if ( history.size() != 4 ) 
		return false;
	std::list<std::string>::const_iterator pos = history.begin();
	if ( *pos != "begin" ) 
		return false;
	++pos;
	if ( *pos != "source" ) 
		return false;
	++pos;
	if ( *pos != "code" ) 
		return false;
	++pos;
	if ( *pos != "control" ) 
		return false;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CHandle
{
	HANDLE handle;
public:
	CHandle() : handle( INVALID_HANDLE_VALUE ) {  }
	CHandle( HANDLE _handle ) : handle( _handle ) {  }
	~CHandle() { Close(); }
	//
	bool IsValid() const { return handle != INVALID_HANDLE_VALUE; }
	void Close() { if ( IsValid() ) CloseHandle( handle ); handle = INVALID_HANDLE_VALUE; }
	//
	bool operator==( const CHandle &hnd ) const { return handle == hnd.handle; }
	bool operator==( HANDLE _handle ) const { return handle == _handle; }
	//
	operator HANDLE() const { return handle; }
	CHandle& operator=( HANDLE _handle ) { Close(); handle = _handle; return *this; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** BuildVersion
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CBuildVersion::CBuildVersion( const char *pszConfigFileName )
{
	char buffer[2048];
	GetPrivateProfileString( "VersionControl", "SS", "", buffer, 2048, pszConfigFileName );
	//szSS = NStr::Format( "\"%s\"", buffer );
	szSS = buffer;
	GetPrivateProfileString( "VersionControl", "CheckOut", "", buffer, 2048, pszConfigFileName );
	szCheckOut = buffer;
	GetPrivateProfileString( "VersionControl", "CheckIn", "", buffer, 2048, pszConfigFileName );
	szCheckIn = buffer;
	GetPrivateProfileString( "VersionControl", "GetVersion", "", buffer, 2048, pszConfigFileName );
	szGetVersion = buffer;
	//
	GetPrivateProfileString( "NMake", "NMake", "", buffer, 2048, pszConfigFileName );
	szNMake = buffer;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildVersion::LoadDSW( const char *pszWorkspaceName, bool bGetLatestVersion )
{
	std::list<std::string> history;
	// load file
	const std::string szWorkspaceName = GetFullName( pszWorkspaceName );
	FILE *file = fopen( szWorkspaceName.c_str(), "rb" );
	if ( file == 0 ) 
		return false;
	const int nSize = _filelength( _fileno(file) );
	std::string szString;
	szString.resize( nSize );
	fread( &(szString[0]), 1, nSize, file );
	fclose( file );
	// parse
	CStringTokenizer<char> tokenizer( szString, ' ' );
	SProject *pCurrProject = 0;
	while ( tokenizer.Next() ) 
	{
		const std::string szToken = tokenizer.GetToken();
		history.push_back( szToken );
		while ( history.size() > 4 ) 
			history.pop_front();
		//
		if ( szToken == "Project:" ) 
		{
			tokenizer.Next();
			std::string szProjectName = tokenizer.GetToken();
			NStr::TrimBoth( szProjectName, " \"" );

			tokenizer.Next();
			std::string szProjectFile = tokenizer.GetToken();
			szProjectFile = GetFullName( szProjectFile );

			pCurrProject = &( mapProjects[szProjectName] );
			pCurrProject->szName = szProjectName;
			pCurrProject->szFileName = szProjectFile;
			pCurrProject->szPathName = ExtractPath( pCurrProject );
		}
		else if ( szToken == "Project_Dep_Name" ) 
		{
			if ( tokenizer.Next() ) 
			{
				if ( pCurrProject ) 
					pCurrProject->depends.push_back( tokenizer.GetToken() );
				else
					return false;
			}
			else
				return false;
		}
		else if ( IsSourceCodeControl(history) )
		{
			tokenizer.Next();
			std::string szSourceControlName = tokenizer.GetToken();
			const int nPos = szSourceControlName.rfind( '"' );
			if ( nPos != std::string::npos ) 
				szSourceControlName.erase( nPos, std::string::npos );
			NStr::TrimBoth( szSourceControlName, '"' );
			if ( !szSourceControlName.empty() && szSourceControlName[szSourceControlName.size() - 1] != '/' )
				szSourceControlName += '/';
			if ( pCurrProject ) 
			{
				pCurrProject->szSourceControl = szSourceControlName;
				if ( bGetLatestVersion && !szSourceControlName.empty() ) 
				{
					ExecuteSourceCommand( szGetVersion, szSourceControlName, pCurrProject->szPathName, " -R -I-" );
				}
			}
			else
				return false;
		}
	}

	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildVersion::LoadDSP( SProject *pProject )
{
	std::list<std::string> history;
	//
	const std::string szProjectPath = pProject->szPathName;
	// load file
	FILE *file = fopen( pProject->szFileName.c_str(), "rb" );
	if ( file == 0 ) 
		return false;
	const int nSize = _filelength( _fileno(file) );
	std::string szString;
	szString.resize( nSize );
	fread( &(szString[0]), 1, nSize, file );
	fclose( file );
	// parse
	CStringTokenizer<char> tokenizer( szString, ' ' );
	while ( tokenizer.Next() ) 
	{
		const std::string szToken = tokenizer.GetToken();
		history.push_back( szToken );
		while ( history.size() > 4 )
			history.pop_front();
		//
		if ( IsBeginSourceFile(history) ) 
		{
			tokenizer.Next();
			std::string szSourceFileName = tokenizer.GetToken();
			NStr::TrimBoth( szSourceFileName, '"' );
			if ( szSourceFileName.empty() ) 
				continue;
			if ( szSourceFileName[0] == '$' ) 
				continue;
			szSourceFileName = GetFullName( szProjectPath + szSourceFileName );
			// don't add resource file as a source - it will be modified to increment build version
			if ( IsResourceFile(szSourceFileName) ) 
				pProject->szResourceFileName = szSourceFileName;
			else
				pProject->sources.push_back( szSourceFileName );
		}
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CProjectFindFunctional
{
	std::string szName;
public:
	explicit CProjectFindFunctional( const std::string &_szName )
		: szName( _szName ) {  }

	bool operator()( const SProject *p ) const { return p->szName == szName; }
};
bool CBuildVersion::LoadProjects( const char *pszWorkspaceName, const char *pszHeadProject, std::list<SProject*> &projects )
{
	LoadDSW( pszWorkspaceName, true );
	if ( mapProjects.empty() ) 
		return false;
	CProjectsMap::iterator posHeadProject = mapProjects.find( pszHeadProject );
	if ( posHeadProject == mapProjects.end() ) 
		return false;
	// extract projects to check
	projects.clear();
	projects.push_back( &( posHeadProject->second ) );
	LoadDSP( projects.back() );
	for ( std::list<SProject*>::iterator it = projects.begin(); it != projects.end(); ++it )
	{
		for ( std::list<std::string>::const_iterator proj = (*it)->depends.begin(); proj != (*it)->depends.end(); ++proj )
		{
			const std::string szProjectName = *proj;
			if ( std::find_if( projects.begin(), projects.end(), CProjectFindFunctional(szProjectName) ) == projects.end() )
			{
				SProject *pProject = &( mapProjects[szProjectName] );
				LoadDSP( pProject );
				projects.push_back( pProject );
			}
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildVersion::CheckProjects( const std::list<SProject*> &projects, const SProject *pHeadProject, std::string *pVersion ) const
{
	const std::string szVersionName = pHeadProject->szFileName.substr( 0, pHeadProject->szFileName.rfind('.') ) + ".ver";
	CHandle hFile = ::CreateFile( szVersionName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
	if ( !hFile.IsValid() ) 
		hFile = ::CreateFile( szVersionName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0 );
	if ( !hFile.IsValid() ) 
		return false;
	FILETIME timeVersion;
	if ( GetFileTime(hFile, &timeVersion) == false )
		return false;
	//
	if ( !IsModified(projects, timeVersion) ) 
		return false;
	// increment version
	// read version
	char buffer[1024];
	DWORD dwReadBytes = 0;
	::ReadFile( hFile, buffer, 1024, &dwReadBytes, 0 );
	buffer[dwReadBytes] = 0;
	hFile.Close();
	*pVersion = buffer;
	//
	bool bNeedCheckIn = CheckOut( pHeadProject, szVersionName );
	hFile = ::CreateFile( szVersionName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0 );
	if ( !hFile.IsValid() ) 
		return false;
	//
	if ( pVersion->empty() ) 
		*pVersion = "1,0,0,1";
	else
	{
		std::string szSubVersion = pVersion->substr( pVersion->rfind(',') + 1 );
		int nVersion = NStr::ToInt( szSubVersion ) + 1;
		szSubVersion = NStr::Format( "%d", nVersion );
		*pVersion = pVersion->substr( 0, pVersion->rfind(',') + 1 ) + szSubVersion;
	}
	DWORD dwNumWrittenBytes = 0;
	::WriteFile( hFile, pVersion->c_str(), pVersion->size(), &dwNumWrittenBytes, 0 );
	//
	if ( bNeedCheckIn ) 
		CheckIn( pHeadProject, szVersionName );
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildVersion::UpdateVersion( const SProject *pProject, const std::string &szVersion ) const
{
	if ( pProject->szResourceFileName.empty() ) 
		return false;
	//
	std::string szVersion2 = szVersion;
	for ( int i = 0; i < szVersion2.size(); ++i )
	{
		if ( szVersion2[i] == ',' )
			szVersion2.insert( i + 1, " " );
	}
	szVersion2 += "\\0\"";
	szVersion2 = "\"" + szVersion2;
	const std::string szInputFileName = pProject->szResourceFileName + '~';
	const std::string szOutputFileName = pProject->szResourceFileName;
	bool bNeedCheckIn = CheckOut( pProject, pProject->szResourceFileName );
	MoveFile( szOutputFileName.c_str(), szInputFileName.c_str() );
	// update version
	{
		std::ifstream input( szInputFileName.c_str() );
		std::ofstream output( szOutputFileName.c_str() );
		//
		if ( !input.is_open() || !output.is_open() ) 
			return false;
		//
		while ( !input.eof() ) 
		{
			char buffer[1024];
			input.getline( buffer, 1024 );
			std::string szString = buffer;
			if ( ReplaceVersion(&szString, szVersion, pszVersionTag) == false )
				ReplaceVersion( &szString, szVersion2, pszVersion2Tag );
			output << szString << std::endl;
		}
	}
	// check in file
	if ( bNeedCheckIn ) 
		CheckIn( pProject, pProject->szResourceFileName ); 
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildVersion::UpdateVersion( const char *pszWorkspaceName, const char *pszHeadProject )
{
	std::list<SProject*> projects;
	// load DSW and all dependent DSP files
	if ( LoadProjects(pszWorkspaceName, pszHeadProject, projects) == false || projects.empty() )
		return false;
	// do we need update version?
	std::string szVersion;
	if ( CheckProjects(projects, projects.front(), &szVersion) == false )
		return true;
	// do version update
	for ( std::list<SProject*>::const_iterator it = projects.begin(); it != projects.end(); ++it )
		UpdateVersion( *it, szVersion );
	//
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CBuildVersion::MakeBuild( const char *pszHeadProject )
{
	CProjectsMap::const_iterator pos = mapProjects.find( pszHeadProject );
	if ( pos == mapProjects.end() ) 
		return false;
	//
	char pszCommandLine[2048];
	strcpy( pszCommandLine, "/f " );
	strcat( pszCommandLine, pszHeadProject );
	strcat( pszCommandLine, ".mak CFG=\"" );
	strcat( pszCommandLine, pszHeadProject );
	strcat( pszCommandLine, " - Win32 BetaRelease\"" );

	return ExecuteCommand( szNMake, pszCommandLine, pos->second.szPathName );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
