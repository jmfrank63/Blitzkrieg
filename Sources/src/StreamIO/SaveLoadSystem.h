#ifndef __SAVELOADSYSTEM_H__
#define __SAVELOADSYSTEM_H__
#pragma ONCE
#include "..\Misc\BasicObjectFactory.h"
class CSaveLoadSystem : public ISaveLoadSystem
{
	CBasicObjectFactory *pFactory;
	IGDB *pGDB;
public:
	CSaveLoadSystem();
	virtual ~CSaveLoadSystem();
	virtual void STDCALL AddFactory( IObjectFactory *pFactory );
	virtual IObjectFactory* STDCALL GetCommonFactory() { return pFactory; }
	virtual void STDCALL SetGDB( IGDB *_pGDB ) { pGDB = _pGDB; }
	virtual IStructureSaver* STDCALL CreateStructureSaver( IDataStream *pStream, IStructureSaver::EAccessMode eAccessMode,
		                                                     interface IProgressHook *pLoadHook );
	virtual IDataTree* STDCALL CreateDataTreeSaver( IDataStream *pStream, IDataTree::EAccessMode eAccessMode, DTChunkID idBaseNode );
	virtual IDataStorage* STDCALL OpenStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE );
	virtual IDataStorage* STDCALL CreateStorage( const char *pszName, DWORD dwAccessMode, DWORD type = STORAGE_TYPE_FILE );
	virtual IDataBase* STDCALL OpenDataBase( const char *pszName, DWORD dwAccessMode, DWORD type = DB_TYPE_INI );
	virtual IDataTable* STDCALL OpenDataTable( IDataStream *pStream, const char *pszBaseNode = "base" );
	virtual IDataTable* STDCALL OpenIniDataTable( IDataStream *pStream );
};
#endif // __SAVELOADSYSTEM_H__
