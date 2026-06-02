#ifndef __IM_REPLAY_LIST_H__
#define __IM_REPLAY_LIST_H__
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "BaseList.h"
enum EReplayError
{
	ERR_NO_ERROR													= 0,
	ERR_BAD_RESOURCES											= 1,
	ERR_BAD_MAP														= 2,
};
class CInterfaceReplayList : public CInterfaceBaseList
{
	OBJECT_NORMAL_METHODS( CInterfaceReplayList );

	bool bInstantLoad;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceReplayList();
	void DisplayError();

protected:
	CInterfaceReplayList() : bInstantLoad( false ) { }
	
	virtual bool FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem = 0 );		//заполняем текущую строчку в списке
	virtual bool OnOk( const std::string &szFullFileName );															//пользователь выбрал файл, обработаем выбор
public:

	virtual bool STDCALL Init();
	void SetFileName( const char *pszFileName );
	virtual void STDCALL StartInterface();
};
class CReplayList : public CInterfaceCommandBase<CInterfaceReplayList, MISSION_INTERFACE_REPLAY_LIST>
{
	OBJECT_NORMAL_METHODS( CReplayList );

	std::string szFileNameToLoad;
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceReplayList *pILM );
	CReplayList() {}
public:	
	virtual void STDCALL Configure( const char *pszConfig )
	{
		if ( pszConfig )
			szFileNameToLoad = pszConfig;
	}
};
#endif // __IM_REPLAY_LIST_H__
