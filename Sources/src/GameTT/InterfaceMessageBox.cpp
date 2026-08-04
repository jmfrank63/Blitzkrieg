#include "StdAfx.h"

#include "InterfaceMessageBox.h"
#include "CommonId.h"
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ 0									,	0							}
};
enum
{
	E_CAPTION															= 20000,
	E_MESSAGE															= 20001,
	E_DIALOG_OK														= 1001,
	E_DIALOG_OK_CANCEL										= 1002,
};
int CInterfaceMessageBox::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &szGlobalVarOnOk );
	saver.Add( 2, &bDoubleButton );
	saver.AddTypedSuper( 3, static_cast<CInterfaceInterMission*>( this ) );
	return 0;
}
CInterfaceMessageBox::CInterfaceMessageBox() : CInterfaceInterMission( "Current" )
{
}
CInterfaceMessageBox::~CInterfaceMessageBox()
{
}
bool CInterfaceMessageBox::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );
	
	return true;
}
void CInterfaceMessageBox::Create( const std::string &szCaptionKey, const std::string &szMessageKey, const bool _bDoubleButton, const std::string &_szGlobalVarOnOk )
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	szGlobalVarOnOk = _szGlobalVarOnOk;
	if ( !szGlobalVarOnOk.empty() )
		RemoveGlobalVar( szGlobalVarOnOk.c_str() );
	bDoubleButton = _bDoubleButton;									// ok == ok if true. if false ok = cancel and ok is single button
	
	const bool bInMission = GetGlobalVar( "AreWeInMission", 0 );

	if ( bInMission )
		pUIScreen->Load( "ui\\Popup\\MessageBoxMission" );
	else
		pUIScreen->Load( "ui\\Popup\\MessageBox" );

	pUIScreen->Reposition( pGFX->GetScreenRect() );

	ITextManager *pTM = GetSingleton<ITextManager>();

	IText *pCaptionText = pTM->GetDialog( szCaptionKey.c_str() );
	IUIElement *pCaption = pUIScreen->GetChildByID( E_CAPTION );
	if ( pCaption && pCaptionText )
		pCaption->SetWindowText( 0, pCaptionText->GetString() );

	IText *pMessageText = pTM->GetDialog( szMessageKey.c_str() );
	IUIElement *pMessage = pUIScreen->GetChildByID( E_MESSAGE );
	if ( pMessage && pMessageText )
		pMessage->SetWindowText( 0, pMessageText->GetString() );

	if ( !bDoubleButton )
	{
		IUIElement *pB = pUIScreen->GetChildByID( IMC_CANCEL );
		pB->ShowWindow( UI_SW_HIDE );
		pB->SetWindowID( 10004 );

		pB = pUIScreen->GetChildByID( IMC_OK );
		pB->ShowWindow( UI_SW_HIDE );

		pB = pUIScreen->GetChildByID( 10003 );
		pB->ShowWindow( UI_SW_SHOW );
		pB->SetWindowID( IMC_CANCEL );
	}

	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceMessageBox::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_OK:
			if ( bDoubleButton && !szGlobalVarOnOk.empty() )
				SetGlobalVar( szGlobalVarOnOk.c_str(), 1 );

		case IMC_CANCEL:
			CloseInterface();
			return true;
	}
	return false;
}
