#include "StdAfx.h"

#include "ZipFile.h"

#include "..\zlib\zlib.h"
#include "StreamAdaptor.h"
#include "MemFileSystem.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** single zip file
// **
// ** format:
// **		//files data
// **		[local file header 1]
// **		[file data 1]
// **		[data descriptor 1]
// **		...
// **		[local file header N]
// **		[file data N]
// **		[data descriptor N]
// **		// central directory
// **		[file header 1]
// **		...
// **		[file header N]
// **		[digital signature]
// **		[central dir header]
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	// here filename follows (wFileNameLen bytes)
	// here extra field follows (wExtraLen bytes)
	bool IsDataDescriptorExist() const { return (flag & 4) != 0; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: data descriptor exist only, if bit 3 of general purpose flag of the corresponding local file header is set
// one can call IsDataDescriptorExist() to check this fact
// if data descriptor exist, then one must get CRC32, CSize and USize from the data descriptor instead of from local file header
struct CZipFile::SZipDataDescriptor
{
	DWORD dwCRC32;												// CRC-32
	DWORD dwCSize;												// compressed size
	DWORD dwUSize;												// uncompressed size
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	// comment follows here (wCommentLen bytes)
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	// filename follows here (wFileNameLen bytes)
	// extra field follows here (wExtraLen bytes)
	// file comment follows here (wCommentLen bytes)
	const char* GetName() const { return (const char *)(this + 1); }
	const char* GetExtra() const { return GetName() + wFileNameLen; }
	const char* GetComment() const { return GetExtra() + wExtraLen; }
};
// host operating system codes
// 0  - MS-DOS and OS/2 (FAT / VFAT / FAT32 file systems) 
// 1  - Amiga 
// 2  - OpenVMS 
// 3  - Unix 
// 4  - VM/CMS 
// 5  - Atari ST 
// 6  - OS/2 H.P.F.S. 
// 7  - Macintosh 
// 8  - Z-System 
// 9  - CP/M 
// 10 - Windows NTFS 
// 11 - MVS 
// 12 - VSE 
// 13 thru 255 - unused

// compression methods
// 0  - The file is stored (no compression) 
// 1  - The file is Shrunk 
// 2  - The file is Reduced with compression factor 1 
// 3  - The file is Reduced with compression factor 2 
// 4  - The file is Reduced with compression factor 3 
// 5  - The file is Reduced with compression factor 4 
// 6  - The file is Imploded  
// 7  - Reserved for Tokenizing compression algorithm  
// 8  - The file is Deflated  
// 9  - Enhanced Deflating using Deflate64(tm) 
// 10 - PKWARE Date Compression Library Imploding 

#pragma pack()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CZipFile::Init( IDataStream *pZipStream )
{
	NI_ASSERT_TF( pZipStream != 0, "NULL stream passed to zip file", return false );
	//
	Fini();
	// Assuming no extra comment at the end, read the whole end record.
	SZipCentralDirHeader cdh;
	pZipStream->Seek( -sizeof(cdh), STREAM_SEEK_END );
	long cdhOffset = pZipStream->GetPos();
	pZipStream->Read( &cdh, sizeof(cdh) );

	// Check
	NI_ASSERT_TF( cdh.dwSignature == SZipCentralDirHeader::SIGNATURE, "Can't recognize zip dir header", return false );

	// Go to the beginning of the directory.
	pZipStream->Seek( cdhOffset - cdh.dwDirSize, STREAM_SEEK_SET );

	// Allocate the data buffer, and read the whole thing.
	m_pDirData = new char[cdh.dwDirSize + cdh.wDirEntries*sizeof(*m_papDir)];
	pZipStream->Read( m_pDirData, cdh.dwDirSize );

	// Now process each entry.
	char *pfh = m_pDirData;
	m_papDir = reinterpret_cast<const SZipFileHeader **>( m_pDirData + cdh.dwDirSize );

	bool bRet = true;

	for ( int i=0; (i < cdh.wDirEntries) && (bRet == true); ++i )
	{
		SZipFileHeader &fh = *reinterpret_cast<SZipFileHeader*>( pfh );

		// Store the address of i-th file for quicker access.
		m_papDir[i] = &fh;

		// Check the directory entry integrity.
		if ( fh.dwSignature != SZipFileHeader::SIGNATURE )
			bRet = false;
		else
		{
			pfh += sizeof( fh );
			// Convert UNIX slashes to DOS backlashes.
			std::replace_if( pfh, pfh + fh.wFileNameLen, [](char c){ return c == '/'; }, '\\' );
			// Skip name, extra and comment fields.
			pfh += fh.wFileNameLen + fh.wExtraLen + fh.wCommentLen;
		}
	}
	if ( bRet == false )
		delete []m_pDirData;
	else
		m_nEntries = cdh.wDirEntries;

	return bRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CZipFile::Fini()
{
	if ( IsOk() )
	{
		delete []m_pDirData;
		m_nEntries = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CZipFile::GetCompressionMethod( int nIndex ) const
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return 0 );
	return m_papDir[nIndex]->wCompression;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CZipFile::GetFileName( int nIndex, std::string *pString ) const
{
	NI_ASSERT_SLOW_TF( pString != 0, "NULL pointer to get file name", return );
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), pString->clear(); return );
	pString->resize( m_papDir[nIndex]->wFileNameLen );
	memcpy( &((*pString)[0]), m_papDir[nIndex]->GetName(), m_papDir[nIndex]->wFileNameLen );
	//(*pString)[ m_papDir[nIndex]->wFileNameLen ] = '\0';
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CZipFile::GetFileLen( int nIndex ) const
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return 0 );
	return m_papDir[nIndex]->dwUSize;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD CZipFile::GetFileAttribs( int nIndex ) const
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return 0 );
	return m_papDir[nIndex]->wExtAttr;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD CZipFile::GetModDateTime( int nIndex ) const
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return 0 );
	return DWORD( (m_papDir[nIndex]->wModDate) << 16 ) | DWORD( m_papDir[nIndex]->wModTime );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CZipFile::IsDirectory( int nIndex ) const 
{ 
	return (m_papDir[nIndex]->wExtAttr & 0x00000010) == 0x00000010; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CZipFile::ReadFile( IDataStream *pStream, int nIndex, void *pBuf )
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return false );
	NI_ASSERT_SLOW_TF( pBuf != 0, "NULL pointer to uncompress file", return false );

	// Quick'n dirty read, the whole file at once.
	// Ungood if the ZIP has huge files inside

	// Go to the actual file and read the local header.
	pStream->Seek( m_papDir[nIndex]->dwHdrOffset, STREAM_SEEK_SET );

	SZipLocalFileHeader hdr;
	pStream->Read( &hdr, sizeof(hdr) );
	NI_ASSERT_TF( hdr.dwSignature == SZipLocalFileHeader::SIGNATURE, "can't recognize zip local header", return false );

	// Skip extra fields
	pStream->Seek( hdr.wFileNameLen + hdr.wExtraLen, STREAM_SEEK_CUR );

	// in the STORE case, just simply read in raw stored data
	if ( hdr.wCompression == SZipLocalFileHeader::COMP_STORE ) 
		return pStream->Read( pBuf, hdr.dwCSize ) == hdr.dwCSize;
	// process DEFLAT unpacking
	NI_ASSERT_TF( hdr.wCompression == SZipLocalFileHeader::COMP_DEFLAT, "Can support STORE and DEFLAT now", return false );

	// Alloc compressed data buffer and read the whole stream
	char *pcData = new char[hdr.dwCSize];
	pStream->Read( pcData, hdr.dwCSize );

	// Setup the inflate stream.
	z_stream stream;
	stream.next_in = (Bytef*)pcData;
	stream.avail_in = (uInt)hdr.dwCSize;
	stream.next_out = (Bytef*)pBuf;
	stream.avail_out = hdr.dwUSize;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;

	// Perform inflation. wbits < 0 indicates no zlib header inside the data.
	int err = inflateInit2( &stream, -MAX_WBITS );
	if ( err == Z_OK )
	{
		err = inflate( &stream, Z_FINISH );
		inflateEnd( &stream );
		// CRAP{ почему-то иногда при распаковке возвращается "buffer error" всесто "stream end"...
		if ( (err == Z_STREAM_END) || (err == Z_BUF_ERROR) )
			err = Z_OK;
		// CRAP}
		inflateEnd( &stream );
	}

	delete []pcData;

	return err == Z_OK;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IDataStream* CZipFile::ReadFile( IDataStream *pStream, int nIndex )
{
	NI_ASSERT_SLOW_TF( (nIndex >= 0) && (nIndex < m_nEntries), NStr::Format("index %d out of range", nIndex), return false );

	// Quick'n dirty read, the whole file at once.
	// Ungood if the ZIP has huge files inside

	// Go to the actual file and read the local header.
	pStream->Seek( m_papDir[nIndex]->dwHdrOffset, STREAM_SEEK_SET );

	SZipLocalFileHeader hdr;
	pStream->Read( &hdr, sizeof(hdr) );
	NI_ASSERT_TF( hdr.dwSignature == SZipLocalFileHeader::SIGNATURE, "can't recognize zip local header", return false );

	// Skip extra fields
	pStream->Seek( hdr.wFileNameLen + hdr.wExtraLen, STREAM_SEEK_CUR );

	// in the STORE case, just simply read in raw stored data
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
	// create new memory stream and setup stats
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
	// proceed with DEFLAT unpacking
	NI_ASSERT_TF( hdr.wCompression == SZipLocalFileHeader::COMP_DEFLAT, "Can support STORE and DEFLAT now", return false );

	// Alloc compressed data buffer and read the whole stream
	char *pcData = new char[hdr.dwCSize];
	pStream->Read( pcData, hdr.dwCSize );

	// Setup the inflate stream.
	z_stream stream;
	stream.next_in = (Bytef*)pcData;
	stream.avail_in = (uInt)hdr.dwCSize;
	stream.next_out = (Bytef*)pBuf;
	stream.avail_out = hdr.dwUSize;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;

	// Perform inflation. wbits < 0 indicates no zlib header inside the data.
	int err = inflateInit2( &stream, -MAX_WBITS );
	if ( err == Z_OK )
	{
		err = inflate( &stream, Z_FINISH );
		inflateEnd( &stream );
		// CRAP{ почему-то иногда при распаковке возвращается "buffer error" всесто "stream end"...
		if ( (err == Z_STREAM_END) || (err == Z_BUF_ERROR) )
			err = Z_OK;
		// CRAP}
		inflateEnd( &stream );
	}

	delete []pcData;

	if ( err == Z_OK )
		return pDstStream;

	pDstStream->AddRef();
	pDstStream->Release();
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
