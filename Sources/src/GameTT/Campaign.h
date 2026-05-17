#ifndef __CAMPAIGN_H__
#define __CAMPAIGN_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "InterMission.h"
#include "iMission.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SCampaignStats;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CInterfaceCampaign : public CInterfaceInterMission
{
	OBJECT_NORMAL_METHODS( CInterfaceCampaign );
	DECLARE_SERIALIZE;
	//
	typedef std::unordered_map< std::string, int > CChapterNameToButtonIndex;
	CChapterNameToButtonIndex chapterNameToButtonIndexMap;
	// input
	NInput::CCommandRegistrator commandMsgs;
	//
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	// disable explicit destruction
	virtual ~CInterfaceCampaign();
	CInterfaceCampaign() : CInterfaceInterMission( "InterMission" ) {  }
	//
	void SetDescriptionText( const struct SChapterStats *pStats );
	void OnCancel();
public:
	static void PlayCampaignMusic();
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
	static const SCampaignStats *ReadCampaignStats();
	virtual void STDCALL OnGetFocus( bool bFocus );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CICCampaign : public CInterfaceCommandBase<IInterfaceBase, MISSION_INTERFACE_CAMPAIGN>
{
	OBJECT_NORMAL_METHODS( CICCampaign );

	virtual void PreCreate( IMainLoop *pML ) { pML->ResetStack(); }
	virtual void PostCreate( IMainLoop *pML, IInterfaceBase *pInterface ) { pML->PushInterface( pInterface ); }
	//
	CICCampaign() {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif		//__CAMPAIGN_H__
