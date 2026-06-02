#include "StdAfx.h"

#include "ZipFile.h"

#include "..\zlib\zlib.h"
#include "StreamAdaptor.h"
#include "MemFileSystem.h"
#pragma pack( 1 )
struct CZipFile::SZipLocalFileHeader
{
	enum { SIGNATURE = 0x04034b50, COMP_STORE = 0, COMP_DEFLAT = 8 };
	DWORD dwSignature;										// local file header signature
	WORD  version;												// version needed to extract
	WORD  flag;														// general purpose bit flag
	WORD  wCompression;										// compression method: COMP_xxxx
	WORD  wModTime;												// last mod file time (MS-DOS)
	WORD  wModDate;												// last mod file date (MS-DOS)
	DWORD dwCRC32;												// CRC-32
	DWORD dwCSize;												// compressed size
	DWORD dwUSize;												// uncompressed size
	WORD  wFileNameLen;										// filename length (w/o zero terminator!)
	WORD  wExtraLen;											// extra field length
	bool IsDataDescriptorExist() const { return (flag & 4) != 0; }
};
struct CZipFile::SZipDataDescriptor
{
	DWORD dwCRC32;												// CRC-32
	DWORD dwCSize;												// compressed size
	DWORD dwUSize;												// uncompressed size
};
struct CZipFile::SZipCentralDirHeader
{
	enum { SIGNATURE = 0x06054b50 };
	DWORD dwSignature;										// end of central dir signature
	WORD  wDisk;													// number of this disk
	WORD  wStartDisk;											// number of disk with start central dir
	WORD  wDirEntries;										// total number of entries in central dir on this disk
	WORD  wTotalDirEntries;								// total number entries in central dir
	DWORD dwDirSize;											// size of central directory
	DWORD dwDirOffset;										// offset of start of central directory with respect to the starting disk nuber
	WORD  wCommentLen;										// zipfile comment length
};
struct CZipFile::SZipFileHeader
{
	enum { SIGNATURE = 0x02014b50, COMP_STORE = 0, COMP_DEFLAT = 8 };
	DWORD dwSignature;										// central file header signature
	BYTE  verMade;												// version made by
	BYTE  os;															// host operating system
	BYTE  verNeeded;											// version needed to extract
	BYTE  osNeeded;												// OS of version needed for extraction
	WORD  flag;														// general purpose bit flag
	WORD  wCompression;										// compression method: COMP_xxxx
	WORD  wModTime;												// last mode file time (MS-DOS)
	WORD  wModDate;												// last mode file date (MS-DOS)
	DWORD dwCRC32;												// CRC-32
	DWORD dwCSize;												// compressed size
	DWORD dwUSize;												// uncompressed size
	WORD  wFileNameLen;										// filename length
	WORD  wExtraLen;											// extra field length
	WORD  wCommentLen;										// file comment length
	WORD  wDiskStart;											// disk number start
	WORD  wIntAttr;												// internal file attributes: bit0 == 1 => ASCII or text file, == 0 => binary data
	DWORD wExtAttr;												// external file attributes, host system dependent
	DWORD dwHdrOffset;										// relative offset of local header from the start of the first disk, on which this file appears
	const char* GetName() const { return (const char *)(this + 1); }
	const char* GetExtra() const { return GetName() + wFileNameLen; }
	const char* GetComment() const { return GetExtra() + wExtraLen; }
};


#pragma pack()
bool CZipFile::Init( IDataStream *pZipStream )
{
	NI_ASSERT_TF( pZipStream != 0, "NULL stream passed to zip file", return false );
	Fini();
	SZipCentralDirHeader cdh;
	pZipStream->Seek( -sizeof(cdh), STREAM_SEEK_END );
	long cdhOffset = pZipStream->GetPos();
	pZipStream->Read( &cdh, sizeof(cdh) );

	NI_ASSERT_TF( cdh.dwSignature == SZipCentralDirHeader::SIGNATURE, "Can't recognize zip dir header", return false );

	pZipStream->Seek( cdhOffset - cdh.dwDirSize, STREAM_SEEK_SET );

	m_pDirData = new char[cdh.dwDirSize + cdh.wDirEntries*sizeof(*m_papDir)];
	pZipStream->Read( m_pDirData, cdh.dwDirSize );

	char *pfh = m_pDirData;
	m_papDir = reinterpret_cast<const SZipFileHeader **>( m_pDirData + cdh.dwDirSize );

	bool bRet = true;

	for ( int i=0; (i < cdh.wDirEntries) && (bRet == true); ++i )
	{
		SZipFileHeader &fh = *reinterpret_cast<SZipFileHeader*>( pfh );

		m_papDir[i] = &fh;

		if ( fh.dwSignature != SZipFileHeader::SIGNATURE )
			bRet = false;
		else
		{
			pfh += sizeof( fh );
			std::replace_if( pfh, pfh + fh.wFileNameLen, [](char c){ return c == '/'; }, '\\' );
			pfh += fh.wFileNameLen + fh.wExtraLen + fh.wCommentLen;
		}
	}
	if ( bRet == false )
		delete []m_pDirData;
	else
		m_nEntries = cdh.wDirEntries;

	return bRet;
}
void CZipFile::Fini()
{
	if ( IsOk() )
	{
		delete []m_pDirData;
		m_nEntries = 0;
	}
}
int CZipFile::GetCompressionMethod( int nIndex ) const
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return 0 );
	return m_papDir[nIndex]->wCompression;
}
void CZipFile::GetFileName( int nIndex, std::string *pString ) const
{
	NI_ASSERT_SLOW_TF( pString != 0, "NULL pointer to get file name", return );
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), pString->clear(); return );
	pString->resize( m_papDir[nIndex]->wFileNameLen );
	memcpy( &((*pString)[0]), m_papDir[nIndex]->GetName(), m_papDir[nIndex]->wFileNameLen );
}
int CZipFile::GetFileLen( int nIndex ) const
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return 0 );
	return m_papDir[nIndex]->dwUSize;
}
DWORD CZipFile::GetFileAttribs( int nIndex ) const
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return 0 );
	return m_papDir[nIndex]->wExtAttr;
}
DWORD CZipFile::GetModDateTime( int nIndex ) const
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return 0 );
	return DWORD( (m_papDir[nIndex]->wModDate) << 16 ) | DWORD( m_papDir[nIndex]->wModTime );
}
bool CZipFile::IsDirectory( int nIndex ) const 
{ 
	return (m_papDir[nIndex]->wExtAttr & 0x00000010) == 0x00000010; 
}
bool CZipFile::ReadFile( IDataStream *pStream, int nIndex, void *pBuf )
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return false );
	NI_ASSERT_SLOW_TF( pBuf != 0, "NULL pointer to uncompress file", return false );


	pStream->Seek( m_papDir[nIndex]->dwHdrOffset, STREAM_SEEK_SET );

	SZipLocalFileHeader hdr;
	pStream->Read( &hdr, sizeof(hdr) );
	NI_ASSERT_TF( hdr.dwSignature == SZipLocalFileHeader::SIGNATURE, "can't recognize zip local header", return false );

	pStream->Seek( hdr.wFileNameLen + hdr.wExtraLen, STREAM_SEEK_CUR );

	if ( hdr.wCompression == SZipLocalFileHeader::COMP_STORE ) 
		return pStream->Read( pBuf, hdr.dwCSize ) == hdr.dwCSize;
	NI_ASSERT_TF( hdr.wCompression == SZipLocalFileHeader::COMP_DEFLAT, "Can support STORE and DEFLAT now", return false );

	char *pcData = new char[hdr.dwCSize];
	pStream->Read( pcData, hdr.dwCSize );

	z_stream stream;
	stream.next_in = (Bytef*)pcData;
	stream.avail_in = (uInt)hdr.dwCSize;
	stream.next_out = (Bytef*)pBuf;
	stream.avail_out = hdr.dwUSize;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;

	int err = inflateInit2( &stream, -MAX_WBITS );
	if ( err == Z_OK )
	{
		err = inflate( &stream, Z_FINISH );
		inflateEnd( &stream );
		if ( (err == Z_STREAM_END) || (err == Z_BUF_ERROR) )
			err = Z_OK;
		inflateEnd( &stream );
	}

	delete []pcData;

	return err == Z_OK;
}
IDataStream* CZipFile::ReadFile( IDataStream *pStream, int nIndex )
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return false );


	pStream->Seek( m_papDir[nIndex]->dwHdrOffset, STREAM_SEEK_SET );

	SZipLocalFileHeader hdr;
	pStream->Read( &hdr, sizeof(hdr) );
	NI_ASSERT_TF( hdr.dwSignature == SZipLocalFileHeader::SIGNATURE, "can't recognize zip local header", return false );

	pStream->Seek( hdr.wFileNameLen + hdr.wExtraLen, STREAM_SEEK_CUR );

	if ( hdr.wCompression == SZipLocalFileHeader::COMP_STORE ) 
	{
		std::string szName;
		GetFileName( nIndex, &szName );
		int nBeginPos = pStream->GetPos();
		if ( nBeginPos + hdr.dwCSize > pStream->GetSize() )
			return 0;
		SStorageElementStats stats;
		stats.nSize = hdr.dwUSize;
		stats.pszName = 0;
		stats.type = SET_STREAM;
		stats.mtime = stats.atime = stats.ctime = GetModDateTime( nIndex );
		return new CStreamRangeAdaptor( pStream, nBeginPos, nBeginPos + hdr.dwCSize, szName.c_str(), &stats );
	}
	CMemFileStream *pDstStream = new CMemFileStream( hdr.dwUSize, 0 );
	{
		std::string szName;
		GetFileName( nIndex, &szName );
		SStorageElementStats stats;
		stats.nSize = hdr.dwUSize;
		stats.pszName = szName.c_str();
		stats.type = SET_STREAM;
		stats.mtime = stats.atime = stats.ctime = GetModDateTime( nIndex );
		pDstStream->SetStats( stats );
	}
	void *pBuf = pDstStream->GetBuffer();
	NI_ASSERT_TF( hdr.wCompression == SZipLocalFileHeader::COMP_DEFLAT, "Can support STORE and DEFLAT now", return false );

	char *pcData = new char[hdr.dwCSize];
	pStream->Read( pcData, hdr.dwCSize );

	z_stream stream;
	stream.next_in = (Bytef*)pcData;
	stream.avail_in = (uInt)hdr.dwCSize;
	stream.next_out = (Bytef*)pBuf;
	stream.avail_out = hdr.dwUSize;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;

	int err = inflateInit2( &stream, -MAX_WBITS );
	if ( err == Z_OK )
	{
		err = inflate( &stream, Z_FINISH );
		inflateEnd( &stream );
		if ( (err == Z_STREAM_END) || (err == Z_BUF_ERROR) )
			err = Z_OK;
		inflateEnd( &stream );
	}

	delete []pcData;

	if ( err == Z_OK )
		return pDstStream;

	pDstStream->AddRef();
	pDstStream->Release();
	return 0;
}
