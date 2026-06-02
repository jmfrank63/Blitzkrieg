#ifndef __IM_UPGRADE_UNIT_H__
#define __IM_UPGRADE_UNIT_H__
#pragma ONCE
#include "InterMission.h"
#include "iMission.h"
class CInterfaceUpgradeUnit : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceUpgradeUnit );
	NInput::CCommandRegistrator commandMsgs;
	bool bToChapter;
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual ~CInterfaceUpgradeUnit();
	CInterfaceUpgradeUnit() : CInterfaceInterMission( "InterMission" ) {  }
	
	void DefaultUpgrades();
public:
	void SetToChapter( const bool _bToChapter ) { bToChapter = _bToChapter; }
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
	virtual void STDCALL OnGetFocus( bool bFocus );
};
class CICUpgradeUnit : public CInterfaceCommandBase<CInterfaceUpgradeUnit, MISSION_INTERFACE_UPGRADE_UNIT>
{
	OBJECT_NORMAL_METHODS( CICUpgradeUnit );
	bool bToChapter;											// if true, then to chapter, oterwise to 
	
	virtual void PreCreate( IMainLoop *pML ) 
	{ 
		if ( bToChapter ) 
			pML->ResetStack(); 
	}
	virtual void PostCreate( IMainLoop *pML, CInterfaceUpgradeUnit *pInterface ) 
	{ 
		pInterface->SetToChapter( bToChapter );
		pML->PushInterface( pInterface ); 
	}
	CICUpgradeUnit() : bToChapter( false ) {  }
public:
	virtual void STDCALL Configure( const char *pszConfig )
	{
		if ( pszConfig )
		{
			bToChapter = NStr::ToInt( pszConfig );
		}
	}

};
#endif		//__IM_UPGRADE_UNIT_H__
