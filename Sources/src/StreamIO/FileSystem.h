#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__
#pragma ONCE
class CFileSystemEnumerator : public IStorageEnumerator
{
	OBJECT_MINIMAL_METHODS( CFileSystemEnumerator );
	HANDLE hFind;													// handle to FindFile
	WIN32_FIND_DATA findinfo;							// result of the FileFind
	std::string szPath;                   // path to the file
	std::string szMask;										// enumeration mask
	std::string szBase;										// base dir for enumeration (from the storage)
	std::string szFoundFileName;					// filename
	SStorageElementStats stats;						// temporary data storage to fill each call
	const bool IsFindValid() const { return hFind != INVALID_HANDLE_VALUE; }
private:
	bool FindFirstFile();
	bool FindNextFile();
	void Close();
	void FillStats();
	bool IsReadOnly() const { return ( findinfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY ) != 0; }
	bool IsSystem() const { return ( findinfo.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ) != 0; }
	bool IsHidden() const { return ( findinfo.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ) != 0; }
	bool IsTemporary() const { return ( findinfo.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY ) != 0; }
	bool IsNormal() const { return ( findinfo.dwFileAttributes & FILE_ATTRIBUTE_NORMAL ) != 0; }
	bool IsArchive() const { return ( findinfo.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ) != 0; }
	bool IsCompressed() const { return ( findinfo.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ) != 0; }
	bool IsDirectory() const { return ( findinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0; }
	bool IsDots() const
	{
		return IsDirectory() &&
					 ( ( (findinfo.cFileName[0] == '.') ) &&
		         ( (findinfo.cFileName[1] == '\0') || ((findinfo.cFileName[1] == '.') && (findinfo.cFileName[2] == '\0')) ) );
	}
public:
	CFileSystemEnumerator( const std::string &_szBase )
		: hFind( INVALID_HANDLE_VALUE ), szBase( _szBase ) { Zero( stats ); }
	virtual ~CFileSystemEnumerator();
	virtual void STDCALL Reset( const char *pszMask );
	virtual bool STDCALL Next();
	virtual const SStorageElementStats* STDCALL GetStats() const { return &stats; }
};
class CFileStream : public IDataStream
{
	OBJECT_MINIMAL_METHODS( CFileStream );
	HANDLE hFile;													// file, associated with this stream
	int nStreamBegin;											// stream begin position
	DWORD dwAccessMode;										// access mode
	std::string szName;
	SStorageElementStats stats;
private:
	bool CanRead() const { return IsOpen() && (dwAccessMode & STREAM_ACCESS_READ); }
	bool CanWrite() const { return IsOpen() && (dwAccessMode & STREAM_ACCESS_WRITE); }
public:
	CFileStream( const char *pszFileName, DWORD dwAccessMode );
	virtual ~CFileStream();
	virtual int STDCALL Read( void *pBuffer, int nLength );
	virtual int STDCALL Write( const void *pBuffer, int nLength );
	virtual int STDCALL LockBegin();
	virtual int STDCALL UnlockBegin();
	virtual int STDCALL GetPos() const;
	virtual int STDCALL Seek( int offset, STREAM_SEEK from );
	virtual int STDCALL GetSize() const;
	virtual bool STDCALL SetSize( int nSize );
	virtual int STDCALL CopyTo( IDataStream *pDstStream, int nLength );
	virtual void STDCALL Flush();
	virtual void STDCALL GetStats( SStorageElementStats *pStats );
	bool IsOpen() const { return hFile != INVALID_HANDLE_VALUE; }
};
class CFileSystem : public IDataStorage
{
	OBJECT_MINIMAL_METHODS( CFileSystem );
	std::string szBase;
	DWORD dwStorageAccessMode;
	bool CreatePathRecursive( const std::string &szName );
public:
	CFileSystem( const char *pszName, DWORD dwAccessMode, bool bCreate );
	virtual const bool STDCALL IsStreamExist( const char *pszName );
	virtual IDataStream* STDCALL CreateStream( const char *pszName, DWORD dwAccessMode );
	virtual IDataStream* STDCALL OpenStream( const char *pszName, DWORD dwAccessMode );
	virtual bool STDCALL GetStreamStats( const char *pszName, SStorageElementStats *pStats );
	virtual bool STDCALL DestroyElement( const char *pszName );
	virtual bool STDCALL RenameElement( const char *pszOldName, const char *pszNewName );
	virtual IStorageEnumerator* STDCALL CreateEnumerator();
	virtual const char* STDCALL GetName() const { return szBase.c_str(); }
	virtual bool STDCALL AddStorage( IDataStorage *pStorage, const char *pszName );
	virtual bool STDCALL RemoveStorage( const char *pszName );
};
#endif // __FILESYSTEM_H__
