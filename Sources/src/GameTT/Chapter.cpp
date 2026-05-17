#include "StdAfx.h"

#include <stdlib.h>
#include <mmsystem.h>

#include "Chapter.h"

#include "..\Misc\Checker.h"
#include "..\Main\gamestats.h"
#include "..\Main\ScenarioTracker.h"
#include "..\RandomMapGen\mapinfo_types.h"
#include "..\RandomMapGen\resource_types.h"
#include "Campaign.h"
#include "CommonId.h"
#include "UIConsts.h"
#include "UnitTypes.h"
#include "etypes.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECommands
{
	MC_UNITS				= 10003,
	MC_PLAYER				= 10004,
	MC_SAVE_GAME		= 10005,
	MC_WAREHOUSE		= 10006,

	E_START_WINDOW_ID	= 20000,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceChapter::~CInterfaceChapter()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CInterfaceChapter::operator &( interface IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CInterfaceInterMission*>(this) );
	
	saver.Add( 2, &missionIndeces );
	saver.Add( 3, &nSelected );
	if ( saver.IsReading() )
	{
		IncrementChapterVisited();
		SetGlobalVar( "Mission.Last.FinishStatus", MISSION_FINISH_UNKNOWN );
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SChapterStats *CInterfaceChapter::ReadChapterStats()
{
	std::string szChapterName = GetGlobalVar( "Chapter.Current.Name" );
	//��������� ���������� � �������
	const SChapterStats *pStats = NGDB::GetGameStats<SChapterStats>( szChapterName.c_str(), IObjectsDB::CHAPTER );
	return pStats;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SRandomMission
{
	std::string szRandomName;
	int nPlayCount;

	SRandomMission() : nPlayCount( 0 ) {}

	bool operator < ( const SRandomMission &b ) { return nPlayCount < b.nPlayCount; }
	bool operator < ( const SRandomMission &b ) const { return nPlayCount > b.nPlayCount; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STemplateMission
{
	std::string szName;
	int nProbability;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceChapter::IncrementChapterVisited()
{
	int nFinishStatus = GetGlobalVar( "Mission.Last.FinishStatus", -1 );
//	NI_ASSERT_T( nFinishStatus != -1, "Global var Mission.Last.FinishStatus does not setted up, error" );
	//���� ���������� 0, �� ������ ������ ��������
	//���� -1, �� ���������� ��� �� ���� �� ���������������, ������ ������ ������ ������ � �� ������ �� ����� ������
	if ( nFinishStatus == ( -1 ) )
	{
		if ( CPtr<IScenarioTracker> pScenarioTracker = GetSingleton<IScenarioTracker>() )
		{
			pScenarioTracker->ClearRandomBonuses( 0 );
			pScenarioTracker->ClearRandomBonuses( 1 );
			pScenarioTracker->ClearRandomBonuses( 2 );
		}
	}

	if ( nFinishStatus != 0 && nFinishStatus != -1 )
	{
		return;			//������ �� ���� ��������, ������ ���������������� �� ����
	}

	//�������� ������ templates ��������, ������� �������� ��� settings ������� �������
	const SCampaignStats *pCampaignStats = CInterfaceCampaign::ReadCampaignStats();
	if ( pCampaignStats == 0 ) 
	{
		return;
	}
	//
	std::string szChapterName = GetGlobalVar( "Chapter.Current.Name" );
	NStr::ToLower( szChapterName );
	SChapterStats *pChapterStats = const_cast<SChapterStats *> ( NGDB::GetGameStats<SChapterStats>( szChapterName.c_str(), IObjectsDB::CHAPTER ) );
	if ( pChapterStats == 0 ) 
	{
		return;
	}
	//�������� ��� ��������� ������ � �������, ��� ����� �������� �� ���������� ������ ���� � ������.
	pChapterStats->RemoveTemplateMissions();

	/**
	int nMinimumUsedMission
	if ( IUserProfile *pUserProfile = GetSingleton<IUserProfile>() )
	{
		for ( int nMissionIndex = 0; nMissionIndex < pCampaignStats->templateMissions.size(); ++nMissionIndex )
		{
			const SMissionStats *pMissionStats = NGDB::GetGameStats<SMissionStats>( pCampaignStats->templateMissions[nMissionIndex].c_str(), IObjectsDB::MISSION );
			if ( pMissionStats == 0 )
			{
				NI_ASSERT_T( pMissionStats != 0,
										 NStr::Format( "Can't get mission stats \"%s\" for %d mission in \"%s\" campaign",
																	 pCampaignStats->templateMissions[i].c_str(), i, pCampaignStats->szParentName.c_str() ) );
				continue;
			}
			if ( pChapterStats->szSettingName == pMissionStats->szSettingName )
			{
				
			}
		}
	}
	std::vector<int> missionIndices;
	/**/

	do
	{
		int nNumberOfFinishedMissions = GetGlobalVar( "Mission.Finished.Counter", 0 );
		int nTotalProbability = 0;			//��� ������ ������ � ������ �����������
		
		std::unordered_map< std::string, int > missionFinishTimes;		//� ���� ������� ����� ������������ ����� ������ � ����� ��� �������� ��������� ���
		//��������� ��������
		for ( int i=0; i<nNumberOfFinishedMissions; i++ )
		{
			const std::string szVarName = NStr::Format( "Mission.Finished.%d", i );
			std::string szMissionName = GetGlobalVar( szVarName.c_str(), "" );
			NI_ASSERT_T( !szMissionName.empty(), "Error while getting mission name" );
			missionFinishTimes[szMissionName] = i;
		}
		
		std::vector<STemplateMission> templates;
		for ( int i = 0; i < pCampaignStats->templateMissions.size(); ++i )
		{
			const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( pCampaignStats->templateMissions[i].c_str(), IObjectsDB::MISSION );
			NI_ASSERT_TF( pStats != 0, NStr::Format("Can't get mission stats \"%s\" for %d mission in \"%s\" campaign", pCampaignStats->templateMissions[i].c_str(), i, pCampaignStats->szParentName.c_str()), continue );
			if ( pStats->szSettingName != pChapterStats->szSettingName )
				continue;
			
			STemplateMission temp;
			temp.szName = pCampaignStats->templateMissions[i];
			
			//���������, ����� ���� �������� ��������� ��� ��� ������
			int nFinishTime = 0;
			std::unordered_map< std::string, int >::iterator findIt = missionFinishTimes.find( temp.szName );
			if ( findIt != missionFinishTimes.end() )
				nFinishTime = findIt->second;			//������ ������������ � ��������, ������ �����-�� �����������

			temp.nProbability = nNumberOfFinishedMissions - nFinishTime + 1;
			temp.nProbability *= temp.nProbability;
			nTotalProbability += temp.nProbability;
			
			templates.push_back( temp );
		}
		
		//������ � ��� ���� ������ ���������� template ������ � �� �����������
		NI_ASSERT_T( nTotalProbability != 0, "Error while randomization template mission, nTotalProbability is 0" );
		if ( nTotalProbability == 0 )
			break;
		
		srand( timeGetTime() );
		SRMContext chapterContext;
		int nRes = LoadDataResource( pChapterStats->szContextName, "", false, 0, RMGC_CONTEXT_NAME, chapterContext );
		NI_ASSERT_T( nRes != 0, "Error while LoadDataResource()" );
		if ( !nRes )
			break;
		
		// �������� ��������� ������ placeHolders
		std::vector<CVec2> tempPlaceHolders;
		for ( int i = 0; i < pChapterStats->placeHolders.size(); ++i )
			tempPlaceHolders.push_back( pChapterStats->placeHolders[i].vPosOnMap );

		// ��� ������ �� ���������� ��������� ���� template �����
		if ( CPtr<IScenarioTracker> pScenarioTracker = GetSingleton<IScenarioTracker>() )
		{
			for ( int nDifficulty = 0; nDifficulty < 3; ++nDifficulty )
			{
				if ( nTotalProbability == 0 )
					break;			//������ ����� ��� ���� template ������
				if ( tempPlaceHolders.empty() )
					break;			//������ �� ������� ������ (place holders) ��� ������

				int nRand = rand() % nTotalProbability;
				//������ ��������������� ��� ������
				int nCurrentTemplateMission = 0;
				int nSum = 0;
				for ( ; nCurrentTemplateMission < templates.size(); nCurrentTemplateMission++ )
				{
					nSum += templates[nCurrentTemplateMission].nProbability;
					if ( nSum > nRand )
						break;		//�����
				}
				NI_ASSERT_T( nCurrentTemplateMission != templates.size(), "Error: Can not find mission name while generating template missions" );

				SChapterStats::SMission mission;
				mission.szMission = templates[nCurrentTemplateMission].szName;

				//������ ��������� �� ����� �������
				nRand = rand() % tempPlaceHolders.size();
				mission.vPosOnMap = tempPlaceHolders[nRand];
				mission.pMission = NGDB::GetGameStats<SMissionStats> ( mission.szMission.c_str(), IObjectsDB::MISSION );
				//����������� ����� �� ����������� random ������
				mission.nMissionDifficulty = nDifficulty;
				
				mission.szMissionBonus = pScenarioTracker->GetRandomBonus( nDifficulty );
				if ( mission.szMissionBonus.empty() )
				{
					//�������� ������ �������
					std::vector<std::string> bonuses;
					chapterContext.GetRandomBonuses( nDifficulty, bonuses );
					for ( std::vector<std::string>::const_iterator bonusIterator = bonuses.begin(); bonusIterator != bonuses.end(); ++bonusIterator )
					{
						pScenarioTracker->AddRandomBonus( nDifficulty, *bonusIterator );
					}
					mission.szMissionBonus = pScenarioTracker->GetRandomBonus( nDifficulty );
				}
				//mission.szMissionBonus = chapterContext.GetRandomBonus( nDifficulty );

				chapterContext.GetAllRandomBonuses( nDifficulty, mission.szAllBonuses );
				pChapterStats->AddMission( mission );

				//�� �� ������ ������������ ������ ��� ���� template, ������� ��� ���� ������� �� ������ ���������� templates
				//� ����� ������� �� ����� ������������
				nTotalProbability -= templates[nCurrentTemplateMission].nProbability;
				std::vector<STemplateMission>::iterator it = templates.begin() + nCurrentTemplateMission;
				templates.erase( it );

				//������ ������ ������� �� ���������� ������ placeHolders
				std::vector<CVec2>::iterator tt = tempPlaceHolders.begin() + nRand;
				tempPlaceHolders.erase( tt );
			}
		}
	} while ( 0 );

	//�������� chapter stats
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	const std::string szChapterFileName = pStorage->GetName() + szChapterName + ".xml";
	{
		if ( CPtr<IDataStream> pStream = CreateFileStream( szChapterFileName.c_str(), STREAM_ACCESS_WRITE ) )
		{
			CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
			saver.Add( "RPG", pChapterStats );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMissionInfo
{
	int nIndex;
	CVec2 vPos;

	SMissionInfo() : nIndex( -1 ), vPos( VNULL2 ) {}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef std::unordered_map< std::string, std::vector<SMissionInfo> > CTemplateInfos;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceChapter::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );
	//	SetBindSection( "intermission" );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceChapter::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\common\\chapter" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );

	// ��������� ChapterScript
	const bool bFirstEnter = GetSingleton<IScenarioTracker>()->StartChapter( GetGlobalVar("Chapter.Current.Name", "UnknownChapter") );
	if ( bFirstEnter )
	{
		std::string szSaveName; 
		szSaveName += CUIConsts::GetCampaignNameAddition();
		szSaveName += " Chapter Start Auto";
		szSaveName += ".sav";
		GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_SAVE, NStr::Format( "%s;1", szSaveName.c_str() ) );
	}

	if ( !GetGlobalVar( "Chapter.IsFirst", 0 ) )
	{
		IPlayerScenarioInfo *pPlayer = GetSingleton<IScenarioTracker>()->GetUserPlayer();
		if ( pPlayer->GetNumNewDepotUpgrades() )
		{
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_NEW_DEPOTUPGRADES, 0 );
		}
	}
	InitWindow();
	// scare player if he passed 3 random missions in second chapter.
	if ( !GetGlobalVar( "AlreadyScared", 0 ) && GetGlobalVar( "ItsSecondChapterAndThreeRandomMissions", 0 ) )
	{
		SetGlobalVar( "AlreadyScared", 1 );
				GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MESSAGE_BOX, 
										NStr::Format( "%s;%s;0;", "Textes\\UI\\MessageBox\\go_scenario_mission_caption",
														 "Textes\\UI\\MessageBox\\go_scenario_mission_message" ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceChapter::InitWindow()
{
	IncrementChapterVisited();

	//��������� ���������� � �������
	const SChapterStats *pStats = ReadChapterStats();
	if ( pStats == 0 )
		return;
	int nMission = GetGlobalVar( "Mission.Current.Index", 0 );		//��� �������� ������, ���� �� ������������ ����� �� ������ ������, �� �� != 0
	
	//��������� ���������� ������ ��� map image control
	IUIContainer *pMap = checked_cast<IUIContainer *> ( pUIScreen->GetChildByID( 100 ) );
	IGFXTexture *pTexture = GetSingleton<ITextureManager>()->GetTexture( pStats->szMapImage.c_str() );
	NI_ASSERT_T( pTexture != 0, "Chapter map texture is invalid" );
	pMap->SetWindowTexture( pTexture );
	CTRect<float> rc( 0.0f, 0.0f, pStats->mapImageRect.x2, pStats->mapImageRect.y2 );
	pMap->SetWindowMap( rc );
	pMap->SetWindowPlacement( 0, &CVec2( pStats->mapImageRect.x1, pStats->mapImageRect.y1 ) );
	
	//�������� ������
	CPtr<IDataStream> pMissionButtonStream = GetSingleton<IDataStorage>()->OpenStream( "ui\\common\\missionbutton.xml", STREAM_ACCESS_READ );
	CTreeAccessor missionButtonSaver = CreateDataTreeSaver( pMissionButtonStream, IDataTree::READ );
	
	CPtr<IDataStream> pTemplateMissionButtonStream = GetSingleton<IDataStorage>()->OpenStream( "ui\\common\\templatemissionbutton.xml", STREAM_ACCESS_READ );
	CTreeAccessor templateMissionButtonSaver = CreateDataTreeSaver( pTemplateMissionButtonStream, IDataTree::READ );

	ITextManager *pTM = GetSingleton<ITextManager>();
	CTemplateInfos templateInfos;
	srand( timeGetTime() );			//������������� random ����������
	missionIndeces.clear();
	int nNumberOfScenarioMissions = 0;
	int nCheatEnabledMissions = GetGlobalVar( "Cheat.Enable.Missions", -1 );
	
	// ��������� ��� ����������� ������
	for ( int i = 0; i < pStats->missions.size(); ++i )
	{
		//Read mission stats
		std::string szMissionName = pStats->missions[i].szMission;
		NStr::ToLower( szMissionName );
		const SMissionStats *pMissionStats = NGDB::GetGameStats<SMissionStats>( szMissionName.c_str(), IObjectsDB::MISSION );
		NI_ASSERT_T( pMissionStats != 0, NStr::Format( "Can not get mission stats %s", szMissionName.c_str() ) );
		
		if ( pMissionStats->IsTemplate() )
			continue;
		//������� ������
		
		if ( nCheatEnabledMissions == -1 )
		{
			//��������, ����� �� ��� ������ ��� ������, ����� �� �� ����� ���������� �� ������ ���
			std::string szVarName = "Mission.";
			szVarName += szMissionName;
			szVarName += ".Finished";
			if ( GetGlobalVar( szVarName.c_str(), -1 ) == 1 )
			{
				//������ ��� ��������
				continue;
			}
			
			//���������, ��������� �� ��� ������, ����� �� ���������� �� � ������ ������
			szVarName = "Mission.";
			szVarName += szMissionName;
			szVarName += ".Enabled";
			if ( GetGlobalVar( szVarName.c_str(), -1 ) != 1 )
			{
				//������ ����������
				continue;
			}
		}

		//������� ������ � ��������� �� � �����
		CPtr<IUIElement> pMissionButton;
		missionButtonSaver.Add( "Element", &pMissionButton );
		CVec2 size;
		pMissionButton->GetWindowPlacement( 0, &size, 0 );
		
		CVec2 vPos = pStats->missions[i].vPosOnMap;
		vPos.x -= size.x / 2;
		vPos.y -= size.y / 2;
		pMissionButton->SetWindowPlacement( &vPos, 0 );
		pMissionButton->SetWindowID( 1000 + missionIndeces.size() );
		pMap->AddChild( pMissionButton );
		missionIndeces.push_back( i );
	}
	nNumberOfScenarioMissions = missionIndeces.size();
	
	//��������� ��� ����������� ������
	for ( int i = 0; i < pStats->missions.size(); ++i )
	{
		//Read mission stats
		std::string szMissionName = pStats->missions[i].szMission;
		NStr::ToLower( szMissionName );
		const SMissionStats *pMissionStats = NGDB::GetGameStats<SMissionStats>( szMissionName.c_str(), IObjectsDB::MISSION );
		NI_ASSERT_T( pMissionStats != 0, NStr::Format( "Can not get mission stats %s", szMissionName.c_str() ) );
		
		if ( !pMissionStats->IsTemplate() )
			continue;

		CPtr<IUIElement> pMissionButton;
		templateMissionButtonSaver.Add( "Element", &pMissionButton );
		CVec2 size;
		pMissionButton->GetWindowPlacement( 0, &size, 0 );
		
		CVec2 vPos = pStats->missions[i].vPosOnMap;
		vPos.x -= size.x / 2;
		vPos.y -= size.y / 2;
		pMissionButton->SetWindowPlacement( &vPos, 0 );
		pMissionButton->SetWindowID( 1000 + missionIndeces.size() );
		pMap->AddChild( pMissionButton );
		missionIndeces.push_back( i );
	}
	if ( !missionIndeces.empty() )
		SetGlobalVar( "NumberOfButtons", (int) missionIndeces.size() - 1 );
	else
		SetGlobalVar( "NumberOfButtons", 0 );
	
	//��������� ����� ���������
	IUIElement *pHeader = pUIScreen->GetChildByID( 20000 );
	NI_ASSERT_T( pHeader != 0, "Invalid chapter header control" );
	CPtr<IText> p2 = pTM->GetDialog( pStats->szHeaderText.c_str() );
	if ( p2 )
	{
		pHeader->SetWindowText( 0, p2->GetString() );
	}

	int nFinishStatus = GetGlobalVar( "Mission.Last.FinishStatus", -1 );
	if ( nFinishStatus == 0 || nFinishStatus == -1 )
	{
		//������� �������� ������ ��������
		SetGlobalVar( "Mission.Last.FinishStatus", MISSION_FINISH_LOSE );		//��� ���� ����� � ������� �� ������������������ random �����
		do
		{
			nMission = 0;
			
			//������ �������� ������, ������ ����� �����������
			if ( nNumberOfScenarioMissions > 0 )
			{
				nMission = rand() % nNumberOfScenarioMissions;
				break;
			}
			
			//������ �������� ������ ����� template missions
			int nNumberOfTemplateMissions = missionIndeces.size() - nNumberOfScenarioMissions;
			NI_ASSERT_T( nNumberOfTemplateMissions > 0, "Can not select active missions. Possibly chapter script has error" );
			if ( nNumberOfTemplateMissions > 0 )
			{
				int nRandom = rand() % nNumberOfTemplateMissions;
				nMission = nNumberOfScenarioMissions + nRandom;
				break;
			}
		} while ( 0 );
	}
	else
	{
		//�������� ������ �������� �������
		for ( int i=0; i<missionIndeces.size(); i++ )
		{
			if ( missionIndeces[i] == nMission )
			{
				nMission = i;
				break;
			}
		}
	}

	NI_ASSERT_T( missionIndeces.size() > 0, "Error: There is no template or scenario missions" );
	if ( !missionIndeces.empty() )
	{
		//��������� �������� ������
		SetMissionDescription( nMission );
		int nActiveMissionId = 1000 + nMission;
		IUIElement *pMissionButton = pMap->GetChildByID( nActiveMissionId );
		if ( pMissionButton )
			pMissionButton->SetState( 1 );
	}

	const bool bFirstChapter = GetGlobalVar( "Chapter.IsFirst", 0 );
	
	IUIButton *pButtonWarehouse = checked_cast<IUIButton*>( pUIScreen->GetChildByID( MC_WAREHOUSE ) );
	IPlayerScenarioInfo *pPlayerInfo = GetSingleton<IScenarioTracker>()->GetUserPlayer();
	pButtonWarehouse->EnableWindow( !bFirstChapter && pPlayerInfo->GetNumDepotUpgrades() && GetSingleton<IScenarioTracker>()->GetUserPlayer()->GetNumUnits() );
	IUIButton *pButtonPlayer = checked_cast<IUIButton*>( pUIScreen->GetChildByID( MC_PLAYER ) );
	pButtonPlayer->EnableWindow( !bFirstChapter );
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	pScene->AddUIScreen( pUIScreen );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceChapter::SetMissionDescription( const int _nSelected )
{
	nSelected = _nSelected;
	//const std::string szMissionName = pStats->missions[nSelected].szMission;
	CheckRange( missionIndeces, nSelected );
	CheckRange( ReadChapterStats()->missions, missionIndeces[nSelected] );
	const SChapterStats::SMission &missionStats = ReadChapterStats()->missions[ missionIndeces[nSelected] ];
	// complexity
	// bonus for mission
	// mission description
	std::wstring szDescription;
	CUIConsts::CreateDescription( &missionStats, &szDescription, true );
	
	IUIComplexScroll * pComplexScroll = checked_cast<IUIComplexScroll *> ( pUIScreen->GetChildByID( 2000 ) );
	pComplexScroll->Clear();

	// mission description (text)
	{
	const std::string szName = "UI\\common\\BlackStaticText.xml";
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, NStr::Format("Can not open stream %s", szName.c_str()) );
	if ( !pStream )
		return;
	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
	CPtr<IUIElement> pTextDescription;
	saver.Add( "Element", &pTextDescription );
	pTextDescription->SetWindowText( 0, szDescription.c_str() );		
	pComplexScroll->AddItem( pTextDescription, true );
	}

	// bonus units info
	const std::string szName = "UI\\common\\UnitInfoItem.xml";
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, NStr::Format("Can not open stream %s", szName.c_str()) );
	if ( !pStream )
		return;
	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
	IObjectsDB *pDB = GetSingleton<IObjectsDB>();


	for ( int i = 0; i < missionStats.szAllBonuses.size(); ++i )
	{
		CPtr<IUIElement> pUnitInfo;
		saver.Add( "Element", &pUnitInfo );

		const SGDBObjectDesc *pDesc = pDB->GetDesc( missionStats.szAllBonuses[i].c_str() );
		NI_ASSERT_T( pDesc != 0, NStr::Format( "valid unit %s", missionStats.szAllBonuses[i].c_str() ) );
		const SUnitBaseRPGStats *pRPG = static_cast<const SUnitBaseRPGStats*>( pDB->GetRPGStats( pDesc ) );
		FillUnitInfoItem( pRPG, checked_cast_ptr<IUIDialog*>(pUnitInfo), i, false, 0 );
		pComplexScroll->AddItem( pUnitInfo, false );
	}
	pUIScreen->Reposition( pGFX->GetScreenRect() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceChapter::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;

	if ( msg.nEventID >= E_START_WINDOW_ID && msg.nEventID < E_START_WINDOW_ID + 9000 )
	{
		const SChapterStats::SMission &missionStats = ReadChapterStats()->missions[ missionIndeces[nSelected] ];
		std::string szTemp = NStr::Format( "%d;", E_UNIT );
		szTemp += missionStats.szAllBonuses[msg.nEventID - E_START_WINDOW_ID];
		FinishInterface( MISSION_COMMAND_ENCYCLOPEDIA, szTemp.c_str() );
		return true;
	}
	
	if ( msg.nEventID >= 1000 && msg.nEventID - 1000 < missionIndeces.size() )
	{
		SetMissionDescription( msg.nEventID - 1000 );
	}
	
	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			{
				//��������, ����� �� � custom chapter
				int nCustomChapter = GetGlobalVar( "Custom.Chapter", 0 );
				if ( nCustomChapter )
				{
					RemoveGlobalVar( "Custom.Chapter" );
					GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_MAIN_MENU, "5" );
					GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_CUSTOM_CHAPTER, 0 );
				}
				else
					FinishInterface( MISSION_COMMAND_CAMPAIGN, 0 );
				return true;
			}

		case MC_SAVE_GAME:
			//FinishInterface( MISSION_COMMAND_STATS, "1" );
			GetSingleton<IMainLoop>()->Command( MISSION_COMMAND_IM_SAVE_MISSION, 0 );

			return true;


		case MC_PLAYER:
			FinishInterface( MISSION_COMMAND_PLAYERS_STATS, 0 );
			return true;

		case MC_UNITS:
			FinishInterface( MISSION_COMMAND_UNITS_POOL, 0 );
			return true;

		case MC_WAREHOUSE:
			//FinishInterface( MISSION_COMMAND_UPGRADE_UNIT, 0 );
			FinishInterface( MISSION_COMMAND_WAREHOUSE, 0 );
			return true;
			
		case IMC_OK:
			//��������� ����� chapter'a
			IUIContainer *pMap = checked_cast<IUIContainer *> ( pUIScreen->GetChildByID( 100 ) );
			NI_ASSERT_T( pMap != 0, "Can't find element 100 - missions map!" );
			//������ ���������� ������
			
			int nSelected = -1;
			for ( int i=0; i<missionIndeces.size(); i++ )
			{
				IUIElement *pButton = pMap->GetChildByID( i + 1000 );
				NI_ASSERT_T( pButton != 0, NStr::Format( "Can not get control by id", i + 1000 ) );
				if ( pButton->GetState() == 1 )
				{
					nSelected = missionIndeces[i];
					break;
				}
			}

			NI_ASSERT_T( nSelected != -1, "Can not find selected mission" );
			if ( nSelected == -1 )
				return true;

			SetGlobalVar( "Mission.Current.Index", nSelected );
	
			const SChapterStats *pStats = ReadChapterStats();
			const std::string szMissionName = pStats->missions[nSelected].szMission;
			SetGlobalVar( "Mission.Current.IsTemplate", pStats->missions[nSelected].pMission->IsTemplate() );
			SetGlobalVar( "Mission.Current.Name", szMissionName.c_str() );
			FinishInterface( MISSION_COMMAND_ABOUT_MISSION, 0 );
			return true;
	}
	
	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
