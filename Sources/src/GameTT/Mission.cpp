#include "StdAfx.h"

#include "Mission.h"

#include "Chapter.h"
#include "AddUnitToMission.h"
#include "CommonId.h"
#include "MinimapCreation.h"
#include "UIConsts.h"
#include "..\RandomMapGen\MapInfo_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\UI\UIMessages.h"
#include "..\StreamIO\ProgressHook.h"
#include "..\Main\ScenarioTracker.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const int ADD_SELECTED_CONST = 100;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ECommands
{
	MC_UNITS						= 10003,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const NInput::SRegisterCommandEntry commands[] = 
{
	{ "inter_ok"				,	IMC_OK				},
	{ "inter_cancel"		, IMC_CANCEL		},
	{ 0									,	0							}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CInterfaceAboutMission::~CInterfaceAboutMission()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const SMissionStats *CInterfaceAboutMission::ReadMissionStats()
{
	std::string szMissionName = GetGlobalVar( "Mission.Current.Name" );
	const SMissionStats *pStats = NGDB::GetGameStats<SMissionStats>( szMissionName.c_str(), IObjectsDB::MISSION );
	return pStats;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceAboutMission::Init()
{
	CInterfaceInterMission::Init();
	commandMsgs.Init( pInput, commands );
	//	SetBindSection( "intermission" );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceAboutMission::StartInterface()
{
	CInterfaceInterMission::StartInterface();
	//загружаем информацию о миссии
	const SMissionStats *pStats = ReadMissionStats();

	NI_ASSERT_T( pStats != 0, "Invalid mission stats" );
	if ( pStats == 0 )
		return;
	// generate map for random mission
	const std::string szVarName = "Mission." + pStats->szParentName + ".Random.Generated";

	const SChapterStats *pChapterStats = CInterfaceChapter::ReadChapterStats();
	
	std::string szChapterUnitsTableFileName = pChapterStats->szContextName;
	int nLevel = 0;

	int nMissionIndex = GetGlobalVar( "Mission.Current.Index", -1 );
	NI_ASSERT_T( nMissionIndex != -1, "Invalid mission index in CInterfaceAboutMission::Init()" );
	nLevel = pChapterStats->missions[nMissionIndex].nMissionDifficulty;

	if ( pStats->IsTemplate() && ( GetGlobalVar( szVarName.c_str(), 0 ) != 1 ) )
	{
		const std::string szMissionName = pStats->szParentName;
		// mark, that this is a random mission
		SetGlobalVar( ("Mission." + szMissionName + ".Random").c_str(), 1 );
		SetGlobalVar( szVarName.c_str(), 1 );
		NStr::DebugTrace( "CInterfaceAboutMission::StartInterface(), add Template Mission: %s\n", pStats->szParentName.c_str() );
		//

		CPtr<IMovieProgressHook> pProgress = CreateObject<IMovieProgressHook>( MAIN_PROGRESS_INDICATOR );
		pProgress->Init( IMovieProgressHook::PT_MAPGEN );
		pProgress->SetNumSteps( RMGC_CREATE_RANDOM_MAP_STEP_COUNT );

		int nFoundGraphIndex = 0;
		int nFoundAngle = 0;

		bool bOnlyGraph = false;
		bool bOnlyAngle = false;
		
		if ( IUserProfile *pUserProfile = GetSingleton<IUserProfile>() )
		{
			//граф
			SRMTemplate randomMapTemplate;
			bool bResult = LoadDataResource( pStats->szTemplateMap, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate );
			NI_ASSERT_T( bResult,
									 NStr::Format( "CInterfaceAboutMission::StartInterface(), Can't load SRMTemplate from %s", pStats->szTemplateMap.c_str() ) );
			NI_ASSERT_T( !randomMapTemplate.graphs.empty(),
									 NStr::Format( "CInterfaceAboutMission::StartInterface(), Graphs is empty for SRMTemplate from %s", pStats->szTemplateMap.c_str() ) );
			
			int nMinimumUsedGraph = pUserProfile->GetUsedTemplateGraphs( pStats->szTemplateMap, randomMapTemplate.graphs[0] );
			for ( int nGraphIndex = 1; nGraphIndex < randomMapTemplate.graphs.size(); ++nGraphIndex )
			{
				int nUsedTemplateGraph = pUserProfile->GetUsedTemplateGraphs( pStats->szTemplateMap, randomMapTemplate.graphs[nGraphIndex] );
				if ( nMinimumUsedGraph > nUsedTemplateGraph )
				{
					nMinimumUsedGraph = nUsedTemplateGraph;
				}
			}
			
			CWeightVector<int> graphIndices;
			for ( int nGraphIndex = 0; nGraphIndex < randomMapTemplate.graphs.size(); ++nGraphIndex )
			{
				int nUsedTemplateGraph = pUserProfile->GetUsedTemplateGraphs( pStats->szTemplateMap, randomMapTemplate.graphs[nGraphIndex] );
				if ( nMinimumUsedGraph == nUsedTemplateGraph )
				{
					graphIndices.push_back( nGraphIndex, randomMapTemplate.graphs.GetWeight( nGraphIndex ) );
				}
			}
			if ( !graphIndices.empty() )
			{
				if ( graphIndices.size() == 1 )
				{
					bOnlyGraph = true;
					nFoundGraphIndex = graphIndices[0];
				}
				else
				{
					nFoundGraphIndex = graphIndices.GetRandom();
				}
			}
			
			//угол
			int nMinimumUsedAngle = pUserProfile->GetUsedAngles( 0 );
			for ( int nAngleIndex = 1; nAngleIndex < 4; ++nAngleIndex )
			{
				int nUsedAngle = pUserProfile->GetUsedAngles( nAngleIndex );
				if ( nMinimumUsedAngle > nUsedAngle )
				{
					nMinimumUsedAngle = nUsedAngle;
				}
			}
			std::vector<int> angleIndices;
			for ( int nAngleIndex = 0; nAngleIndex < 4; ++nAngleIndex )
			{
				if ( nMinimumUsedAngle == pUserProfile->GetUsedAngles( nAngleIndex ) )
				{
					angleIndices.push_back( nAngleIndex );
				}
			}
			if ( !angleIndices.empty() )
			{
				if ( angleIndices.size() == 1 )
				{
					bOnlyAngle = true;
					nFoundAngle = angleIndices[0];
				}
				else
				{
					nFoundAngle = angleIndices[rand() % angleIndices.size()];
				}
			}
		}

		SRMUsedTemplateInfo usedTemplateInfo;
		bool bRes = CMapInfo::CreateRandomMap( const_cast<SMissionStats*>( pStats ), szChapterUnitsTableFileName, nLevel, nFoundGraphIndex, nFoundAngle, true, true, &usedTemplateInfo, pProgress );
		GetSingleton<IUserProfile>()->AddUsedTemplate( usedTemplateInfo.szTemplateName, 1, usedTemplateInfo.szGraphName, ( bOnlyGraph ? 2 : 1 ), usedTemplateInfo.nGraphAngle, ( bOnlyAngle ? 2 : 1 ) );

		pProgress->Stop();
		NI_ASSERT_T( bRes != false, "Can not generate random map" );
		if ( !bRes )
			return;

		//для сейвов
		SetGlobalVar( "Chapter.Units.Table.FileName", szChapterUnitsTableFileName.c_str() );
		SetGlobalVar( "Chapter.Units.Table.Level", nLevel );
		SetGlobalVar( "Chapter.Units.Table.GraphName", usedTemplateInfo.szGraphName.c_str() );
		SetGlobalVar( "Chapter.Units.Table.Angle", usedTemplateInfo.nGraphAngle );

		IDataStorage *pStorage = GetSingleton<IDataStorage>();
		const std::string szFullMissionName = pStorage->GetName() + szMissionName + ".xml";
		CPtr<IDataStream> pStream = CreateFileStream( szFullMissionName.c_str(), STREAM_ACCESS_WRITE );
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
		saver.Add( "RPG", const_cast<SMissionStats*>(pStats) );
	}
	else if ( pStats->IsTemplate() )
	{
		NStr::DebugTrace( "CInterfaceAboutMission::StartInterface(), Template Mission exists: %s\n", pStats->szParentName.c_str() );
	}

	const std::string szFullMapName = "maps\\" + pStats->szFinalMap;
	CMinimapCreation::Create1Minimap( szFullMapName, pStats->szMapImage );


	pUIScreen = CreateObject<IUIScreen>( UI_SCREEN );
	pUIScreen->Load( "ui\\common\\mission" );
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	
	ITextManager *pTM = GetSingleton<ITextManager>();
	
	//установим правильный размер для map image control
	IUIObjMap *pMap = checked_cast<IUIObjMap *> ( pUIScreen->GetChildByID( 100 ) );
	IGFXTexture *pTexture = GetSingleton<ITextureManager>()->GetTexture( pStats->szMapImage.c_str() );
	NI_ASSERT_T( pTexture != 0, "Mission map texture is invalid" );

	pMap->SetMapTexture( pTexture );
	pMap->Init();

	const int DELTAX = 5;
	const int DELTAY = 3;
	CVec2 vMapControlSize;
	{
		pMap->GetWindowPlacement( 0, &vMapControlSize, 0 );
		vMapControlSize.x -= DELTAX*2;
		vMapControlSize.y -= DELTAY*2;
	}

	//установим текст заголовка
	IUIElement *pHeader = pUIScreen->GetChildByID( 20000 );
	NI_ASSERT_T( pHeader != 0, "Invalid mission header control" );
	CPtr<IText> p2 = pTM->GetDialog( pStats->szHeaderText.c_str() );
	NI_ASSERT_T( p2 != 0, NStr::Format("Can not read text header resource %s", pStats->szHeaderText.c_str()) );
	if ( p2 != 0 )
		pHeader->SetWindowText( 0, p2->GetString() );
	
	//установим текст описания
	IUIElement *pDesc = checked_cast<IUIElement *> ( pUIScreen->GetChildByID( 2000 ) );
	NI_ASSERT_T( pDesc != 0, "Invalid mission text description control" );
	std::wstring szDescription;
	CUIConsts::CreateDescription( &pChapterStats->missions[nMissionIndex], &szDescription, false );
	pDesc->SetWindowText( 0, reinterpret_cast<const WORD*>( szDescription.c_str() ) );
	//CPtr<IText> pText = pTM->GetDialog( pStats->szDescriptionText.c_str() );
	//NI_ASSERT_T( pText != 0, NStr::Format("Can't find description text \"%s\"", pStats->szDescriptionText.c_str()) );
	//if ( pText != 0 )
		//pDesc->SetWindowText( 0, pText->GetString() );
	
	IUIElement *pObjectivesScreen = pUIScreen->GetChildByID( 4000 );
	pObjectivesScreen->ShowWindow( UI_SW_SHOW );		//здесь произойдет авто загрузка objectives

	//загрузим флажки Objectives для отображения на минимапе
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	CPtr<IDataStream> pStream = pStorage->OpenStream( "ui\\common\\objective_flag.xml", STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, "CInterfaceAboutMission error: Can not open stream ui\\common\\objective_flag.xml" );
	if ( !pStream )
		return;
	CTreeAccessor objectiveFlagSaver = CreateDataTreeSaver( pStream, IDataTree::READ );

	pStream = pStorage->OpenStream( "ui\\common\\objective_sel_flag.xml", STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, "CInterfaceAboutMission error: Can not open stream ui\\common\\objective_sel_flag.xml" );
	if ( !pStream )
		return;
	CTreeAccessor objectiveSelectedFlagSaver = CreateDataTreeSaver( pStream, IDataTree::READ );

	int nObjectiveIndex = 0;
	std::string szMissionName = GetGlobalVar( "Mission.Current.Name" );
	for ( int i=0; i<pStats->objectives.size(); i++ )
	{
		int nObjectiveState = 0;

		int nDefault = -1;
		if ( !pStats->objectives[i].bSecret )
			nDefault = 0;
		
		std::string szObjName = NStr::Format( "temp.%s.%s%d", szMissionName.c_str(), "objective", i );
		
		nObjectiveState = GetGlobalVar( szObjName.c_str(), nDefault );
		if ( nObjectiveState == -1 )
			continue;		//objective не виден
		
		//рассчитаем координаты objective в статическом минимап контроле
		const CVec2 &v = pStats->objectives[i].vPosOnMap;

		CVec2 vPos;
		if ( v.x == 0 && v.y == 0 )
		{
			vPos.x = -1000;
			vPos.y = -1000;
		}
		else
		{
			vPos.x = vMapControlSize.x/2 + (v.x/pTexture->GetSizeX(0))*(vMapControlSize.x/2) - (v.y/pTexture->GetSizeY(0))*vMapControlSize.y;
			vPos.y = (v.x/pTexture->GetSizeX(0))*(vMapControlSize.x/4) + (v.y/pTexture->GetSizeY(0))*(vMapControlSize.y/2);
		}

		//Add flag
		CPtr<IUIElement> pFlag;
		objectiveFlagSaver.Add( "Element", &pFlag );

		CVec2 vFlagSize;
		pFlag->GetWindowPlacement( 0, &vFlagSize, 0 );
		vPos.x -= vFlagSize.x/2;
		vPos.x += DELTAX;
		vPos.y -= vFlagSize.y;
		vPos.y += DELTAY;
		pFlag->SetWindowPlacement( &vPos, 0 );
		pFlag->SetWindowID( nObjectiveIndex );
		pMap->AddChild( pFlag );

		//Add selected flag
		CPtr<IUIElement> pSelectedFlag;
		objectiveSelectedFlagSaver.Add( "Element", &pSelectedFlag );
		pSelectedFlag->SetWindowPlacement( &vPos, 0 );
		pSelectedFlag->SetWindowID( nObjectiveIndex + ADD_SELECTED_CONST );
		pMap->AddChild( pSelectedFlag );
		pSelectedFlag->ShowWindow( UI_SW_HIDE );
		nObjectiveIndex++;
	}
	
	pUIScreen->Reposition( pGFX->GetScreenRect() );
	// add UI screen to scene
	pScene->AddUIScreen( pUIScreen );

	//инициализируем свободные слоты в миссии дефалтовыми юнитами
//	bool bRes = CInterfaceAddUnitToMission::AddDefaultSlotsToST();
	//NI_ASSERT_T( bRes != 0, "CInterfaceAddUnitToMission::AddDefaultSlotsToST() FAILED" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CInterfaceAboutMission::ProcessMessage( const SGameMessage &msg )
{
	if ( CInterfaceInterMission::ProcessMessage( msg ) )
		return true;

	switch ( msg.nEventID )
	{
		case IMC_CANCEL:
			FinishInterface( MISSION_COMMAND_CHAPTER, 0 );
			return true;

		case MC_UNITS:
			FinishInterface( MISSION_COMMAND_ADD_UNIT_TO_MISSION, 0 );
			return true;

		case IMC_OK:
		{
			int nTimeToFade = GetGlobalVar( "Sound.TimeToFade", 5000 );
			GetSingleton<ISFX>()->StopStream( nTimeToFade );

			const SMissionStats *pStats = ReadMissionStats();
			FinishInterface( MISSION_COMMAND_MISSION, (pStats->szFinalMap + ".xml").c_str() );
		}

		case UI_NOTIFY_SELECTION_CHANGED:
		case UI_NOTIFY_SELECTION_CHANGED+1:
			UpdateActiveObjectiveFlag( msg.nParam );
			return true;

		break;
	}

	//
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceAboutMission::UpdateActiveObjectiveFlag( bool bShow )
{
	IUIContainer *pObjectivesScreen = checked_cast<IUIContainer *> ( pUIScreen->GetChildByID( 4000 ) );
	IUIShortcutBar *pBar = checked_cast<IUIShortcutBar *> ( pObjectivesScreen->GetChildByID( 10 ) );
	NI_ASSERT_TF( pBar != 0, "CInterfaceAboutMission Error: Can not get IUIShortcutBar control", return );
	int nSelBar = 0, nSelItem = 0;
	pBar->GetSelectionItem( &nSelBar, &nSelItem );

	//скрываем предыдущий активный objective flag
	ShowActiveObjective( false );

	//показываем новый активный objective flag
	if ( bShow || ( nSelBar != -1 && nSelItem != -1 ) )
	{
		m_nActiveObjective = nSelBar;
		ShowActiveObjective( true );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CInterfaceAboutMission::ShowActiveObjective( bool bShow )
{
	IUIObjMap *pMap = checked_cast<IUIObjMap *> ( pUIScreen->GetChildByID( 100 ) );
	if ( m_nActiveObjective != -1 )
	{
		IUIElement *pElement = pMap->GetChildByID( m_nActiveObjective );
		if ( !pElement )
			return;

		NI_ASSERT_T( pElement != 0, NStr::Format("CInterfaceAboutMission error: can not get control by id %d", m_nActiveObjective) );
		if ( bShow )
			pElement->ShowWindow( UI_SW_HIDE );
		else
			pElement->ShowWindow( UI_SW_SHOW );

		pElement = pMap->GetChildByID( m_nActiveObjective + ADD_SELECTED_CONST );
		NI_ASSERT_T( pElement != 0, NStr::Format("CInterfaceAboutMission error: can not get control by id %d", m_nActiveObjective+ADD_SELECTED_CONST) );
		if ( bShow )
			pElement->ShowWindow( UI_SW_SHOW );
		else
			pElement->ShowWindow( UI_SW_HIDE );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
