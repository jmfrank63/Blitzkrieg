#ifndef __IM_CUSTOM_CAMPAIGN_H__
#define __IM_CUSTOM_CAMPAIGN_H__
#pragma ONCE
#include "CustomList.h"
class CInterfaceCustomCampaign : public CInterfaceCustomList
{
	OBJECT_NORMAL_METHODS( CInterfaceCustomCampaign );
	virtual ~CInterfaceCustomCampaign();
protected:
	CInterfaceCustomCampaign() {}
	
	virtual bool FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem = 0 );		//заполняем текущую строчку в списке
	virtual bool OnOk( const std::string &szFullFileName );															//пользователь выбрал файл, обработаем выбор
public:
	virtual bool STDCALL Init();
};
class CICCustomCampaign : public CInterfaceCommandBase<CInterfaceCustomCampaign, MISSION_INTERFACE_CUSTOM_CAMPAIGN>
{
	OBJECT_NORMAL_METHODS( CICCustomCampaign );
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceCustomCampaign *pInterface ) { pML->PushInterface( pInterface ); }
	CICCustomCampaign() {  }
};
#endif		//__IM_CUSTOM_CAMPAIGN_H__
