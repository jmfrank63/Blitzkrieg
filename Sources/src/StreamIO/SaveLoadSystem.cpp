#include "StdAfx.h"

#include "SaveLoadSystem.h"

#include "StructureSaver2.h"
#include "DataTreeXML.h"

#include "FileSystem.h"
#include "ZipFileSystem.h"
#include "MemFileSystem.h"
#include "CommonFileSystem.h"
#include "ModFileSystem.h"

#include "DataTableXML.h"
#include "DataBase.h"
#include "IniFile.h"
CSaveLoadSystem theSaveLoadSystem;
ISaveLoadSystem* STDCALL GetSLS_Hook()
{
	return &theSaveLoadSystem;
}
CSaveLoadSystem::CSaveLoadSystem()
	: pFactory( 0 )
	, pGDB( 0 )
{
}
CSaveLoadSystem::~CSaveLoadSystem()
{
	if ( pFactory )
		delete pFactory;
}
void CSaveLoadSystem::AddFactory( IObjectFactory *_pFactory )
{
	if ( pFactory == 0 )	
		pFactory = new CBasicObjectFactory();
	NI_ASSERT_SLOW_TF( pFactory != 0, "basic save-load factory was not created", return );
	pFactory->Aggregate( _pFactory );
}
IStructureSaver* CSaveLoadSystem::CreateStructureSaver( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode, 
		interface IProgressHook *pLoadHook )
{
	NI_ASSERT_TF( pStream != 0, "Can't create structure saver from NULL stream", return 0 );
	return new CStructureSaver2( pStream, eAccessMode, pLoadHook, pFactory, pGDB );
}
IDataTree* CSaveLoadSystem::CreateDataTreeSaver( IDataStream *pStream, IDataTree::EAccessMode eAccessMode, DTChunkID idBaseNode )
{
	NI_ASSERT_TF( pStream != 0, "Can't create data tree saver from NULL stream", return 0 );
	InitCOM();
	CDataTreeXML *pDT = new CDataTreeXML( eAccessMode );
	pDT->Open( pStream, idBaseNode );
	return pDT;
}
IDataStorage* CSaveLoadSystem::OpenStorage( const char *pszName, DWORD dwAccessMode, DWORD type )
{
	switch ( type )
	{
		case STORAGE_TYPE_MOD:
			return new CModFileSystem( pszName, dwAccessMode );
		case STORAGE_TYPE_COMMON:
			return new CCommonFileSystem( pszName, dwAccessMode );
		case STORAGE_TYPE_FILE:
			return new CFileSystem( pszName, dwAccessMode, false );
		case STORAGE_TYPE_ZIP:
			return new CZipFileSystem( pszName, dwAccessMode );
		case STORAGE_TYPE_MEM:
			return new CMemFileSystem( dwAccessMode );
	}
	return 0;
}
IDataStorage* CSaveLoadSystem::CreateStorage( const char *pszName, DWORD dwAccessMode, DWORD type )
{
	switch ( type )
	{
		case STORAGE_TYPE_FILE:
			return new CFileSystem( pszName, dwAccessMode, true );
	}
	return 0;
}
IDataTable* CSaveLoadSystem::OpenDataTable( IDataStream *pStream, const char *pszBaseNode )
{
	InitCOM();
	CDataTableXML *pTable = new CDataTableXML();
	pTable->Open( pStream, pszBaseNode );
	return pTable;
}
IDataBase* CSaveLoadSystem::OpenDataBase( const char *pszName, DWORD dwAccessMode, DWORD type )
{
	NI_ASSERT_TF( type == DB_TYPE_INI, "Can support only .ini files now", return 0 );
	CIniFileDataBase *pDB = new CIniFileDataBase( pszName, dwAccessMode );
	return pDB;
}
IDataTable* CSaveLoadSystem::OpenIniDataTable( IDataStream *pStream )
{
	CIniFile *pFile = new CIniFile();
	if ( pFile->Load(pStream) == false )
	{
		pFile->AddRef();
		pFile->Release();
		return 0;
	}
	return pFile;
}
