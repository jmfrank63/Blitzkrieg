#ifndef __IM_TUTORIAL_LIST_H__
#define __IM_TUTORIAL_LIST_H__
#pragma ONCE
#include "CustomList.h"
class CInterfaceTutorialList : public CInterfaceCustomList
{
	OBJECT_NORMAL_METHODS( CInterfaceTutorialList );
	virtual ~CInterfaceTutorialList() {}
protected:
	CInterfaceTutorialList() {}
	
	virtual bool FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem = 0 );		//заполняем текущую строчку в списке
	virtual bool OnOk( const std::string &szFullFileName );															//пользователь выбрал файл, обработаем выбор
public:
	virtual bool STDCALL Init();
};
class CICTutorialList : public CInterfaceCommandBase<CInterfaceTutorialList, MISSION_INTERFACE_TUTORIAL_LIST>
{
	OBJECT_NORMAL_METHODS( CICTutorialList );
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceTutorialList *pInterface ) { pML->PushInterface( pInterface ); }
	CICTutorialList() {  }
};
#endif		//__IM_TUTORIAL_LIST_H__
