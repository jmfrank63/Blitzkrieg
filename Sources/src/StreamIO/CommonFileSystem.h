#ifndef __COMMONFILESYSTEM_H__
#define __COMMONFILESYSTEM_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCommonFileInfo
{
	DWORD dwModTime;										// file modification time
	IDataStorage *pStorage;							// storage, this file stored in
	//
	SCommonFileInfo() : dwModTime( 0 ), pStorage( 0 ) {  }
	SCommonFileInfo( DWORD _dwModTime, IDataStorage *_pStorage ) : dwModTime( _dwModTime ), pStorage( _pStorage ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCommonFileSystemEnumerator : public IStorageEnumerator
{
	OBJECT_MINIMAL_METHODS( CCommonFileSystemEnumerator );
	std::string szMask;										// enumeration mask
	std::string szFileName;								// current enumerated file name
	SStorageElementStats stats;						// temporary data storage to fill each call
	//
	typedef std::unordered_map<std::string, SCommonFileInfo> CFilesMap;
	const CFilesMap &files;
	CFilesMap::const_iterator itCurrFile;
	bool bReset;
public:
	CCommonFileSystemEnumerator( const CFilesMap &_files ) : files( _files ), bReset( true ), itCurrFile( files.begin() ) {  }
	//
	virtual void STDCALL Reset( const char *pszName );
	virtual bool STDCALL Next();
	virtual const SStorageElementStats* STDCALL GetStats() const { return &stats; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCommonFileSystem : public IDataStorage
{
	OBJECT_MINIMAL_METHODS( CCommonFileSystem );
	//
	CPtr<IDataStorage> pZipStorage;				// zip file system
	CPtr<IDataStorage> pFileStorage;			// open file system
	typedef std::unordered_map<std::string, SCommonFileInfo> CFilesMap;
	CFilesMap files;											// ������������ ����� ����� � ����������, ����������� ��� ��� ��������
	//
	std::string szBase;
	DWORD dwStorageAccessMode;
	//
	void EnumerateFiles( const std::string &szName, IDataStorage *pStorage );
public:
	CCommonFileSystem( const char *pszName, DWORD dwAccessMode );
	// ���������, ���� �� ����� �����
	virtual const bool STDCALL IsStreamExist( const char *pszName );
	// ������� � ������� ����� � ��������� ������ � ������� �������
	virtual IDataStream* STDCALL CreateStream( const char *pszName, DWORD dwAccessMode );
	// ������� ������������ ����� � ��������� ������ � ������� �������
	virtual IDataStream* STDCALL OpenStream( const char *pszName, DWORD dwAccessMode );
	// �������� �������� stream'�
	virtual bool STDCALL GetStreamStats( const char *pszName, SStorageElementStats *pStats );
	// ����� ������� ���������
	virtual bool STDCALL DestroyElement( const char *pszName );
	// ������������� �������
	virtual bool STDCALL RenameElement( const char *pszOldName, const char *pszNewName );
	// ������������ ���������
	virtual IStorageEnumerator* STDCALL CreateEnumerator();
	// �������� ��� ����� storage
	virtual const char* STDCALL GetName() const { return szBase.c_str(); }
	// �������� ����� MOD
	virtual bool STDCALL AddStorage( IDataStorage *pStorage, const char *pszName );
	// ������ MOD
	virtual bool STDCALL RemoveStorage( const char *pszName );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __COMMONFILESYSTEM_H__
