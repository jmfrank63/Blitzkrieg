#include "StdAfx.h"

#include "SwitchToNextChapter.h"
#include "CommonId.h"
#include "..\Main\GameStats.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
static const NInput::SRegisterCommandEntry commands1[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ "inter_cancel"		, IMC_OK				},
	{ 0									,	0							}
};
CInterfaceNextChapter::CInterfaceNextChapter() : CInterfaceInterMission( "InterMission" )
{
}
CInterfaceNextChapter::~CInterfaceNextChapter()
{
}
bool CInterfaceNextChapter::Init()
{
	CInterfaceInterMission::Init();
	return true;
}
void CInterfaceNextChapter::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	
	
	std::string szChapterName = GetGlobalVar( "Chapter.Current.Name" );
	const SChapterStats *pStats = NGDB::GetGameStats<SChapterStats>( szChapterName.c_str(), IObjectsDB::CHAPTER );

	bool bTemplateExists = false;
	for ( int i = 0; i < pStats->missions.size(); ++i )
	{
		if ( pStats->missions[i].pMission->IsTemplate() )
		{
			bTemplateExists = true;
			break;
		}
	}

	bAllowStay = bTemplateExists;
	if ( bTemplateExists ) // allow stay in current chapter.
	{
		commandMsgs.Init( pInput, commands );
		pUIScreen->Load( "ui\\Popup\\NextChapter" );
	}
	else
	{
		commandMsgs.Init( pInput, commands1 );
		pUIScreen->Load( "ui\\Popup\\NextChapterNoStay" );
	}
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceNextChapter::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			if ( bAllowStay )
			{
				CloseInterface();
				return true;
			}
		case IMC_OK:
		{
			std::string szChapterName = GetGlobalVar( "Chapter.Current.Name", "" );
			NI_ASSERT_T( szChapterName.size() > 0, "There is no global var Chapter.Current.Name" );
			std::string szVarName = "Chapter." + szChapterName + ".Status";
			SetGlobalVar( szVarName.c_str(), 2 );

			szVarName = "Chapter.New.Available";
			std::string szNewChapter = GetGlobalVar( szVarName.c_str(), "" );
			NI_ASSERT_T( szNewChapter.size() > 0, "New chapter name is empty" );
			SetGlobalVar( "Chapter.Current.Name", szNewChapter.c_str() );
			RemoveGlobalVar( szVarName.c_str() );
			SetGlobalVar( "NextChapter.Confirmed", 1 );
			RemoveGlobalVar( "Mission.Last.FinishStatus" );

			CloseInterface();
			return true;
		}
	}
	
	return false;
}
