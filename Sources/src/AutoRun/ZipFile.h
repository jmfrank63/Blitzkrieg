#ifndef __ZIPFILE_H__
#define __ZIPFILE_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "FileUtils.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CZipFile
{
	struct SZipLocalFileHeader;
	struct SZipFileHeader;
	struct SZipDataDescriptor;
	struct SZipCentralDirHeader;
	//
	char *m_pDirData;											// Raw data buffer.
	int m_nEntries;												// Number of entries.
	const SZipFileHeader **m_papDir;			// Pointers to the dir entries in pDirData.
	NFile::CFile zipfile;									// main zip file
	typedef std::unordered_map<std::string, int> CFilesMap;
	CFilesMap files;											// map <file name> ==> <file index>
public:
	CZipFile() : m_nEntries(0) {  }
	~CZipFile() { Fini(); }
	//
	bool Init( const std::string &szFileName );
	void Fini();
	bool IsOk() const { return (m_nEntries != 0); }

	int GetNumFiles() const { return m_nEntries; }
	//
	int GetFileIndex( const std::string &szFileName ) const 
	{ 
		CFilesMap::const_iterator pos = files.find( szFileName );
		return pos == files.end() ? -1 : pos->second;
	}
	//
	int GetCompressionMethod( int nIndex ) const;
	void GetFileName( int nIndex, std::string *pString ) const;
	int GetFileLen( int nIndex ) const;
	DWORD GetFileAttribs( int nIndex ) const;
	DWORD GetModDateTime( int nIndex ) const;	// high word - date, low word - time
	bool IsDirectory( int nIndex ) const;

	bool ReadFile( const int nIndex, void *pBuf );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ZIPFILE_H__
