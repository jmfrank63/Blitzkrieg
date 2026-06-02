#ifndef __IM_LOAD_MISSION_H__
#define __IM_LOAD_MISSION_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "BaseList.h"
class CInterfaceIMLoadMission : public CInterfaceBaseList
{
	OBJECT_NORMAL_METHODS( CInterfaceIMLoadMission );
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceIMLoadMission();
protected:
	CInterfaceIMLoadMission() {}
	
	virtual bool FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem = 0 );		//заполняем текущую строчку в списке
	virtual bool OnOk( const std::string &szFullFileName );															//пользователь выбрал файл, обработаем выбор
public:
	virtual bool STDCALL Init();
};
class CICIMLoadMission : public CInterfaceCommandBase<CInterfaceIMLoadMission, MISSION_INTERFACE_IM_LOAD_MISSION>
{
	OBJECT_NORMAL_METHODS( CICIMLoadMission );
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceIMLoadMission *pILM );
	CICIMLoadMission() {}
public:
};
#endif // __IM_LOAD_MISSION_H__
