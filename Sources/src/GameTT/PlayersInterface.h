#if !defined(AFX_PLAYERSINTERFACE_H__9CF28404_2337_40C5_AEFE_480D866078C6__INCLUDED_)
#define AFX_PLAYERSINTERFACE_H__9CF28404_2337_40C5_AEFE_480D866078C6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "InterMission.h"
#include "iMission.h"
class CAfterMissionPopups;
class CPlayersInterface : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CPlayersInterface );
	DECLARE_SERIALIZE;
	NInput::CCommandRegistrator commandMsgs;
	CPtr<CAfterMissionPopups> pPopups;
	
	bool bAfterMission;										// this screen is called after mission
	bool bDisableGetFocus;								// for disable first OnGetFocus
	
	CPlayersInterface() : CInterfaceInterMission( "InterMission" ) { }

	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	void SetValues( IUIListRow * pRow, const float fCurrentVal, const float fFormerVal, const int nMultiply );
	
public:
	void Create( const bool bAfterMission );
	virtual bool STDCALL Init();
	virtual void STDCALL OnGetFocus( bool bFocus );
};
class CICPlayersInterface : public CInterfaceCommandBase<CPlayersInterface, MISSION_INTERFACE_PLAYERS_STATS>
{
	OBJECT_NORMAL_METHODS( CICPlayersInterface );

	bool bAfterMission;

	virtual void PreCreate( IMainLoop *pML ) 
	{ 
		if ( GetGlobalVar("FinishingCampaign", 0) != 0 || bAfterMission ) 
			pML->ResetStack();
	}
	
	virtual void PostCreate( IMainLoop *pML, CPlayersInterface *pIMM )
	{ 
		pIMM->Create( bAfterMission );
		pML->PushInterface( pIMM ); 
	}

public:
	CICPlayersInterface () : bAfterMission( false ) {  }

	virtual void STDCALL Configure( const char *pszConfig ) 
	{  
		if ( pszConfig )
			bAfterMission = NStr::ToInt( pszConfig );
	}
};
#endif // !defined(AFX_PLAYERSINTERFACE_H__9CF28404_2337_40C5_AEFE_480D866078C6__INCLUDED_)
