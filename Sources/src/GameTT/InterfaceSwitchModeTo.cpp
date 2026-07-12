#include "StdAfx.h"

#include "InterfaceSwitchModeTo.h"
#include "CommonID.h"
#include "MultiplayerCommandManager.h"
enum EControls
{
	E_REPLAY_EDIT_BOX											= 2000,
	E_OK_BUTTON														= 10002,
	E_MESSAGE_TEXT_ID											= 20001,
	E_CAPTION_ID													= 20000,
};
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
void CInterfaceSwitchModeTo::SEnumDirs::operator()( const NFile::CFileIterator &fileIt ) const
{
	pModDirs->push_back( CModName( fileIt.GetFilePath(), fileIt.GetFileName() ) );
}
CInterfaceSwitchModeTo::~CInterfaceSwitchModeTo()
{
}
bool CInterfaceSwitchModeTo::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );
	
	return true;
}
bool CInterfaceSwitchModeTo::Create(	const std::string &szDesiredModName,
																			const std::string &szDesiredModVer,
																			const int nCommandID,
																			const std::string &_szComandParams,
																			const bool _bSilentSwitch )
{
	bSilentSwitch = _bSilentSwitch;
	nCommandOnOk = nCommandID;
	szCommandParams = _szComandParams ;
	ITextManager *pTM = GetSingleton<ITextManager>();
	IMainLoop *pML = GetSingleton<IMainLoop>();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );

	bModExists = false;
	bool bVersionMismatch = false;
	std::string szLocalMODVersion = "";
	std::string szLocalMODName = "";

	if ( szDesiredModName == "" )								// default game (no mod)
	{
		bModExists = true;
	}
	else
	{
		std::string szDesiredModDirectory;

		std::string szModsDirectory = GetSingleton<IMainLoop>()->GetBaseDir();
		szModsDirectory += "Mods\\";
		CModNames modDirs;
		SEnumDirs cb;
		cb.pModDirs = &modDirs;
		NFile::EnumerateFiles( szModsDirectory.c_str(), "*.*", cb, false );

		const std::string szMODPath = std::string( pML->GetBaseDir() ) + "mods\\" + szDesiredModDirectory;

		for ( CModNames::iterator filesIter = modDirs.begin(); 
					filesIter != modDirs.end() && !bModExists;  
					++filesIter )
		{
			if ( CPtr<IDataStorage> pMOD = OpenStorage((filesIter->first + "\\data\\*.pak").c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON) )
			{
				if ( CPtr<IDataStream> pStream = pMOD->OpenStream("mod.xml", STREAM_ACCESS_READ) )
				{
					{
						CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
						saver.Add( "MODName", &szLocalMODName );
						saver.Add( "MODVersion", &szLocalMODVersion );
						
						if ( szLocalMODName == szDesiredModName )
						{
							if ( szLocalMODVersion == szDesiredModVer )
							{
								bModExists = true;
								szDirName = filesIter->second; 
							}
							else
								bVersionMismatch = true;
						}
					}
				}
			}
		}
	}

	if ( bModExists && bSilentSwitch )
	{
		return true;
	}

	if ( bModExists && 
			 szDesiredModName == GetGlobalVar( "MOD.Name", "" ) &&
			 szDesiredModVer == GetGlobalVar( "MOD.Version", "" ) )
	{
		return true;
	}

	IText * pText = 0;
	IText * pCaptionText = 0;
	if ( !bModExists )				// don't have local mod
	{
		pUIScreen->Load( "ui\\Popup\\DontHaveMod" );
		if ( bVersionMismatch ) // have mod, but version is incorrect
		{
			pText = pTM->GetDialog( "Textes\\UI\\Intermission\\MainMenu\\Mods\\message_mod_version_mismatch" );
			pCaptionText = pTM->GetDialog( "Textes\\UI\\Intermission\\MainMenu\\Mods\\caption_mod_version_mismatch" );
		}
		else
		{
			pText = pTM->GetDialog( "Textes\\UI\\Intermission\\MainMenu\\Mods\\message_mod_not_exists" );
			pCaptionText = pTM->GetDialog( "Textes\\UI\\Intermission\\MainMenu\\Mods\\caption_mod_not_exists" );
		}
	}
	else
	{
		pUIScreen->Load( "ui\\Popup\\SwitchModTo" );
		pText = pTM->GetDialog( "Textes\\UI\\Intermission\\MainMenu\\Mods\\message_would_you_like_to_switch" );
		pCaptionText = pTM->GetDialog( "Textes\\UI\\Intermission\\MainMenu\\Mods\\caption_would_you_like_to_switch" );
	}

	{
	IUIElement *pEl = pUIScreen->GetChildByID( E_MESSAGE_TEXT_ID );
	if ( pText )
		pEl->SetWindowText( 0, pText->GetString() );
	}
	{
	IUIElement *pEl = pUIScreen->GetChildByID( E_CAPTION_ID );
	if ( pCaptionText )
		pEl->SetWindowText( 0, pCaptionText->GetString() );
	}

	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
	return false;
}
void CInterfaceSwitchModeTo::OnOk()
{
	IMainLoop * pML = GetSingleton<IMainLoop>();
	pML->Command( MAIN_COMMAND_CHANGE_MOD, szDirName.c_str() );
	pML->Command( nCommandOnOk, szCommandParams.c_str() );
	SFromUINotification notification( EUTMN_SWITCH_MOD_OK, 0 );
	GetSingleton<IMPToUICommandManager>()->AddNotificationFromUI( notification );
}
bool CInterfaceSwitchModeTo::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_OK:
			if ( bModExists ) 
			{
				OnOk();
				return true;
			}
			
		case IMC_CANCEL:
			if ( bSilentSwitch )
				GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_EXIT_GAME, 0 );
			else
			{
				CloseInterface();
				SFromUINotification notification( EUTMN_CANCEL_CONNECT_TO_SERVER, 0 );
				GetSingleton<IMPToUICommandManager>()->AddNotificationFromUI( notification );
			}
			return true;
			
	}
	
	return false;
}
