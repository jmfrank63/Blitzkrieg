#include "StdAfx.h"

#include "IMTutorial.h"
#include "iMission.h"
#include "CommonId.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ "inter_cancel"		, IMC_OK				},
	{ 0									,	0							}
};

CInterfaceIMTutorial::CInterfaceIMTutorial() : CInterfaceInterMission( "Current" )
{
}
CInterfaceIMTutorial::~CInterfaceIMTutorial()
{
}
bool CInterfaceIMTutorial::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );
	
	return true;
}
void CInterfaceIMTutorial::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\Popup\\IMTutorial" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	const WORD *pText = GetGlobalWVar( "TutorialText", (const WORD *) 0 );
	NI_ASSERT_T( pText != 0, "Invalid tutorial text" );
	if ( !pText )
		return;

	IUIDialog *pDialog = checked_cast<IUIDialog*> ( pUIScreen->GetChildByID( 100 ) );
	IUIElement *pElement = pDialog->GetChildByID( 3000 );
	pElement->SetWindowText( 0, pText );
	RemoveGlobalVar( "TutorialText" );

	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceIMTutorial::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_OK:
		case IMC_CANCEL:
			CloseInterface();
			return true;
	}
	
	return false;
}
