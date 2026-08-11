#include "StdAfx.h"
#include "../StreamIO/ProfilePaths.h"
#include "../Platform/LegacyText.h"

#include "../Main/Transceiver.h"
#include "../Main/GameStats.h"
#include "../Main/AILogicCommand.h"
#include "../StreamIO/RandomGen.h"
#include "../Formats/fmtSaveLoad.h"
#include "../Misc/FileUtils.h"
#include "CommonId.h"
#include "CutScenesHelper.h"
#include "CutsceneList.h"
#include "UIConsts.h"

static const NInput::SRegisterCommandEntry commonCommands[] = 
{
	{ "cancel_load"	,	IMC_CANCEL					},
	{ "load_mission", IMC_OK							},
	{ "key_up",				MESSAGE_KEY_UP			},
	{ "key_down",			MESSAGE_KEY_DOWN		},
	{ "key_left",			MESSAGE_KEY_LEFT		},
	{ "key_right",		MESSAGE_KEY_RIGHT		},
	{ 0							,	0										}
};
void CCutsceneList::PostCreate( IMainLoop *pML, CInterfaceCutsceneList *pILM )
{
	pML->PushInterface( pILM );
}
namespace
{
// The profile's unlock list lives in config.cfg and can be lost. A save game
// is durable evidence the player already watched the campaign intro (it always
// plays when a campaign starts), so campaign cutscenes are additionally
// derived from the saves on disk.
class CCollectSaveHeaders
{
	std::vector<NSaveLoad::SFileHeader> &headers;
public:
	explicit CCollectSaveHeaders( std::vector<NSaveLoad::SFileHeader> &_headers ) : headers( _headers ) {  }
	bool operator()( const NFile::CFileIterator &it )
	{
		if ( it.IsDirectory() )
			return true;
		CPtr<IDataStream> pStream = OpenFileStream( it.GetFilePath().c_str(), STREAM_ACCESS_READ );
		if ( pStream == 0 )
			return true;
		CStreamAccessor stream = pStream;
		DWORD dwSignature = 0;
		stream >> dwSignature;
		if ( dwSignature != NSaveLoad::SFileHeader::SIGNATURE )
			return true;
		NSaveLoad::SFileHeader hdr;
		stream >> hdr;
		headers.push_back( hdr );
		return true;
	}
};
bool ContainsPathSegment( const std::string &szPath, const std::string &szSegment )
{
	const std::string szInner = "\\" + szSegment + "\\";
	if ( szPath.find( szInner ) != std::string::npos )
		return true;
	const std::string szLast = "\\" + szSegment;
	return szPath.size() >= szLast.size() && szPath.compare( szPath.size() - szLast.size(), szLast.size(), szLast ) == 0;
}
void AppendCampaignCutScenesFromSaves( std::list<std::string> &cutscenes )
{
	// same saves-dir resolution as the load dialog (LoadMission.cpp)
	std::string szBaseDir = std::string( GetSingleton<IDataStorage>()->GetName() );
	szBaseDir = szBaseDir.substr( 0, szBaseDir.rfind('\\') );
	szBaseDir = szBaseDir.substr( 0, szBaseDir.rfind('\\') );
	const std::string szModname = GetSingleton<IUserProfile>()->GetMOD();
	szBaseDir += "\\";
	szBaseDir += NProfile::Segment();
	if ( !szModname.empty() )
	{
		szBaseDir += "mods\\";
		szBaseDir += szModname;
	}
	szBaseDir += "saves\\";
	std::vector<NSaveLoad::SFileHeader> headers;
	NFile::EnumerateFiles( szBaseDir.c_str(), "*.sav", CCollectSaveHeaders(headers), true );
	if ( headers.empty() )
		return;
	for ( int nCampaign = 0; nCampaign < 3; ++nCampaign )
	{
		const std::string szPartyName = CUIConsts::GetPartyNameByNumber( nCampaign );
		const std::string szCampaignName = "scenarios\\campaigns\\" + szPartyName + "\\" + szPartyName;
		const SCampaignStats *pStats = NGDB::GetGameStats<SCampaignStats>( szCampaignName.c_str(), IObjectsDB::CAMPAIGN );
		if ( pStats == 0 || pStats->szIntroMovie.empty() )
			continue;
		if ( std::find(cutscenes.begin(), cutscenes.end(), pStats->szIntroMovie) != cutscenes.end() )
			continue;
		bool bSeen = false;
		for ( int h = 0; h < headers.size() && !bSeen; ++h )
		{
			std::string szChapter = headers[h].szChapterName;
			std::string szMission = headers[h].szMissionName;
			NStr::ToLower( szChapter );
			NStr::ToLower( szMission );
			// exact membership: the save's chapter is one of this campaign's chapters
			for ( int c = 0; c < pStats->chapters.size() && !bSeen; ++c )
			{
				std::string szCampaignChapter = pStats->chapters[c].szChapter;
				NStr::ToLower( szCampaignChapter );
				if ( !szCampaignChapter.empty() && szCampaignChapter == szChapter )
					bSeen = true;
			}
			// fallback: all campaign content lives under a "<party>" directory
			if ( !bSeen && ( ContainsPathSegment(szChapter, szPartyName) || ContainsPathSegment(szMission, szPartyName) ) )
				bSeen = true;
		}
		if ( bSeen )
			cutscenes.push_back( pStats->szIntroMovie );
	}
}
}
CInterfaceCutsceneList::~CInterfaceCutsceneList()
{
}
bool CInterfaceCutsceneList::Init()
{
	NStr::SetCodePage( GetACP() );
	CInterfaceScreenBase::Init();
	SetBindSection( "loadmission" );
	commandMsgs.Init( pInput, commonCommands );
	
	return true;
}
void CInterfaceCutsceneList::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\lists\\IMCutsceneList" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	IUIListControl *pList = checked_cast<IUIListControl*>( pUIScreen->GetChildByID( 1000 ) );
	NI_ASSERT( pList != 0 );
	
	ITextManager *pTextMan = GetSingleton<ITextManager>();
	std::list<std::string> cutscenes;
	NCutScenes::GetCutScenesList( cutscenes );
	AppendCampaignCutScenesFromSaves( cutscenes );
	int nSceneIndex = 0;
	for ( std::list<std::string>::const_iterator it = cutscenes.begin(); it != cutscenes.end(); ++it, ++nSceneIndex )
	{
		pList->AddItem();
		IUIListRow *pRow = pList->GetItem( nSceneIndex );
		pRow->SetUserData( nSceneIndex );
		
		IUIContainer *pContainer = checked_cast<IUIContainer*> ( pRow->GetElement( 0 ) );
		std::string szVideoName = *it;
		cutscenesList.push_back( szVideoName );

		CPtr<IText> pText = pTextMan->GetDialog( szVideoName.c_str() );
		if ( CPtr<IText> pText = pTextMan->GetDialog(szVideoName.c_str()) ) 
			pContainer->SetWindowText( 0, pText->GetString() );
		else
		{
			const int nPos = szVideoName.rfind('\\');
			if ( nPos != std::string::npos )
				szVideoName = szVideoName.substr( nPos + 1 );
			const std::wstring wszVideoName = NStr::ToUnicode( szVideoName );
			pContainer->SetWindowText( 0, NPlatform::WordStringData( NPlatform::WordStringFromWide( wszVideoName.c_str() ) ) );
		}
		
		IUIElement *pElement = pContainer->GetChildByID( 1 );
		NI_ASSERT_T( pElement != 0, "Invalid list control name dialog, it should contain icon" );
		pElement->SetState( 1 );			//����
	}
	
	int nSelItem = GetGlobalVar( "LastCutscene", -1 );
	if ( nSelItem >= 0 )
		pList->SetSelectionItem( nSelItem );

	pList->InitialUpdate();
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
}
bool CInterfaceCutsceneList::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			CloseInterface();
			RemoveGlobalVar( "LastCutscene" );
			return true;
			
		case IMC_OK:
			if ( IUIElement *pElement = pUIScreen->GetChildByID( 1000 ) ) 
			{
				IUIListControl *pList = checked_cast<IUIListControl*>( pElement );
				if ( !pList )
					return true;			// �� ������� list control
				int nSelItem = pList->GetSelectionItem();			// ������ � ������
				if ( nSelItem == -1 )
					return true;

				IUIListRow *pSelRow = pList->GetItem( nSelItem );
				int nSel = pSelRow->GetUserData();						// ������ � �������
				std::string szVideo = cutscenesList[ nSel ];
				szVideo += NStr::Format( ";%d;99", MISSION_COMMAND_MAIN_MENU );
				IMainLoop *pML = GetSingleton<IMainLoop>();
				CloseInterface();
				GetSingleton<ISFX>()->StopStream( GetGlobalVar( "Sound.TimeToFade", 5000 ) );

				pML->Command( MISSION_COMMAND_VIDEO, szVideo.c_str() );
				SetGlobalVar( "LastCutscene", nSelItem );
			}
			return true;
	}

	return false;
}
