#ifndef __SAVELOADCOMMON_H__
#define __SAVELOADCOMMON_H__
#pragma ONCE
#include "..\Misc\FileUtils.h"
struct SLoadFileDesc
{
	std::string szFileName;
	FILETIME time;
	int nSize;
	SLoadFileDesc() {  }
	SLoadFileDesc( const std::string &_szFileName, const FILETIME &_time, const int _nSize )
		: szFileName( _szFileName ), time( _time ), nSize( _nSize ) {  }
};
class CGetFiles2Load
{
	std::vector<SLoadFileDesc> &files;
	std::string szPath;
public:
	CGetFiles2Load( std::vector<SLoadFileDesc> &_files, const std::string &_szPath )
		: files( _files ), szPath( _szPath ) {  }
	bool operator()( const NFile::CFileIterator &it )
	{
		if ( !it.IsDirectory() && (it.GetLength() > 1024) )
		{
			std::string szFileName = it.GetFilePath();
			// Use the basename rather than stripping a fixed szPath prefix: the
			// enumerator's path and szPath can differ in form (drive letter /
			// separator / case), which left the full path in the list and broke
			// load ("shows path in file selector" / "cannot load - not found").
			const std::string::size_type nSlash = szFileName.find_last_of( "\\/" );
			if ( nSlash != std::string::npos )
				szFileName = szFileName.substr( nSlash + 1 );
			files.push_back( SLoadFileDesc(szFileName, it.GetLastWriteTime(), it.GetLength()) );
		}
		return true;
	}
};
struct SLoadFileLessFunctional
{
	bool operator()( const SLoadFileDesc &f1, const SLoadFileDesc &f2 ) const { return CompareFileTime( &f1.time, &f2.time ) == 1; }
};
#endif // __SAVELOADCOMMON_H__