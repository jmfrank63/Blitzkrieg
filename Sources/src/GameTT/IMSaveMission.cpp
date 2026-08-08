#include "StdAfx.h"
#include "../Platform/LegacyText.h"

#include "IMSaveMission.h"
#include "CommonId.h"
#include "IMLoadMission.h"
#include "MultiplayerCommandManager.h"
#include "../Main/ScenarioTracker.h"
void CICIMSaveMission::PostCreate( IMainLoop *pML, CInterfaceIMSaveMission *pILM )
{
	pML->PushInterface( pILM );
}
CInterfaceIMSaveMission::~CInterfaceIMSaveMission()
{
}
void CInterfaceIMSaveMission::OnGetFocus( bool bFocus )
{
	CInterfaceBaseList::OnGetFocus( bFocus );
	if ( bFocus && GetGlobalVar( "SaveGame.OkOverrite", 0 ) )
	{
		RemoveGlobalVar( "SaveGame.OkOverrite" );
		Save();
	}
}
bool CInterfaceIMSaveMission::Init()
{
	fileMasks.clear();
	fileMasks.push_back( "*.sav" );
	szTopDir = GetSingleton<IMainLoop>()->GetBaseDir();
	
	const std::string szModname = GetSingleton<IUserProfile>()->GetMOD();
	if ( !szModname.empty() )
	{
		szTopDir += "mods\\";
		szTopDir += szModname;
	}
	szTopDir += "saves\\";
	
	szCurrentDir = szTopDir;
	szInterfaceName = "ui\\Lists\\IMSaveMission";
	nSortType = E_SORT_BY_TIME;
	nFirstSortColumn = 1;
	bNotDiveIntoSubdirs = true;
	CInterfaceBaseList::Init();
	bClosed = false;
	return true;
}
void CInterfaceIMSaveMission::StartInterface()
{
	CInterfaceBaseList::StartInterface();
	OnSelectionChanged();
	pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
	IUIEditBox *pEdit = checked_cast<IUIEditBox *>( pUIScreen->GetChildByID( 2000 ) );		//should be EditBox
	pEdit->SetFocus( true );
	pEdit->SetCursor( -1 );
	pEdit->SetSel( 0, -1 );
}
bool CInterfaceIMSaveMission::FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem )
{
	IUIElement *pElement = pRow->GetElement( 1 );
	if ( !pElement )
		return false;

	std::string szShortName = szFullFileName.substr( szTopDir.size() );
	const int nExtensionPos = szShortName.find( ".sav" );
	if ( nExtensionPos != std::string::npos )
		szShortName.resize( Max(0, int(szShortName.size()) - 4 ) );
	szSaves[pRow->GetUserData()] = szShortName;

	const std::string szVal = GetFileChangeTimeString( szFullFileName.c_str() );
	const std::wstring wszVal = NStr::ToUnicode( szVal );
	pElement->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszVal.c_str() ) ) );
	return true;
}
bool CInterfaceIMSaveMission::OnOk()
{
	if ( bClosed ) return false;
	
	IUIEditBox *pEdit = checked_cast<IUIEditBox *>( pUIScreen->GetChildByID( 2000 ) );		//should be EditBox
	NI_ASSERT_T( pEdit != 0, "Can't find editbox control with ID 2000" );
	const std::wstring wszEdit = MakeWideStringFromWordString( pEdit->GetWindowText( pEdit->GetState() ) );
	std::string szEdit = NStr::ToAscii( wszEdit );
	NStr::TrimBoth( szEdit );
	if ( szEdit.size() == 0 )
	{
		IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		//should be List Control
		IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
		if ( !pList )
			return true;			//не нашелся list control
		int nSave = pList->GetSelectionItem();
		if ( nSave == -1 )
			return true;

		szEdit = szSaves[nSave];
	}
	else
	{
		szEdit += ".sav";
	}

	szProspectiveFileName = szEdit;
	std::string szFileName = GetSingleton<IMainLoop>()->GetBaseDir();
	szFileName += "saves\\";
	szFileName += szEdit;
				
	if ( NFile::IsFileExist( szFileName.c_str() ) )
	{
		GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX, 
															NStr::Format( "%s;%s;1;SaveGame.OkOverrite", "Textes\\UI\\MessageBox\\overwrite_save_caption_im",
														 "Textes\\UI\\MessageBox\\overwrite_save_message_im" ) );
		return true;
	}
	else
		Save();
	return true;
}
void CInterfaceIMSaveMission::Save()
{
	IMainLoop *pML = GetSingleton<IMainLoop>();
	CloseInterface();
	pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
	pML->Command( MAIN_COMMAND_SAVE, szProspectiveFileName.c_str() );
	bClosed = true;
}
bool CInterfaceIMSaveMission::OnOk( const std::string &szFullFileName )
{
	return OnOk();
}
void CInterfaceIMSaveMission::OnSelectionChanged()
{
	IUIElement *pElement = pUIScreen->GetChildByID( 1000 );		//should be List Control
	IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
	if ( !pList )
		return ;			//не нашелся list control

	int nSave = pList->GetSelectionItem();
	if ( nSave == -1 )
		return ;
	
	IUIListRow *pSelRow = pList->GetItem( nSave );
	std::string szEdit = szSaves[pSelRow->GetUserData()];
	pElement = pUIScreen->GetChildByID( 2000 );
	const std::wstring wszEdit = NStr::ToUnicode( szEdit );
	pElement->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszEdit.c_str() ) ) );
	
	IUIEditBox *pEdit = checked_cast<IUIEditBox *>( pUIScreen->GetChildByID( 2000 ) );		//should be EditBox
	pEdit->SetCursor( -1 );
	pEdit->SetSel( 0, -1 );
	pEdit->ShowWindow( UI_SW_SHOW );
	pEdit->SetFocus( true );
}
bool CInterfaceIMSaveMission::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceBaseList::ProcessMessage( msg ) )
		return true;

	switch ( msg.nEventID )
	{
	case MC_SET_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );

		break;
	case MC_CANCEL_TEXT_MODE:
		pInput->SetTextMode( INPUT_TEXT_MODE_NOTEXT );

		break;

	case IMC_SELECTION_CHANGED:
		OnSelectionChanged();
		return true;

	case 7777:
		OnOk();
		return true;

	case 7778:
	case IMC_CANCEL:
		{
			IMainLoop *pML = GetSingleton<IMainLoop>();
			CloseInterface();
			return true;
		}
		break;
	}

	return false;
}

