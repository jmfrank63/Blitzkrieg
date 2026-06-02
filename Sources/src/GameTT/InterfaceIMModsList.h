#ifndef __INTERFACEIMMODSLIST_H__
#define __INTERFACEIMMODSLIST_H__
#pragma ONCE
#include "BaseList.h"
class CInterfaceIMModsList : public CInterfaceBaseList
{
	OBJECT_NORMAL_METHODS( CInterfaceIMModsList );
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceIMModsList();
protected:
	CInterfaceIMModsList() {}
	
	virtual bool FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem = 0 );		//заполняем текущую строчку в списке
	virtual bool OnOk( const std::string &szFullFileName );															//пользователь выбрал файл, обработаем выбор
	virtual void PrepareList( std::vector<std::string> *pFiles );
public:
	virtual bool STDCALL Init();
};
class CICIMModsList : public CInterfaceCommandBase<CInterfaceIMModsList, MISSION_INTERFACE_MODS_LIST>
{
	OBJECT_NORMAL_METHODS( CICIMModsList );
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceIMModsList *pI )
	{
		pML->PushInterface( pI );
	}
	CICIMModsList() {}
public:
};
#endif // __INTERFACEIMMODSLIST_H__
