#ifndef __ZIPFILE_H__
#define __ZIPFILE_H__
#pragma ONCE
class CZipFile
{
	struct SZipLocalFileHeader;
	struct SZipFileHeader;
	struct SZipDataDescriptor;
	struct SZipCentralDirHeader;
	char *m_pDirData;											// Raw data buffer.
	int m_nEntries;												// Number of entries.
	const SZipFileHeader **m_papDir;			// Pointers to the dir entries in pDirData.
public:
	CZipFile() : m_nEntries(0) {  }
	~CZipFile() { Fini(); }
	bool Init( IDataStream *pStream );
	void Fini();
	bool IsOk() const { return (m_nEntries != 0); }

	int GetNumFiles() const { return m_nEntries; }
	int GetCompressionMethod( int nIndex ) const;
	void GetFileName( int nIndex, std::string *pString ) const;
	int GetFileLen( int nIndex ) const;
	DWORD GetFileAttribs( int nIndex ) const;
	DWORD GetModDateTime( int nIndex ) const;	// high word - date, low word - time
	bool IsDirectory( int nIndex ) const;

	bool ReadFile( IDataStream *pStream, int nIndex, void *pBuf );
	IDataStream* ReadFile( IDataStream *pStream, int nIndex );
};
#endif // __ZIPFILE_H__
