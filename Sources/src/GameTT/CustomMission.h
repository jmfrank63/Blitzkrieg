#ifndef __IM_CUSTOM_MISSION_H__
#define __IM_CUSTOM_MISSION_H__
#pragma ONCE
#include "CustomList.h"
class CInterfaceCustomMission : public CInterfaceCustomList
{
	OBJECT_NORMAL_METHODS( CInterfaceCustomMission );
	virtual ~CInterfaceCustomMission();
protected:
	CInterfaceCustomMission() {}
	
	virtual bool FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem = 0 );		//заполняем текущую строчку в списке
	virtual bool OnOk( const std::string &szFullFileName );															//пользователь выбрал файл, обработаем выбор
public:
	virtual bool STDCALL Init();
};
class CICCustomMission : public CInterfaceCommandBase<CInterfaceCustomMission, MISSION_INTERFACE_CUSTOM_MISSION>
{
	OBJECT_NORMAL_METHODS( CICCustomMission );
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceCustomMission *pInterface ) { pML->PushInterface( pInterface ); }
	CICCustomMission() {  }
};
#endif		//__IM_CUSTOM_MISSION_H__
