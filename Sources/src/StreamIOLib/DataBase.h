#ifndef __DATABASE_H__
#define __DATABASE_H__
#pragma ONCE
class CIniFileDataBase : public IDataBase
{
	OBJECT_MINIMAL_METHODS( CIniFileDataBase );
	std::string szBase;
	DWORD dwStorageAccessMode;
public:
	CIniFileDataBase( const char *pszName, DWORD dwAccessMode );
	virtual IDataTable* STDCALL CreateTable( const char *pszName, DWORD dwAccessMode );
	virtual IDataTable* STDCALL OpenTable( const char *pszName, DWORD dwAccessMode );
	virtual bool STDCALL DestroyElement( const char *pszName );
	virtual bool STDCALL RenameElement( const char *pszOldName, const char *pszNewName );
};
#endif // __DATABASE_H__
