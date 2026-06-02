#ifndef __FILE_UTILS_H__
#define __FILE_UTILS_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <io.h>
#include <string>
namespace NFile
{
class CFile
{
	HANDLE hFile;
	std::string szFilePath;
public:
	struct SStatus
	{
		FILETIME ctime;											// creation date/time of file
		FILETIME mtime;											// last modification date/time of file
		FILETIME atime;											// last access date/time of file
		int nSize;													// logical size of file in bytes
		DWORD dwAttributes;									// logical OR of CFile::Attribute enum values
		std::string szPathName;							// absolute path name
	};
	enum EOpenFlags 
	{
		modeRead				= 0x0000,
		modeWrite				= 0x0001,
		modeReadWrite		= 0x0002,
		shareCompat			=	0x0000,
		shareExclusive	= 0x0010,
		shareDenyWrite	= 0x0020,
		shareDenyRead		= 0x0030,
		shareDenyNone		= 0x0040,
		modeNoInherit		= 0x0080,
		modeCreate			= 0x1000,
		modeNoTruncate	= 0x2000
	};
	enum EAttribute 
	{
		readOnly							= 0x00000001,
		hidden								= 0x00000002,
		system								= 0x00000004,
		directory							= 0x00000010,
		archive								= 0x00000020,
		encrypted							= 0x00000040,
		normal								= 0x00000080,
		temporary							= 0x00000100,
		sparse_file						= 0x00000200,
		reparse_point					= 0x00000400,
		compressed						= 0x00000800,
		offline								= 0x00001000,
		not_content_indexed		= 0x00002000
	};
	enum ESeekPosition { begin = 0x0, current = 0x1, end = 0x2 };
public:
	CFile() : hFile( INVALID_HANDLE_VALUE ) {  }
	CFile( const char *pszFileName, DWORD dwOpenFlags ) { Open( pszFileName, dwOpenFlags ); }
	virtual ~CFile() { Close(); }
	bool Open( const char *pszFileName, DWORD dwOpenFlags );
	CFile* Duplicate() const;
	void Close();
	bool Flush();
	bool IsOpen() const { return hFile != INVALID_HANDLE_VALUE; }
	int Read( void *pBuf, int nCount );
	int Write( const void *pBuf, int nCount );
	int Seek( int nOffset, ESeekPosition eFrom );
	int GetPosition() const;
	int SetLength( int nNewLen );
	int GetLength() const;
	bool GetStatus( SStatus *pStatus ) const;
	DWORD GetAttributes() const;
	bool SetAttributes( DWORD dwAttributes );
	operator HANDLE() const { return hFile; }
	const std::string& GetFilePath() const;	// complete path name (ZB. c:\program files\name.txt)
	const std::string GetFileName() const;	// title + ext (ZB. name.txt)
	const std::string GetFileTitle() const;	// title onle (ZB. name)
	const std::string GetFileExt() const;		// extension onle (ZB. txt)
	static bool Rename( const char *pszOldName, const char *pszNewName );
	static bool Remove( const char *pszFileName );
	static DWORD GetAttributes( const char *pszFileName );
	static bool SetAttributes( const char *pszFileName, DWORD dwAttributes );
};
class CFileIterator
{
	HANDLE hFind;													// find file handle of the last search result
	WIN32_FIND_DATA findinfo;							// last search info
	std::string szPath;                   // path to the file
	std::string szMask;
	bool IsValid() const { return hFind != INVALID_HANDLE_VALUE; }
public:
	CFileIterator() : hFind( INVALID_HANDLE_VALUE ) {  }
	CFileIterator( const char *pszMask ) { FindFirstFile(pszMask); }
	~CFileIterator() { Close(); }
	const CFileIterator& FindFirstFile( const char *pszMask );
	const CFileIterator& Next();
	bool Close();
	bool IsEnd() const { return !IsValid(); }
	const CFileIterator& operator++() { return Next(); }
	DWORD GetAttribs() const { return findinfo.dwFileAttributes; }
	bool IsReadOnly() const { return ( GetAttribs() & NFile::CFile::readOnly ) != 0; }
	bool IsSystem() const { return ( GetAttribs() & NFile::CFile::system ) != 0; }
	bool IsHidden() const { return ( GetAttribs() & NFile::CFile::hidden ) != 0; }
	bool IsTemporary() const { return ( GetAttribs() & NFile::CFile::temporary ) != 0; }
	bool IsNormal() const { return ( GetAttribs() & NFile::CFile::normal ) != 0; }
	bool IsArchive() const { return ( GetAttribs() & NFile::CFile::archive ) != 0; }
	bool IsCompressed() const { return ( GetAttribs() & NFile::CFile::compressed ) != 0; }
	bool IsDirectory() const { return ( GetAttribs() & NFile::CFile::directory ) != 0; }
	bool IsDots() const
	{
		return ( ( findinfo.cFileName[0] == '.' ) && 
			       ( (findinfo.cFileName[1] == '\0') || 
						   ((findinfo.cFileName[1] == '.') && (findinfo.cFileName[2] == '\0')) ) );
	}
	bool IsMatchesMask( DWORD dwMask ) const { return (GetAttribs() & dwMask) == dwMask; }
	FILETIME GetCreationTime() const { return findinfo.ftCreationTime; }
	FILETIME GetLastAccessTime() const { return findinfo.ftLastAccessTime; }
	FILETIME GetLastWriteTime() const { return findinfo.ftLastWriteTime; }
	int GetLength() const { return findinfo.nFileSizeLow; }
	const std::string GetFileName() const { return findinfo.cFileName; }
	const std::string GetFilePath() const { return szPath + findinfo.cFileName; }
	const std::string GetFileTitle() const;
	const std::string GetFileExt() const;
	const std::string& GetBasePath() const { return szPath; }
	const std::string& GetBaseMask() const { return szMask; }
};
template <class TEnumFunc>
void EnumerateFiles( const char *pszStartDir, const char *pszMask, TEnumFunc callback, bool bRecurse )
{
	std::string szDir = pszStartDir;
	for ( CFileIterator it( (szDir + pszMask).c_str() ); !it.IsEnd(); ++it )
	{
		if ( !it.IsDirectory() )
			callback( it );
	}
	for ( CFileIterator it( (szDir + "*.*").c_str() ); !it.IsEnd(); ++it )
	{
		if ( it.IsDirectory() )
		{
			if ( it.IsDots() )
				continue;
			if ( bRecurse )
				EnumerateFiles( (it.GetFilePath() +  "\\").c_str(), pszMask, callback, bRecurse );
			callback( it );
		}
	}
}
class CGetAllFiles
{
	std::vector<std::string> *pFileVector;
public:
	CGetAllFiles( std::vector<std::string> *pFiles ) : pFileVector( pFiles ) {  }
	void operator() ( const NFile::CFileIterator &it )
	{
		if ( !it.IsDirectory() )
		{
			pFileVector->push_back( it.GetFilePath() );
		}
	}
};
void DeleteFiles( const char *pszStartDir, const char *pszMask, bool bRecursive );
void DeleteDirectory( const char *pszDir );
void CreatePath( const char *pszFullPath );
void GetDirNames( const char *pszDirName, std::list<std::string> *pNames, bool bRecursive = true );
void GetFileNames( const char *pszDirName, const char *pszMask, std::list<std::string> *pNames, bool bRecurse = true );
bool IsFileExist( const char *pszFileName );
std::string GetFullName( const std::string &szPath );
double GetFreeDiskSpace( const char *pszDrive );
}; // namespace NFile ends
template <class TYPE>
	inline NFile::CFile& operator<<( NFile::CFile &file, const TYPE &data )
	{
		file.Write( &data, sizeof(data) );
		return file;
	}
template <class TYPE>
	inline NFile::CFile& operator>>( NFile::CFile &file, TYPE &data )
	{
		file.Read( &data, sizeof(data) );
		return file;
	}
template <>
	inline NFile::CFile& operator<<( NFile::CFile &file, const std::string &data )
	{
		int nLength = data.size();
		file << nLength;
		file.Write( data.c_str(), nLength * sizeof(data[0]) );
		return file;
	}
template <>
	inline NFile::CFile& operator>>( NFile::CFile &file, std::string &data )
	{
		int nLength = 0;
		file >> nLength;
		data.resize( nLength );
		file.Read( &(data[0]), nLength * sizeof(data[0]) );
		return file;
	}
template <>
	inline NFile::CFile& operator<<( NFile::CFile &file, const std::wstring &data )
	{
		int nLength = data.size();
		file << nLength;
		file.Write( data.c_str(), nLength * sizeof(data[0]) );
		return file;
	}
template <>
	inline NFile::CFile& operator>>( NFile::CFile &file, std::wstring &data )
	{
		int nLength = 0;
		file >> nLength;
		data.resize( nLength );
		file.Read( &(data[0]), nLength * sizeof(data[0]) );
		return file;
	}
#endif // __FILE_UTILS_H__
