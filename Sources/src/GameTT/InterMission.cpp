#include "StdAfx.h"

#include "InterMission.h"

#include "WorldClient.h"
#include "CommonID.h"
#include "MultiplayerCommandManager.h"
#include "../Main/ScenarioTracker.h"
#include "../UI/UIMessages.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_tutorial"	,	TUTORIAL_WINDOW_ID	},
	{ "inter_ok"				,	IMC_OK							},
	{ "inter_cancel"		, IMC_CANCEL					},
	{ 0									,	0										}
};
bool CInterfaceInterMission::Init()
{
	CInterfaceScreenBase::Init();
	while ( pScene->ToggleShow(SCENE_SHOW_HAZE) != false );
	intermissionMsgs.Init( pInput, commands );
	SetBindSection( "intermission" );
	return true;
}
bool CInterfaceInterMission::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
	case MC_SET_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
		return true;

	case MC_CANCEL_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
		return true;
	}
	
	return false;
}
bool CInterfaceInterMission::StepLocal( bool bAppActive )
{
	if ( !bAppActive )
		return false;
	const CVec2 vPos = pCursor->GetPos();
	CInterfaceScreenBase::OnCursorMove( vPos );
	if ( pUIScreen )		//в некоторых экранах pUIScreen нету
		pUIScreen->Update( pTimer->GetAbsTime() );
	return true;
}
void CInterfaceInterMission::DrawAdd()
{
}
bool CInterfaceMultiplayerScreen::Init()
{
	CInterfaceInterMission::Init();
	SetBindSection( "multiplayer_screens" );

	pCommandManager = GetSingleton<IMPToUICommandManager>();

	return true;
}
bool CInterfaceMultiplayerScreen::StepLocal( bool bAppActive )
{
	if ( pCommandManager )
	{
		SToUICommand cmd;
		if( pCommandManager->PeekCommandToUI( &cmd ) || pCommandManager->PeekChatMessageToUI() )
		{
			GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_CMD, NStr::Format("%d", WCC_MULTIPLAYER_TO_UI_UPDATE) );
		}
	}
	return CInterfaceInterMission::StepLocal( bAppActive );
}
