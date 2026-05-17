// TemplateEditorFrame1.cpp : implementation file
//

#include "resource.h"
#include "stdafx.h"

#include "..\GFX\GFX.h"
#include "..\GFX\GFXHelper.h"

#include "..\Scene\Terrain.h"
#include "..\Image\Image.h"
#include "..\AILogic\AILogic.h"
#include "..\Scene\Scene.h"
#include "..\Common\Icons.h"
#include "..\Anim\Animation.h"
#include <Mmsystem.h>

#include "editor.h" 
#include "frames.h"
#include "GameWnd.h"
#include "MainFrm.h"
#include "browedit.h" 

#include "TabTileEditDialog.h"
#include "TemplateEditorFrame1.h"
#include "NewMapDialog.h"
#include "OpenMapDialog.h"
#include "PropertieDialog.h"
#include "GetGroupID.h"
#include "MapOptionsDialog.h"

#include "..\Main\RPGStats.h"
#include "..\Main\GameStats.h"
#include "..\Main\TextSystem.h"

//*****************************************************
#include "AIStartCommandsDialog.h"
#include "ProgressDialog.h"
//*****************************************************

//*****************************************************
#include "TerrainState.h"
#include "TileDrawState.h"
#include "StateTerrainFields.h"
#include "VectorObjectsState.h"
#include "SimpleObjectsState.h"
#include "RoadDrawState.h"
#include "ObjectPlacerState.h"
#include "DrawShadeState.h"
#include "MapToolState.h"
#include "StateAIGeneral.h"
#include "StateGroups.h"
#include "SetupFilterDialog.h"
//*****************************************************

//*****************************************************
#include "..\RandomMapGen\RMG_Types.h"
#include "..\RandomMapGen\IB_Types.h"
//*****************************************************

//*****************************************************
#include "RMG_CreateContainerDialog.h"
#include "RMG_CreateGraphDialog.h"
#include "RMG_CreateFieldDialog.h"
#include "RMG_CreateTemplateDialog.h"
//*****************************************************

//*****************************************************
#include "CreateRandomMapDialog.h"
#include "MapEditorOptions.h"
//*****************************************************

#include "SetAnim.h"

#include "DrawingTools.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE 
static char THIS_FILE[] = __FILE__;
#endif

/**
		pPartyColors[0] = 0xF0F0;
		pPartyColors[1] = 0xFF00;
		pPartyColors[2] = 0xF00F;
		pPartyColors[3] = 0xFFF0;
		pPartyColors[4] = 0xF0FF;
		pPartyColors[5] = 0xFF0F;
		pPartyColors[6] = 0xFFFF;
		pPartyColors[7] = 0xFF80;
		pPartyColors[8] = 0xFF08;
		pPartyColors[9] = 0xF8F0;
		pPartyColors[10] = 0xF0F8;
		pPartyColors[11] = 0xF80F;
		pPartyColors[12] = 0xF08F;
		pPartyColors[13] = 0xFF88;
		pPartyColors[14] = 0xF8F8;
		pPartyColors[15] = 0xF88F;
		pPartyColors[16] = 0x0000;
/**/
const BYTE _xHC = 0x30;
const SColor PLAYER_COLORS[16] = { SColor( 0xFF, 0x00, 0xFF, 0x00 ),	//0
																	 SColor( 0xFF, 0xFF, 0x00, 0x00 ),	//1
																	 SColor( 0xFF, 0x00, 0x00, 0xFF ),	//2
																	 SColor( 0xFF, 0xFF, 0xFF, 0x00 ),	//3
																	 SColor( 0xFF, 0x00, 0xFF, 0xFF ),	//4
																	 SColor( 0xFF, 0xFF, 0x00, 0xFF ),	//5
																	 SColor( 0xFF, 0xFF, 0xFF, 0xFF ),	//6
																	 SColor( 0xFF, 0xFF, _xHC, 0x00 ),	//7
																	 SColor( 0xFF, 0xFF, 0x00, _xHC ),	//8
																	 SColor( 0xFF, _xHC, 0xFF, 0x00 ),	//9
																	 SColor( 0xFF, 0x00, 0xFF, _xHC ),	//10
																	 SColor( 0xFF, _xHC, 0x00, 0xFF ),	//11
																	 SColor( 0xFF, 0x00, _xHC, 0xFF ),	//12
																	 SColor( 0xFF, 0x00, _xHC, _xHC ),	//13
																	 SColor( 0xFF, _xHC, 0x00, _xHC ),	//14
																	 SColor( 0xFF, _xHC, _xHC, 0x00 ) };//15

const int TEFConsts::THUMBNAILTILE_WIDTH  = 64;
const int TEFConsts::THUMBNAILTILE_HEIGHT = 64;

const int TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_X  = 10;
const int TEFConsts::THUMBNAILTILEWITHTEXT_SPACE_Y  = 35;

int TEFConsts::THUMBNAILTILE_SPACE_X  = 10;
int TEFConsts::THUMBNAILTILE_SPACE_Y  = 25;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTemplateEditorFrame, SECWorksheet)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CTemplateEditorFrame::STATE_TAB_LABELS[STATE_COUNT] =
{
	"Terrain",
	"Objects, Fences, Bridges",
	"Entrenchments, Roads, Rivers",
	"Map Tools",
	"Reinforcement Groups",
	"AI Settings",
};

const char* CTemplateEditorFrame::STATE_TERRAIN_TAB_LABELS[STATE_TERRAIN_COUNT] =
{
	"Tiles",
	"Heights",
	"Fields",
};

const char* CTemplateEditorFrame::STATE_SO_TAB_LABELS[STATE_SO_COUNT] =
{
	"Objects",
	"Fences",
	"Bridges",
};

const char* CTemplateEditorFrame::STATE_VO_TAB_LABELS[STATE_VO_COUNT] =
{
	"Entrenchments",
	"Roads",
	"Rivers",
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char CTemplateEditorFrame::RIVERS_3D_MAP_NAME[] = _T( "maps\\river3d.xml" ); 
const char CTemplateEditorFrame::ROADS_3D_MAP_NAME[] = _T( "maps\\road3d.xml" );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::SetMapModified( bool bModified )
{
	bMapModified = bModified;
	if ( !m_currentMapName.empty() )
	{
		std::string szExtention;
		if ( mapEditorOptions.bSaveAsBZM )
		{
			szExtention = ".bzm";
		}
		else
		{
			szExtention = ".xml";
		}
		
		std::string szSize;
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
				szSize = NStr::Format( " %dx%d", rTerrainInfo.patches.GetSizeX(), rTerrainInfo.patches.GetSizeY() );
			}
		}
		
		std::string szMODKey;
		if ( ( !currentMapInfo.szMODName.empty() ) && ( !currentMapInfo.szMODVersion.empty() ) )
		{
			szMODKey = " MOD: " + CMODCollector::GetKey( currentMapInfo.szMODName, currentMapInfo.szMODVersion );
		}

		const std::string szExtentions = szExtention + std::string( bMapModified ? " * " : " " ) + szSize + szMODKey;
		const int MAX_NAME_SIZE = 133;
		std::string szCurrentMapName;
		if ( m_currentMapName.size() + szExtentions.size() > MAX_NAME_SIZE )
		{
			if ( ( MAX_NAME_SIZE - szExtentions.size() - 4 ) < 0 )
			{
				szCurrentMapName = m_currentMapName;
			}
			else
			{
				szCurrentMapName = m_currentMapName.substr( 0, MAX_NAME_SIZE - szExtentions.size() - 4 ) + "..." + m_currentMapName.substr( m_currentMapName.size() - 1 );
			}
		}
		else
		{
			szCurrentMapName = m_currentMapName;
		}
		SetWindowText( ( szCurrentMapName + szExtentions ).c_str() );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFileCreateRandomMission() 
{
	const std::string szTemplatesFolder( "Scenarios\\Templates\\" );
		const std::string szChaptersFolder( "Scenarios\\Chapters\\" );

	CCreateRandomMapDialog randomMapDialog;	
	
	std::list<std::string> files;
	BeginWaitCursor();
	if ( GetEnumFilesInDataStorage( RMGC_SETTING_DEFAULT_FOLDER, &files ) )
	{
		for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
		{
			std::string szFileName = ( *fileIterator ).substr( 0, ( *fileIterator ).find( "." ) );
			randomMapDialog.szSettings.push_back( szFileName );
		}
	}
	files.clear();
	if ( GetEnumFilesInDataStorage( szTemplatesFolder, &files ) )
	{
		for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
		{
			std::string szFileName = ( *fileIterator ).substr( 0, ( *fileIterator ).find( "." ) );
			randomMapDialog.szTemplates.push_back( szFileName );
		}
	}
	files.clear();
	if ( GetEnumFilesInDataStorage( szChaptersFolder, &files ) )
	{
		for ( std::list<std::string>::const_iterator fileIterator = files.begin(); fileIterator != files.end(); ++fileIterator )
		{
			std::string szFileName = ( *fileIterator ).substr( 0, ( *fileIterator ).find( "." ) );
			randomMapDialog.szContexts.push_back( szFileName );
		}
	}

	EndWaitCursor();	
	randomMapDialog.szSettings.push_back( RMGC_ANY_SETTING_NAME );
	
	if ( randomMapDialog.DoModal() == IDOK )
	{
		SMissionStats missionStats;
		randomMapDialog.GetTemplate( &( missionStats.szTemplateMap ) );
		randomMapDialog.GetSetting( &( missionStats.szSettingName ) );
		if ( missionStats.szSettingName == RMGC_ANY_SETTING_NAME )
		{
			missionStats.szSettingName.clear();	
		}
		randomMapDialog.GetMap( &( missionStats.szFinalMap ) );
		missionStats.szMapImage = missionStats.szFinalMap + "_large";
		
		std::string szContext;
		randomMapDialog.GetContext( &szContext );

		const int nLevel = randomMapDialog.GetLevel();
		const int nGraph = randomMapDialog.GetGraph();
		const int nAngle = randomMapDialog.GetAngle();
		const bool bSaveAsBZM = randomMapDialog.SaveAsBZM();
		const bool bSaveAsDDS = randomMapDialog.SaveAsDDS();
		
		std::string szMapsFolder = "maps\\";
		std::string szOutputMapName;
		if ( missionStats.szFinalMap.find( szMapsFolder ) == 0 )
		{
			szOutputMapName = missionStats.szFinalMap;
			missionStats.szFinalMap = missionStats.szFinalMap.substr( szMapsFolder.size() );
		}
		else
		{
			szOutputMapName = szMapsFolder + missionStats.szFinalMap;
		}
		szOutputMapName += std::string( bSaveAsBZM ? ".bzm" : ".xml" );
	
		bool bResult = false;
		{
			CCreateRandomMapProgress randomMapProgress( this, RMGC_CREATE_RANDOM_MAP_STEP_COUNT, "Create Random Map", NStr::Format( "Creating random map: %s...", szOutputMapName.c_str() ) ) ;
			bResult = CMapInfo::CreateRandomMap( &missionStats, szContext, nLevel, nGraph, nAngle, bSaveAsBZM, bSaveAsDDS, 0, &randomMapProgress );
		}
		CString strTitle;
		strTitle.LoadString( IDR_EDITORTYPE );
		if ( bResult )
		{
			const CString strMessage = NStr::Format( "Random Map created: %s", szOutputMapName.c_str() );
			MessageBox( strMessage, strTitle, MB_OK | MB_ICONINFORMATION );
		}
		else
		{	
			const CString strMessage = NStr::Format( "Random Map creation error: %s", szOutputMapName.c_str() );
			MessageBox( strMessage, strTitle, MB_OK | MB_ICONSTOP );
		}
	}		
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::CreateMiniMap()
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				if ( !m_currentMapName.empty() )
				{
					if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
					{
						if ( NeedSaveChanges() )
						{
							CCreateRandomMapProgress randomMapProgress( this, RMGC_CREATE_MINIMAP_IMAGE_STEP_COUNT, "Create Minimap", "Creating minimap..." );

							CMapInfo mapInfo;
							if ( m_currentMapName.find( pDataStorage->GetName() ) == 0 )
							{
								std::string szMapName = m_currentMapName.substr( strlen( pDataStorage->GetName() ) );
								if ( mapEditorOptions.bSaveAsBZM )
								{
									CPtr<IDataStream> pStream = pDataStorage->OpenStream( ( szMapName + ".bzm" ).c_str(), STREAM_ACCESS_READ );
									CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
									CSaverAccessor saver = pSaver;
									saver.Add( 1, &mapInfo );
								}
								else
								{
									CPtr<IDataStream> pStream = pDataStorage->OpenStream( ( szMapName + ".xml" ).c_str(), STREAM_ACCESS_READ );
									CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStream, IDataTree::READ );
									CTreeAccessor saver = pSaver;
									saver.AddTypedSuper( &mapInfo );
								}
							}
							else
							{
								if ( mapEditorOptions.bSaveAsBZM )
								{
									CPtr<IDataStream> pStream = OpenFileStream( m_currentMapName + ".bzm", STREAM_ACCESS_READ );
									CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
									CSaverAccessor saver = pSaver;
									saver.Add( 1, &mapInfo );
								}
								else
								{
									CPtr<IDataStream> pStream = OpenFileStream( m_currentMapName + ".xml", STREAM_ACCESS_READ );
									CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStream, IDataTree::READ );
									CTreeAccessor saver = pSaver;
									saver.AddTypedSuper( &mapInfo );
								}
							}
							mapInfo.UnpackFrameIndices();

							CRMImageCreateParameterList imageCreateParameterList;
							imageCreateParameterList.push_back( SRMImageCreateParameter( m_currentMapName + "_large", CTPoint<int>( 0x200, 0x200 ), true, false, SRMImageCreateParameter::INTERMISSION_IMAGE_BRIGHTNESS, SRMImageCreateParameter::INTERMISSION_IMAGE_CONSTRAST, SRMImageCreateParameter::INTERMISSION_IMAGE_GAMMA ) ); 
							imageCreateParameterList.push_back( SRMImageCreateParameter( m_currentMapName, CTPoint<int>( 0x100, 0x100 ), true ) ); 
							imageCreateParameterList.push_back( SRMImageCreateParameter( m_currentMapName + "_large", CTPoint<int>( 0x200, 0x200 ), false, false, SRMImageCreateParameter::INTERMISSION_IMAGE_BRIGHTNESS, SRMImageCreateParameter::INTERMISSION_IMAGE_CONSTRAST, SRMImageCreateParameter::INTERMISSION_IMAGE_GAMMA ) ); 
							imageCreateParameterList.push_back( SRMImageCreateParameter( m_currentMapName, CTPoint<int>( 0x100, 0x100 ), false ) ); 
							mapInfo.CreateMiniMapImage( imageCreateParameterList, &randomMapProgress );

							if ( g_frameManager.GetMiniMapWindow() )
							{
								g_frameManager.GetMiniMapWindow()->OnMinimapGame();
							}
						}
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
CTemplateEditorFrame::CTemplateEditorFrame() : isReservePositionActive( false ), isStartCommandPropertyActive( false ), m_pTabTileEditDialog( 0 ) , m_tileY( 0 ) , m_tileX( 0 ),
	m_brushDY( 2 ) , m_brushDX( 2 ), 
	m_lastMouseTileY( -1 ), m_lastMouseTileX( -1 ) , m_mapEditorBarPtr( 0 ),
	m_lastPoint( 0, 0 ), m_firstPoint( 0, 0 ), m_currentMovingObjectPtrAI( 0 ),
  dlg( 0 ), m_ifCanMultiSelect( false ), m_currentMovingObjectForPlacementPtr( 0 ),
	m_ifCanMovingMultiGroup( false ), ifFitToAI( true ), m_minimapDialogRect( 0, 0, 0, 0 ),
	bFireRangePressed( false ), m_bNeedUpdateUnitHeights( false ), bShowScene6( true ), bShowScene7( false ), bWireframe( false ), bShowScene11( true ), bShowScene1( true ), bShowScene2( true ), bShowScene3( false ), bShowScene4( true ), bShowScene8( false ), bShowScene0( true ), bShowScene9( false ),  bShowAIPassability( false ), 	bShowStorageCoverage( false ), bNeedDrawUnitsSelection( true ), bMapModified( false ), bShowScene13( true ), vScreenCenter( VNULL3 )
{
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// ����� ����
	inputStates.AddInputState( static_cast<CTerrainState*>( 0 ) );								//STATE_TERRAN								= 0
	inputStates.AddInputState( static_cast<CSimpleObjectsState*>( 0 ) );					//STATE_SIMPLE_OBJECTS				= 1
	inputStates.AddInputState( static_cast<CVectorObjectsState*>( 0 ) );					//STATE_VECTOR_OBJECTS				= 2
	inputStates.AddInputState( static_cast<CMapToolState*>( 0 ) );								//STATE_TOOLS									= 3
	inputStates.AddInputState( static_cast<CGroupsState*>( 0 ) );									//STATE_GROUPS								= 4
	inputStates.AddInputState( static_cast<CAIGState*>( 0 ) );										//STATE_AI_GENERAL						= 5
	inputStates.SetActiveState( STATE_TERRAIN_TILES );

// ************************************************************************************************************************ //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	char pBuffer[0xFFF];
	GetCurrentDirectory( 0xFFF, pBuffer );
	szStartDirectory = pBuffer + std::string( "\\" );
	
	m_cursorName = MAKEINTRESOURCE( IDC_APPSTARTING );		//RR
} 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CTemplateEditorFrame::~CTemplateEditorFrame()
{
}
/*
	ON_COMMAND(ID_FILE_LOADMAP, OnFileLoadMap)
	ON_COMMAND(ID_FILE_NEWMAP, OnFileNewMap)
*/

BEGIN_MESSAGE_MAP(CTemplateEditorFrame, SECWorksheet)
	//{{AFX_MSG_MAP(CTemplateEditorFrame)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MBUTTONDBLCLK()
	ON_WM_KEYDOWN()
	ON_WM_SETFOCUS()
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_KILLFOCUS()
	ON_UPDATE_COMMAND_UI( ID_INDICATOR_TILEPOS, OnUpdateTileCoord)
	ON_COMMAND(ID_FILE_SAVEMAP, OnFileSaveMap)
	ON_COMMAND(ID_BUTTONFILLAREA, OnFillArea)
	ON_COMMAND(ID_EDIT_UNIT_CREATION_INFO, OnEditUnitCreationInfo)
	ON_COMMAND(ID_ADD_START_COMMANDS, OnAddStartCommand)
	ON_COMMAND(ID_SHOW_FIRE_RANGE, OnShowFireRange)
	ON_COMMAND(ID_SHOW_SCENE_0, OnShowScene0)
	ON_COMMAND(ID_SHOW_SCENE_1, OnShowScene1)
	ON_COMMAND(ID_SHOW_SCENE_2, OnShowScene2)
	ON_COMMAND(ID_SHOW_SCENE_3, OnShowScene3)
	ON_COMMAND(ID_SHOW_SCENE_4, OnShowScene4)
	ON_COMMAND(ID_SHOW_SCENE_6, OnShowScene6)
	ON_COMMAND(ID_SHOW_SCENE_7, OnShowScene7)
	ON_COMMAND(ID_SHOW_SCENE_8, OnShowScene8)
	ON_COMMAND(ID_SHOW_SCENE_9, OnShowScene9)
	ON_COMMAND(ID_SHOW_SCENE_10, OnShowScene10)
	ON_COMMAND(ID_SHOW_SCENE_11, OnShowScene11)
	ON_COMMAND(ID_SHOW_SCENE_13, OnShowScene13)
	ON_COMMAND(ID_SHOW_SCENE_12, OnShowScene12)
	ON_COMMAND(ID_SHOW_SCENE_14, OnShowScene14)
	ON_COMMAND(ID_RESERVE_POSITIONS, OnReservePositions)
	ON_UPDATE_COMMAND_UI(ID_RESERVE_POSITIONS, OnUpdateReservePositions)
	ON_WM_DESTROY() 
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, OnItemchangedList1)
	ON_COMMAND(ID_EDIT_PASTE, OnObjectPaste)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_BUTTONFOG, OnButtonFog)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_COMMAND(ID_TOOLS_CLEARCASH, OnToolsClearCash)
	ON_WM_SETCURSOR()
	ON_COMMAND(ID_BUTTONAI, OnButtonAI)
	ON_COMMAND(ID_BUTTONFIT, OnButtonfit)
	ON_COMMAND(ID_BUTTOSHOWINFO, OnButtonShowLayers)
	ON_COMMAND(ID_TOOLS_DELR, OnCheckMap)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_CREATENEWPROJECT, OnFileCreateNewProject)
	ON_COMMAND(ID_BUTTONSHOWBOXES, OnButtonshowBoxes)
	ON_COMMAND(ID_OPTIONS, OnOptions)
	ON_COMMAND(ID_BUTTONSETCAMERA, OnButtonSetCameraPos)
	ON_BN_CLICKED(IDC_BUTTON1, OnButtonDeleteArea)
	ON_COMMAND(ID_SHOW_STORAGE_COVERAGE, OnShowStorageCoverage)
	ON_COMMAND(ID_NEW_TEMPLATE, OnNewTemplate)
	ON_COMMAND(ID_NEW_CONTAINER, OnNewContainer)
	ON_COMMAND(ID_NEW_GRAPH, OnNewGraph)
	ON_COMMAND(ID_NEW_FIELD, OnNewField)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNIT_CREATION_INFO, OnUpdateEditUnitCreationInfo)
	ON_UPDATE_COMMAND_UI(ID_BUTTONUPDATE, OnUpdateButtonupdate)
	ON_UPDATE_COMMAND_UI(ID_ADD_START_COMMANDS, OnUpdateAddStartCommand)
	ON_UPDATE_COMMAND_UI(ID_START_COMMANDS_LIST, OnUpdateStartCommandList)
	ON_COMMAND(ID_FILE_CREATE_RANDOM_MISSION, OnFileCreateRandomMission)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_14, OnUpdateShowScene14)
	ON_UPDATE_COMMAND_UI(ID_BUTTONFIT, OnUpdateButtonFit)
	ON_UPDATE_COMMAND_UI(ID_BUTTONFILLAREA, OnUpdateButtonfillarea)
	ON_UPDATE_COMMAND_UI(ID_BUTTONSETCAMERA, OnUpdateButtonsetcamera)
	ON_UPDATE_COMMAND_UI(ID_OPTIONS, OnUpdateOptions)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_6, OnUpdateShowScene6)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_7, OnUpdateShowScene7)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_10, OnUpdateShowScene10)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_11, OnUpdateShowScene11)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_13, OnUpdateShowScene13)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_1, OnUpdateShowScene1)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_2, OnUpdateShowScene2)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_3, OnUpdateShowScene3)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_4, OnUpdateShowScene4)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_8, OnUpdateShowScene8)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_0, OnUpdateShowScene0)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_9, OnUpdateShowScene9)
	ON_UPDATE_COMMAND_UI(ID_BUTTONAI, OnUpdateButtonai)
	ON_UPDATE_COMMAND_UI(ID_SHOW_STORAGE_COVERAGE, OnUpdateShowStorageCoverage)
	ON_UPDATE_COMMAND_UI(ID_SHOW_SCENE_12, OnUpdateShowScene12)
	ON_UPDATE_COMMAND_UI(ID_SHOW_FIRE_RANGE, OnUpdateShowFireRange)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateFileSave)
	ON_COMMAND(ID_DIPLOMACY, OnDiplomacy)
	ON_UPDATE_COMMAND_UI(ID_DIPLOMACY, OnUpdateDiplomacy)
	ON_COMMAND(ID_FILTER_COMPOSER, OnFilterComposer)
	ON_UPDATE_COMMAND_UI(ID_FILTER_COMPOSER, OnUpdateFilterComposer)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_DELR, OnUpdateToolsDelr)
	ON_COMMAND(ID_START_COMMANDS_LIST, OnStartCommandsList)
	ON_COMMAND(ID_TOOLS_OPTIONS, OnToolsOptions)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_OPTIONS, OnUpdateToolsOptions)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_RUN_GAME, OnUpdateToolsRunGame)
	ON_COMMAND(ID_TOOLS_RUN_GAME, OnToolsRunGame)
	ON_COMMAND(ID_BUTTONUPDATE, OnButtonUpdate)
	ON_UPDATE_COMMAND_UI( ID_INDICATOR_OBJECTTYPE, OnUpdateTileCoord)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_BINARY, OnUpdateFileSave)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVEMAP, OnUpdateFileSave)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVEMAP_BINARY, OnUpdateFileSave)
	ON_COMMAND(ID_FILE_SAVE_BZM, OnFileSaveBzm)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_BZM, OnUpdateFileSaveBzm)
	ON_COMMAND(ID_FILE_SAVE_XML, OnFileSaveXml)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_XML, OnUpdateFileSaveXml)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTemplateEditorFrame message handlers

void CTemplateEditorFrame::OnSetFocus(CWnd* pOldWnd) 
{
	if ( g_frameManager.GetActiveFrameType() != CFrameManager::E_TEMPLATE_FRAME )
	{
		CPtr<ICamera> pCamera = GetSingleton<ICamera>();
		pCamera->SetPlacement( VNULL3, 700, -ToRadian(90.0f + 30.0f), ToRadian(45.0f) );
	}
	g_frameManager.SetActiveFrame( this );
	SECWorksheet::OnSetFocus(pOldWnd);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::ShowFrameWindows(int nCommand)
{
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	if ( m_mapEditorBarPtr )
		theApp.ShowSECControlBar( m_mapEditorBarPtr, nCommand );

	if ( nCommand == SW_SHOW )
	{
		RECT r;
		int newX = 0;
		int newY = 0;
		GetClientRect( &r );
		g_frameManager.GetGameWnd()->SetParent( this );
		if ( ( r.right - r.left ) > GAME_SIZE_X ) { newX = ( ( r.right - r.left ) - GAME_SIZE_X ) >> 1; }
		if ( ( r.bottom - r.top ) > GAME_SIZE_Y ) { newY = ( ( r.bottom - r.top ) - GAME_SIZE_Y ) >> 1; }
		g_frameManager.GetGameWnd()->SetWindowPos( &wndTop, newX , newY, GAME_SIZE_X, GAME_SIZE_Y, SWP_SHOWWINDOW );
	}   
	else
		g_frameManager.GetGameWnd()->ShowWindow( nCommand );

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CTemplateEditorFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if ( SECWorksheet::OnCreate(lpCreateStruct) == -1 )
		return -1;
	
	// TODO: Add your specialized creation code here
	g_frameManager.SetTemplateEditorFrame( this );

	CWorldBase::Init( GetSingletonGlobal() );

	LoadMapEditorOptions();

	if ( !ucHelper.IsInitialized() )
	{
		ucHelper.Initialize();	
	}
	
	if ( !aiscHelper.IsInitialized() )
	{
		aiscHelper.Initialize();	
	}

	if ( !msHelper.IsInitialized() )
	{
		msHelper.Initialize();
	}

	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnPaint() 
{
	IScene *pScene = GetSingleton<IScene>();
	if ( pGFX && pScene )
	{
		if ( bShowStorageCoverage )
		{
			CTRect<float> rcRect = pGFX->GetScreenRect();
			CVec2 vLT, vLB, vRB, vRT;
			GetVisibilityRectBounds( rcRect, &vLT, &vRT, &vLB, &vRB );

			CSceneDrawTool storageCoverageDrawTool;
			storageCoverageDrawTool.AddAIMarkerAITiles( storageCoverageTileArray, 1, vLT * 2, vRT * 2, vLB * 2, vRB * 2 );
			storageCoverageDrawTool.DrawToScene();
		}
		else
		{
			if ( !bShowAIPassability )
			{
				CSceneDrawTool storageCoverageDrawTool;
				storageCoverageDrawTool.bAIMarkerTilesValid = true;
				storageCoverageDrawTool.DrawToScene();
			}
		}
	}

	if ( ITerrain *pTerrain = pScene->GetTerrain() )
	{
		if ( inputStates.GetActiveState() == STATE_SIMPLE_OBJECTS ) 
		{
			DrawUnitsSelection();
			DrawAIStartCommandRedLines();
			DrawReservePositionRedLines();
		}
		else if ( ( inputStates.GetActiveState() == STATE_AI_GENERAL ) ||
							( inputStates.GetActiveState() == STATE_VECTOR_OBJECTS ) ||
							( inputStates.GetActiveState() == STATE_TERRAIN ) ||
							( inputStates.GetActiveState() == STATE_SIMPLE_OBJECTS ) )
		{
			inputStates.Draw( this );
		}

		//------------------ ������ �������--------------------------------
		if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
		{
			STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

			CSceneDrawTool drawTool;
			DWORD areaColor = 0xff00ff00;
			for ( std::vector<SScriptArea>::iterator it = m_scriptAreas.begin(); it != m_scriptAreas.end(); ++it )
			{
				if ( it->eType == SScriptArea::EAT_RECTANGLE )
				{
					CVec2 center = it->center;
					CVec2 hsize = it->vAABBHalfSize;

					drawTool.DrawLine3D( CVec2( center.x - hsize.x, center.y - hsize.y ), CVec2( center.x + hsize.x, center.y - hsize.y ), areaColor, rTerrainInfo.altitudes, 16 );
					drawTool.DrawLine3D( CVec2( center.x + hsize.x, center.y - hsize.y ), CVec2( center.x + hsize.x, center.y + hsize.y ), areaColor, rTerrainInfo.altitudes, 16 );
					drawTool.DrawLine3D( CVec2( center.x + hsize.x, center.y + hsize.y ), CVec2( center.x - hsize.x, center.y + hsize.y ), areaColor, rTerrainInfo.altitudes, 16 );
					drawTool.DrawLine3D( CVec2( center.x - hsize.x, center.y + hsize.y ), CVec2( center.x - hsize.x, center.y - hsize.y ), areaColor, rTerrainInfo.altitudes, 16 );
				}
				else if ( it->eType == SScriptArea::EAT_CIRCLE )
				{
					CVec2 center = it->center;
					float r = it->fR;
					const int numAngles = 12;
					drawTool.DrawCircle3D( center, r, 32, areaColor, rTerrainInfo.altitudes, false );
				}
			}
			drawTool.DrawToScene();
		}
	}


	CPaintDC dc(this); // device context for painting

	if ( g_frameManager.GetActiveWnd() != this ) {return;} 
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER | GFXCLEAR_STENCIL );
	pGFX->BeginScene();

	/**
	if ( m_bGrid )
	{
		if ( ITerrain *pTerrain = GetSingleton<IScene>()->GetTerrain() )
			pTerrain->EnableGrid( m_bGrid ) ;
	}
	else
	{
		if ( ITerrain *pTerrain = GetSingleton<IScene>()->GetTerrain() )
			pTerrain->EnableGrid( m_bGrid ) ;
	}
/**/

	ICamera	*pCamera = GetSingleton<ICamera>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();

	if ( GetFocus() == this )
	{
		RECT r2;
		GetClientRect( &r2 );
		r2.bottom -= 2;
		r2.right -= 2;
		CBrush brA;
		brA.CreateSolidBrush( RGB( 255, 30, 30 ) ); 
		dc.FrameRect( &r2, &brA );
	}

	//------------------ ������ �������--------------------------------
	if ( ITerrain *pTerrain = pScene->GetTerrain() )
	{
		if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
		{
			STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

			pTimer->Update( timeGetTime() );
			Update( pTimer->GetGameTime() );
			pScene->Draw( pCamera );	

			pGFX->SetupDirectTransform();
			pGFX->SetShadingEffect( 3 );
			DWORD areaColor = 0xff00ff00;
			for ( std::vector<SScriptArea>::iterator it = m_scriptAreas.begin(); it != m_scriptAreas.end(); ++it )
			{
				CVec3 vPos = CVec3( it->center, 0.0f );
				CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &vPos );
				CVec2 v;  
				GetSingleton<IScene>()->GetPos2( &v, vPos );
				pGFX->DrawStringA( it->szName.c_str(), v.x, v.y, areaColor );
			}
			pGFX->RestoreTransform();
		}
	}

	pGFX->EndScene();
	pGFX->Flip();

	//--------------- ��� minimap'a-----------------------------
	if ( g_frameManager.GetMiniMapWindow() && ::IsWindow( g_frameManager.GetMiniMapWindow()->GetSafeHwnd() ) ) 
	{
		g_frameManager.GetMiniMapWindow()->UpdateScreenFrame();
	}
	//----------------------------------------------------------
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void NormalizeRange( std::vector<SMainTileDesc> *pTiles )
{
	float absLength = 0;
	for ( std::vector<SMainTileDesc>::const_iterator it = pTiles->begin(); it != pTiles->end(); ++it )
	{
		absLength += it->fProbTo - it->fProbFrom;
	}
	float curRandPos = 0;
	for ( std::vector<SMainTileDesc>::iterator it = pTiles->begin(); it != pTiles->end(); ++it )
	{
		it->fProbTo = curRandPos + ( 100.0f / float(absLength) ) * ( it->fProbTo -  it->fProbFrom );
	 	it->fProbFrom = curRandPos;
		curRandPos = it->fProbTo;	
	}	
}

int GetRandomTile( const std::vector<SMainTileDesc> &tiles )
{
	const float fRndVal = float( rand() % 10000 ) / 100.0f;
	for ( std::vector<SMainTileDesc>::const_iterator it = tiles.begin(); it != tiles.end(); ++it )
	{
		if ( (fRndVal >= it->fProbFrom) && (fRndVal <= it->fProbTo) )
			return it->nIndex;
	}
	// failed to find with required probability - return '0' tile
	return tiles[0].nIndex;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::OnSize(UINT nType, int cx, int cy) 
{
	SECWorksheet::OnSize(nType, cx, cy);
	
	// TODO: Add your message handler code here
	RECT r;
	int newX = 0;
	int newY = 0;
	GetClientRect( &r );
	if ( ( r.right - r.left ) > GAME_SIZE_X ) 
	{
		newX = ( ( r.right - r.left ) - GAME_SIZE_X ) >> 1;
	}
	if ( ( r.bottom - r.top ) > GAME_SIZE_Y ) 
	{
		newY = ( ( r.bottom - r.top ) - GAME_SIZE_Y ) >> 1;
	}
	if ( g_frameManager.GetActiveFrameType() == CFrameManager::E_TEMPLATE_FRAME ) 
	{
		g_frameManager.GetGameWnd()->SetWindowPos( &wndTop, newX , newY, GAME_SIZE_X, GAME_SIZE_Y, SWP_SHOWWINDOW );
	}

	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			ICamera *pCamera = GetSingleton<ICamera>();

			CVec3 vCamera = pCamera->GetAnchor();

			pCamera->SetAnchor( VNULL3 );
			pTimer->Update( timeGetTime() );
			Update( pTimer->GetGameTime() );
			vScreenCenter = GetScreenCenter();
			
			NormalizeCamera( &vCamera );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			const float fAddValue = 30.0f;
			inputStates.OnKeyDown( nChar, nRepCnt, nFlags, this );

			CVec3 v = pCamera->GetAnchor();

			if ( nChar == VK_LEFT ) 
			{
				v.x -= fAddValue;
				v.y -= fAddValue;
				NormalizeCamera( &v );
				RedrawWindow();
			}
			else if ( nChar == VK_RIGHT )
			{
				v.x += fAddValue;
				v.y += fAddValue;
				NormalizeCamera( &v );
				RedrawWindow();
			}
			else if ( nChar == VK_UP )
			{
				v.x -= fAddValue;
				v.y += fAddValue;
				NormalizeCamera( &v );
				RedrawWindow();
			}
			else if ( nChar == VK_DOWN )
			{
				v.x += fAddValue;
				v.y -= fAddValue;
				NormalizeCamera( &v );
				RedrawWindow();
			}
		}
	}
	SECWorksheet::OnKeyDown(nChar, nRepCnt, nFlags);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::NormalizeCamera( CVec3 *pvCamera )
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			ICamera *pCamera = GetSingleton<ICamera>();
			const STerrainInfo& rTerrainInfo = dynamic_cast<ITerrainEditor*>( pTerrain )->GetTerrainInfo();

			if ( pvCamera->x < ( 0.0f - vScreenCenter.x ) )
			{
				pvCamera->x = ( 0.0f - vScreenCenter.x );
			}
			else if ( pvCamera->x > ( rTerrainInfo.tiles.GetSizeX() * fWorldCellSize - vScreenCenter.x ) )
			{
				pvCamera->x = rTerrainInfo.tiles.GetSizeX() * fWorldCellSize - vScreenCenter.x;
			}

			if ( pvCamera->y < ( 0.0f - vScreenCenter.y ) )
			{
				pvCamera->y = ( 0.0f - vScreenCenter.y );
			}
			else if ( pvCamera->y > ( rTerrainInfo.tiles.GetSizeY() * fWorldCellSize - vScreenCenter.y ) )
			{
				pvCamera->y = rTerrainInfo.tiles.GetSizeY() * fWorldCellSize - vScreenCenter.y;
			}
			pCamera->SetAnchor( *pvCamera );

			/**
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			pCamera->SetAnchor( *pvCamera );
			pTimer->Update( timeGetTime() );
			Update( pTimer->GetGameTime() );
			bool bCameraChanged = false;
			CVec3 vCenter = GetScreenCenter();
			if ( vCenter.x < 0.0f )
			{
				pvCamera->x -= vCenter.x;
				bCameraChanged = true;
			}
			else if ( vCenter.x > (  rTerrainInfo.tiles.GetSizeX() * fWorldCellSize ) )
			{
				pvCamera->x += (  rTerrainInfo.tiles.GetSizeX() * fWorldCellSize ) - vCenter.x;
				bCameraChanged = true;
			}

			if ( vCenter.y < 0.0f )
			{
				pvCamera->y -= vCenter.y;
				bCameraChanged = true;
			}
			else if ( vCenter.y > (  rTerrainInfo.tiles.GetSizeY() * fWorldCellSize ) )
			{
				pvCamera->y += (  rTerrainInfo.tiles.GetSizeY() * fWorldCellSize ) - vCenter.y;
				bCameraChanged = true;
			}
			if ( bCameraChanged )
			{
				pCamera->SetAnchor( *pvCamera );
			}
			/**/
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CTemplateEditorFrame::OnEraseBkgnd(CDC* pDC) 
{
	// TODO: Add your message handler code here and/or call default
	RECT r;
	RECT r2;
	g_frameManager.GetGameWnd()->GetWindowRect( &r);
	ScreenToClient( &r );
	pDC->ExcludeClipRect( &r );	
	GetClientRect( &r2 );
	CBrush brA;
	brA.CreateSolidBrush( RGB(100, 100, 100)); 
	pDC->FillRect( &r2, &brA );
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnKillFocus(CWnd* pNewWnd) 
{
	SECWorksheet::OnKillFocus(pNewWnd);
	// TODO: Add your message handler code here
	RedrawWindow();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CTemplateEditorFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( message == UM_CHANGE_SHORTCUT_BAR_PAGE ) //�������� ����� ������
	{
	
		ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
		if ( terra )
		{		
			dynamic_cast<ITerrainEditor*>(terra)->SetMarker( NULL, 0 );
		}

	//	if ( m_currentMovingObjectPtr ) { m_currentMovingObjectPtr = 0; }	
	
		static_cast<CObjectPlacerState*>( inputStates.GetInputState( STATE_SIMPLE_OBJECTS ) )->ClearAllSelection( this );
		/* for delete 
		if ( m_currentMovingObjectPtrAI ) { m_currentMovingObjectPtrAI = 0; }	
		*/
		//if ( m_currentObjectForPastePtrAI ) { m_currentObjectForPastePtrAI = 0; }	
		if ( m_currentMovingObjectForPlacementPtr != 0 ) //������� �������������� ��������
		{
			RemoveObject( m_currentMovingObjectForPlacementPtr );
			m_currentMovingObjectForPlacementPtr = 0;
		}	
		
		//============== ����� �������� ��� advanced clipboard=====
		if ( m_currentMovingPasteGroupName != "" )
		{
			// ������� ������� ���������� ��������
			for( std:: vector<CPtr<IVisObj> >::iterator it = m_currentMovingObjectsForPlacementPtr.begin();
			it != m_currentMovingObjectsForPlacementPtr.end(); ++it )
			{
				RemoveObject( *it );
			}
			m_currentMovingPasteGroupName = "";
			m_currentMovingObjectsForPlacementPtr.clear();
		}

		//=========================================================
		SendMessage(  WM_USER + 5 );
		inputStates.SetActiveState( wParam );
		if ( ( wParam == STATE_VECTOR_OBJECTS ) && ( lParam != CInputStateParameter::INVALID_STATE ) )
		{
			if ( CVectorObjectsState* pVectorObjectsState = dynamic_cast<CVectorObjectsState*>( inputStates.GetInputState( STATE_VECTOR_OBJECTS ) ) )
			{
				pVectorObjectsState->SetActiveState( lParam );
			}
		}
		else if ( ( wParam == STATE_SIMPLE_OBJECTS ) && ( lParam != CInputStateParameter::INVALID_STATE ) )
		{
			if ( CSimpleObjectsState* pSimpleObjectsState = dynamic_cast<CSimpleObjectsState*>( inputStates.GetInputState( STATE_SIMPLE_OBJECTS ) ) )
			{
				pSimpleObjectsState->SetActiveState( lParam );
			}
		}
		else if ( ( wParam == STATE_TERRAIN ) && ( lParam != CInputStateParameter::INVALID_STATE ) )
		{
			if ( CTerrainState* pTerrainState = dynamic_cast<CTerrainState*>( inputStates.GetInputState( STATE_TERRAIN ) ) )
			{
				pTerrainState->SetActiveState( lParam );
			}
		}
		return 0;
	}
	if ( message == WM_USER + 2 ) //�������� ������ 
	{
		CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
	
		CTabSimpleObjectsDialog* pObjectWnd = m_mapEditorBarPtr->GetObjectWnd();
		if ( pObjectWnd )
		{
			pObjectWnd->m_imageList.DeleteAllItems();
			pObjectWnd->UpdateObjectsListStyle();

			int nImageCount = pODB->GetNumDescs();
			const SGDBObjectDesc *descPtr = pODB->GetAllDescs(); 

			std::vector<std::string> objects;
			std::unordered_map<std::string, int> objectIndices;
			
			for ( int nImageIndex = 0; nImageIndex != nImageCount; ++nImageIndex )
			{
				if ( pObjectWnd->FilterName( descPtr[nImageIndex].szPath ) )
				{
					std::string szObjectName = descPtr[nImageIndex].GetName();
					objects.push_back( szObjectName );
					objectIndices[szObjectName] = nImageIndex;
				}
			}		
			std::sort( objects.begin(), objects.end() );
			
			int nObjectsCount = 0;
			for ( std::vector<std::string>::const_iterator objectIterator = objects.begin(); objectIterator != objects.end(); ++objectIterator )
			{
				nObjectsCount = pObjectWnd->m_imageList.InsertItem( nObjectsCount, objectIterator->c_str(), pObjectWnd->objectsImageIndices[*objectIterator] );
				pObjectWnd->m_imageList.SetItemData( nObjectsCount, PackDWORD( objectIndices[*objectIterator], nObjectsCount ) );
				++nObjectsCount;
			}

			pObjectWnd->UpdateObjectsListStyle();
			if ( g_frameManager.GetMiniMapWindow() )
			{
				g_frameManager.GetMiniMapWindow()->UpdateMinimap( true );
			}
		}
		return 0;
	}
	if ( message == WM_USER + 3 ) //�������� �������� ���������� �����  
	{
		static_cast<CObjectPlacerState*>( inputStates.GetInputState( STATE_SIMPLE_OBJECTS ) )->ClearAllSelection( this ); 		

		//================ ����� �������� ��� advanced clipboard =====================
		if ( m_currentMovingPasteGroupName != "" )
		{
			// ������� ������� ���������� ��������
			for( std:: vector<CPtr<IVisObj> >::iterator it = m_currentMovingObjectsForPlacementPtr.begin();
			it != m_currentMovingObjectsForPlacementPtr.end(); ++it )
			{
				RemoveObject( *it );
			}
			m_currentMovingPasteGroupName = "";
			
			m_currentMovingObjectsForPlacementPtr.clear();
		}
		//=============================================================================
/*	for delete
		if ( m_currentMovingObjectPtrAI ) // ������� ���������
		{
			m_currentMovingObjectPtrAI->pVisObj->Select( SGVOSS_UNSELECTED );
			m_currentMovingObjectPtrAI = 0;
		}
		*/

		if ( m_currentMovingObjectForPlacementPtr != 0 ) //������� �������������� ��������
		{
			//static_cast<IScene*>( GetSingleton()->Get( SCNGRPH_SCENE_GRAPH ) )->RemoveObject( m_currentMovingObjectForPlacementPtr );
			RemoveObject( m_currentMovingObjectForPlacementPtr );
			m_currentMovingObjectForPlacementPtr = 0;
		}
	}

	if ( message == WM_USER + 4 ) //�������� ���� �  �������� ���������� �����  
	{
		if ( m_currentMovingObjectForPlacementPtr != 0 ) 
		{
			m_currentMovingObjectForPlacementPtr->SetDirection( m_mapEditorBarPtr->GetObjectWnd()->GetDefaultDirAngel() * 182.04f );
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			pTimer->Update( timeGetTime() );
			m_currentMovingObjectForPlacementPtr->Update( pTimer->GetGameTime() );
			RedrawWindow();
		}

		if ( m_currentMovingObjectPtrAI ) 
		{
			if ( m_currentMovingObjectPtrAI->pAIObj )
				GetSingleton<IAIEditor>()->TurnObject( m_currentMovingObjectPtrAI->pAIObj, m_mapEditorBarPtr->GetObjectWnd()->GetDefaultDirAngel() * 182.04f );
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			pTimer->Update( timeGetTime() );
			Update( pTimer->GetGameTime() );
			SetMapModified();
			RedrawWindow();
		}
		//���� ������������
		if ( m_currentMovingObjectsAI.size() )
		{
			for ( std::vector<SMapObject*>::iterator it = m_currentMovingObjectsAI.begin(); it != m_currentMovingObjectsAI.end(); ++it )
			{
				if ( m_objectsAI.find( *it ) != m_objectsAI.end() )
				{			
					if ( (*it)->pAIObj && GetSingleton<IAIEditor>()->GetFormationOfUnit( (*it)->pAIObj ) )
					{
						// ������������ �������� 
						IRefCount* obj = GetSingleton<IAIEditor>()->GetFormationOfUnit( (*it)->pAIObj ) ;
						GetSingleton<IAIEditor>()->TurnObject( obj, m_mapEditorBarPtr->GetObjectWnd()->GetDefaultDirAngel() * 182.04f );					
					}
					if ( (*it)->pAIObj && !GetSingleton<IAIEditor>()->GetFormationOfUnit( (*it)->pAIObj )  )
					{
						GetSingleton<IAIEditor>()->TurnObject( (*it)->pAIObj, m_mapEditorBarPtr->GetObjectWnd()->GetDefaultDirAngel() * 182.04f );
					}
					IGameTimer *pTimer = GetSingleton<IGameTimer>();
					pTimer->Update( timeGetTime() );
					Update( pTimer->GetGameTime() );
					SetMapModified();
				}
			}
			RedrawWindow();
		}
	}
	if ( message == WM_USER + 5 ) //���� �������� ���� �������
	{
		if ( dlg ) 
		{
			dlg->DestroyWindow();
			delete dlg;
			dlg = 0;
			isStartCommandPropertyActive = false;
			SetMapModified();
			/**
			for ( std::vector<CMutableUnitCreation>::iterator mutableUnitCreationIterator = m_unitCreationInfo.mutableUnits.begin(); mutableUnitCreationIterator != m_unitCreationInfo.mutableUnits.end(); ++mutableUnitCreationIterator )
			{
				if ( mutableUnitCreationIterator->mutableAviation.vAppearPoints.empty() )
				{
					mutableUnitCreationIterator->mutableAviation.vAppearPoints.push_back( VNULL3 );
				}
			}
			if ( inputStates.GetActiveState() == STATE_SOUNDS )
			{
				inputStates.Update();
			}
			/**/
		}
	}
	if ( message == WM_USER + 6 ) //���� ����������� ����� ��������
	{
		if ( isStartCommandPropertyActive )
		{
			IManipulator* pMan = dlg->GetCurrentManipulator();
			if ( pMan )
			{
				CVec3 v( VNULL3 );
				variant_t value;
				pMan->GetValue( "Position:x", &value );
				v.x = float( value ) * 2 * SAIConsts::TILE_SIZE;
				pMan->GetValue( "Position:y", &value );
				v.y = float( value ) * 2 * SAIConsts::TILE_SIZE; 
				RecalculateStartCommandRedLines( v );
				SetMapModified();
				//DrawAIStartCommandRedLines();
			}
		}

		//DrawReservePositionRedLines();
		//DrawUnitsSelection();
		//!!!!
		/**
		if ( inputStates.GetActiveState() == STATE_SOUNDS )
		{
			inputStates.Update();
		}
		else
		/**/
		{ 
			RedrawWindow();
		}
	}
	if ( message == WM_USER + 7 ) //�������� ��������� check box'�� � ��������� ������
	{
		//���� ���-�� ���� ���������������� �� ���� ��������
		
		/* for delete
		if ( m_currentMovingObjectPtrAI ) 
				m_currentMovingObjectPtrAI->pVisObj->Select( SGVOSS_UNSELECTED );
		for ( std::vector<SMapObject*>::iterator it = m_currentMovingObjectsAI.begin(); it != m_currentMovingObjectsAI.end(); ++it )
		{
			(*it)->pVisObj->Select( SGVOSS_UNSELECTED );		
		}
		m_currentMovingObjectsAI.clear();	
		m_shiftsForMovingObjectsAI.clear();	

		*/
		for ( std::unordered_map< SMapObject *, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
		{ 
			
			int tmp =  m_reinforcementGroup.GetGroupById( it->second->nScriptID );
			if ( tmp != -1 
				&& m_mapEditorBarPtr->GetGroupMngWnd()->IfIDChecked( tmp ) )
			{
				RemoveFromScene( it->first );	
			}
			else
			{
				if ( !IsInScene( it->first ) )
					AddToScene( it->first );
			}
		}
		RedrawWindow();
	}
	return SECWorksheet::WindowProc(message, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateTileCoord(CCmdUI* pCmdUI) 
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( inputStates.GetActiveState() != STATE_AI_GENERAL )
			{
				int nControlIndex = -1;
				//����� �� ������� ��� ������� � ���  ����� � ����������  ��� ��������
				if ( m_currentMovingObjectPtrAI && g_frameManager.GetActiveFrameType() == CFrameManager::E_TEMPLATE_FRAME )
				{
					nControlIndex = theApp.GetMainFrame()->m_wndStatusBar.CommandToIndex( ID_INDICATOR_OBJECTTYPE );
					CString text;
					// ��� ��������
					/**
					int numObj = GetSingleton<IScene>()->GetNumSceneObjects();
					vector< std::pair<const SGDBObjectDesc*, CVec3> > vecTmp( numObj );
					GetSingleton<IScene>()->GetAllSceneObjects( &vecTmp[0] );		
					int pos = 0;
					for ( int i = 0; i != vecTmp.size(); ++i )
					{
						if ( vecTmp[i].second == m_currentMovingObjectPtrAI->pVisObj->GetPosition() )
							pos = i;	
					}
					/**/
					CVec3 vPos = m_currentMovingObjectPtrAI->pVisObj->GetPosition();
					Vis2AI( &vPos );

					CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
					if ( SEditorObjectItem *pTMPObj = GetEditorObjectItem( m_currentMovingObjectPtrAI ) )
					{
						CGDBPtr<SObjectBaseRPGStats> pStats = dynamic_cast<const SObjectBaseRPGStats*>( pIDB->GetRPGStats( m_currentMovingObjectPtrAI->pDesc ) );
						if ( pStats )
						{
							const CArray2D<BYTE> &rArray = pStats->GetPassability();
							text.Format("Name: %s, Script ID: %d, Pos: [%.2f, %.2f], Box: [%d, %d]" , pTMPObj->sDesc.szKey.c_str(), pTMPObj->nScriptID, vPos.x / ( 2 * SAIConsts::TILE_SIZE ), vPos.y / ( 2 * SAIConsts::TILE_SIZE ), rArray.GetSizeX(), rArray.GetSizeY() );
						}
						else
						{
							text.Format("Name: %s, Script ID: %d, Pos: [%.2f, %.2f]" , pTMPObj->sDesc.szKey.c_str(), pTMPObj->nScriptID, vPos.x / ( 2 * SAIConsts::TILE_SIZE ), vPos.y / ( 2 * SAIConsts::TILE_SIZE ) );
						}
						theApp.GetMainFrame()->m_wndStatusBar.SetPaneText( nControlIndex, text );
					}
				}
				else if ( !m_currentMovingObjectsAI.empty() && 
									( g_frameManager.GetActiveFrameType() == CFrameManager::E_TEMPLATE_FRAME ) )
				{
					IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
					nControlIndex = theApp.GetMainFrame()->m_wndStatusBar.CommandToIndex( ID_INDICATOR_OBJECTTYPE );
					int nRetVal = 0;
					{
						std::set<IRefCount*> squads;
						for ( std::vector<SMapObject*>::const_iterator it = m_currentMovingObjectsAI.begin(); it != m_currentMovingObjectsAI.end(); ++it )
						{
							IRefCount* pSquad = pAIEditor->GetFormationOfUnit( ( *it )->pAIObj ) ;
							if ( pSquad )
							{
								squads.insert( pSquad );
							}
							else
							{
								++nRetVal;
							}
						}
						for( std::set< IRefCount* >::iterator it = squads.begin(); it != squads.end(); ++it )
						{
							++nRetVal;
						}
					}

					CString text;
					if ( nRetVal == 1 )
					{
						if ( SEditorObjectItem *pTMPObj = GetEditorObjectItem( m_currentMovingObjectsAI[0] ) )
						{
							if ( IRefCount* pSquad = pAIEditor->GetFormationOfUnit( m_currentMovingObjectsAI[0]->pAIObj ) )
							{
								const CVec3 vPos = CVec3( pAIEditor->GetCenter( pSquad ), 0.0f );
								const int numId = pAIEditor->GetUnitDBID( pSquad );
								const std::string szName = GetSingleton<IObjectsDB>()->GetDesc( numId )->szKey;
								text.Format("Name: %s, Script ID: %d, Pos: [%.2f, %.2f]" , szName.c_str(), pTMPObj->nScriptID, vPos.x / ( 2 * SAIConsts::TILE_SIZE ), vPos.y / ( 2 * SAIConsts::TILE_SIZE ) );
							}
						}
					}
					else
					{
						text.Format("%d objects selected", nRetVal );
					}
					theApp.GetMainFrame()->m_wndStatusBar.SetPaneText( nControlIndex, text );
				}
				else
				{
					nControlIndex = theApp.GetMainFrame()->m_wndStatusBar.CommandToIndex( ID_INDICATOR_OBJECTTYPE );
					CString text;
					text.Format("Name: no selected" );
					theApp.GetMainFrame()->m_wndStatusBar.SetPaneText( nControlIndex, text );
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FindSpaneHlp( int num, SMapObjectInfo obj )
{
	return obj.link.nLinkID == num;
}

/**
std::pair< std::string, int > FindGroupIdForObject( SLoadMapInfo::TLogicsMap &logics, int linkId )
{
	std::pair< std::string, int > retVal;
	retVal.second = 0;
	// 	typedef std::hash_multimap<std::string, std::vector<int> > TLogicsMap;
  int num = 0;
	for( std::hash_multimap<std::string, std::vector<int> >::iterator it = logics.begin(); it != logics.end(); ++it )
	{
		num++;
		for( int i = 0; i != it->second.size(); ++i )
		{
			if( it->second[i] == linkId )
			{
				retVal.second = num;
				retVal.first = it->first;			
			}
		}
	}
	return retVal;
}
/**/

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void NormalizeMapName( std::string &szSelectedFileMap, bool bTruncate )
{
	IDataStorage *pStorage = GetSingleton<IDataStorage>();

	NStr::ToLower( szSelectedFileMap );
	
	if ( szSelectedFileMap[1] != ':' )
	{
		if ( szSelectedFileMap.find( "maps\\" ) != 0 )
		{
			if ( szSelectedFileMap.find( "scenarios\\patches\\" ) == 0 )
			{
			}
			else
			{
				szSelectedFileMap = std::string( "maps\\" ) + szSelectedFileMap;
			}
		}
		szSelectedFileMap = std::string( pStorage->GetName() ) + szSelectedFileMap;
	}
	if ( bTruncate )
	{
		if ( ( szSelectedFileMap.rfind( ".bzm" ) == ( szSelectedFileMap.size() - 4 ) ) || 
				 ( szSelectedFileMap.rfind( ".xml" ) == ( szSelectedFileMap.size() - 4 ) ) )
		{
			if ( szSelectedFileMap.size() > 255 )
			{
				std::string szNewSelectedFileMap;
				szNewSelectedFileMap = szSelectedFileMap.substr( 0, 251 ) + szSelectedFileMap.substr( szSelectedFileMap.size() - 4 );
				szSelectedFileMap = szNewSelectedFileMap;
			}
		}
		else
		{
			if ( szSelectedFileMap.size() > 251 )
			{
				std::string szNewSelectedFileMap;
				szNewSelectedFileMap = szSelectedFileMap.substr( 0, 251 );
				szSelectedFileMap = szNewSelectedFileMap;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFileLoadMap( const std::string &rszFileName ) 
{
	{
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
		IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
		IScene *pScene = GetSingleton<IScene>();
		if ( !pDataStorage || !pObjectsDB || !pAIEditor || !pScene )
		{
			return;
		}
	}

	const std::string szOutputFile( "logs\\loadmap_log.txt" );

	if ( !NeedSaveChanges() )
	{
		return;
	}
	
	//������� ��� ������������������ ����
	modCollector.Collect();
	COpenMapDialog openMapDialog;
	//������� ��� ������������������ �����
	{
		const std::string szMapsFolder( "Maps\\" );
		GetEnumFilesInDataStorage( szMapsFolder, &( openMapDialog.mapNames ) );
	}

	
	if ( ( !rszFileName.empty() ) || ( openMapDialog.DoModal() == IDOK ) )
	{
		std::string szSelectedFileMap;
		if ( rszFileName.empty() )
		{
			szSelectedFileMap = openMapDialog.GetMapName();
			NormalizeMapName( szSelectedFileMap, false );
		}
		else
		{
			szSelectedFileMap = rszFileName;
		}
		NStr::ToLower( szSelectedFileMap );
		const std::string szSelectedFileMapFullName = szSelectedFileMap;
		
		//��������� �����
		CPtr<IDataStream> pStream = 0;
		{
			IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
			if ( szSelectedFileMapFullName.find( pDataStorage->GetName() ) == 0 )
			{
				const std::string szSelectedFileMapStorageName = szSelectedFileMap.substr( strlen( pDataStorage->GetName() ) );

				pStream = pDataStorage->OpenStream( szSelectedFileMapStorageName.c_str(), STREAM_ACCESS_READ );
				if ( !pStream )
				{
					if ( pStream = pDataStorage->OpenStream( ( szSelectedFileMapStorageName + std::string( ".xml" ) ).c_str(), STREAM_ACCESS_READ ) )
					{
						szSelectedFileMap += std::string( ".xml" );
					}
					else if ( pStream = pDataStorage->OpenStream( ( szSelectedFileMapStorageName + std::string( ".bzm" ) ).c_str(), STREAM_ACCESS_READ ) )
					{
						szSelectedFileMap += std::string( ".bzm" );
					}
				}
			}
			else
			{
				pStream = OpenFileStream( szSelectedFileMapFullName.c_str(), STREAM_ACCESS_READ );
				if ( !pStream )
				{
					if ( pStream = OpenFileStream( ( szSelectedFileMapFullName + std::string( ".xml" ) ).c_str(), STREAM_ACCESS_READ ) )
					{
						szSelectedFileMap += std::string( ".xml" );
					}
					else if ( pStream = OpenFileStream( ( szSelectedFileMapFullName + std::string( ".bzm" ) ).c_str(), STREAM_ACCESS_READ ) )
					{
						szSelectedFileMap += std::string( ".bzm" );
					}
				}
			}
			//NI_ASSERT_T( pStream != 0, NStr::Format("Can't open stream \"%s\" to read map", szSelectedFileMapFullName.c_str() ) );
		}
		
		bool bMapLoaded = false;
		bool bMODNotFound = false;
		bool bSomeRemoved = false;

		std::string szShortOutputString;
		std::string szOutputString;

		CMapInfo loadedMapInfo;

		std::string szOldMODKey;
		std::string szNewMODKey;
		std::string szMapMODKey;

		CProgressDialog progressDialog;

		bool bLocalLastSaveBinary = mapEditorOptions.bSaveAsBZM;
		if ( pStream )
		{
			if ( ( szSelectedFileMap.rfind( ".bzm" ) == ( szSelectedFileMap.size() - 4 ) ) || ( szSelectedFileMap.rfind( ".xml" ) == ( szSelectedFileMap.size() - 4 ) ) )
			{
				bLocalLastSaveBinary = ( szSelectedFileMap.rfind( ".bzm" ) == ( szSelectedFileMap.size() - 4 ) );
				szSelectedFileMap = szSelectedFileMap.substr( 0, szSelectedFileMap.size() - 4 );
			}
		
			progressDialog.Create( IDD_PROGRESS, this );
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.ShowWindow( SW_SHOW ); 
				progressDialog.SetWindowText( "Opening Blitzkrieg Map" );
				progressDialog.SetProgressRange( 0, 18 ); 
				progressDialog.SetProgressMessage( NStr::Format( "Opening Blitzkrieg Map: %s...", szSelectedFileMap.c_str() ) );
			}

			BeginWaitCursor();

			try
			{
				if ( bLocalLastSaveBinary )
				{
					CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
					CSaverAccessor saver = pSaver;
					saver.Add( 1, &loadedMapInfo );
					bLocalLastSaveBinary = true;
				}
				else
				{
					CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStream, IDataTree::READ );
					CTreeAccessor saver = pSaver;
					saver.AddTypedSuper( &loadedMapInfo );
					bLocalLastSaveBinary = false;
				}
				
				bMapLoaded = loadedMapInfo.IsValid();
				
				if ( bMapLoaded )
				{
					szOldMODKey = CMODCollector::GetKey( currentMapInfo.szMODName, currentMapInfo.szMODVersion );
					szMapMODKey = CMODCollector::GetKey( loadedMapInfo.szMODName, loadedMapInfo.szMODVersion );
					
					if ( rszFileName.empty() )
					{
						szNewMODKey = openMapDialog.GetMODKey();
						if ( szNewMODKey == RMGC_CURRENT_MOD_FOLDER )
						{
							szNewMODKey = szOldMODKey;
						}
						else if ( szNewMODKey == RMGC_OWN_MOD_FOLDER )
						{
							szNewMODKey = szMapMODKey;
						}
						else if ( szNewMODKey == RMGC_NO_MOD_FOLDER )
						{
							szNewMODKey.clear();
						}
					}
					else
					{
						szNewMODKey = szMapMODKey;
					}
					
					if ( ( !szNewMODKey.empty() ) &&
							 ( modCollector.availableMODs.find( szNewMODKey ) == modCollector.availableMODs.end() ) )
					{
						bMODNotFound = true;
					}
				}
			}
			catch ( ... )
			{
			}
		}
		if ( ( !bMapLoaded ) || bMODNotFound )
		{
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.DestroyWindow();
			}
			MSG msg;
			PeekMessage( &msg, GetSafeHwnd(), WM_MOUSEMOVE, WM_RBUTTONUP, PM_REMOVE );
			EndWaitCursor();

			CString strTitle;
			strTitle.LoadString( IDR_EDITORTYPE );
			if ( !bMapLoaded )
			{
				theApp.GetMainFrame()->RemoveFromRecentList( szSelectedFileMap );
				MessageBox( NStr::Format( "Can't open file: <%s>.\nFile Not found or have invalid format.",
																	szSelectedFileMapFullName.c_str() ),
										strTitle,
										MB_OK | MB_ICONEXCLAMATION );
			}
			else
			{
				MessageBox( NStr::Format( "Can't find selected MOD: <%s>. Please select any other MOD.",
																	szNewMODKey.c_str() ),
										strTitle,
										MB_OK | MB_ICONEXCLAMATION );
			}
			return;
		}
		
		ClearAllDataBeforeNewMap();

		mapEditorOptions.bSaveAsBZM = bLocalLastSaveBinary;
		currentMapInfo = loadedMapInfo;
		m_currentMapName = szSelectedFileMap;
		NStr::ToLower( m_currentMapName );
		theApp.GetMainFrame()->AddToRecentList( m_currentMapName + ( mapEditorOptions.bSaveAsBZM ? ".bzm" : ".xml" ) );

		//������ ���:
		if ( szOldMODKey != szNewMODKey )
		{
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.SetProgressRange( 0, 21 ); 
				progressDialog.IterateProgressPosition();
			}
			ClearAllBeforeNewMOD();
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.IterateProgressPosition();
			}
			if ( szNewMODKey.empty() ) 
			{
				LoadNewMOD( "" );
			}
			else
			{
				const CMODCollector::CMODNode &rMODNode = modCollector.availableMODs[szNewMODKey];
				LoadNewMOD( rMODNode.szMODFolder );
			}
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.IterateProgressPosition();
			}
			LoadAllAfterNewMOD();
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.IterateProgressPosition();
			}
		}
		else
		{
			progressDialog.IterateProgressPosition();
		}
		
		if ( szNewMODKey.empty() )
		{
			currentMapInfo.szMODName.clear();
			currentMapInfo.szMODVersion.clear();
		}
		else
		{
			const CMODCollector::CMODNode &rMODNode = modCollector.availableMODs[szNewMODKey];
			currentMapInfo.szMODName = rMODNode.szMODName;
			currentMapInfo.szMODVersion = rMODNode.szMODVersion;
		}
		
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
		IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
		IScene *pScene = GetSingleton<IScene>();

		if ( szMapMODKey != szNewMODKey )
		{
			std::string szOldMODLabel;
			std::string szNewMODLabel;

			if ( szMapMODKey.empty() )
			{
				szOldMODLabel = RMGC_NO_MOD_FOLDER;
			}
			else
			{
				szOldMODLabel = szMapMODKey;
			}
			if ( szNewMODKey.empty() )
			{
				szNewMODLabel = RMGC_NO_MOD_FOLDER;
			}
			else
			{
				szNewMODLabel = szNewMODKey;
			}

			szShortOutputString = NStr::Format( "MOD was changed.\nOld MOD: %s.\nNew MOD: %s.", szOldMODLabel.c_str(), szNewMODLabel.c_str() );
			
			DWORD dwTime = ::GetTickCount();
			bSomeRemoved = currentMapInfo.RemoveNonExistingObjects( pDataStorage, pObjectsDB, &szOutputString );
			dwTime = ::GetTickCount() - dwTime;
			NStr::DebugTrace( "CMapInfo::RemoveNonExistingObjects( %s ): %d\n", szSelectedFileMapFullName.c_str(), dwTime );
			
			if ( bSomeRemoved )
			{
				szShortOutputString += NStr::Format( "\nSome objects was deleted.\nFor further information see %s%s file.", pDataStorage->GetName(), szOutputFile.c_str() );
			}
			//���������� ��� - ������� ����������
			bSomeRemoved = true;
		}

		progressDialog.IterateProgressPosition();

		currentMapInfo.UnpackFrameIndices();
		if ( ( currentMapInfo.terrain.altitudes.GetSizeX() == 0 ) || ( currentMapInfo.terrain.altitudes.GetSizeX() == 0 ) )
		{
			currentMapInfo.terrain.altitudes.SetSizes( currentMapInfo.terrain.patches.GetSizeX() * 16 + 1, currentMapInfo.terrain.patches.GetSizeY() * 16 + 1 );
			currentMapInfo.terrain.altitudes.SetZero();
		}
		CMapInfo::UpdateTerrainShades( &( currentMapInfo.terrain ),
																	 CTRect<int>( 0,
																								0,
																								currentMapInfo.terrain.altitudes.GetSizeX(),
																								currentMapInfo.terrain.altitudes.GetSizeY() ),
																								CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( currentMapInfo.nSeason ) ) );
		
		progressDialog.IterateProgressPosition();

		m_reinforcementGroup = currentMapInfo.reinforcements;
		m_reinforcementGroupCheckBoxes.groupsCheckBoxes.clear();
		for ( std::unordered_map< int, SReinforcementGroupInfo::SGroupsVector >::iterator it = m_reinforcementGroup.groups.begin(); it != m_reinforcementGroup.groups.end(); ++it )
		{ 
			m_reinforcementGroupCheckBoxes.groupsCheckBoxes.insert( std::make_pair( it->first, -1 ) );
		}
		m_entrenchments = currentMapInfo.entrenchments;
		
		m_scriptAreas = currentMapInfo.scriptAreas;
		CalculateAreasFromAI();
		
		SetSeason( currentMapInfo.nSeason );

		m_unitCreationInfo = currentMapInfo.unitCreation;
		for ( SLoadMapInfo::TReservePositionsList::const_iterator reservePositionIterator = currentMapInfo.reservePositionsList.begin();
					reservePositionIterator != currentMapInfo.reservePositionsList.end();
					++reservePositionIterator )
		{
			m_reservePositions.push_back( CMutableReservePosition( *reservePositionIterator ) );
		}
		for ( SLoadMapInfo::TStartCommandsList::const_iterator startCommandIterator = currentMapInfo.startCommandsList.begin();
					startCommandIterator != currentMapInfo.startCommandsList.end();
					++startCommandIterator )
		{
			m_startCommands.push_back( CMutableAIStartCommand( *startCommandIterator ) );
		}

		progressDialog.IterateProgressPosition();

		//�������� �  AI terrain
		pAIEditor->SetDiplomacies( currentMapInfo.diplomacies );
		pAIEditor->Init( currentMapInfo.terrain );
		pAIEditor->ToggleShow( 0 );

		progressDialog.IterateProgressPosition();

		{
			ITerrain *pTerrain = CreateTerrain();
			pTerrain->Load( szSelectedFileMapFullName.c_str(), currentMapInfo.terrain );
			pScene->SetTerrain( pTerrain );
		}

		ITerrain *pTerrain = pScene->GetTerrain();
		const STerrainInfo& rTerrainInfo = dynamic_cast<ITerrainEditor*>( pTerrain )->GetTerrainInfo();
		progressDialog.IterateProgressPosition();

		const int nSelectedSeason = currentMapInfo.GetSelectedSeason();

		std::string szRoads3DDescName = currentMapInfo.szSeasonFolder + "Roads3D\\";
		std::string szRiverDescName = currentMapInfo.szSeasonFolder + "Rivers\\";
		progressDialog.IterateProgressPosition();

		LoadDataResource( currentMapInfo.terrain.szTilesetDesc, "", false, 0, "tileset", descrTile );
		m_mapEditorBarPtr->GetTabTileEditDialog()->CreateTilesList( currentMapInfo.szSeasonFolder, CMapInfo::MOST_COMMON_TILES[nSelectedSeason] );	
		progressDialog.IterateProgressPosition();

		
		m_mapEditorBarPtr->GetRoads3DTab()->CreateVSOList( szRoads3DDescName );
		progressDialog.IterateProgressPosition();
		
		m_mapEditorBarPtr->GetRiversTab()->CreateVSOList( szRiverDescName );
		progressDialog.IterateProgressPosition();

		SetFocus();

		GRect terrainRect( 0, 0, 0, 0 );
		if ( pTerrain )
		{
			const STerrainInfo	&rTerrainInfo = dynamic_cast<ITerrainEditor*>( pTerrain )->GetTerrainInfo();
			terrainRect = GRect( 0, 0, rTerrainInfo.tiles.GetSizeX() * fWorldCellSize - 1, rTerrainInfo.tiles.GetSizeY() * fWorldCellSize - 1 );
		}		

		std::vector<SMapObjectInfo> tmpSpans;
		std::map<SMapObject*,SMapObjectInfo > m_objectsTmpMap; // �������� ��� ���� �������� SMapObjectInfo
 
		for ( std::vector<SMapObjectInfo>::iterator it = currentMapInfo.objects.begin(); it != currentMapInfo.objects.end(); ++it )
		{			
			//SMapObjectInfo testVar = *it;
			CVec3 vPos;
			AI2Vis( &vPos, it->vPos );
			if( pAIEditor->IsObjectInsideOfMap( *it ) )
			{
				if( pObjectsDB->GetDesc( it->szName.c_str() )->eGameType != SGVOGT_BRIDGE ) // ����� ���� ������� �� �������
				{
					if ( terrainRect.contains( vPos.x, vPos.y ) )				
					{
						//SMapObjectInfo tmpInfo = *it;
						SMapObject *ptr = AddObjectByAI( *it, it->nPlayer ,true );		
						m_objectsTmpMap.insert( std::make_pair( ptr, *it ) );
					}
				}
				else
				{
					tmpSpans.push_back( *it );
					/*IRefCount *pAiObject = 0;
					GetSingleton<IAIEditor>()->AddNewObject( info, &pAiObject );
					IGameTimer *pTimer = GetSingleton<IGameTimer>();
					int time = pTimer->GetGameTime( );
					Update( time );
					tmpBridgeForAdd.push_back( FindSpanByAI( pAiObject ) );*/
				}
			}
		}
		
		progressDialog.IterateProgressPosition();

		for ( std::vector<SMapObjectInfo>::iterator it = currentMapInfo.scenarioObjects.begin(); it != currentMapInfo.scenarioObjects.end(); ++it )
		{			
			//SMapObjectInfo testVar = *it;
			CVec3 vPos;
			AI2Vis( &vPos, it->vPos );
			if( pAIEditor->IsObjectInsideOfMap( *it ) )
			{
				if( pObjectsDB->GetDesc( it->szName.c_str() )->eGameType != SGVOGT_BRIDGE ) // ����� ���� ������� �� �������
				{
					if ( terrainRect.contains( vPos.x, vPos.y ) )				
					{
						//SMapObjectInfo tmpInfo = *it;
						SMapObject *ptr = AddObjectByAI( *it, it->nPlayer ,true, true );		
						m_objectsTmpMap.insert( std::make_pair( ptr, *it ) );
					}
				}
				else
				{
					tmpSpans.push_back( *it );
					/*IRefCount *pAiObject = 0;
					GetSingleton<IAIEditor>()->AddNewObject( info, &pAiObject );
					IGameTimer *pTimer = GetSingleton<IGameTimer>();
					int time = pTimer->GetGameTime( );
					Update( time );
					tmpBridgeForAdd.push_back( FindSpanByAI( pAiObject ) );*/
				}
			}
		}

		progressDialog.IterateProgressPosition();
		//----------------------------------------------------------------------------------------------------------
		// ����������� ����� ( ����� ��� ��� ������� SMapObject ���� SMapObjectInfo) �.� �������� ��� link 
		for ( std::map<SMapObject*,SMapObjectInfo >::iterator it = m_objectsTmpMap.begin(); it != m_objectsTmpMap.end(); ++it )
		{			
			SMapObjectInfo tmpInfo = it->second;

			if( it->second.link.nLinkWith != 0 )  // ��� �������� ��� ���� link 
			{
				// ������ SMapObject � ����� linkId 
				for ( std::map<SMapObject*,SMapObjectInfo >::iterator it2 = m_objectsTmpMap.begin(); it2 != m_objectsTmpMap.end(); ++it2 )
				{
					if( it2->second.link.nLinkID == it->second.link.nLinkWith &&  it2->first != it->first )
					{
						//����� ������� SMapObject �������� ��� link 
						GetEditorObjectItem( it->first )->pLink =  it2->first;
	
						IRefCount* pFormation = pAIEditor->GetFormationOfUnit( it->first->pAIObj  );
						if( pFormation )
						{
							// ������ ���� �������� �� ������ ��������� � ����� �����
							IRefCount **pUnits;
							int nLength;
							pAIEditor->GetUnitsInFormation( pFormation, &pUnits, &nLength);	
							for( int i = 0; i != nLength; ++i )
							{
								GetEditorObjectItem( FindByAI( pUnits[i] ) )->pLink = it2->first;
								// �������� � ������ �������� ��� ( ������ � �������� ��� ��� ���������� �� ����� ������ � 0- ��  �������� )
								CVec2 vTmp = pAIEditor->GetCenter( it2->first->pAIObj );
								pAIEditor->MoveObject(  FindByAI( pUnits[i] )->pAIObj, vTmp.x - 30 , vTmp.y + 30 );
							}
						}
						break; 
					} 
				}
			}
		}		
		
		progressDialog.IterateProgressPosition();
	
		//----------------------------------------------------------------------------------------------------------
		//��������� ReservePositions:
		//��� ������ ��������� �������
		for ( TMutableReservePositionList::iterator reservePositionIterator = m_reservePositions.begin(); reservePositionIterator != m_reservePositions.end(); )
		{
			reservePositionIterator->pArtilleryObject = 0;
			reservePositionIterator->pTruckObject = 0;
			if ( reservePositionIterator->nArtilleryLinkID != 0 ) 
					 //( reservePositionIterator->nTruckLinkID != 0 ) )
			{
				//��� ������� ������������������� �������
				//������� linkID
				//���� �� squad
				// ������ SMapObject � ����� linkId 
				for ( std::map<SMapObject*,SMapObjectInfo>::iterator it2 = m_objectsTmpMap.begin(); it2 != m_objectsTmpMap.end(); ++it2 )
				{
					if ( it2->second.link.nLinkID == reservePositionIterator->nArtilleryLinkID )
					{
						IRefCount* pFormation = pAIEditor->GetFormationOfUnit( it2->first->pAIObj );
						if( !pFormation )
						{
							reservePositionIterator->pArtilleryObject = it2->first;
						}
					}
					else if ( it2->second.link.nLinkID == reservePositionIterator->nTruckLinkID )
					{
						IRefCount* pFormation = pAIEditor->GetFormationOfUnit( it2->first->pAIObj );
						if( !pFormation )
						{
							reservePositionIterator->pTruckObject = it2->first;
						}
					}
					if ( ( ( reservePositionIterator->nArtilleryLinkID != 0 ) == ( reservePositionIterator->pArtilleryObject != 0 ) ) &&
							 ( ( reservePositionIterator->nTruckLinkID != 0 ) == ( reservePositionIterator->pTruckObject != 0 ) ) )
					{
						break;
					}
				}
			}
			//���� ��� �� ������ �������, �� ������� ��������� �������
			if ( ( reservePositionIterator->pArtilleryObject == 0 ) && 
				   ( reservePositionIterator->pTruckObject == 0 ) )
			{
				 reservePositionIterator = m_reservePositions.erase( reservePositionIterator );		
			}
			else
			{
				 ++reservePositionIterator;		
			}
		}
		
		progressDialog.IterateProgressPosition();
		//----------------------------------------------------------------------------------------------------------
		//��������� StartCommands:
		//��� ������ ��������� �������
		for ( TMutableAIStartCommandList::iterator startCommandIterator = m_startCommands.begin(); startCommandIterator != m_startCommands.end(); )
		{
			//��� ������� ������������������� �������
			//bool isAllPresent = true;
			startCommandIterator->pMapObjects.clear();
			for ( std::vector<int>::iterator mapObjectIterator = startCommandIterator->unitLinkIDs.begin(); mapObjectIterator	!= startCommandIterator->unitLinkIDs.end(); ++mapObjectIterator )
			{
				//���� ��� �� �������
				if ( (*mapObjectIterator) != 0 )
				{
					// ������ SMapObject � ����� linkId 
					for ( std::map<SMapObject*,SMapObjectInfo>::iterator it2 = m_objectsTmpMap.begin(); it2 != m_objectsTmpMap.end(); ++it2 )
					{
						if( it2->second.link.nLinkID == (*mapObjectIterator) )
						{
							IRefCount* pFormation = pAIEditor->GetFormationOfUnit( it2->first->pAIObj );
							if( pFormation )
							{
								IRefCount **pUnits;
								int nLength;
								pAIEditor->GetUnitsInFormation( pFormation, &pUnits, &nLength );	
								for( int index = 0; index < nLength; ++index )
								{
									startCommandIterator->pMapObjects.push_back( FindByAI( pUnits[index] ) );
								}
							}
							else
							{
								startCommandIterator->pMapObjects.push_back( it2->first );
							}
							break;
						}
					}
				}
			}
			startCommandIterator->unitLinkIDs.clear();
			//���� ��� �� ������ �������, �� ������� ��������� �������
			if ( !startCommandIterator->pMapObjects.empty() )
			{
				 ++startCommandIterator;		
			}
			else
			{
				 startCommandIterator = m_startCommands.erase( startCommandIterator );		
			}
		}
		progressDialog.IterateProgressPosition();
		//----------------------------------------------------------------------------------------------------------
		//-----------------��������� ��������� ������ -------------------------------------------------------------- 
		//					std::pair< std::string, int > FindGroupIdForObject( SLoadMapInfo::TLogicsMap &logics, int linkId )
		/**
		for ( std::map<SMapObject*,SMapObjectInfo >::iterator it = m_objectsTmpMap.begin(); it != m_objectsTmpMap.end(); ++it )
		{
				// �� ������ 
				SMapObject* pMapObject = it->first;
				if ( pMapObject )
				{
					if( !GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj ) )
					{
						GetEditorObjectItem( it->first )->nLogicGroupId = FindGroupIdForObject( logics, m_objectsTmpMap[ it->first ].link.nLinkID ).second;
						GetEditorObjectItem( it->first )->szBehavior = FindGroupIdForObject( logics, m_objectsTmpMap[ it->first ].link.nLinkID ).first;				
					}
					else
					{
						// ��� ��� ���� ������� 
						// m_objectsTmpMap -  ��� ���������� ������ ���� �������� �� ������� ������ ( �� ���������� )
						//
						IRefCount* pFormation = GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj  );					
						IRefCount **pUnits;
						int nLength;
						GetSingleton<IAIEditor>()->GetUnitsInFormation( pFormation, &pUnits, &nLength);	
						for( int i = 0; i != nLength; ++i )
						{
							GetEditorObjectItem( FindByAI( pUnits[i] ) )->nLogicGroupId = FindGroupIdForObject( logics, m_objectsTmpMap[ it->first ].link.nLinkID ).second;
							GetEditorObjectItem( FindByAI( pUnits[i] ) )->szBehavior = FindGroupIdForObject( logics, m_objectsTmpMap[ it->first ].link.nLinkID ).first;
						}
					}
				}
		}
		/**/
		//----------------------------------------------------------------------------------------------------------

		m_Spans.clear();
		IGameTimer *pTimer = GetSingleton<IGameTimer>();
		for( int i = 0; i != currentMapInfo.bridges.size(); ++i )
		{
			std::vector< CPtr<SBridgeSpanObject> > tmpBridgeForAdd;
			for( int j = 0; j != currentMapInfo.bridges[i].size(); ++j )
			{
				int bridgeId = currentMapInfo.bridges[i][j];
				std::vector<SMapObjectInfo>::iterator it = std::find_if( tmpSpans.begin(), tmpSpans.end(), [bridgeId](const SMapObjectInfo& obj){ return FindSpaneHlp(bridgeId, obj); } )				;
				if( it != tmpSpans.end() )
				{
					bool bFutureBuild = false;
					if ( it->fHP < 0 )
					{
						it->fHP = 1.0f;
						bFutureBuild = true;
					}
					IRefCount *pAiObject = 0;
					pAIEditor->AddNewObject( *it, &pAiObject );
					int time = pTimer->GetGameTime( );
					Update( time );
					CPtr<SBridgeSpanObject> pSpan = FindSpanByAI( pAiObject );
					if ( bFutureBuild )
					{
						pSpan->SetSpecular( 0xFF0000FF );
						pSpan->SetHP( -( pSpan->GetMaxHP() ) );
					}
					tmpBridgeForAdd.push_back( pSpan );
				}
			}
			m_Spans.push_back( tmpBridgeForAdd );
		}

		progressDialog.IterateProgressPosition();

		// turn warfog off
		while ( pScene->ToggleShow( SCENE_SHOW_WARFOG ) == true ){}

		//-------------�������� �������� ������------------
		m_mapEditorBarPtr->GetObjectWnd()->FillPlayers();
		MakeCamera();
		CVec3 vCamera( VNULL3 );
		if ( currentMapInfo.playersCameraAnchors.empty() )
		{
			vCamera = currentMapInfo.vCameraAnchor;
		}
		else
		{
			vCamera = currentMapInfo.playersCameraAnchors[0];
		}
		
		{
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			ICamera *pCamera = GetSingleton<ICamera>();

			pCamera->SetAnchor( VNULL3 );
			pTimer->Update( timeGetTime() );
			Update( pTimer->GetGameTime() );
			vScreenCenter = GetScreenCenter();
		}

		NormalizeCamera( &vCamera );
		//---------------------------------------------------

		{
			IVisObjBuilder* pVisObjBuilder = GetSingleton<IVisObjBuilder>();
			IScene *pScene = GetSingleton<IScene>();
			for( TMapSoundInfoList::const_iterator mapSoundInfoIterator = currentMapInfo.soundsList.begin(); mapSoundInfoIterator != currentMapInfo.soundsList.end(); ++mapSoundInfoIterator )
			{
				mapSoundInfo.push_back( CMutableMapSoundInfo( *mapSoundInfoIterator ) );
				
				mapSoundInfo[mapSoundInfo.size() - 1].pVisObj = pVisObjBuilder->BuildObject( "editor\\service\\Sound\\1", 0, SGVOT_MESH );
				pScene->AddObject( mapSoundInfo[mapSoundInfo.size() - 1].pVisObj, SGVOGT_UNIT );
				pScene->MoveObject( mapSoundInfo[mapSoundInfo.size() - 1].pVisObj, mapSoundInfo[mapSoundInfo.size() - 1].vPos );
			}
		}

		CalculateReinforcementGroups();	
		SendMessage( WM_USER + 7 );

		// �������� �����
		CalculateTrenchToAI();

		if ( isReservePositionActive )
		{
			m_CurentReservePosition.vPos = VNULL2;
			m_CurentReservePosition.pArtilleryObject = 0;
			m_CurentReservePosition.pTruckObject = 0;
			//DrawReservePositionRedLines();
		}

		/**
		if ( CTabSoundsDialog *pTabSoundsDialog = m_mapEditorBarPtr->GetSoundsTab() )
		{
			pTabSoundsDialog->SetCircleSound( currentMapInfo.szForestCircleSounds );
			pTabSoundsDialog->SetAmbientSound( currentMapInfo.szForestAmbientSounds );

			//if ( m_mapEditorBarPtr->GetSoundsTab()->IsDlgButtonChecked( m_mapEditorBarPtr->GetSoundsTab()->vID[3] ) )
			//{
			//	FillCreatedSounds();
			//}
		}
		/**/

		progressDialog.IterateProgressPosition();

		RemoveAddedMapSoundInfo();
		for ( int nIndex = 0; nIndex < 3; ++nIndex )
		{
			addedMapSoundInfo[nIndex].clear();
			TMapSoundInfoList soundsList;
			currentMapInfo.AddSounds( &soundsList, ( 1 << nIndex ) );
			for( TMapSoundInfoList::const_iterator mapSoundInfoIterator = soundsList.begin(); mapSoundInfoIterator != soundsList.end(); ++mapSoundInfoIterator )
			{
				addedMapSoundInfo[nIndex].push_back( CMutableMapSoundInfo( *mapSoundInfoIterator ) );
			}
		}
		FillAddedMapSoundInfo();

		//UpdateObjectsZ( CTRect<int>( 0, 0, rTerrainInfo.altitudes.GetSizeX(), rTerrainInfo.altitudes.GetSizeY() ) );

		currentMapInfo.terrain.patches.Clear();
		currentMapInfo.terrain.tiles.Clear();
		currentMapInfo.terrain.altitudes.Clear();
		//
		//currentMapInfo.terrain.roads.clear();
		currentMapInfo.terrain.roads3.clear();
		currentMapInfo.terrain.rivers.clear();
		//
		currentMapInfo.objects.clear();
		currentMapInfo.scenarioObjects.clear();
		currentMapInfo.entrenchments.clear();
		currentMapInfo.bridges.clear();
		currentMapInfo.reinforcements.groups.clear();
		currentMapInfo.scriptAreas.clear();
		currentMapInfo.startCommandsList.clear();
		currentMapInfo.reservePositionsList.clear();
		currentMapInfo.soundsList.clear();

		CalculateAreas();

		progressDialog.IterateProgressPosition();
		if ( progressDialog.GetSafeHwnd() != 0 )
		{
			progressDialog.DestroyWindow();
		}
		
		m_mapEditorBarPtr->GetToolsTab()->UpdateControls();
		m_mapEditorBarPtr->GetGroupMngWnd()->UpdateControls();
		m_mapEditorBarPtr->GetShade()->UpdateControls();

		if ( g_frameManager.GetMiniMapWindow() )
		{
			g_frameManager.GetMiniMapWindow()->UpdateMinimap( true );
		}

		MSG msg;
		PeekMessage( &msg, GetSafeHwnd(), WM_MOUSEMOVE, WM_RBUTTONUP, PM_REMOVE );
		EndWaitCursor();
		
		SetMapModified( bSomeRemoved );
		inputStates.Enter();
		if ( bSomeRemoved )
		{
			if ( !szShortOutputString.empty() )
			{
				std::string szTextPath = std::string( pDataStorage->GetName() ) + szOutputFile;
				if ( CPtr<IDataStream> pFileStream = CreateFileStream( szTextPath.c_str(), STREAM_ACCESS_WRITE ) )
				{
					pFileStream->Write( szOutputString.c_str(), szOutputString.size() );
				}

				CString strTitle;
				strTitle.LoadString( IDR_EDITORTYPE );
				MessageBox( szShortOutputString.c_str(), strTitle, MB_OK | MB_ICONINFORMATION );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::ShowFireRange( bool isShow )
{
	if ( CMainFrame* pMainFrame = theApp.GetMainFrame() )
	{
		if ( IAILogic* pAILogic = GetSingleton<IAILogic>() )
		{
			if ( !isShow )
			{
				if( pMainFrame->nFireRangeRegisterGroup != ( -1 ) )
				{
					pAILogic->ShowAreas( pMainFrame->nFireRangeRegisterGroup, ACTION_NOTIFY_RANGE_AREA, false );
					pAILogic->UnregisterGroup( pMainFrame->nFireRangeRegisterGroup );
					pMainFrame->nFireRangeRegisterGroup = -1;
				}
			}
			else
			{
				int currentSel = pMainFrame->pwndFireRangeFilterComboBox->GetCurSel();
				if ( currentSel != CB_ERR  )
				{
					CString currentFilter;
					pMainFrame->pwndFireRangeFilterComboBox->GetLBText( currentSel, currentFilter );	
					std::vector<IRefCount*> pAIObjects;
					if ( currentFilter.Compare( CSetupFilterDialog::SELECTED_UNITS ) == 0 )
					{
						if ( m_currentMovingObjectPtrAI && m_currentMovingObjectPtrAI->pAIObj )
						{
							if ( m_currentMovingObjectPtrAI->IsHuman() || m_currentMovingObjectPtrAI->IsTechnics() )
							{
								pAIObjects.push_back( m_currentMovingObjectPtrAI->pAIObj );
							}
						}
						else if ( !m_currentMovingObjectsAI.empty() )
						{
							for ( int nObjectsIndex = 0; nObjectsIndex < m_currentMovingObjectsAI.size(); ++nObjectsIndex )
							{
								if ( m_currentMovingObjectsAI[nObjectsIndex] && m_currentMovingObjectsAI[nObjectsIndex]->pAIObj )
								{
									if ( m_currentMovingObjectsAI[nObjectsIndex]->IsHuman() || m_currentMovingObjectsAI[nObjectsIndex]->IsTechnics() )
									{
										pAIObjects.push_back( m_currentMovingObjectsAI[nObjectsIndex]->pAIObj );
									}
								}
							}
						}
					}
					else
					{
						const TFilterHashMap &rAllFilters = m_mapEditorBarPtr->GetObjectWnd()->m_allFilters;
						TFilterHashMap::const_iterator filterIterator = rAllFilters.find( std::string( currentFilter ) );
						if ( filterIterator != rAllFilters.end() )
						{
							for ( std::unordered_map< SMapObject *, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
							{ 
								if( it->first->IsHuman() || it->first->IsTechnics() )
								{
									std::string szName = it->first->pDesc->szPath;
									NStr::ToLower( szName );
									if ( filterIterator->second.Check( szName ) )
									{
										pAIObjects.push_back( it->first->pAIObj );
									}
								}
							}
						}
					}
					if ( pMainFrame->nFireRangeRegisterGroup != ( -1 ) )
					{
						//ShowFireRange( false );
						pAILogic->ShowAreas( pMainFrame->nFireRangeRegisterGroup, ACTION_NOTIFY_RANGE_AREA, false );
						pAILogic->UnregisterGroup( pMainFrame->nFireRangeRegisterGroup );
						pMainFrame->nFireRangeRegisterGroup = -1;
					}
					if ( !pAIObjects.empty() )
					{
						pMainFrame->nFireRangeRegisterGroup = pAILogic->GenerateGroupNumber();
						pAILogic->RegisterGroup( &( pAIObjects[0] ), pAIObjects.size(), pMainFrame->nFireRangeRegisterGroup );
						pAILogic->ShowAreas( pMainFrame->nFireRangeRegisterGroup, ACTION_NOTIFY_SHOOT_AREA, true );
					}
				}
			}
			RedrawWindow();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnDestroy() 
{
	SaveMapEditorOptions();
	ClearAllDataBeforeNewMap();
	SECWorksheet::OnDestroy();
} 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CTemplateEditorFrame::GetCurrentDirection()
{
	
		ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
		if ( terra )
		{	
	//		int x1,x2,y1,y2;		
	//		(dynamic_cast< ITerrainEditor* >(terra))->GetTileIndex( CVec3( m_firstPoint.x, m_firstPoint.y, 0 ), &x1, &y1 );
	//		(dynamic_cast< ITerrainEditor* >(terra))->GetTileIndex( CVec3( m_lastPoint.x, m_lastPoint.y, 0 ), &x2, &y2 );
			
			return ( abs( m_firstPoint.x - m_lastPoint.x ) < abs( m_firstPoint.y -  m_lastPoint.y ) )?0:1;
		}
		return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	// TODO: Add your control notification handler code here
	if ( pNMListView->uChanged == LVIF_STATE && pNMListView->uNewState == 3 )
	{
	
	}
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::OnObjectPaste() 
{
	inputStates.OnObjectPaste( this );
	RedrawWindow();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::OnEditCopy() 
{
/**
		m_currentObjectForPastePtrAI = m_currentMovingObjectPtrAI;
		m_currentForPasteObjectsAI.clear();
		m_shiftsForPasteObjectsAI.clear();

	// ���� ������������
	if ( m_currentMovingObjectsAI.size() )
	{
			//�������� ������� � �������� ����� ����
			CVec3 v = m_currentMovingObjectsAI[0]->pVisObj->GetPosition();
			for ( std::vector<SMapObject*>::iterator it = m_currentMovingObjectsAI.begin(); it != m_currentMovingObjectsAI.end(); ++it )
			{
				if ( (*it)->pAIObj && ! GetSingleton<IAIEditor>()->GetFormationOfUnit( (*it)->pAIObj  ) )
				{
					CVec3 tmpVec = (*it)->pVisObj->GetPosition();
					m_shiftsForPasteObjectsAI.insert( std::make_pair( *it, tmpVec - v ) );
					m_currentForPasteObjectsAI.push_back( *it );
				}
			}	
	}
/**/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IVisObj* CTemplateEditorFrame::AddObject( const SGDBObjectDesc &desc, int p, bool temp )
{
	IVisObj *pVisObj = 0;
	if ( desc.eGameType == SGVOGT_SQUAD )
	{
		if ( IObjectsDB *pIDB = GetSingleton<IObjectsDB>() )
		{
			if ( const SSquadRPGStats* pSquadRPGStats = NGDB::GetRPGStats<SSquadRPGStats>( pIDB, &desc ) )
			{
				if ( !pSquadRPGStats->members.empty() )
				{
					if ( const SGDBObjectDesc *pDesc = pIDB->GetDesc( pSquadRPGStats->members[0]->szParentName.c_str() ) )
					{
						pVisObj = pVOB->BuildObject( ( pDesc->szPath + "\\1" ).c_str(), 0, pDesc->eVisType );
						if ( pVisObj != 0 )
						{
							SEditorObjectItem tmpItem;
							tmpItem.sDesc = desc;
							tmpItem.nPlayer = p;
							m_objects.insert( std::make_pair( pVisObj, tmpItem ) );
						}
					}
				}
			}
		}
	}
	else
	{
		pVisObj = pVOB->BuildObject( ( desc.szPath + "\\1" ).c_str(), 0, desc.eVisType );
		if ( pVisObj != 0 )
		{
			SEditorObjectItem tmpItem;
			tmpItem.sDesc = desc;
			tmpItem.nPlayer = p;
			m_objects.insert( std::make_pair( pVisObj, tmpItem ) );
		}
	}
	return pVisObj;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// �������� �� IVisObj ���� ��� ��������� ������� ( AI � ��� �� ����� ) 
void CTemplateEditorFrame::RemoveObject(IVisObj *object)
{
	GetSingleton<IScene>()->RemoveObject( object );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::RemoveObject(SMapObject *object)
{
	if ( m_objectsAI.find( object ) != m_objectsAI.end()  )
	{
		if ( IGameTimer *pTimer = GetSingleton<IGameTimer>() )
		{
			if ( IAIEditor *pAIEditor = GetSingleton<IAIEditor>() )
			{
				if ( object->pAIObj )
				{
					if ( object->fHP < 1.0f )
					{
						float hpAdded = object->fHP - 1.0f;
						hpAdded *= object->pRPG->fMaxHP;
						
						pAIEditor->DamageObject( object->pAIObj, hpAdded );
						pTimer->Update( timeGetTime() );
						Update( pTimer->GetGameTime() );
					}
				}		

				// ����� ���������� �������� ����� ��� undo ���� �������� 
				// �������������� unit'a�
				if ( object == m_currentMovingObjectPtrAI )
				{
					m_currentMovingObjectPtrAI = 0;
				}
				if ( object->pAIObj )
				{
					pAIEditor->DeleteObject( object->pAIObj );
				}
				else
				{
					RemoveMapObj( object );
				}
				pTimer->Update( timeGetTime() );
				Update( pTimer->GetGameTime() );
				if ( GetEditorObjectItem( object ) )
				{
					delete GetEditorObjectItem( object );
				}

				RemoveObjectFromAIStartCommand( object );
				RemoveObjectFromReservePositions( object );

				m_objectsAI.erase( object );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnButtonFog() 
{
	// TODO: Add your command handler code here
	GetSingleton<IScene>()->ToggleShow( SCENE_SHOW_WARFOG );
	RedrawWindow();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFileNewMap() 
{
	{
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
		IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
		IScene *pScene = GetSingleton<IScene>();
		if ( !pDataStorage || !pObjectsDB || !pAIEditor || !pScene )
		{
			return;
		}
	}

	if ( !NeedSaveChanges() )
	{
		return;
	}

	modCollector.Collect();
	CNewMapDialog newMapDialog;
	if ( newMapDialog.DoModal() == IDOK )
	{
		int nSelectedSizeX = newMapDialog.GetSizeX();
		int nSelectedSizeY = newMapDialog.GetSizeY();
		bool bSquareMap	= newMapDialog.IsSquareMap();
		if ( bSquareMap )
		{
			nSelectedSizeY = nSelectedSizeX;
		}
		int nSelectedSeason = newMapDialog.GetSeason();
		std::string szSelectedFileMap = newMapDialog.GetMapName();
		NormalizeMapName( szSelectedFileMap, true );

		if ( ( szSelectedFileMap.rfind( ".bzm" ) == ( szSelectedFileMap.size() - 4 ) ) || ( szSelectedFileMap.rfind( ".xml" ) == ( szSelectedFileMap.size() - 4 ) ) )
		{
			mapEditorOptions.bSaveAsBZM = ( szSelectedFileMap.rfind( ".bzm" ) == ( szSelectedFileMap.size() - 4 ) );
			szSelectedFileMap = szSelectedFileMap.substr( 0, szSelectedFileMap.size() - 4 );
		}
		m_currentMapName = szSelectedFileMap;
		NStr::ToLower( m_currentMapName );
		
		CProgressDialog progressDialog;
		progressDialog.Create( IDD_PROGRESS, this );
		if ( progressDialog.GetSafeHwnd() != 0 )
		{
			progressDialog.ShowWindow( SW_SHOW ); 
			progressDialog.SetWindowText( "Creating Blitzkrieg Map" );
			progressDialog.SetProgressRange( 0, 10 ); 
			progressDialog.SetProgressMessage( NStr::Format( "Creating Blitzkrieg Map: %s...", m_currentMapName.c_str() ) );
		}

		BeginWaitCursor();

		const std::string szOldMODKey = CMODCollector::GetKey( currentMapInfo.szMODName, currentMapInfo.szMODVersion );
		std::string szNewMODKey = newMapDialog.GetMODKey();
		if ( szNewMODKey == RMGC_CURRENT_MOD_FOLDER )
		{
			szNewMODKey = szOldMODKey;
		}

		ClearAllDataBeforeNewMap();
		
		//������ ���:
		if ( szOldMODKey != szNewMODKey )
		{
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.SetProgressRange( 0, 15 ); 
				progressDialog.IterateProgressPosition();
			}
			ClearAllBeforeNewMOD();
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.IterateProgressPosition();
			}
			if ( szNewMODKey.empty() ) 
			{
				LoadNewMOD( "" );
			}
			else
			{
				const CMODCollector::CMODNode &rMODNode = modCollector.availableMODs[szNewMODKey];
				LoadNewMOD( rMODNode.szMODFolder );
			}
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.IterateProgressPosition();
			}
			LoadAllAfterNewMOD();
			if ( progressDialog.GetSafeHwnd() != 0 )
			{
				progressDialog.IterateProgressPosition();
			}
		}
		else
		{
			progressDialog.IterateProgressPosition();
		}
		
		if ( szNewMODKey.empty() )
		{
			currentMapInfo.szMODName.clear();
			currentMapInfo.szMODVersion.clear();
		}
		else
		{
			const CMODCollector::CMODNode &rMODNode = modCollector.availableMODs[szNewMODKey];
			currentMapInfo.szMODName = rMODNode.szMODName;
			currentMapInfo.szMODVersion = rMODNode.szMODVersion;
		}
		
		IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
		IObjectsDB *pObjectsDB = GetSingleton<IObjectsDB>();
		IAIEditor *pAIEditor = GetSingleton<IAIEditor>();
		IScene *pScene = GetSingleton<IScene>();
		
		currentMapInfo.szSeasonFolder = CMapInfo::SEASON_FOLDERS[nSelectedSeason];
		currentMapInfo.nSeason = CMapInfo::REAL_SEASONS[nSelectedSeason];
		
		SetSeason( currentMapInfo.nSeason );

		currentMapInfo.terrain.szTilesetDesc = currentMapInfo.szSeasonFolder + "tileset";
		currentMapInfo.terrain.szCrossetDesc = currentMapInfo.szSeasonFolder + "crosset";
		currentMapInfo.terrain.szNoise = currentMapInfo.szSeasonFolder + "noise";
		
		currentMapInfo.terrain.patches.Clear();
		currentMapInfo.terrain.patches.SetSizes( nSelectedSizeX, nSelectedSizeY );
		for ( int x = 0; x != nSelectedSizeX; ++x )
		{
			for ( int y = 0; y != nSelectedSizeY; ++y )
			{
				STerrainPatchInfo inf;
				inf.nStartX = x * 16;
				inf.nStartY = y * 16;
				currentMapInfo.terrain.patches[y][x] = inf;
			}
		}
		progressDialog.IterateProgressPosition();

		currentMapInfo.terrain.tiles.Clear();
		currentMapInfo.terrain.tiles.SetSizes( nSelectedSizeX * 16, nSelectedSizeY * 16 );
		currentMapInfo.terrain.altitudes.SetSizes( nSelectedSizeX * 16 + 1, nSelectedSizeY * 16 + 1 );
		currentMapInfo.terrain.altitudes.SetZero();
		progressDialog.IterateProgressPosition();

		currentMapInfo.FillTerrain( CMapInfo::MOST_COMMON_TILES[nSelectedSeason] );
		progressDialog.IterateProgressPosition();

		m_unitCreationInfo = currentMapInfo.unitCreation;
		//currentMapInfo.UpdateTerrain( CTRect<int>( 0, 0, nSelectedSizeX, nSelectedSizeY ) );
		
		//init AI
		pAIEditor->SetDiplomacies( currentMapInfo.diplomacies );
		pAIEditor->Init( currentMapInfo.terrain );
		pAIEditor->ToggleShow( 0 );
		progressDialog.IterateProgressPosition();

		{
			ITerrain *pTerrain = CreateTerrain();
			pTerrain->Load( m_currentMapName.c_str(), currentMapInfo.terrain );
			pScene->SetTerrain( pTerrain );
		}

		progressDialog.IterateProgressPosition();

		ITerrain *pTerrain = pScene->GetTerrain();
		STerrainInfo& rTerrainInfo = const_cast<STerrainInfo&>( dynamic_cast<ITerrainEditor*>( pTerrain )->GetTerrainInfo() );
		{
			CMapInfo::UpdateTerrainShades( &rTerrainInfo, CTRect<int>( 0, 0, rTerrainInfo.altitudes.GetSizeX(), rTerrainInfo.altitudes.GetSizeY() ), CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( GetSeason() ) ) );
			dynamic_cast<ITerrainEditor*>( pTerrain )->Update( CTRect<int>( 0,
																												 0, 
																												 rTerrainInfo.patches.GetSizeX() - 1, 
																												 rTerrainInfo.patches.GetSizeY() - 1 ) );
		}			

		std::string szRoads3DDescName = currentMapInfo.szSeasonFolder + "Roads3D\\";
		std::string szRiverDescName = currentMapInfo.szSeasonFolder + "Rivers\\";

		progressDialog.IterateProgressPosition();

		LoadDataResource( currentMapInfo.terrain.szTilesetDesc, "", false, 0, "tileset", descrTile );
		m_mapEditorBarPtr->GetTabTileEditDialog()->CreateTilesList( currentMapInfo.szSeasonFolder, CMapInfo::MOST_COMMON_TILES[nSelectedSeason] );
		progressDialog.IterateProgressPosition();

		m_mapEditorBarPtr->GetRoads3DTab()->CreateVSOList( szRoads3DDescName );
		progressDialog.IterateProgressPosition();

		m_mapEditorBarPtr->GetRiversTab()->CreateVSOList( szRiverDescName );
		progressDialog.IterateProgressPosition();

		SetFocus();

		// turn warfog off
		while ( pScene->ToggleShow( SCENE_SHOW_WARFOG ) == true )
		{
		}

		progressDialog.IterateProgressPosition();

		//-------------�������� ��������� ������------------
		m_mapEditorBarPtr->GetObjectWnd()->FillPlayers();
		MakeCamera();
		CVec3 vCamera( VNULL3 );
		if ( currentMapInfo.playersCameraAnchors.empty() )
		{
			vCamera = currentMapInfo.vCameraAnchor;
		}
		else
		{
			vCamera = currentMapInfo.playersCameraAnchors[0];
		}

		{
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			ICamera *pCamera = GetSingleton<ICamera>();

			pCamera->SetAnchor( VNULL3 );
			pTimer->Update( timeGetTime() );
			Update( pTimer->GetGameTime() );
			vScreenCenter = GetScreenCenter();
		}
		
		NormalizeCamera( &vCamera );
		//---------------------------------------------------

		if ( isReservePositionActive )
		{
			m_CurentReservePosition.vPos = VNULL2;
			m_CurentReservePosition.pArtilleryObject = 0;
			m_CurentReservePosition.pTruckObject = 0;
			//DrawReservePositionRedLines();
		}

		m_reinforcementGroup = currentMapInfo.reinforcements;
		m_reinforcementGroupCheckBoxes.groupsCheckBoxes.clear();
		CalculateReinforcementGroups();

		CalculateAreas();
		
		progressDialog.IterateProgressPosition();

		if ( progressDialog.GetSafeHwnd() != 0 )
		{
			progressDialog.DestroyWindow();
		}
		m_mapEditorBarPtr->GetToolsTab()->UpdateControls();
		m_mapEditorBarPtr->GetGroupMngWnd()->UpdateControls();
		m_mapEditorBarPtr->GetShade()->UpdateControls();
			
		if ( g_frameManager.GetMiniMapWindow() )
		{
			g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( true );
		}

		MSG msg;
		PeekMessage( &msg, GetSafeHwnd(), WM_MOUSEMOVE, WM_RBUTTONUP, PM_REMOVE );
		EndWaitCursor();

		SetMapModified();
		inputStates.Enter();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::AddTileCmd( std::vector<STileRedoCmdInfo> &inf, bool cmd)
{
	//cmd - ��������� � ��������� �������, !cmd - ������� �����
	ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
	if ( terra )
	{
		/**
		if ( cmd )
		{
			CTileRedoCmd* tmpPtr = new CTileRedoCmd;
			tmpPtr->Init( this, inf );
			m_undoStack.push( tmpPtr );
		}
		/**/

		int x,y;

		for ( std::vector<STileRedoCmdInfo>::iterator it = inf.begin();it != inf.end(); ++it )
		{
			if ( cmd )
			{
				(dynamic_cast< ITerrainEditor* >(terra))->SetTile( it->posX, it->posY, it->newTile );	
			}
			else
			{
				(dynamic_cast< ITerrainEditor* >(terra))->SetTile( it->posX, it->posY, it->oldTile );	
			}
			x = it->posX;
			y = it->posY;
		}
		if ( !cmd )//���� AddTileCmd ������ �� undo �� update
		{
			const STerrainInfo &terrainInfo =  (dynamic_cast< ITerrainEditor* >(terra))->GetTerrainInfo();
			GRect gr( (x - 2 )  >> 4 ,(y - 2 )>> 4 , ( x + 3 ) >> 4 , ( y + 3 )>> 4  ) ;
			gr .intersect( GRect(0, 0, terrainInfo.patches.GetSizeX() - 1,terrainInfo.patches.GetSizeY() - 1) );
			CTRect<int> r( gr.left(), gr.top(), gr.right(), gr.bottom() );
			dynamic_cast<ITerrainEditor*>(terra)->Update( r );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::OnEditUndo() 
{
/**
	// TODO: Add your command handler code here
	if ( !m_undoStack.empty() )
	{
		m_undoStack.top()->Undo();
		delete m_undoStack.top();
		m_undoStack.pop();
	}
	RedrawWindow();
/**/
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::MoveObject(IVisObj *obj, CVec3 &pos, bool isFormation )
{
		if ( m_objects.find( obj ) != m_objects.end() )
		{
			GetSingleton<IScene>()->MoveObject( obj, pos );
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			pTimer->Update( timeGetTime() );
			obj->Update( pTimer->GetGameTime() );
		}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
void CTemplateEditorFrame::DeleteRoad( SRoadItem &road)
{
	if ( std::find( m_roads.begin(), m_roads.end(), road ) != m_roads.end() )
	{
		m_roads.erase( std::find( m_roads.begin(), m_roads.end(), road ) );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::AddRoad(SRoadItem &s)
{
			m_roads.push_back( s );
}
/**/

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTemplateEditorFrame::IfCashedFile( std::string name)
{

	std::string szFileName1 = std::string( GetSingleton<IDataStorage>()->GetName() ) +  std::string("editor\\cache\\") + name;
	std::string szFileName2 = std::string( GetSingleton<IDataStorage>()->GetName() ) + name;
	
	HANDLE handle1 = CreateFile(szFileName1.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL); // tmp ��������
	HANDLE handle2 = CreateFile(szFileName2.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL); // - ��� ��� ���� ���������
	
	bool ifShouldScaleAndLoad = false;
	if ( handle1 != INVALID_HANDLE_VALUE ) 
	{
		FILETIME time1;
		FILETIME time2;
		GetFileTime(handle1, &time1, NULL, NULL );
		GetFileTime(handle2, &time2, NULL, NULL );
		if ( CompareFileTime( &time1, &time2 ) == -1 ) 
		{
			ifShouldScaleAndLoad = true;
		}
	}
	else
	{
		ifShouldScaleAndLoad = true;
	}
	CloseHandle(handle1);
	CloseHandle(handle2);
	return ifShouldScaleAndLoad;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::OnToolsClearCash() 
{
	// TODO: Add your command handler code here
	std::string szFileName1 = std::string( GetSingleton<IDataStorage>()->GetName() ) +  std::string("editor\\cache");
	NFile::DeleteDirectory( szFileName1.c_str() );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CTemplateEditorFrame::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	if( m_cursorName != "" )
	{
		SetCursor( LoadCursor(0, m_cursorName ) );			//RR
		return 0;
	}
	else
	{
		return SECWorksheet::OnSetCursor(pWnd, nHitTest, message);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::GetTileIndexBy2DPoint(int x, int y, int &xtile, int &ytile)
{
	CVec3 v; 
	CVec2 p ( x, y );
	GetSingleton<IScene>()->GetPos3( &v, p );
	ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
	if ( terra )
	{
		(dynamic_cast< ITerrainEditor* >(terra))->GetTileIndex( v, &xtile, &ytile );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//������ ��������� ������������� �������
void CTemplateEditorFrame::FillGRect( GRect &r, std::vector< CTPoint<int> > &points  )
{
	for ( int i = r.top(); i != r.bottom(); ++i )
	{
		for ( int j = r.left(); j != r.right(); ++j )
		{
			points.push_back( CTPoint<int>( j, i ) );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SMapObject* CTemplateEditorFrame::AddObjectByAI(SMapObjectInfo &info, int p ,bool temp, bool bScenarioUnit )
{
	IRefCount *pAiObject = 0;
	SMapObject *pMapObj = 0;
	if ( info.fHP > 1.0f )
	{
		info.fHP = 1.0f; 
	}
	// � �������� ���� nFrameIndex == -1
	const SGDBObjectDesc* descTmp = GetSingleton<IObjectsDB>()->GetDesc( info.szName.c_str() ); 
	if ( !descTmp )
		return 0;
	int nActiveFrameIndex = info.nFrameIndex;
	if ( descTmp->eGameType  == SGVOGT_SQUAD )
	{
		info.nFrameIndex = 0;
	}

	int tmpInfoPlayer = info.nPlayer;
	if ( descTmp->eGameType != SGVOGT_BUILDING )
	{
		info.nPlayer = 0;
	}
	if( GetSingleton<IAIEditor>()->IsObjectInsideOfMap( info ) )
	{
		GetSingleton<IAIEditor>()->AddNewObject( info, &pAiObject );
		if ( descTmp->eGameType == SGVOGT_BUILDING )
		{
			GetSingleton<IAIEditor>()->SetPlayer( pAiObject, info.nPlayer );
		}
	}
	
	info.nPlayer = tmpInfoPlayer;
	

	if ( pAiObject )
	{
		if( !GetSingleton<IAIEditor>()->IsFormation( pAiObject ) )  // ������ ��������� 
		{
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			int time = pTimer->GetGameTime( );
			Update( time );
			pMapObj = FindByAI( pAiObject );
			SEditorObjectItem *tmpItem;

			switch ( descTmp->eGameType )
			{
				case SGVOGT_ENTRENCHMENT:
					tmpItem = new STrenchEditorObjectItem;
					break;
				case SGVOGT_BUILDING:
					tmpItem = new SBuildingEditorObjectItem;
					break;
				default:
					tmpItem = new SUnitEditorObjectItem;
			};

	/*if( descTmp->eGameType  != SGVOGT_ENTRENCHMENT )
				tmpItem = new SUnitEditorObjectItem;
			else
				tmpItem = new STrenchEditorObjectItem;*/
				
			if( pMapObj ) 
			{
				tmpItem->sDesc = *( pMapObj->pDesc );
				tmpItem->nPlayer = info.nPlayer;
				
				//tmpItem->szBehavior = info.szLogic;
				tmpItem->nScriptID = info.nScriptID;
				tmpItem->bScenarioUnit = bScenarioUnit;
				if ( info.nFrameIndex != -1 )
					tmpItem->frameIndex = info.nFrameIndex;
				tmpItem->pObj = pMapObj;
				tmpItem->pLink = 0; 	
				
				SetAnim( pMapObj->pVisObj, pMapObj->pDesc, 0 );

				m_objectsAI.insert( std::make_pair( pMapObj, tmpItem ) );
				/**
				if ( !temp )
				{
					CAddObjRedoCmd* tmpPtr = new CAddObjRedoCmd();
					tmpPtr->Init( this, pMapObj );
					m_undoStack.push( tmpPtr );
				}
				/**/

				if ( bScenarioUnit )
				{
					pMapObj->pVisObj->SetSpecular( 0xFF0000FF );
				}
			}
			else
			{
				delete tmpItem;
			}
		}
		else
		{
			IRefCount **pUnits;
			int nLength;
			IGameTimer *pTimer = GetSingleton<IGameTimer>();
			int time = pTimer->GetGameTime( );
			Update( time );
			GetSingleton<IAIEditor>()->GetUnitsInFormation( pAiObject, &pUnits, &nLength );	
			for( int i = 0 ; i != nLength; ++i )
			{
				pMapObj = FindByAI( pUnits[i] );
				
				SEditorObjectItem *tmpItem = new SUnitEditorObjectItem;
				tmpItem->sDesc = *( pMapObj->pDesc );
				tmpItem->nPlayer = info.nPlayer;
				//tmpItem->szBehavior = info.szLogic;
				tmpItem->nScriptID = info.nScriptID;
				tmpItem->bScenarioUnit = bScenarioUnit;
				if ( nActiveFrameIndex >= 0 )
				{
					tmpItem->frameIndex = nActiveFrameIndex;
				}
				else
				{
					tmpItem->frameIndex = 0;
				}

				tmpItem->pObj = pMapObj;
				tmpItem->pLink = 0;

				SetAnim( pMapObj->pVisObj, pMapObj->pDesc, 0 );

				m_objectsAI.insert( std::make_pair( pMapObj, tmpItem ) );

				/**
				if ( !temp )
				{
					CAddObjRedoCmd* tmpPtr = new CAddObjRedoCmd();
					tmpPtr->Init( this, pMapObj );
					m_undoStack.push( tmpPtr );
				}
				/**/
			}
		}
	}
	return pMapObj;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AddMapObjectInfo( std::vector<SMapObjectInfo> &objects, SMapObjectInfo &info )
{
	CVec2 vPos( info.vPos.x, info.vPos.y );
	Vis2AI( &vPos );
	const float fZ = GetSingleton<IAILogic>()->GetZ( vPos );
	info.vPos.x = vPos.x;
	info.vPos.y = vPos.y;

	info.vPos.z = fZ;
	AI2Vis( &info.vPos );
	//if ( fabs(info.vPos.z) < 10 ) 
		info.vPos.z = 0;
	objects.push_back( info );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::SaveMap( std::string &name, bool isBinary )
{
	theApp.GetMainFrame()->AddToRecentList( name );

	CProgressDialog progressDialog;
	progressDialog.Create( IDD_PROGRESS, this );
	if ( progressDialog.GetSafeHwnd() != 0 )
	{
		progressDialog.ShowWindow( SW_SHOW ); 
		progressDialog.SetWindowText( "Saving Blitzkrieg Map" );
		progressDialog.SetProgressRange( 0, 8 ); 
		progressDialog.SetProgressMessage( NStr::Format( "Saving Blitzkrieg Map: %s...", name.c_str() ) );
	}

	BeginWaitCursor();
	SaveReservePosition();

	ITerrain *pTerrain = GetSingleton<IScene>()->GetTerrain();
	if ( pTerrain )
	{
		CheckMap( false );

		//���������������� �����
		GetSingleton<IAIEditor>()->HandOutLinks();

		//��������� nLinkID � ReservePositions:
		//��� ������ ��������� �������
		for ( TMutableReservePositionList::iterator reservePositionIterator = m_reservePositions.begin(); reservePositionIterator != m_reservePositions.end(); )
		{
			reservePositionIterator->nArtilleryLinkID = 0;
			reservePositionIterator->nTruckLinkID = 0;
			if ( reservePositionIterator->pArtilleryObject ) //&& reservePositionIterator->pTruckObject )
			{
				//��� ������� ������������������� �������
				//������� linkID
				//���� �� squad
				if ( !GetSingleton<IAIEditor>()->GetFormationOfUnit( reservePositionIterator->pArtilleryObject->pAIObj ) )
				{
					reservePositionIterator->nArtilleryLinkID  = GetSingleton<IAIEditor>()->AIToLink( reservePositionIterator->pArtilleryObject->pAIObj );
				}
				//���� �� squad
				if ( reservePositionIterator->pTruckObject )
				{
					if ( !GetSingleton<IAIEditor>()->GetFormationOfUnit( reservePositionIterator->pTruckObject->pAIObj ) )
					{
						reservePositionIterator->nTruckLinkID  = GetSingleton<IAIEditor>()->AIToLink( reservePositionIterator->pTruckObject->pAIObj );
					}
				}
				else
				{
					reservePositionIterator->nTruckLinkID = RMGC_INVALID_LINK_ID_VALUE;
				}
			}
			//���� ��� �� ������ �������, �� ������� ��������� �������
			if ( ( reservePositionIterator->nArtilleryLinkID == RMGC_INVALID_LINK_ID_VALUE ) && 
					 ( reservePositionIterator->nTruckLinkID == RMGC_INVALID_LINK_ID_VALUE ) )
			{
				 reservePositionIterator = m_reservePositions.erase( reservePositionIterator );		
			}
			else
			{
				currentMapInfo.reservePositionsList.push_back( *reservePositionIterator );
				++reservePositionIterator;		
			}
		}
			
		progressDialog.IterateProgressPosition();

		//��������� nLinkID � StartCommands:
		//��� ������ ��������� �������
		for ( TMutableAIStartCommandList::iterator startCommandIterator = m_startCommands.begin(); startCommandIterator != m_startCommands.end(); )
		{
			//��� ������� ������������������� �������
			//bool isAllPresent = true;
			startCommandIterator->unitLinkIDs.clear();
			for ( std::list<SMapObject*>::iterator mapObjectIterator = startCommandIterator->pMapObjects.begin(); mapObjectIterator	!= startCommandIterator->pMapObjects.end(); ++mapObjectIterator )
			{
				//���� ��� �� �������
				if ( m_objectsAI.find(*mapObjectIterator) != m_objectsAI.end() )
				{
					//������� linkID
					int linkID = 0;
					//� ������ ���� ��� squad
					if ( GetSingleton<IAIEditor>()->GetFormationOfUnit( (*mapObjectIterator)->pAIObj ) )
					{
						linkID = GetSingleton<IAIEditor>()->AIToLink( GetSingleton<IAIEditor>()->GetFormationOfUnit( (*mapObjectIterator)->pAIObj ) );
					}
					//���� �� squad
					else
					{
						linkID = GetSingleton<IAIEditor>()->AIToLink( (*mapObjectIterator)->pAIObj );
					}
					//���� linkID ������
					if ( linkID != 0 )
					{
						bool isNotPresent = true;
						for ( std::vector<int>::const_iterator it = startCommandIterator->unitLinkIDs.begin(); it != startCommandIterator->unitLinkIDs.end(); ++it )
						{
							if ( (*it) == linkID )
							{
								isNotPresent = false;
								break;
							}
						}
						//� �� �� ������������ ������
						if ( isNotPresent )
						{
							//��������� ���
							startCommandIterator->unitLinkIDs.push_back( linkID );
						}
					}
				}
				//else
				//{
				//	isAllPresent = false;
				//	break;
				//}
			}
				
			//���� ��� �� ������ �������, �� ������� ��������� �������
			if ( startCommandIterator->unitLinkIDs.empty() )
			{
				 startCommandIterator = m_startCommands.erase( startCommandIterator );		
			}
			else
			{
				currentMapInfo.startCommandsList.push_back( SAIStartCommand( *startCommandIterator ) );
				++startCommandIterator;		
			}
		}

		progressDialog.IterateProgressPosition();

		std::unordered_map<IRefCount*, int, SDefaultPtrHash> squads;
		for ( std::unordered_map<SMapObject*, SEditorObjectItem*, SDefaultPtrHash>::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
		{
			if ( GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj ) )
			{	
				squads[GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj )] = it->second->frameIndex;
			}
		}

		progressDialog.IterateProgressPosition();
			
		//---------------------------------------------------------------------
		//		����� ������� ������� �� ������
		//---------------------------------------------------------------------
		CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
		for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
		{
			if ( !GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj ) )
			{
				SMapObjectInfo tmpObj;
				SEditorObjectItem *pTmp = it->second;
				tmpObj.nScriptID = it->second->nScriptID;
				//tmpObj.szLogic = it->second->szBehavior;
				tmpObj.nDir = it->first->pVisObj->GetDirection();
				tmpObj.nPlayer = it->second->nPlayer;
				tmpObj.vPos = it->first->pVisObj->GetPosition();
				tmpObj.nFrameIndex = GetAnimationFrameIndex( it->first->pVisObj );
				Vis2AI( &tmpObj.vPos );
				tmpObj.szName = it->second->sDesc.szKey;
				tmpObj.fHP = it->first->fHP;
				SMapObjectInfo::SLinkInfo link;
				link.nLinkID = GetSingleton<IAIEditor>()->AIToLink( it->first->pAIObj ); 

				// � ������� ������ ��� ����� � �� ������������ ������� �� ������� ��������� 
				if( it->second->pLink )
				{
					if( it->second->pLink->IsValid() )
					{
						link.nLinkWith = GetSingleton<IAIEditor>()->AIToLink( it->second->pLink->pAIObj );
					}
				}
				
				tmpObj.link = link;		
				if ( pTmp->bScenarioUnit )
				{
					AddMapObjectInfo( currentMapInfo.scenarioObjects, tmpObj );
				}
				else
				{
					AddMapObjectInfo( currentMapInfo.objects, tmpObj );
				}
				//currentMapInfo.objects.push_back( tmpObj );
			}
		}

		progressDialog.IterateProgressPosition();
		
		//---------------------------------------------------------------------
		//			 ������ ������ 
		//---------------------------------------------------------------------
		//CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
		for( std::unordered_map<IRefCount*, int, SDefaultPtrHash>::iterator it = squads.begin(); it != squads.end(); ++it )
		{
			// ������� ���� �� ������ ��������� �� ������ 
			IRefCount **pUnits;
			int nLength;
			GetSingleton<IAIEditor>()->GetUnitsInFormation( it->first, &pUnits, &nLength);	
			SEditorObjectItem *tmpEditiorObj =	m_objectsAI[ FindByAI( pUnits[0] ) ];

			SMapObjectInfo tmpObj;
			tmpObj.nScriptID = tmpEditiorObj->nScriptID ; 
			tmpObj.nDir = GetSingleton<IAIEditor>()->GetDir( it->first );
			tmpObj.nPlayer = tmpEditiorObj->nPlayer;
			//tmpObj.szLogic = tmpEditiorObj->szBehavior;


			tmpObj.vPos = CVec3( GetSingleton<IAIEditor>()->GetCenter( it->first ), 0.0f );
			tmpObj.nFrameIndex = 0;

			int numId = GetSingleton<IAIEditor>()->GetUnitDBID( it->first);
			tmpObj.szName = pODB->GetDesc( numId )->szKey;//FindByAI( it->first )->pDesc->szKey;//tmpEditiorObj->sDesc.szKey;
			tmpObj.fHP = FindByAI( pUnits[0] )->fHP;
			SMapObjectInfo::SLinkInfo link;
			tmpObj.nFrameIndex = it->second;
			if ( tmpObj.nFrameIndex < 0 )
			{
				tmpObj.nFrameIndex = 0;
			}
			link.nLinkID = GetSingleton<IAIEditor>()->AIToLink( it->first );

			// � ������� ������ ��� ����� � �� ������������ ������� �� ������� ��������� 
			if( tmpEditiorObj->pLink )
			{
				if( tmpEditiorObj->pLink->IsValid() )
				{
					link.nLinkWith = GetSingleton<IAIEditor>()->AIToLink( tmpEditiorObj->pLink->pAIObj );
				}
			}
			
			tmpObj.link = link;
			if ( tmpEditiorObj->bScenarioUnit )
			{
				AddMapObjectInfo( currentMapInfo.scenarioObjects, tmpObj );
			}
			else
			{
				AddMapObjectInfo( currentMapInfo.objects, tmpObj );
			}
			//currentMapInfo.objects.push_back( tmpObj );	
		}
		//---------------------------------------------------------------------
		//		�����
		//---------------------------------------------------------------------
		for( int i = 0; i != m_Spans.size(); ++i )
		{
			std::vector<int> tmpBridge;
			for( int j = 0; j != m_Spans[i].size(); ++j )
			{			
				SMapObjectInfo tmpObj;
				tmpObj.nScriptID = -1 ; 
				tmpObj.nDir = 0;
				tmpObj.nPlayer = 0;
				WORD wDir;
				m_Spans[i][j]->GetPlacement( &tmpObj.vPos, &wDir );
				Vis2AI( &tmpObj.vPos );
				tmpObj.szName = m_Spans[i][j]->GetDesc()->szKey;//FindByAI( *it )->pDesc->szKey;//tmpEditiorObj->sDesc.szKey;
				tmpObj.nFrameIndex = m_Spans[i][j]->nIndex;

				if ( m_Spans[i][j]->GetHP() > 0 )
				{
					tmpObj.fHP = 1.0f;
				}
				else
				{
					tmpObj.fHP = -1.0f;
				}

				SMapObjectInfo::SLinkInfo link;
				link.nLinkID = GetSingleton<IAIEditor>()->AIToLink( m_Spans[i][j]->pAIObj );
				tmpObj.nScriptID = GetSingleton<IAIEditor>()->GetObjectScriptID( m_Spans[i][j]->pAIObj );
				tmpObj.link = link;
				tmpBridge.push_back( link.nLinkID );
				
				const SBridgeRPGStats *pStats = NGDB::GetRPGStats<SBridgeRPGStats>( tmpObj.szName.c_str() );
				if ( pStats )
				{
					if ( ( j == 0 ) && ( pStats->direction == SBridgeRPGStats::EDirection::HORIZONTAL ) )
					{
						tmpObj.vPos.x -= 0.1f;
					}
					else if ( ( j == ( m_Spans[i].size() - 1 ) ) && ( pStats->direction == SBridgeRPGStats::EDirection::VERTICAL ) )
					{
						tmpObj.vPos.y += 0.1f;
					}
				}
				tmpObj.vPos.z = 0.0f;
				currentMapInfo.objects.push_back( tmpObj );
			}
			currentMapInfo.bridges.push_back( tmpBridge );
			tmpBridge.clear();
		}

		progressDialog.IterateProgressPosition();

		ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
		currentMapInfo.terrain = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
		CMapInfo::UpdateTerrainShades( &( currentMapInfo.terrain ),
																	 CTRect<int>( 0,
																								0,
																								currentMapInfo.terrain.altitudes.GetSizeX(),
																								currentMapInfo.terrain.altitudes.GetSizeY() ),
																	 CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( GetSeason() ) ) );

		currentMapInfo.reinforcements = m_reinforcementGroup;
		
		CalculateTrenchFromAI();
		currentMapInfo.entrenchments = m_entrenchments;
		
		CalculateAreasToAI();
		currentMapInfo.scriptAreas = m_scriptAreas;
		CalculateAreasFromAI();
		
		//currentMapInfo.nSeason = GetSeason();
		currentMapInfo.unitCreation = m_unitCreationInfo.Mutate();

		currentMapInfo.soundsList.clear();
		for( TMutableMapSoundInfoVector::iterator mutabeMapSoundInfoIterator = mapSoundInfo.begin(); mutabeMapSoundInfoIterator != mapSoundInfo.end(); ++mutabeMapSoundInfoIterator )
		{
			currentMapInfo.soundsList.push_back( mutabeMapSoundInfoIterator->Mutate() );
		}

		progressDialog.IterateProgressPosition();

		/**
		if ( CTabSoundsDialog *pTabSoundsDialog = m_mapEditorBarPtr->GetSoundsTab() )
		{
			currentMapInfo.szForestCircleSounds = pTabSoundsDialog->GetCircleSound();
			currentMapInfo.szForestAmbientSounds = pTabSoundsDialog->GetAmbientSound();

			RemoveAddedMapSoundInfo();
			for ( int nIndex = 0; nIndex < 3; ++nIndex )
			{
				addedMapSoundInfo[nIndex].clear();
				TMapSoundInfoList soundsList;
				currentMapInfo.AddSounds( &soundsList, 1 << nIndex );
				for( TMapSoundInfoList::const_iterator mapSoundInfoIterator = soundsList.begin(); mapSoundInfoIterator != soundsList.end(); ++mapSoundInfoIterator )
				{
					addedMapSoundInfo[nIndex].push_back( CMutableMapSoundInfo( *mapSoundInfoIterator ) );
				}
			}
			FillAddedMapSoundInfo();
		}
		/**/

		currentMapInfo.PackFrameIndices();

		progressDialog.IterateProgressPosition();

		SQuickLoadMapInfo quickLoadMapInfo;
		quickLoadMapInfo.FillFromMapInfo( currentMapInfo );

		CPtr<IDataStream> pStream = CreateFileStream( ( name.c_str() ), STREAM_ACCESS_WRITE );
		if ( isBinary ) 
		{
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
			CSaverAccessor saver = pSaver;
			saver.Add( 1, &currentMapInfo );
			saver.Add( RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER, &quickLoadMapInfo );
		}
		else
		{
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
			CTreeAccessor saver = pSaver;
			saver.AddTypedSuper( &currentMapInfo );
			saver.Add( RMGC_QUICK_LOAD_MAP_INFO_NAME, &quickLoadMapInfo );
		}

		progressDialog.IterateProgressPosition();

		if ( ( !currentMapInfo.playersCameraAnchors.empty() ) && ( currentMapInfo.playersCameraAnchors[0] == VNULL3 ) )
		{
			currentMapInfo.playersCameraAnchors[0] = currentMapInfo.vCameraAnchor;	
		}
		currentMapInfo.terrain.patches.Clear();
		currentMapInfo.terrain.tiles.Clear();
		currentMapInfo.terrain.altitudes.Clear();
		//
		//currentMapInfo.terrain.roads.clear();
		currentMapInfo.terrain.roads3.clear();
		currentMapInfo.terrain.rivers.clear();
		//
		currentMapInfo.objects.clear();
		currentMapInfo.scenarioObjects.clear();
		currentMapInfo.entrenchments.clear();
		currentMapInfo.bridges.clear();
		currentMapInfo.reinforcements.groups.clear();
		currentMapInfo.scriptAreas.clear();
		currentMapInfo.startCommandsList.clear();
		currentMapInfo.reservePositionsList.clear();
		currentMapInfo.soundsList.clear();

		SetMapModified( false );
		RedrawWindow();	
	}		
	
	EndWaitCursor();
	if ( progressDialog.GetSafeHwnd() != 0 )
	{
		progressDialog.DestroyWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SetIDGroup(  int id , SMapObject *obj )
{
	obj->nSelectionGroupID  = id;
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::OnButtonShowLayers() 
{
	// TODO: Add your command handler code here
	ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
	if ( terra )
	{
		ToggleSceneDepthComplexity();
		RedrawWindow();	
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::CalculateReinforcementGroups( bool update )
{
/*	m_reinforcementGroups.clear();
	for ( std::unordered_map< SMapObject *, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
	{ 
		if ( m_reinforcementGroups.find( it->first->nSelectionGroupID ) == m_reinforcementGroups.end() )
		{
			std::vector< SMapObject * > vec;
			m_reinforcementGroups.insert( std::make_pair( it->first->nSelectionGroupID, vec ) );
		}
		m_reinforcementGroups[ it->first->nSelectionGroupID ].push_back( it->first );						
	}		
	m_mapEditorBarPtr->GetGroupMngWnd()->m_groupList.ResetContent( );
	for ( std::map< int, std::vector< SMapObject * > >::iterator it = m_reinforcementGroups.begin(); it != m_reinforcementGroups.end(); ++it )
	{ 
		int num = m_mapEditorBarPtr->GetGroupMngWnd()->m_groupList.AddString( NStr::Format( "Group N:%d", it->first ) );	
		m_mapEditorBarPtr->GetGroupMngWnd()->m_groupList.SetItemData ( num , it->first );	
	}
*/
	if ( update )
	{
		m_mapEditorBarPtr->GetGroupMngWnd()->m_groupList.ResetContent();
		m_mapEditorBarPtr->GetGroupMngWnd()->m_groupInfo.ResetContent();

		for ( std::unordered_map< int, SReinforcementGroupInfo::SGroupsVector >::iterator it = m_reinforcementGroup.groups.begin(); it != m_reinforcementGroup.groups.end(); ++it )
		{ 
			int num = m_mapEditorBarPtr->GetGroupMngWnd()->m_groupList.AddString( NStr::Format( "Group N:%d", it->first ) );	
			m_mapEditorBarPtr->GetGroupMngWnd()->m_groupList.SetItemData ( num , it->first );	
			if ( m_reinforcementGroupCheckBoxes.GetCheckState( num ) )
			{
				m_mapEditorBarPtr->GetGroupMngWnd()->m_groupList.SetCheck( num, 1 );
			}
		}
	}
	else
	{
		//������� ���������
		m_reinforcementGroupCheckBoxes.groupsCheckBoxes.clear();
		for ( std::unordered_map< int, SReinforcementGroupInfo::SGroupsVector >::iterator it = m_reinforcementGroup.groups.begin(); it != m_reinforcementGroup.groups.end(); ++it )
		{ 
			if ( m_mapEditorBarPtr->GetGroupMngWnd()->IfIDChecked( it->first ) )
			{
				m_reinforcementGroupCheckBoxes.groupsCheckBoxes.insert( std::make_pair( it->first, 1 ) );
			}
			else
			{
				m_reinforcementGroupCheckBoxes.groupsCheckBoxes.insert( std::make_pair( it->first, -1 ) );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFileOpen() 
{
	// TODO: Add your command handler code here
	OnFileLoadMap( "" );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::OnFileCreateNewProject() 
{
	// TODO: Add your command handler code here
	OnFileNewMap();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::OnButtonshowBoxes() 
{
	// TODO: Add your command handler code here
	ITerrain *terra = GetSingleton<IScene>()->GetTerrain();
	if ( terra )
	{
		ToggleBoundingBoxes();
		RedrawWindow();	
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::Pick( CVec2 &point, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible, bool IsMoving )
{
	GetSingleton<IScene>()->Pick( point, ppObjects, pnNumObjects, type );
	int nRealPos = 0;

	//NStr::DebugTrace( "CTemplateEditorFrame::Pick0: %d\n", *pnNumObjects );
	
	for ( int i = 0; i != *pnNumObjects; ++i )
	{
		if ( IsExistByVis( (*ppObjects)[i].first ) && ( IsMoving || FindByVis( (*ppObjects)[i].first )->pDesc->eGameType != SGVOGT_ENTRENCHMENT )
			&& FindByVis( (*ppObjects)[i].first )->pDesc->eGameType != SGVOGT_BRIDGE )
		{
			(*ppObjects)[nRealPos] = (*ppObjects)[i];
			++nRealPos;
		}
	}
	*pnNumObjects = nRealPos;
} 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::Pick( const CTRect<float> &rcRect, std::pair<IVisObj*, CVec2> **ppObjects, int *pnNumObjects, EObjGameType type, bool bVisible, bool IsMoving )
{
	GetSingleton<IScene>()->Pick( rcRect, ppObjects, pnNumObjects, type );
	int nRealPos = 0;

	//NStr::DebugTrace( "CTemplateEditorFrame::Pick1: %d\n", *pnNumObjects );

	for ( int i = 0; i != *pnNumObjects; ++i )
	{
		if ( IsExistByVis( (*ppObjects)[i].first ) && ( IsMoving || FindByVis( (*ppObjects)[i].first )->pDesc->eGameType != SGVOGT_ENTRENCHMENT )
			&& FindByVis( (*ppObjects)[i].first )->pDesc->eGameType != SGVOGT_BRIDGE  
			&&  GetEditorObjectItem( FindByVis( (*ppObjects)[i].first ) )->pLink == 0 )
			// ��� ��� � ������ �� �������� 
		{
			(*ppObjects)[nRealPos] = (*ppObjects)[i];
			++nRealPos;
		}
	}
	*pnNumObjects = nRealPos;
} 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::MakeCamera()
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		for ( int nPlayerIndex = 0; nPlayerIndex < cameraVisObj.size(); ++nPlayerIndex )
		{
			pScene->RemoveObject( cameraVisObj[nPlayerIndex] );
			cameraVisObj[nPlayerIndex] = 0;
		}
		cameraVisObj.clear();
		cameraVisObj.resize( currentMapInfo.diplomacies.size(), 0 );
		
		if ( currentMapInfo.playersCameraAnchors.size() != ( currentMapInfo.diplomacies.size() - 1 ) )
		{
			currentMapInfo.playersCameraAnchors.resize( currentMapInfo.diplomacies.size() - 1, VNULL3 );
		}

		for ( int nPlayerIndex = 0; nPlayerIndex < currentMapInfo.diplomacies.size(); ++nPlayerIndex )
		{
			cameraVisObj[nPlayerIndex] = GetSingleton<IVisObjBuilder>()->BuildObject( "editor\\service\\camera\\1", 0, SGVOT_MESH );
			pScene->AddObject( cameraVisObj[nPlayerIndex], SGVOGT_UNIT );
			if ( nPlayerIndex < ( currentMapInfo.diplomacies.size() - 1 ) )
			{
				pScene->MoveObject( cameraVisObj[nPlayerIndex], currentMapInfo.playersCameraAnchors[nPlayerIndex] );
				cameraVisObj[nPlayerIndex]->SetSpecular( PLAYER_COLORS[nPlayerIndex] );
			}
			else
			{
				pScene->MoveObject( cameraVisObj[nPlayerIndex], currentMapInfo.vCameraAnchor );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::CalculateTrenchFromAI()
{
	m_entrenchments.clear();
	for( int i = 0; i != m_entrenchmentsAI.size() ; ++i )
	{
			SEntrenchmentInfo tmpTrench;
			// ���������� �� i - ��� ����� ( �.� �� ��� ���������  )
			for( int i2 = 0; i2 != m_entrenchmentsAI[i].size(); ++i2 )
			{
				std::vector< int >  tmpSegment;
				// ������ ���� �� �������� �.� �� ��������� 
				for( int i3 = 0; i3 != m_entrenchmentsAI[i][i2].size(); ++i3 )
				{
					tmpSegment.push_back(  GetSingleton<IAIEditor>()->AIToLink( m_entrenchmentsAI[i][i2][i3] ) );
				}
				tmpTrench.sections.push_back( tmpSegment );
			}
			m_entrenchments.push_back( tmpTrench );
	}

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::CalculateTrenchToAI()
{
	m_entrenchmentsAI.clear();
	for( int i = 0; i != m_entrenchments.size() ; ++i )
	{
		std::vector< std::vector< IRefCount* > >  tmpTrench;
			// ���������� �� i - ��� ����� ( �.� �� ��� ���������  )
			for( int i2 = 0; i2 != m_entrenchments[i].sections.size(); ++i2 )
			{
				std::vector< IRefCount* >  tmpSegment;
				// ������ ���� �� �������� �.� �� ��������� 
				for( int i3 = 0; i3 != m_entrenchments[i].sections[i2].size(); ++i3 )
				{
					tmpSegment.push_back(  GetSingleton<IAIEditor>()->LinkToAI( m_entrenchments[i].sections[i2][i3] ) );
				}
				tmpTrench.push_back( tmpSegment );
			}
			m_entrenchmentsAI.push_back( tmpTrench );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::CalculateAreas()
{
	m_mapEditorBarPtr->GetToolsTab()->m_areas.ResetContent();
	int index = 0;
	for( std::vector<SScriptArea>::iterator it = m_scriptAreas.begin(); it != m_scriptAreas.end(); ++it )
	{
		int nStringIndex = m_mapEditorBarPtr->GetToolsTab()->m_areas.InsertString( index, it->szName.c_str() );
		if ( nStringIndex >= 0 )
		{
			m_mapEditorBarPtr->GetToolsTab()->m_areas.SetItemData( nStringIndex, index );
		}
		++index;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTemplateEditorFrame::OnButtonDeleteArea() 
{
	// TODO: Add your control notification handler code here

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::CalculateAreasFromAI()
{
	for( std::vector<SScriptArea>::iterator it = m_scriptAreas.begin(); it != m_scriptAreas.end(); ++it )
	{
		CVec3 tmp( it->center.x, it->center.y, 0 );
		AI2Vis( &tmp );
		it->center.x  = tmp.x;
		it->center.y  = tmp.y;


		CVec3 tmp2( it->vAABBHalfSize.x, it->vAABBHalfSize.y, 0 );
		AI2Vis( &tmp2 );
		it->vAABBHalfSize.x  = tmp2.x;
		it->vAABBHalfSize.y  = tmp2.y;

		CVec3 v( it->fR, 0, 0 ); 
		AI2Vis( &v );
		it->fR = v.x;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::CalculateAreasToAI()
{
	for( std::vector<SScriptArea>::iterator it = m_scriptAreas.begin(); it != m_scriptAreas.end(); ++it )
	{
		CVec3 tmp( it->center.x, it->center.y, 0 );
		Vis2AI( &tmp );
		it->center.x  = tmp.x;
		it->center.y  = tmp.y;
		
		
		CVec3 tmp2( it->vAABBHalfSize.x, it->vAABBHalfSize.y, 0 );
		Vis2AI( &tmp2 );
		it->vAABBHalfSize.x  = tmp2.x;
		it->vAABBHalfSize.y  = tmp2.y;
		
		CVec3 v( it->fR, 0, 0 ); 
		Vis2AI( &v );
		it->fR = v.x;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void  CTemplateEditorFrame::FillMapInfoParamForObject( SMapObjectInfo &info, SMapObject* obj  )
{
	// !!����� - � AdvancedClipboarde ��� �������� � �������� ����������� 
	info.szName =obj->pDesc->szKey;
	info.vPos = obj->pVisObj->GetPosition( );
	info.nDir = GetSingleton<IAIEditor>()->GetDir( obj->pAIObj );
	info.nPlayer = GetEditorObjectItem( obj )->nPlayer;//obj->diplomacy;
	info.nFrameIndex = GetEditorObjectItem( obj )->frameIndex;
	info.nScriptID = GetEditorObjectItem( obj )->nScriptID;
	info.fHP = obj->fHP;
	//info.szLogic =  GetEditorObjectItem( obj )->szBehavior;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::LoadMapEditorOptions()
{
	if ( CPtr<IDataStream> pStreamOption = GetSingleton<IDataStorage>()->OpenStream( "editor\\options.xml", STREAM_ACCESS_READ ) )
	{
		CTreeAccessor treeOption = CreateDataTreeSaver( pStreamOption, IDataTree::READ );
		
		treeOption.Add( "MinMapDialogRect", &m_minimapDialogRect );
		treeOption.Add( "SavePath", &m_szSaveFilePath );
		treeOption.Add( "RecentList", &( theApp.GetMainFrame()->recentList ) ); 
		treeOption.Add( "MapEditorOptions", &mapEditorOptions );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::SaveMapEditorOptions()
{
	std::string szOptionsFileName = std::string( GetSingleton<IDataStorage>()->GetName() ) + "editor\\options.xml";
	
	if( CPtr<IDataStream> pStream = CreateFileStream( szOptionsFileName .c_str(), STREAM_ACCESS_WRITE ) )
	{
		CTreeAccessor treeOption = CreateDataTreeSaver( pStream, IDataTree::WRITE );
		
		treeOption.Add( "MinMapDialogRect", &m_minimapDialogRect );
		treeOption.Add( "SavePath", &m_szSaveFilePath );
		treeOption.Add( "RecentList", &( theApp.GetMainFrame()->recentList ) ); 
		treeOption.Add( "MapEditorOptions", &mapEditorOptions );
	}
}

void CTemplateEditorFrame::ClearAllDataBeforeNewMap()
{
	DWORD dwTime = ::GetTickCount();

	inputStates.Leave();

	/**
	while( !m_undoStack.empty() )
	{
		delete m_undoStack.top();
		m_undoStack.pop();
	}
	/**/

	if ( dlg )
	{
		dlg->DestroyWindow();
		delete dlg;
		dlg = 0;
	}

	m_entrenchments.clear(); 
	m_entrenchmentsAI.clear();
	m_scriptAreas.clear();
	m_tempSpans.clear();
	m_Spans.clear();
	m_objects.clear();
	m_currentMovingObjectForPlacementPtr = 0;
	m_pickedObjects.clear();
	isReservePositionActive = false;
	isStartCommandPropertyActive = false;
	
	for ( std::unordered_map< SMapObject *, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
	{
		if ( it->second )
		{
			delete ( it->second );
			it->second = 0;
		}
	}
	m_objectsAI.clear();

	m_currentMovingObjectPtrAI = 0;
	m_currentMovingObjectsAI.clear();
	m_shiftsForMovingObjectsAI.clear();
	m_squadsShiftsForMovingObjectsAI.clear();
	m_currentForPasteObjectsAI.clear();
	m_shiftsForPasteObjectsAI.clear();
	//m_currentObjectForPastePtrAI = 0;
	m_currentFences.clear();
	m_startCommands.clear();
	m_reservePositions.clear();
	Clear();
	GetSingleton<IAIEditor>()->Clear();

	for( TMutableMapSoundInfoVector::iterator mutabeMapSoundInfoIterator = mapSoundInfo.begin(); mutabeMapSoundInfoIterator != mapSoundInfo.end(); ++mutabeMapSoundInfoIterator )
	{
		mutabeMapSoundInfoIterator->pVisObj = 0;
	}
	for ( int nIndex = 0; nIndex < 3; ++nIndex )
	{
		for( TMutableMapSoundInfoVector::iterator mutabeMapSoundInfoIterator = addedMapSoundInfo[nIndex].begin(); mutabeMapSoundInfoIterator != addedMapSoundInfo[nIndex].end(); ++mutabeMapSoundInfoIterator )
		{
			mutabeMapSoundInfoIterator->pVisObj = 0;
		}
		addedMapSoundInfo[nIndex].clear();
	}
	mapSoundInfo.clear();
	
	storageCoverageTileArray.Clear();
	m_unitCreationInfo.units.clear();
	m_unitCreationInfo.mutableUnits.clear();
	currentMapInfo.Clear();
	//bShowScene6 = true;
	bShowScene7 = false;
	//bWireframe = false;
	bShowScene11 = true;
	//bShowScene13 = true;
	//bShowScene1 = true;
	//bShowScene2 = true;
	//bShowScene3 = false;
	//bShowScene4 = true;
	bShowScene8 = false;
	//bShowScene0 = true;
	//bShowScene9 = false;
	bShowAIPassability = false;
	bShowStorageCoverage = false;
	bFireRangePressed = false;

	dwTime = ::GetTickCount() - dwTime;
	NStr::DebugTrace( "CTemplateEditorFrame::ClearAllDataBeforeNewMap() %d ms\n", dwTime );
}

void CTemplateEditorFrame::PopFromBuilding( SEditorObjectItem *obj )
{
	if ( !obj )
	{
		return;
	}
	
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>() )
		{
			float xLocal = 0;
			float yLocal = 0;
			float fDiff = 20.0f;
			CVec2 vOldPos( 0, 0 );
			
			if( IsInWorld( obj->pObj ) )
			{
				if ( obj->sDesc.eGameType == SGVOGT_ENTRENCHMENT )
				{
					CGDBPtr<SEntrenchmentRPGStats> pStats = dynamic_cast<const SEntrenchmentRPGStats*>( pIDB->GetRPGStats( obj->pObj->pDesc ) );
					if( pStats ) 
					{
						vOldPos = GetSingleton<IAIEditor>()->GetCenter( obj->pObj->pAIObj );
					}
				}
				else if ( obj->sDesc.eGameType == SGVOGT_BUILDING )
				{
					CGDBPtr<SBuildingRPGStats> pStats = dynamic_cast<const SBuildingRPGStats*>( pIDB->GetRPGStats( obj->pObj->pDesc ) );
					if( pStats ) 
					{
						SBuildingRPGStats::SEntrance entr = pStats->entrances[0];
						vOldPos = GetSingleton<IAIEditor>()->GetCenter( obj->pObj->pAIObj );
						CVec3 vOffset = entr.vPos;
						Vis2AI( &vOffset );
						vOldPos += CVec2( vOffset.x, vOffset.y );
					}
				}					
				else if ( obj->sDesc.eGameType == SGVOGT_UNIT )
				{
					CGDBPtr<SMechUnitRPGStats> pStats = dynamic_cast<const SMechUnitRPGStats*>( pIDB->GetRPGStats( obj->pObj->pDesc ) );
					if( pStats ) 
					{
						CVec2 entr = pStats->vEntrancePoint;
						vOldPos = GetSingleton<IAIEditor>()->GetCenter( obj->pObj->pAIObj );
						vOldPos += CVec2( entr.x, entr.y );
					}
				}

				std::set<IRefCount*> squads;
				for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
				{ 
					if( GetEditorObjectItem( it->first )->pLink == obj->pObj )
					{
						IRefCount* pSquad = GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj ) ;
						if ( pSquad )
						{
							if ( squads.find( pSquad ) == squads.end() )
							{
								squads.insert( pSquad );
								xLocal += fDiff;
								yLocal += fDiff;
								MoveObject( pSquad, vOldPos.x + xLocal, vOldPos.y + yLocal, true );
							}
						}
						else
						{
							xLocal += fDiff;
							yLocal += fDiff;
							MoveObject( it->first->pAIObj, vOldPos.x + xLocal, vOldPos.y + yLocal, false );
						}
						GetEditorObjectItem( it->first )->pLink = 0;
					}
				}

				RedrawWindow();
			}
		}
	}
}

int CTemplateEditorFrame::GetNumSoldiersInBuilding( SEditorObjectItem *obj )
{
	int nRetVal = 0;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		std::set<IRefCount*> squads;
		for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
		{
			if ( GetEditorObjectItem( it->first )->pLink == obj->pObj )
			{
				IRefCount* pSquad = GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj ) ;
				if ( pSquad )
				{
					squads.insert( pSquad );
				}
				else
				{
					++nRetVal;
				}
			}
		}
		for( std::set< IRefCount* >::iterator it = squads.begin(); it != squads.end(); ++it )
		{
			++nRetVal;
		}
	}
	return nRetVal ;
}

int CTemplateEditorFrame::GetSoldiersInBuilding( SEditorObjectItem *obj, std::vector<SEditorObjectItem*> &units )
{
	int nRetVal = 0;
	for ( std::unordered_map< SMapObject *, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
	{ 
		if( GetEditorObjectItem( it->first )->pLink ==  obj->pObj )
		{
			units.push_back( it->second );
		}
	}
	return units.size();
}

void CTemplateEditorFrame::MoveObject( IRefCount *pAiObject, short x, short y, bool isFormation )
{
		CVec2 oldPos = GetSingleton<IAIEditor>()->GetCenter( pAiObject );
		oldPos.x = x - oldPos.x;
		oldPos.y = y - oldPos.y;
		// ������ oldPos - ���������� 

		GetSingleton<IAIEditor>()->MoveObject( pAiObject, x, y );
		SMapObject *pObject = FindByAI( pAiObject );
		if( pObject && !isFormation )
		{
			if( GetNumSoldiersInBuilding( GetEditorObjectItem( pObject ) ) )
			{
				std::vector<SEditorObjectItem*> units; 
				GetSoldiersInBuilding( GetEditorObjectItem( pObject ), units );
				for( std::vector<SEditorObjectItem*>::iterator it = units.begin(); it != units.end(); ++it ) 
				{
					CVec2 vTmp = GetSingleton<IAIEditor>()->GetCenter( (*it)->pObj->pAIObj );
					GetSingleton<IAIEditor>()->MoveObject(  (*it)->pObj->pAIObj, oldPos.x + vTmp.x, oldPos.y + vTmp.y );
				}
			}
		}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnNewContainer() 
{
	CRMGCreateContainerDialog createContainerDialog;
	createContainerDialog.DoModal();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnNewGraph() 
{
	CRMGCreateGraphDialog createGraphDialog;
	createGraphDialog.DoModal();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnNewField() 
{
	CRMGCreateFieldDialog createFieldDialog;
	createFieldDialog.DoModal();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnNewTemplate() 
{
	modCollector.Collect();
	CRMGCreateTemplateDialog createTemplateDialog;
	createTemplateDialog.DoModal();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::NewObjectAdded( SMapObject *pMO )
{
	bool bShowHPs = TRUE;
	if ( pMO->pDesc == 0 )
	{
		return;
	}
	//
	if ( pMO->pDesc->eGameType == SGVOGT_BUILDING )
	{
		ISceneIconBar *pBar = (ISceneIconBar*)GetCommonFactory()->CreateObject( SCENE_ICON_BAR );
		pBar->SetColor( 0xff00ff00 );
		pBar->SetSize( CVec2(80, 3) );
		static_cast_ptr<IObjVisObj*>(pMO->pVisObj)->AddIcon( pBar, ICON_HP_BAR, VNULL3, VNULL3, ICON_HP_BAR, ICON_ALIGNMENT_HCENTER | ICON_ALIGNMENT_TOP | ICON_PLACEMENT_VERTICAL );
		pBar->Enable( bShowHPs );
		pBar->SetLength( pMO->fHP );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//������ �� ���������� ���������
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnAddStartCommand()
{
	ITerrain *pTerrain = GetSingleton<IScene>()->GetTerrain();
	if ( !pTerrain || dlg || ( ( m_currentMovingObjectsAI.empty() ) && ( m_currentMovingObjectPtrAI == 0 ) ) )
	{
		return;
	}

	bool bSomeUnitsSelected = false;
	if ( m_currentMovingObjectPtrAI )
	{
		if ( m_currentMovingObjectPtrAI->IsHuman() || m_currentMovingObjectPtrAI->IsTechnics() )
		{
			bSomeUnitsSelected = true;
		}
	}
	else
	{
			for ( std::vector<SMapObject*>::iterator it = m_currentMovingObjectsAI.begin(); it != m_currentMovingObjectsAI.end(); ++it )
		{
			if ( (*it)->IsHuman() || (*it)->IsTechnics() )
			{
				bSomeUnitsSelected = true;
				break;
			}
		}
	}

	if ( bSomeUnitsSelected )
	{
		dlg = new CPropertieDialog;
		dlg->Create( CPropertieDialog::IDD, this );
		dlg->SetWindowText( "Unit Start Command Property" );
		dlg->ClearVariables();
		isStartCommandPropertyActive = true;

		m_startCommands.push_back( CMutableAIStartCommand() );
		CMutableAIStartCommand &rStartCommand = *( --m_startCommands.end() );
		if ( m_currentMovingObjectPtrAI )
		{
			if ( m_currentMovingObjectPtrAI->IsHuman() || m_currentMovingObjectPtrAI->IsTechnics() )
			{
				rStartCommand.pMapObjects.push_back( m_currentMovingObjectPtrAI );
			}
		}
		else
		{		
			for ( std::vector<SMapObject*>::iterator it = m_currentMovingObjectsAI.begin(); it != m_currentMovingObjectsAI.end(); ++it )
			{
				if ( (*it)->IsHuman() || (*it)->IsTechnics() )
				{
					rStartCommand.pMapObjects.push_back( (*it) );
				}
			}
		}
		dlg->AddObjectWithProp( rStartCommand.GetManipulator() );

		m_PointForAIStartCommand.clear();
		AddStartCommandRedLines( --m_startCommands.end() );
		//DrawAIStartCommandRedLines();
		SetMapModified();
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::AddStartCommandRedLines( TMutableAIStartCommandList::iterator startCommandIterator )
{
	if ( !startCommandIterator->pMapObjects.empty() )
	{
		for ( std::list<SMapObject*>::iterator mapObjectIterator = startCommandIterator->pMapObjects.begin(); mapObjectIterator	!= startCommandIterator->pMapObjects.end(); ++mapObjectIterator )
		{
			CVec2 center = GetSingleton<IAIEditor>()->GetCenter( (*mapObjectIterator)->pAIObj );
			AI2Vis( &center );
			m_PointForAIStartCommand.push_back( CVec3(center.x, center.y, 0.0f ) );
			CVec2 pos = startCommandIterator->vPos;
			AI2Vis( &pos );
			m_PointForAIStartCommand.push_back( CVec3( pos, 0.0f ) );
		}
	}
}

void CTemplateEditorFrame::RecalculateStartCommandRedLines( const CVec3& rPos )
{
	if ( ( !m_PointForAIStartCommand.empty() ) && 
		   ( ( m_PointForAIStartCommand.size() & 0x01 ) == 0 ) )
	{
		CVec3 pos = rPos;
		AI2Vis( &pos );
		for ( std::vector<CVec3>::iterator startPointIterator = m_PointForAIStartCommand.begin(); startPointIterator != m_PointForAIStartCommand.end(); )
		{
			++startPointIterator;
			(*startPointIterator) = pos;
			++startPointIterator;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::AddObjectToAIStartCommand( SMapObject *object, bool isRemove )
{
	for ( TMutableAIStartCommandList::iterator startCommandIterator = m_startCommands.begin(); startCommandIterator != m_startCommands.end(); )
	{
		bool isShifted = false;
		for ( std::list<SMapObject*>::iterator mapObjectIterator = startCommandIterator->pMapObjects.begin(); mapObjectIterator	!= startCommandIterator->pMapObjects.end(); ++mapObjectIterator )
		{
			if ( (*mapObjectIterator) == object )
			{
				if ( isRemove )
				{
					m_SelectedStartCommands.push_back( (*startCommandIterator) );
					startCommandIterator = m_startCommands.erase( startCommandIterator );
					isShifted = true;
					break;
				}
				else if ( !startCommandIterator->flag )
				{
					m_SelectedStartCommands.push_back( (*startCommandIterator) );
					startCommandIterator->flag = true;
					++startCommandIterator;
					isShifted = true;
					break;
				}
			}
		}
		if ( !isShifted )
		{
			++startCommandIterator;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::ClearAIStartCommandFlags()
{
	//��������� StartCommands:
	//��� ������ ��������� �������
	for ( TMutableAIStartCommandList::iterator startCommandIterator = m_startCommands.begin(); startCommandIterator != m_startCommands.end(); ++startCommandIterator )
	{
		startCommandIterator->flag = false;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::CreateSelectedAIStartCommandList( bool isRemove )
{
	m_SelectedStartCommands.clear();
	ClearAIStartCommandFlags();

	if ( m_currentMovingObjectPtrAI )
	{
		AddObjectToAIStartCommand( m_currentMovingObjectPtrAI, isRemove );
	}
	else
	{
		for ( int index = 0; index < m_currentMovingObjectsAI.size(); ++index )
		{
			AddObjectToAIStartCommand( m_currentMovingObjectsAI[index], isRemove );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::RemoveObjectFromAIStartCommand( SMapObject *object )
{
	//��������� StartCommands:
	//��� ������ ��������� �������
	for ( TMutableAIStartCommandList::iterator startCommandIterator = m_startCommands.begin(); startCommandIterator != m_startCommands.end(); )
	{
		//��� ������� ������������������� �������
		for ( std::list<SMapObject*>::iterator mapObjectIterator = startCommandIterator->pMapObjects.begin(); mapObjectIterator	!= startCommandIterator->pMapObjects.end(); )
		{
			if ( (*mapObjectIterator) == object )
			{
				mapObjectIterator = startCommandIterator->pMapObjects.erase( mapObjectIterator );		
			}
			else
			{
				//���� ��� �� �������
				++mapObjectIterator;
			}
		}
		//���� ��� �� ������ �������, �� ������� ��������� �������
		if ( !startCommandIterator->pMapObjects.empty() )
		{
			 ++startCommandIterator;		
		}
		else
		{
			 startCommandIterator = m_startCommands.erase( startCommandIterator );		
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnStartCommandsList()
{
	CreateSelectedAIStartCommandList( true );
	if ( !m_SelectedStartCommands.empty() )
	{
		CAIStartCommandsDialog dialog;
		dialog.m_frame =  this;
		dialog.m_startCommands = m_SelectedStartCommands;
		dialog.m_startCommandsUndo = m_SelectedStartCommands;
		dialog.DoModal();
		SetMapModified();
		if ( dialog.bAddCommand )
		{
			OnAddStartCommand();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateAddStartCommand(CCmdUI* pCmdUI)
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bool bSomeUnitsSelected = false;
				if ( m_currentMovingObjectPtrAI )
				{
					if ( m_currentMovingObjectPtrAI->IsHuman() || m_currentMovingObjectPtrAI->IsTechnics() )
					{
						bSomeUnitsSelected = true;
					}
				}
				else
				{
						for ( std::vector<SMapObject*>::iterator it = m_currentMovingObjectsAI.begin(); it != m_currentMovingObjectsAI.end(); ++it )
					{
						if ( (*it)->IsHuman() || (*it)->IsTechnics() )
						{
							bSomeUnitsSelected = true;
							break;
						}
					}
				}

				bEnable = ( inputStates.GetActiveState() == STATE_SIMPLE_OBJECTS ) && ( dlg == 0 ) && bSomeUnitsSelected;
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateStartCommandList(CCmdUI* pCmdUI)
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				CreateSelectedAIStartCommandList();
				bEnable = ( inputStates.GetActiveState() == STATE_SIMPLE_OBJECTS ) && ( dlg == 0 ) && ( !m_SelectedStartCommands.empty() );
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

//	TMutableReservePositionList m_reservePositions;
//	CMutableReservePosition m_CurentReservePosition;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnReservePositions()
{
	isReservePositionActive = !isReservePositionActive;
	if ( isReservePositionActive )
	{
		m_CurentReservePosition.vPos = VNULL2;
		m_CurentReservePosition.pArtilleryObject = 0;
		m_CurentReservePosition.pTruckObject = 0;
		//DrawReservePositionRedLines();
	}
	else
	{
		if ( m_CurentReservePosition.pArtilleryObject && 
			   //m_CurentReservePosition.pTruckObject && 
				 ( m_CurentReservePosition.vPos != VNULL2 ) )
		{
			m_reservePositions.push_front( m_CurentReservePosition );
			SetMapModified();
		}
	}
	RedrawWindow();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::SaveReservePosition()
{
	if ( isReservePositionActive )
	{
		if ( m_CurentReservePosition.vPos != VNULL2 && 
				 m_CurentReservePosition.pArtilleryObject )
		{
			bool bAdd = true;
			if ( m_CurentReservePosition.pTruckObject == 0 )
			{
				CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
				CGDBPtr<SMechUnitRPGStats> pArtilleryStats = dynamic_cast<const SMechUnitRPGStats*>( pIDB->GetRPGStats( m_CurentReservePosition.pArtilleryObject->pDesc ) );
				if ( IsArtillery( pArtilleryStats->type ) && 
						 ( !pArtilleryStats->vPeoplePoints.empty() ) ) 
				{
					bAdd = false;
				}
			}
			if ( bAdd )
			{
				m_reservePositions.push_front( m_CurentReservePosition );
				SetMapModified();
			}
		}
		
		m_CurentReservePosition.vPos = VNULL2;
		m_CurentReservePosition.pTruckObject = 0;
		m_CurentReservePosition.pArtilleryObject = 0;
		m_ReservePositionSequence.clear();

		RedrawWindow();		
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateReservePositions(CCmdUI* pCmdUI)
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = ( inputStates.GetActiveState() == STATE_SIMPLE_OBJECTS );
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( isReservePositionActive );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::RemoveObjectFromReservePositions( SMapObject *object )
{
	//��������� StartCommands:
	//��� ������ ��������� �������
	for ( TMutableReservePositionList::iterator reservePositionIterator = m_reservePositions.begin();
				reservePositionIterator != m_reservePositions.end(); )
	{
		//���� ��� �� ������ �������, �� ������� ��������� �������
		if ( ( reservePositionIterator->pArtilleryObject == object ) || 
			   ( reservePositionIterator->pTruckObject == object ) )
		{
			 reservePositionIterator = m_reservePositions.erase( reservePositionIterator );		
		}
		else
		{
			 ++reservePositionIterator;		
		}
	}
	if ( m_CurentReservePosition.pArtilleryObject == object )
	{
		m_CurentReservePosition.pArtilleryObject = 0;
	}
	if ( m_CurentReservePosition.pTruckObject == object )
	{
		m_CurentReservePosition.pTruckObject = 0;
	}
	
	//DrawReservePositionRedLines();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnMouseMove( UINT nFlags, CPoint point ) 
{
	bool ifAnyPopWindows = false; 
	if( dlg && dlg->IsWindowVisible() )
	{
		ifAnyPopWindows = true;
	}
	if( !ifAnyPopWindows )
	{
		SetFocus();
	}

	m_lastMouseTileX = m_lastMouseTileY = -1;
	m_lastMouseX = point.x;
	m_lastMouseY = point.y;

	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			inputStates.OnMouseMove( nFlags, CTPoint<int>( point.x, point.y ), this );
		}
	}
	SECWorksheet::OnMouseMove(nFlags, point);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnLButtonDown( UINT nFlags, CPoint point ) 
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			inputStates.OnLButtonDown( nFlags, CTPoint<int>( point.x, point.y ), this );
		}
	}
	SetFocus();
	SECWorksheet::OnLButtonDown( nFlags, point );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnLButtonUp( UINT nFlags, CPoint point ) 
{
	if ( GetFocus( ) != this )
	{
		SetFocus();
	}
	else
	{
		if ( IScene* pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				inputStates.OnLButtonUp( nFlags, CTPoint<int>( point.x, point.y ), this );
			}
		}
		SECWorksheet::OnLButtonUp( nFlags, point );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnLButtonDblClk( UINT nFlags, CPoint point ) 
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			inputStates.OnLButtonDblClk( nFlags, CTPoint<int>( point.x, point.y ), this );
		}
	}
	SECWorksheet::OnLButtonDblClk( nFlags, point );
} 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnRButtonDown( UINT nFlags, CPoint point ) 
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			inputStates.OnRButtonDown( nFlags, CTPoint<int>( point.x, point.y ), this );
		}
	}
	SetFocus();
	SECWorksheet::OnRButtonDown( nFlags, point );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnRButtonUp( UINT nFlags, CPoint point ) 
{
	if ( GetFocus( ) != this )
	{
		SetFocus();
	}
	else
	{
		if ( IScene* pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				inputStates.OnRButtonUp( nFlags, CTPoint<int>( point.x, point.y ), this );
			}
		}
		SECWorksheet::OnRButtonUp( nFlags, point );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnRButtonDblClk( UINT nFlags, CPoint point ) 
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			inputStates.OnRButtonDblClk( nFlags, CTPoint<int>( point.x, point.y ), this );
		}
	}
	SECWorksheet::OnRButtonDblClk( nFlags, point );
} 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnMButtonDown( UINT nFlags, CPoint point ) 
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			inputStates.OnMButtonDown( nFlags, CTPoint<int>( point.x, point.y ), this );
		}
	}
	SetFocus();
	SECWorksheet::OnMButtonDown( nFlags, point );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnMButtonUp( UINT nFlags, CPoint point ) 
{
	if ( GetFocus( ) != this )
	{
		SetFocus();
	}
	else
	{
		if ( IScene* pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				inputStates.OnMButtonUp( nFlags, CTPoint<int>( point.x, point.y ), this );
			}
		}
		SECWorksheet::OnMButtonUp( nFlags, point );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnMButtonDblClk( UINT nFlags, CPoint point ) 
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			inputStates.OnMButtonDblClk( nFlags, CTPoint<int>( point.x, point.y ), this );
		}
	}
	SECWorksheet::OnMButtonDblClk( nFlags, point );
} 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFileSave() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				if ( m_currentMapName != "" )
				{
					std::string szName;
					if ( mapEditorOptions.bSaveAsBZM )
					{
						szName = m_currentMapName + ".bzm";
					}
					else
					{
						szName = m_currentMapName + ".xml";
					}
					SaveMap( szName, mapEditorOptions.bSaveAsBZM );
				}
				MSG msg;
				PeekMessage( &msg, GetSafeHwnd(), WM_MOUSEMOVE, WM_RBUTTONUP, PM_REMOVE );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFileSaveXml() 
{
	mapEditorOptions.bSaveAsBZM = false;
	OnFileSave();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFileSaveBzm() 
{
	mapEditorOptions.bSaveAsBZM = true;
	OnFileSave();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateFileSaveBzm(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateFileSaveXml(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFileSaveMap() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				IDataStorage *pStorage = GetSingleton<IDataStorage>();
				if ( m_szSaveFilePath.empty() )
				{
					m_szSaveFilePath = std::string( pStorage->GetName() )  + std::string( "\\maps\\" );
				}
				CFileDialog dlg( false,
												 ( mapEditorOptions.bSaveAsBZM ? ".bzm" : ".xml" ),
												 "",
												 OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
												 ( mapEditorOptions.bSaveAsBZM ? "BZM files (*.bzm)|*.bzm|XML files (*.xml)|*.xml||" :
																												 "XML files (*.xml)|*.xml|BZM files (*.bzm)|*.bzm||" ) );
				
				dlg.m_ofn.lpstrInitialDir = m_szSaveFilePath.c_str();
				if ( dlg.DoModal() == IDOK )
				{
					std::string szSelectedFileMap = dlg.GetPathName();		
					NormalizeMapName( szSelectedFileMap, true );
					bool bLocalSaveAsBZM = mapEditorOptions.bSaveAsBZM;
					if ( ( szSelectedFileMap.rfind( ".bzm" ) == ( szSelectedFileMap.size() - 4 ) ) || ( szSelectedFileMap.rfind( ".xml" ) == ( szSelectedFileMap.size() - 4 ) ) )
					{
						const std::string szMapExtention = szSelectedFileMap.substr( szSelectedFileMap.size() - 4 );
						szSelectedFileMap = szSelectedFileMap.substr( 0, szSelectedFileMap.size() - 4 );

						if ( szMapExtention == ".bzm" )
						{
							mapEditorOptions.bSaveAsBZM = true;
						}
						else if ( szMapExtention == ".xml" )
						{
							mapEditorOptions.bSaveAsBZM = false;
						}
					}
					m_currentMapName = szSelectedFileMap;
					NStr::ToLower( m_currentMapName );

					m_szSaveFilePath = m_currentMapName.substr( 0, m_currentMapName.rfind( "\\" ) );
					OnFileSave();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::DrawAIStartCommandRedLines()
{
	if ( dlg && isStartCommandPropertyActive )
	{
		if ( ( !m_PointForAIStartCommand.empty() ) && 
				 ( ( m_PointForAIStartCommand.size() & 0x01 ) == 0 ) )
		{
			if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
			{
				if ( IScene *pScene = GetSingleton<IScene>() )
				{
					if ( ITerrain *pTerrain = pScene->GetTerrain() )
					{
						ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
						STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

						CSceneDrawTool drawTool;
						
						float fRadius = fWorldCellSize;
						DWORD dwColor = 0xffff8080;
						DWORD dwSections = 16;

						for ( std::vector<CVec3>::iterator startPointIterator = m_PointForAIStartCommand.begin(); startPointIterator != m_PointForAIStartCommand.end(); )
						{
							const CVec3& rStartPoint = (*startPointIterator);
							++startPointIterator;
							const CVec3& rFinishPoint = (*startPointIterator);
							++startPointIterator;
							drawTool.DrawLine3D( rStartPoint, rFinishPoint, dwColor, rTerrainInfo.altitudes, 16 );
							drawTool.DrawCircle3D( rStartPoint, fRadius, dwSections, dwColor, rTerrainInfo.altitudes );
							drawTool.DrawCircle3D( rStartPoint, fRadius - 2, dwSections, dwColor, rTerrainInfo.altitudes );
						}
						std::vector<CVec3>::iterator startPointIterator = m_PointForAIStartCommand.begin();
						++startPointIterator;
						const CVec3& rPoint = (*startPointIterator);
						drawTool.DrawCircle3D( rPoint, fRadius, dwSections, dwColor, rTerrainInfo.altitudes );
						drawTool.DrawCircle3D( rPoint, fRadius - 2, dwSections, dwColor, rTerrainInfo.altitudes );

						drawTool.DrawToScene();
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::DrawReservePositionRedLines()
{
	if ( isReservePositionActive )
	{
		if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
		{
			if ( IScene *pScene = GetSingleton<IScene>() )
			{
				if ( ITerrain *pTerrain = pScene->GetTerrain() )
				{
					ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
					STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
					
					CSceneDrawTool drawTool;

					float fRadius = fWorldCellSize;
					DWORD dwColor = 0xffff8080;
					DWORD dwCurrectColor = 0xff80ff80;
					DWORD dwSections = 16;

					for ( TMutableReservePositionList::iterator reservePositionIterator = m_reservePositions.begin(); reservePositionIterator != m_reservePositions.end(); ++reservePositionIterator )
					{
						CVec2 vArtilleryPos = GetSingleton<IAIEditor>()->GetCenter( reservePositionIterator->pArtilleryObject->pAIObj );
						CVec2 vTruckPos = VNULL2;
						if ( reservePositionIterator->pTruckObject )
						{
							vTruckPos = GetSingleton<IAIEditor>()->GetCenter( reservePositionIterator->pTruckObject->pAIObj );
						}
						CVec2 vPos = CVec2( reservePositionIterator->vPos.x, reservePositionIterator->vPos.y );
						AI2Vis( &vArtilleryPos );
						if ( vTruckPos != VNULL2 )
						{
							AI2Vis( &vTruckPos );
						}
						AI2Vis( &vPos );
						drawTool.DrawCircle3D( CVec3( vArtilleryPos, 0.0f ), fRadius, dwSections, dwColor, rTerrainInfo.altitudes );
						drawTool.DrawCircle3D( CVec3( vArtilleryPos, 0.0f ), fRadius - 2, dwSections, dwColor, rTerrainInfo.altitudes );
						if ( vTruckPos != VNULL2 )
						{
							drawTool.DrawCircle3D( CVec3( vTruckPos, 0.0f ), fRadius, dwSections, dwColor, rTerrainInfo.altitudes );
							drawTool.DrawCircle3D( CVec3( vTruckPos, 0.0f ), fRadius - 2, dwSections, dwColor, rTerrainInfo.altitudes );
						}
						drawTool.DrawCircle3D( CVec3( vPos, 0.0f ), fRadius, dwSections, dwColor, rTerrainInfo.altitudes );
						drawTool.DrawCircle3D( CVec3( vPos, 0.0f ), fRadius - 2, dwSections, dwColor, rTerrainInfo.altitudes );
						if ( vTruckPos != VNULL2 )
						{
							drawTool.DrawLine3D( CVec3( vArtilleryPos, 0.0f ), CVec3( vTruckPos, 0.0f ), dwColor, rTerrainInfo.altitudes, 16 );
							drawTool.DrawLine3D( CVec3( vPos, 0.0f ), CVec3( vTruckPos, 0.0f ), dwColor, rTerrainInfo.altitudes, 16 );
						}
						drawTool.DrawLine3D( CVec3( vArtilleryPos, 0.0f ), CVec3( vPos, 0.0f ), dwColor, rTerrainInfo.altitudes, 16 );
					}
					CVec2 vArtilleryPos = VNULL2;
					CVec2 vTruckPos = VNULL2;
					CVec2 vPos = CVec2( m_CurentReservePosition.vPos.x, m_CurentReservePosition.vPos.y );
					if ( m_CurentReservePosition.pArtilleryObject )
					{
						vArtilleryPos = GetSingleton<IAIEditor>()->GetCenter( m_CurentReservePosition.pArtilleryObject->pAIObj );
						AI2Vis( &vArtilleryPos );
					}
					if ( m_CurentReservePosition.pTruckObject )
					{
						vTruckPos = GetSingleton<IAIEditor>()->GetCenter( m_CurentReservePosition.pTruckObject->pAIObj );
						AI2Vis( &vTruckPos );
					}
					if ( vPos != VNULL2 )
					{
						AI2Vis( &vPos );
						drawTool.DrawCircle3D( CVec3( vPos, 0.0f ), fRadius, dwSections, dwCurrectColor, rTerrainInfo.altitudes );
						drawTool.DrawCircle3D( CVec3( vPos, 0.0f ), fRadius - 2, dwSections, dwCurrectColor, rTerrainInfo.altitudes );
					}
					if ( m_CurentReservePosition.pArtilleryObject )
					{
						drawTool.DrawCircle3D( CVec3( vArtilleryPos, 0.0f ), fRadius, dwSections, dwCurrectColor, rTerrainInfo.altitudes );
						drawTool.DrawCircle3D( CVec3( vArtilleryPos, 0.0f ), fRadius - 2, dwSections, dwCurrectColor, rTerrainInfo.altitudes );
					}
					if ( m_CurentReservePosition.pTruckObject )
					{
						drawTool.DrawCircle3D( CVec3( vTruckPos, 0.0f ), fRadius, dwSections, dwCurrectColor, rTerrainInfo.altitudes );
						drawTool.DrawCircle3D( CVec3( vTruckPos, 0.0f ), fRadius - 2, dwSections, dwCurrectColor, rTerrainInfo.altitudes );
					}
					if ( m_CurentReservePosition.pArtilleryObject && m_CurentReservePosition.pTruckObject )
					{
						drawTool.DrawLine3D( CVec3( vArtilleryPos, 0.0f ), CVec3( vTruckPos, 0.0f ), dwCurrectColor, rTerrainInfo.altitudes, 16 );
					}
					if ( m_CurentReservePosition.pArtilleryObject && ( vPos != VNULL2 ) )
					{
						drawTool.DrawLine3D( CVec3( vArtilleryPos, 0.0f ), CVec3( vPos, 0.0f ), dwCurrectColor, rTerrainInfo.altitudes, 16 );
					}
					if ( ( vPos != VNULL2 ) && m_CurentReservePosition.pTruckObject )
					{
						drawTool.DrawLine3D( CVec3( vPos, 0.0f ), CVec3( vTruckPos, 0.0f ), dwCurrectColor, rTerrainInfo.altitudes, 16 );
					}
					drawTool.DrawToScene();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::DrawUnitsSelection()
{
	if ( bNeedDrawUnitsSelection )
	{
		if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
		{
			if ( IScene *pScene = GetSingleton<IScene>() )
			{
				if ( ITerrain *pTerrain = pScene->GetTerrain() )
				{
					ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
					STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

					float fRadius = fWorldCellSize;
					DWORD dwSections = 16;
					DWORD dwColor = 0xffFFffFF;

					if ( m_currentMovingObjectPtrAI )
					{
						CSceneDrawTool drawTool;
						drawTool.DrawCircle3D( m_currentMovingObjectPtrAI->pVisObj->GetPosition(), fRadius, dwSections, dwColor, rTerrainInfo.altitudes );
						drawTool.DrawCircle3D( m_currentMovingObjectPtrAI->pVisObj->GetPosition(), fRadius - 2, dwSections, dwColor, rTerrainInfo.altitudes );
						drawTool.DrawToScene();
					}
					else if ( !m_currentMovingObjectsAI.empty() )
					{
						CSceneDrawTool drawTool;
						for ( int nObjectsIndex = 0; nObjectsIndex < m_currentMovingObjectsAI.size(); ++nObjectsIndex )
						{
							drawTool.DrawCircle3D( m_currentMovingObjectsAI[nObjectsIndex]->pVisObj->GetPosition(), fRadius, dwSections, dwColor, rTerrainInfo.altitudes );
							drawTool.DrawCircle3D( m_currentMovingObjectsAI[nObjectsIndex]->pVisObj->GetPosition(), fRadius - 2, dwSections, dwColor, rTerrainInfo.altitudes );
						}
						drawTool.DrawToScene();
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CTemplateEditorFrame::GetAnimationFrameIndex( struct IVisObj *pVisObj)
{
	ISpriteVisObj *pSprite = dynamic_cast<ISpriteVisObj*>( pVisObj );
	if ( pSprite )
		return static_cast<ISpriteAnimation*>( pSprite->GetAnimation() )->GetFrameIndex();
	else
	{
		SMapObject *obj = FindByVis( pVisObj );
		if ( obj )
		{
			return GetEditorObjectItem( obj )->frameIndex;
		}	
		return -1;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::UpdateObjectsZ( const CTRect<int> &rUpdateRect )
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

				for ( TMutableMapSoundInfoVector::iterator mutabeMapSoundInfoIterator = mapSoundInfo.begin(); mutabeMapSoundInfoIterator != mapSoundInfo.end(); ++mutabeMapSoundInfoIterator )
				{
					CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &( mutabeMapSoundInfoIterator->vPos ) );
				}

				for ( int nIndex = 0; nIndex < 3; ++nIndex )
				{
					for ( TMutableMapSoundInfoVector::iterator mutabeMapSoundInfoIterator = addedMapSoundInfo[nIndex].begin(); mutabeMapSoundInfoIterator != addedMapSoundInfo[nIndex].end(); ++mutabeMapSoundInfoIterator )
					{
						CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &( mutabeMapSoundInfoIterator->vPos ) );
					}
				}

				for( int nVSOIndex = 0; nVSOIndex < rTerrainInfo.roads3.size(); ++nVSOIndex )
				{
					CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &( rTerrainInfo.roads3[nVSOIndex] ) );
					pTerrainEditor->UpdateRoad( rTerrainInfo.roads3[nVSOIndex].nID );
				}

				for( int nVSOIndex = 0; nVSOIndex < rTerrainInfo.rivers.size(); ++nVSOIndex )
				{
					CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &( rTerrainInfo.rivers[nVSOIndex] ) );
					pTerrainEditor->UpdateRiver( rTerrainInfo.rivers[nVSOIndex].nID );
				}				

			}
		}
	}
}

void  CTemplateEditorFrame::FillAddedMapSoundInfo()
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

				IVisObjBuilder* pVisObjBuilder = GetSingleton<IVisObjBuilder>();
				/**
				if ( CTabSoundsDialog *pTabSoundsDialog = m_mapEditorBarPtr->GetSoundsTab() )
				{
					for ( int nIndex = 0; nIndex < 3; ++nIndex )
					{
						if ( pTabSoundsDialog->GetSoundsState( nIndex ) )
						{
							for ( TMutableMapSoundInfoVector::iterator mutabeMapSoundInfoIterator = addedMapSoundInfo[nIndex].begin(); mutabeMapSoundInfoIterator != addedMapSoundInfo[nIndex].end(); ++mutabeMapSoundInfoIterator )
							{
								mutabeMapSoundInfoIterator->pVisObj = pVisObjBuilder->BuildObject( "editor\\service\\Sound\\1", 0, SGVOT_MESH );
								pScene->AddObject( mutabeMapSoundInfoIterator->pVisObj, SGVOGT_UNIT );
								pScene->MoveObject( mutabeMapSoundInfoIterator->pVisObj, mutabeMapSoundInfoIterator->vPos );

								CVSOBuilder::UpdateZ( rTerrainInfo.altitudes, &( mutabeMapSoundInfoIterator->vPos ) );
							}
						}
					}
				}
				/**/
			}
		}
	}
}

void  CTemplateEditorFrame::RemoveAddedMapSoundInfo()
{
	if ( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
			{
				for ( int nIndex = 0; nIndex < 3; ++nIndex )
				{
					for ( TMutableMapSoundInfoVector::iterator mutabeMapSoundInfoIterator = addedMapSoundInfo[nIndex].begin(); mutabeMapSoundInfoIterator != addedMapSoundInfo[nIndex].end(); ++mutabeMapSoundInfoIterator )
					{
						pScene->RemoveObject( mutabeMapSoundInfoIterator->pVisObj );
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::GetFilesInDataStorage()
{
	if ( enumFilesInDataStorageParameter.empty() )
	{
		DWORD dwTime = GetTickCount();
		std::string szFolder;
		for ( int nSeason = 0; nSeason < CMapInfo::SEASON_COUNT; ++nSeason )
		{
			enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
			szFolder = std::string( CMapInfo::SEASON_FOLDERS[nSeason] ) + std::string( "roads3d\\" );
			NStr::ToLower( szFolder );
			enumFilesInDataStorageParameter.back().szPath = szFolder;
			enumFilesInDataStorageParameter.back().szExtention = ".xml";
			
			enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
			szFolder = std::string( CMapInfo::SEASON_FOLDERS[nSeason] ) + std::string( "rivers\\" );
			NStr::ToLower( szFolder );
			enumFilesInDataStorageParameter.back().szPath = szFolder;
			enumFilesInDataStorageParameter.back().szExtention = ".xml";
		}
		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( RMGC_SETTING_DEFAULT_FOLDER );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = ".xml";

		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( "scenarios\\templates\\" );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = ".xml";
		
		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( "scenarios\\chapters\\" );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = "context.xml";
		
		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( "bridges\\" );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = ".xml";

		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( "fences\\" );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = ".xml";

		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( "scenarios\\patches\\" );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = ".bzm";

		/**/
		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( "scenarios\\patches\\" );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = ".xml";
		/**/

		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( "maps\\" );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = ".bzm";

		/**/
		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( "maps\\" );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = ".xml";
		/**/

		enumFilesInDataStorageParameter.push_back( SEnumFilesInDataStorageParameter() );
		szFolder = std::string( "scenarios\\fieldsets\\" );
		NStr::ToLower( szFolder );
		enumFilesInDataStorageParameter.back().szPath = szFolder;
		enumFilesInDataStorageParameter.back().szExtention = ".xml";

		enumFolderStructureParameter.nIgnoreFolderCount = 1;
		EnumFilesInDataStorage( &enumFilesInDataStorageParameter, GetSingleton<IDataStorage>(), &enumFolderStructureParameter );
/**
		for ( TEnumFolders::const_iterator enumFolderIterator = enumFolderStructureParameter.folders.begin();
					enumFolderIterator != enumFolderStructureParameter.folders.end();
					++enumFolderIterator )
		{
			NStr::DebugTrace( "%s\n", enumFolderIterator->first.c_str() );
		}
/**/
		dwTime = GetTickCount() - dwTime;
		NStr::DebugTrace( "CTemplateEditorFrame::GetFilesInDataStorage() %d ms\n", dwTime );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTemplateEditorFrame::GetEnumFilesInDataStorage( const std::string &rszFolder, std::list<std::string> *pList )
{
	if ( pList )
	{
		GetFilesInDataStorage();
		std::string szFolder = rszFolder;
		NStr::ToLower( szFolder );
		bool bAdded = false;
		for ( int nParameterIndex = 0; nParameterIndex < enumFilesInDataStorageParameter.size(); ++nParameterIndex )
		{
			if ( enumFilesInDataStorageParameter[nParameterIndex].szPath == szFolder )
			{
				pList->insert( pList->end(),
											 enumFilesInDataStorageParameter[nParameterIndex].fileNames.begin(),
											 enumFilesInDataStorageParameter[nParameterIndex].fileNames.end() );
				bAdded = true;
			}
		}
		return bAdded;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFillArea() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				CString strTitle;
				strTitle.LoadString( IDR_EDITORTYPE );
				if ( MessageBox( "Do you really want FILL ENTIRE MAP with selected tile type?", strTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) == IDYES )
				{
					ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
					STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

					int nTileIndex = m_pTabTileEditDialog->m_TilesList.GetNextItem( -1, LVNI_SELECTED );
					if ( nTileIndex >= 0 )
					{
						nTileIndex = m_pTabTileEditDialog->m_TilesList.GetItemData( nTileIndex );

						CTRect<int> terrainRect( 0, 0 , rTerrainInfo.patches.GetSizeX() * 16, rTerrainInfo.patches.GetSizeY() * 16 );
						
						for ( int i = terrainRect.miny; i < terrainRect.maxy; i++ )
						{
							for ( int j = terrainRect.minx; j < terrainRect.maxx; j++ )
							{
								pTerrainEditor->SetTile( j, i, descrTile.terrtypes[nTileIndex].GetMapsIndex() );
							}
						}	
						
						terrainRect.maxx =- 1;
						terrainRect.maxy =- 1;
						pTerrainEditor->Update( terrainRect  );
						
						if ( g_frameManager.GetMiniMapWindow() )
						{
							g_frameManager.GetMiniMapWindow()->UpdateMinimapEditor( true );
						}

						OnButtonUpdate();
						SetMapModified();	
						RedrawWindow();
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateButtonfillarea(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				int nTileIndex = m_pTabTileEditDialog->m_TilesList.GetNextItem( -1, LVNI_SELECTED );
				if ( nTileIndex >= 0 )
				{
					bEnable = ( ( inputStates.GetActiveState() == STATE_TERRAIN ) &&
											( m_mapEditorBarPtr->GetTerrainState() == STATE_TERRAIN_TILES ) );
				}
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowFireRange()
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bFireRangePressed = !bFireRangePressed;
				ShowFireRange( bFireRangePressed );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 CTemplateEditorFrame::GetScreenCenter()
{
	CRect r;
	GetClientRect( &r );

	int newX = 0;
	int newY = 0;
	if ( r.Width() > GAME_SIZE_X )
	{
		newX = GAME_SIZE_X / 2; 
	}
	else
	{
		newX = r.Width() / 2;
	}
	if ( r.Height() > GAME_SIZE_Y )
	{
		newY = GAME_SIZE_Y / 2; 
	}
	else
	{
		newY = r.Height() / 2;
	}

	CVec3 newPos;
	GetPos3( &newPos, newX, newY );
	return newPos;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnButtonSetCameraPos() 
{

	int nPlayer = m_mapEditorBarPtr->GetObjectWnd()->GetPlayer();
	CVec3 newPos = GetScreenCenter();
	if ( ( nPlayer >= 0 ) && ( nPlayer < ( currentMapInfo.diplomacies.size() - 1 ) ) )
	{
		GetSingleton<IScene>()->MoveObject( cameraVisObj[nPlayer], newPos );
		currentMapInfo.playersCameraAnchors[nPlayer] = newPos;
	}
	else if ( nPlayer == ( currentMapInfo.diplomacies.size() - 1 ) )
	{
		GetSingleton<IScene>()->MoveObject( cameraVisObj[nPlayer], newPos );
		currentMapInfo.vCameraAnchor = newPos;
	}
	SetMapModified();
	RedrawWindow();	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateButtonsetcamera( CCmdUI* pCmdUI ) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//������� �������
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnEditUnitCreationInfo()
{
	ITerrain *pTerrain = GetSingleton<IScene>()->GetTerrain();
	if ( !pTerrain || dlg )
	{
		return;
	}

	dlg = new CPropertieDialog;
	dlg->Create( CPropertieDialog::IDD, this );
	dlg->SetWindowText( "Map Unit Creation Property" );
	dlg->ClearVariables();
	dlg->AddObjectWithProp( m_unitCreationInfo.GetManipulator() );
	SetMapModified();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateEditUnitCreationInfo(CCmdUI* pCmdUI)
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = ( dlg == 0 );
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnOptions() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				CMapOptionsDialog mapOptionsDialog;
				mapOptionsDialog.m_name = currentMapInfo.szScriptFile.c_str();
				if ( mapOptionsDialog.DoModal() == IDOK )
				{
					currentMapInfo.szScriptFile = mapOptionsDialog.m_name;
					SetMapModified();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateOptions( CCmdUI* pCmdUI ) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnButtonUpdate() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				CProgressDialog progressDialog;
				progressDialog.Create( IDD_PROGRESS, this );
				if ( progressDialog.GetSafeHwnd() != 0 )
				{
					progressDialog.ShowWindow( SW_SHOW ); 
					progressDialog.SetWindowText( "Updating Blitzkrieg Map" );
					progressDialog.SetProgressRange( 0, m_objectsAI.size() + 7 ); 
					progressDialog.SetProgressPosition( 0 );
				}

				BeginWaitCursor();

				progressDialog.SetProgressMessage( "Updating terrain..." );
				progressDialog.IterateProgressPosition();
				ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
				
				pAIEditor->UpdateAllHeights();
				
				progressDialog.IterateProgressPosition();
				
				pAIEditor->UpdateTerrain( CTRect<int>( 0, 0, rTerrainInfo.tiles.GetSizeX(), rTerrainInfo.tiles.GetSizeY() ), rTerrainInfo );
				
				progressDialog.IterateProgressPosition();
				
				CMapInfo::UpdateTerrainShades( &rTerrainInfo, CTRect<int>( 0, 0, rTerrainInfo.altitudes.GetSizeX(), rTerrainInfo.altitudes.GetSizeY() ), CVertexAltitudeInfo::GetSunLight( static_cast<CMapInfo::SEASON>( GetSeason() ) ) );
				
				progressDialog.IterateProgressPosition();
				
				pTerrainEditor->Update( CTRect<int>( 0,
																						 0, 
																						 rTerrainInfo.patches.GetSizeX() - 1, 
																						 rTerrainInfo.patches.GetSizeY() - 1 ) );
				
				progressDialog.IterateProgressPosition();
				
				UpdateObjectsZ( CTRect<int>( 0, 0, rTerrainInfo.altitudes.GetSizeX(), rTerrainInfo.altitudes.GetSizeY() ) );
				
				progressDialog.IterateProgressPosition();
				progressDialog.SetProgressMessage( "Updating objects..." );
				//�������� ��� ��������� � AI ������
				if ( ifFitToAI )
				{
					for ( std::unordered_map< SMapObject *, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
					{	
						SMapObject *pMO = it->first;
						// fit only sprite objects: buildings, objects and terra objects
						if ( (pMO->pDesc->eVisType == SGVOT_SPRITE) && 
								 ((pMO->pDesc->eGameType == SGVOGT_BUILDING) || 
									(pMO->pDesc->eGameType == SGVOGT_OBJECT) || 
									(pMO->pDesc->eGameType == SGVOGT_TERRAOBJ)) )
						{
							const SObjectBaseRPGStats *pRPG = static_cast<const SObjectBaseRPGStats*>( pMO->pRPG.GetPtr() );
							ISpriteVisObj *pSprite = static_cast<ISpriteVisObj*>( pMO->pVisObj.GetPtr() );
							const int nFrameIndex = static_cast<ISpriteAnimation*>( pSprite->GetAnimation() )->GetFrameIndex();
							const CArray2D<BYTE> &passability = pRPG->GetPassability( nFrameIndex );
							// fit ����������� ������ � ��������, ������� ����� ��������� ������ ������������
							if ( !passability.IsEmpty() )
							{
								CVec3 vPos = pMO->pVisObj->GetPosition();
								FitVisOrigin2AIGrid( &vPos, pRPG->GetOrigin( nFrameIndex ) );

								CVec3 vAI;
								Vis2AI( &vAI, vPos );
								if ( pMO->pAIObj )
								{
									//GetSingleton<IAIEditor>()->MoveObject( pMO->pAIObj, vAI.x, vAI.y );
										MoveObject( pMO->pAIObj, vAI.x, vAI.y );
								}
							}
						}
						progressDialog.IterateProgressPosition();
					}
					IGameTimer *pTimer = GetSingleton<IGameTimer>();
					pTimer->Update( timeGetTime() );
					Update( pTimer->GetGameTime() );
					progressDialog.IterateProgressPosition();
				}

				EndWaitCursor();
				if ( progressDialog.GetSafeHwnd() != 0 )
				{
					progressDialog.DestroyWindow();
				}
				SetMapModified();
				RedrawWindow();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateButtonupdate(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene14()
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				m_bNeedUpdateUnitHeights = !m_bNeedUpdateUnitHeights;

				/**
				if ( m_bNeedUpdateUnitHeights )
				{
					OnButtonUpdate();
				}
				/**/
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene14( CCmdUI* pCmdUI ) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( m_bNeedUpdateUnitHeights );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnButtonfit() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				ifFitToAI	= !ifFitToAI;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateButtonFit( CCmdUI* pCmdUI )
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( ifFitToAI );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene6()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene6 = !bShowScene6;
		while ( pScene->ToggleShow( SCENE_SHOW_TERRAIN ) != bShowScene6 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene6(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene6 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene7()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene7 = !bShowScene7;
		while ( pScene->ToggleShow( SCENE_SHOW_GRID ) != bShowScene7 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene7(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene7 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene10()
{
	if ( IGFX* pGFX = GetSingleton<IGFX>() )
	{
		bWireframe = !bWireframe;
		pGFX->SetWireframe( bWireframe );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene10(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				if ( IGFX* pGFX = GetSingleton<IGFX>() )
				{
					bEnable =  true;
				}
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bWireframe );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene11()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene11 = !bShowScene11;
		while ( pScene->ToggleShow( SCENE_SHOW_NOISE ) != bShowScene11 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene13()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene13 = !bShowScene13;
		while ( pScene->ToggleShow( SCENE_SHOW_BORDER ) != bShowScene13 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene11(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene11 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene13(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene13 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene1()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene1 = !bShowScene1;
		while ( pScene->ToggleShow( SCENE_SHOW_UNITS ) != bShowScene1 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene1(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene1 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene2()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene2 = !bShowScene2;
		while ( pScene->ToggleShow( SCENE_SHOW_OBJECTS ) != bShowScene2 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene2(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene2 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene3()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene3 = !bShowScene3;
		while ( pScene->ToggleShow( SCENE_SHOW_BBS ) != bShowScene3 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene3(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene3 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene4()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene4 = !bShowScene4;
		while ( pScene->ToggleShow( SCENE_SHOW_SHADOWS ) != bShowScene4 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene4(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene4 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene8()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene8 = !bShowScene8;
		while ( pScene->ToggleShow( SCENE_SHOW_WARFOG ) != bShowScene8 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene8(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene8 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene0()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene0 = !bShowScene0;
		while ( pScene->ToggleShow( SCENE_SHOW_HAZE ) != bShowScene0 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene0(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene0 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene9()
{
	if ( IScene* pScene = GetSingleton<IScene>() )
	{
		bShowScene9 = !bShowScene9;
		while ( pScene->ToggleShow( SCENE_SHOW_DEPTH_COMPLEXITY ) != bShowScene9 );
		RedrawWindow();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene9(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowScene9 );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnButtonAI() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bShowAIPassability = !bShowAIPassability;
				if ( bShowAIPassability )
				{
					bShowStorageCoverage = false;
				}
				while( ToggleAIInfo() != bShowAIPassability );
				ShowStorageCoverage();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateButtonai(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowAIPassability );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowStorageCoverage() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				if ( m_mapEditorBarPtr )
				{					
					if ( CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = m_mapEditorBarPtr->GetObjectWnd() )
					{
						bShowStorageCoverage = !bShowStorageCoverage;
						ShowStorageCoverage();
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::ShowStorageCoverage() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				if ( m_mapEditorBarPtr )
				{					
					/**
					storageCoverageTileArray.SetZero();
					
					if ( bShowStorageCoverage )
					{
						if ( CTabSimpleObjectsDialog *pTabSimpleObjectsDialog = m_mapEditorBarPtr->GetObjectWnd() )
						{
							if ( bShowAIPassability )
							{
								OnButtonAI();
							}
					
							ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
							STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

							BeginWaitCursor();
							if ( bShowStorageCoverage )
							{
								if ( ( storageCoverageTileArray.GetSizeX() == 0 ) || ( storageCoverageTileArray.GetSizeY() == 0 ) )
								{
									storageCoverageTileArray.SetSizes( rTerrainInfo.tiles.GetSizeX() * 2, rTerrainInfo.tiles.GetSizeY() * 2 );
								}
								int nPlayer = pTabSimpleObjectsDialog->GetPlayer();
								if ( nPlayer >= 0 && nPlayer < ( currentMapInfo.diplomacies.size() - 1 ) )
								{
									nPlayer = currentMapInfo.diplomacies[nPlayer];
									pAIEditor->SetDiplomacies( currentMapInfo.diplomacies );
									pAIEditor->RecalcPassabilityForPlayer( &storageCoverageTileArray, nPlayer );
								}
							}

						}
					}
					/**/
					if ( g_frameManager.GetMiniMapWindow() )
					{
						g_frameManager.GetMiniMapWindow()->UpdateMinimap( true );
					}
					
					EndWaitCursor();
					RedrawWindow();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowStorageCoverage(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bShowStorageCoverage );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnShowScene12()
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bNeedDrawUnitsSelection = !bNeedDrawUnitsSelection;
				//DrawUnitsSelection();
				RedrawWindow();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowScene12(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable =  true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( bNeedDrawUnitsSelection );	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateShowFireRange( CCmdUI* pCmdUI )
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
	pCmdUI->SetCheck( g_frameManager.GetTemplateEditorFrame()->bFireRangePressed );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateFileSave(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTemplateEditorFrame::NeedSaveChanges()
{
	if ( !bMapModified )
	{
		return true;
	}
	bool bNeedSaveChanges = true;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				CString strTitle;
				strTitle.LoadString( IDR_EDITORTYPE );
				int nCode = MessageBox( NStr::Format( "Do you want save changes in %s?",
																							m_currentMapName.c_str() ),
																strTitle,
																MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON1 );
				if ( nCode != IDCANCEL )
				{
					if ( nCode == IDYES )
					{
						OnFileSave();
					}
				}
				else
				{
					bNeedSaveChanges = false;
				}
			}
		}
	}
	return bNeedSaveChanges;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnDiplomacy() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				if ( m_mapEditorBarPtr->GetObjectWnd() )
				{
					m_mapEditorBarPtr->GetObjectWnd()->OnDiplomacyButton();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateDiplomacy(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnFilterComposer() 
{
	m_mapEditorBarPtr->GetObjectWnd()->OnButtonNewFilter();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateFilterComposer(CCmdUI* pCmdUI) 
{
	bool bEnable = true;
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateToolsDelr(CCmdUI* pCmdUI) 
{
	bool bEnable = false;
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				bEnable = true;
			}
		}
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnCheckMap() 
{
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				CheckMap( true );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::CheckMap( bool bProgressIsVisible ) 
{
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	if ( !pStorage )
	{
		return;
	}
	if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				if ( IObjectsDB *pODB = GetSingleton<IObjectsDB>() )
				{
					int nCount = m_objectsAI.size();
					CProgressDialog progressDialog;
					if ( bProgressIsVisible )
					{
						progressDialog.Create( IDD_PROGRESS, this );
						if ( progressDialog.GetSafeHwnd() != 0 )
						{
							progressDialog.ShowWindow( SW_SHOW ); 
							progressDialog.SetWindowText( "Updating Blitzkrieg Map" );
							progressDialog.SetProgressRange( 0, nCount * 2 ); 
							progressDialog.SetProgressPosition( 0 );
						}
					}

					BeginWaitCursor();

					std::string szMessage0;
					std::string szMessage1;
					std::string szMessage2;
					std::string szMessage3;

					std::string szShortMessage0;
					std::string szShortMessage1;
					std::string szShortMessage2;

					int nCount0 = 0;
					int nCount1 = 0;
					int nCount2 = 0;
					int nMaxCount = 8;

					//������� ������� � ����������� �����������
					if ( bProgressIsVisible )
					{
						progressDialog.SetProgressMessage( "Deleting double objects..." );
					}
				
					std::set<IRefCount*> squadsForDelete;
					std::set<SMapObject*> setForDelete;
					std::map<SMapObject*, int> mapForChangePalyer;

					for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
					{
						if ( setForDelete.find( it->first ) == setForDelete.end() )
						{
							if ( it->second->nPlayer >= currentMapInfo.diplomacies.size() )
							{
								mapForChangePalyer[it->first] = it->second->nPlayer;
								it->second->nPlayer = ( currentMapInfo.diplomacies.size() - 1 ); 
							}
							for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator it2 = it; it2 != m_objectsAI.end(); ++it2 )
							{
								if ( ( it != it2 ) && ( setForDelete.find( it2->first ) == setForDelete.end() ) )
								{
									CVec3 v1 = it->first->pVisObj->GetPosition();
									CVec3 v2 = it2->first->pVisObj->GetPosition();
									if ( ( v1 == v2 ) &&
											 ( it->first->pDesc->szKey == it2->first->pDesc->szKey ) &&
											 ( it->second->frameIndex == it2->second->frameIndex ) )
									{
										if ( !pAIEditor->GetFormationOfUnit( it->first->pAIObj ) )
										{
											setForDelete.insert( it2->first );
										}
									}
								}
							}
						}

						if ( bProgressIsVisible )
						{
							progressDialog.IterateProgressPosition();
						}
					}

					if ( !setForDelete.empty() )
					{
						m_currentMovingObjectPtrAI = 0;
						m_currentMovingObjectsAI.clear();

						szMessage0 = "Double objects was deleted:";
						szShortMessage0 = "Double objects was deleted:";
						for ( std::set<SMapObject*>::iterator it = setForDelete.begin(); it != setForDelete.end(); ++it )
						{
							if ( !pAIEditor->GetFormationOfUnit( ( *it )->pAIObj ) )
							{
								CVec3 vPos = ( *it )->pVisObj->GetPosition();
								Vis2AI( &vPos );
								std::string szAddMessge = NStr::Format( "\n%s, pos: [%.2f, %.2f], scriptID: %d",
																												( *it )->pDesc->szKey.c_str(),
																												vPos.x / ( 2 * SAIConsts::TILE_SIZE ),
																												vPos.y  / ( 2 * SAIConsts::TILE_SIZE ),
																												m_objectsAI[( *it )]->nScriptID );
								szMessage0 += szAddMessge;
								if ( nCount0 < nMaxCount )
								{
									szShortMessage0 += szAddMessge + std::string( "     " );
								}
								else if ( nCount0 == nMaxCount )
								{
									szShortMessage0 += std::string( "\nMore objects...     " );
								}
								++nCount0;
							}
							RemoveObject( *it );
						}
					}

					if ( bProgressIsVisible )
					{
						progressDialog.SetProgressMessage( "Fixing invalid object links..." );
					}
					
					//������ ������ ����������� �� �������������� ������� + �� ���� ������ ��������� ��� ������� ( ������ � ��������� )
					bool bSomeDeleted = true;
					//while ( bSomeDeleted )
					{
						if ( bProgressIsVisible )
						{
							progressDialog.SetProgressPosition( nCount );
						}
						bSomeDeleted = false;
						squadsForDelete.clear();
						setForDelete.clear();

						std::set<SMapObject*> usedObjects;
						
						for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
						{
							IRefCount *pSquad = pAIEditor->GetFormationOfUnit( it->first->pAIObj );
							if ( pSquad )
							{
								IRefCount **pUnits;
								int nLength;
								pAIEditor->GetUnitsInFormation( pSquad, &pUnits, &nLength);	
								if ( nLength > 0 )
								{
									SEditorObjectItem *tmpEditiorObj =	m_objectsAI[ FindByAI( pUnits[0] ) ];
									if( tmpEditiorObj->pLink && !tmpEditiorObj->pLink->IsValid() )
									{
										squadsForDelete.insert( pSquad );
										for ( int nUnitIndex = 0; nUnitIndex < nLength; ++nUnitIndex )
										{
											SEditorObjectItem *pUnit = m_objectsAI[ FindByAI( pUnits[nUnitIndex] ) ];
											if ( pUnit )
											{
												pUnit->pLink = 0;
											}
										}
										bSomeDeleted = true;
									}
								}
							}
							else
							{
								if( it->second->pLink )
								{ 
									if( !it->second->pLink->IsValid() )
									{
										setForDelete.insert( it->first );
										it->second->pLink = 0;
										bSomeDeleted = true;
									}
									else
									{
										if ( usedObjects.find( it->second->pLink ) == usedObjects.end() )
										{
											usedObjects.insert( it->second->pLink );
										}
										else
										{
											setForDelete.insert( it->first );
											it->second->pLink = 0;
											bSomeDeleted = true;
										}
									}
								}
							}
							if ( bProgressIsVisible )
							{
								progressDialog.IterateProgressPosition();
							}
						}
	
						if ( bSomeDeleted )
						{
							m_currentMovingObjectPtrAI = 0;
							m_currentMovingObjectsAI.clear();

							if ( szMessage1.empty() )
							{
								szMessage1 = "Invalid  or double links was fixed:";
								szShortMessage1 = "Invalid or double links was fixed:";
							}

							for ( std::set<SMapObject*>::iterator it = setForDelete.begin(); it != setForDelete.end(); ++it )
							{
								if ( !pAIEditor->GetFormationOfUnit( ( *it )->pAIObj ) )
								{
									CVec3 vPos = ( *it )->pVisObj->GetPosition();
									Vis2AI( &vPos );
									std::string szAddMessge = NStr::Format( "\n%s, pos: [%.2f, %.2f], scriptID: %d",
																													( *it )->pDesc->szKey.c_str(),
																													vPos.x / ( 2 * SAIConsts::TILE_SIZE ),
																													vPos.y / ( 2 * SAIConsts::TILE_SIZE ),
																													m_objectsAI[( *it )]->nScriptID );
									szMessage1 += szAddMessge;
									if ( nCount1 < nMaxCount )
									{
										szShortMessage1 += szAddMessge + std::string( "     " );
									}
									else if ( nCount1 == nMaxCount )
									{
										szShortMessage1 += std::string( "\nMore objects...     " );
									}
									++nCount1;
								}
							}		
							
							for ( std::set<IRefCount*>::iterator it = squadsForDelete.begin(); it != squadsForDelete.end(); ++it )
							{
								IRefCount **pUnits;
								int nLength;
								pAIEditor->GetUnitsInFormation( (*it), &pUnits, &nLength );	
								SEditorObjectItem *tmpEditiorObj =	m_objectsAI[ FindByAI( pUnits[0] ) ];

								CVec2 vPos = pAIEditor->GetCenter( *it );
								int numId = pAIEditor->GetUnitDBID( *it );
								std::string szAddMessge = NStr::Format( "\n%s, pos: [%.2f, %.2f], scriptID: %d",
																												pODB->GetDesc( numId )->szKey.c_str(),
																												vPos.x / ( 2 * SAIConsts::TILE_SIZE ),
																												vPos.y / ( 2 * SAIConsts::TILE_SIZE ),
																												tmpEditiorObj->nScriptID );
								szMessage1 += szAddMessge;
								if ( nCount1 < nMaxCount )
								{
									szShortMessage1 += szAddMessge + std::string( "     " );
								}
								else if ( nCount1 == nMaxCount )
								{
									szShortMessage1 += std::string( "\nMore objects...     " );
								}
								++nCount1;
							}
						}
					}

					if ( bProgressIsVisible )
					{
						progressDialog.SetProgressMessage( "Fixing invalid player numbers..." );
					}
					
					//���������� ����� ������ � ������
					if ( !mapForChangePalyer.empty() )
					{
						std::map<IRefCount*, int> squadsMapForChangePalyer;
						m_currentMovingObjectPtrAI = 0;
						m_currentMovingObjectsAI.clear();

						szMessage2 = "Invalid player numbers was fixed:";
						szShortMessage2 = "Invalid player numbers was fixed:";
						for ( std::map<SMapObject*, int>::iterator it = mapForChangePalyer.begin(); it != mapForChangePalyer.end(); ++it )
						{
							IRefCount *pSquad = pAIEditor->GetFormationOfUnit( it->first->pAIObj );
							if ( !pSquad )
							{
								CVec3 vPos = it->first->pVisObj->GetPosition();
								Vis2AI( &vPos );
								std::string szAddMessge = NStr::Format( "\n%d->%d, %s, pos: [%.2f, %.2f], scriptID: %d",
																												it->second,
																												currentMapInfo.diplomacies.size() - 1,
																												it->first->pDesc->szKey.c_str(),
																												vPos.x / ( 2 * SAIConsts::TILE_SIZE ),
																												vPos.y / ( 2 * SAIConsts::TILE_SIZE ),
																												m_objectsAI[it->first]->nScriptID );
								szMessage2 += szAddMessge;
								if ( nCount2 < nMaxCount )
								{
									szShortMessage2 += szAddMessge + std::string( "     " );
								}
								else if ( nCount2 == nMaxCount )
								{
									szShortMessage2 += std::string( "\nMore objects...     " );
								}
								++nCount2;
							}
							else
							{
								squadsMapForChangePalyer[pSquad] = it->second;
							}
						}
						if ( !squadsMapForChangePalyer.empty() )
						{
							for ( std::map<IRefCount*, int>::iterator it = squadsMapForChangePalyer.begin(); it != squadsMapForChangePalyer.end(); ++it )
							{
								IRefCount **pUnits;
								int nLength;
								pAIEditor->GetUnitsInFormation( it->first, &pUnits, &nLength );	
								SEditorObjectItem *tmpEditiorObj =	m_objectsAI[ FindByAI( pUnits[0] ) ];

								CVec2 vPos = pAIEditor->GetCenter( it->first );
								int numId = pAIEditor->GetUnitDBID( it->first );
								std::string szAddMessge = NStr::Format( "\n%d->%d, %s, pos: [%.2f, %.2f], scriptID: %d",
																												it->second,
																												currentMapInfo.diplomacies.size() - 1,
																												pODB->GetDesc( numId )->szKey.c_str(),
																												vPos.x / ( 2 * SAIConsts::TILE_SIZE ),
																												vPos.y / ( 2 * SAIConsts::TILE_SIZE ),
																												tmpEditiorObj->nScriptID );
								szMessage2 += szAddMessge;
								if ( nCount2 < nMaxCount )
								{
									szShortMessage2 += szAddMessge + std::string( "     " );
								}
								else if ( nCount2 == nMaxCount )
								{
									szShortMessage2 += std::string( "\nMore objects...     " );
								}
								++nCount2;
							}
						}
					}	
					
					if ( bProgressIsVisible )
					{
						progressDialog.SetProgressMessage( "Checking unit creation information..." );
					}
					m_unitCreationInfo.MutableValidate();
					if ( !ucHelper.IsInitialized() )
					{
						ucHelper.Initialize();	
					}
					for ( int nUnitCreationInfoIndex = 0; nUnitCreationInfoIndex < m_unitCreationInfo.mutableUnits.size(); ++nUnitCreationInfoIndex )
					{
						bool bNotExists = true;
						for( std::list<std::string>::const_iterator partyIterator = ucHelper.partyList.begin(); partyIterator != ucHelper.partyList.end(); ++partyIterator )
						{
							if ( ( *partyIterator ) == m_unitCreationInfo.mutableUnits[nUnitCreationInfoIndex].szPartyName )
							{
								bNotExists = false;
								break;
							}
						}
						if ( bNotExists )
						{
							if ( szMessage3.empty() )
							{
								szMessage3 = "Invalid unit creation info player parties was detected:";
							}
							szMessage3 = szMessage3 + NStr::Format( "\nPlayer: %d, part set to: %s     ", nUnitCreationInfoIndex, SUnitCreationInfo::DEFAULT_PARTY_NAME );
							m_unitCreationInfo.mutableUnits[nUnitCreationInfoIndex].szPartyName = SUnitCreationInfo::DEFAULT_PARTY_NAME;
						}
					}

					EndWaitCursor();

					if ( bProgressIsVisible )
					{
						if ( progressDialog.GetSafeHwnd() != 0 )
						{
							progressDialog.DestroyWindow();
						}
					}
					std::string szMessage;
					szMessage = szMessage0;

					if ( !szMessage.empty() && !szMessage1.empty() )
					{
						szMessage += "\n\n";
					}
					szMessage += szMessage1;

					if ( !szMessage.empty() && !szMessage2.empty() )
					{
						szMessage += "\n\n";
					}
					szMessage += szMessage2;

					if ( !szMessage.empty() && !szMessage3.empty() )
					{
						szMessage += "\n\n";
					}
					szMessage += szMessage3;
					
					std::string szShortMessage;
					szShortMessage = szShortMessage0;
					if ( !szShortMessage.empty() && !szShortMessage1.empty() )
					{
						szShortMessage += "\n\n";
					}
					szShortMessage += szShortMessage1;

					if ( !szShortMessage.empty() && !szMessage2.empty() )
					{
						szShortMessage += "\n\n";
					}
					szShortMessage += szShortMessage2;

					if ( !szShortMessage.empty() && !szMessage3.empty() )
					{
						szShortMessage += "\n\n";
					}
					szShortMessage += szMessage3;

					if ( !szMessage.empty() )
					{
						CString strMessage( szMessage.c_str() );
						strMessage.Replace( "\n", "\r\n" );

						std::string szTextPath = NStr::Format( "%s%s", pStorage->GetName(), "logs\\checkmap_log.txt" );
						szShortMessage += NStr::Format( "\n\nLog file: %s", szTextPath.c_str() );

						CPtr<IDataStream> pFileStream = 0;
						if ( pFileStream = CreateFileStream( szTextPath.c_str(), STREAM_ACCESS_WRITE ) )
						{
							pFileStream->Write( LPCTSTR( strMessage ), strMessage.GetLength() );
						}

						CString strTitle;
						strTitle.LoadString( IDR_EDITORTYPE );
						SetMapModified();
						MessageBox( szShortMessage.c_str(), strTitle, MB_OK | MB_ICONINFORMATION );
					}
					RedrawWindow();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::ClearAllBeforeNewMOD()
{
	if ( GetSingleton<IGFX>() )
	{
		GetSingleton<IGFX>()->Clear();
	}
	if ( GetSingleton<IAnimationManager>() )
	{
		GetSingleton<IAnimationManager>()->Clear( ISharedManager::CLEAR_ALL );
	}
	if ( GetSingleton<IFontManager>() )
	{
		GetSingleton<IFontManager>()->Clear( ISharedManager::CLEAR_ALL );
	}
	if ( GetSingleton<IMeshManager>() )
	{
		GetSingleton<IMeshManager>()->Clear( ISharedManager::CLEAR_ALL );
	}
	if ( GetSingleton<ITextureManager>() )
	{
		GetSingleton<ITextureManager>()->Clear( ISharedManager::CLEAR_ALL );
	}
	if ( GetSingleton<ITextManager>() )
	{
		GetSingleton<ITextManager>()->Clear( ISharedManager::CLEAR_ALL );
	}

	m_mapEditorBarPtr->GetTabTileEditDialog()->DeleteImageList();
	m_mapEditorBarPtr->GetObjectWnd()->DeleteImageList();
	m_mapEditorBarPtr->GetRiversTab()->DeleteImageList();
	m_mapEditorBarPtr->GetRoads3DTab()->DeleteImageList();
	m_mapEditorBarPtr->GetBridgeWnd()->DeleteImageList();
	m_mapEditorBarPtr->GetFenceWnd()->DeleteImageList();

	enumFilesInDataStorageParameter.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::LoadNewMOD( const std::string &rszNewMODFolder )
{
	if ( CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>() )
	{
		std::string szMODPath = rszNewMODFolder;
		NStr::ToLower( szMODPath );

		pDataStorage->RemoveStorage( "MOD" );

		if ( !rszNewMODFolder.empty() ) 
		{
			if ( CPtr<IDataStorage> pMOD = OpenStorage( ( szMODPath + "data\\*.pak" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON ) )
			{
				if ( CPtr<IDataStream> pStream = pMOD->OpenStream( "mod.xml", STREAM_ACCESS_READ ) )
				{
					GetSingleton<IDataStorage>()->AddStorage( pMOD, "MOD" );
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::LoadAllAfterNewMOD()
{
	GetSingleton<IObjectsDB>()->LoadDB();
	GetSingleton<IGFX>()->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\medium" ) );
	
	m_mapEditorBarPtr->GetTabTileEditDialog()->CreateImageList();
	m_mapEditorBarPtr->GetObjectWnd()->CreateImageList();
	m_mapEditorBarPtr->GetRiversTab()->CreateImageList();
	m_mapEditorBarPtr->GetRoads3DTab()->CreateImageList();
	m_mapEditorBarPtr->GetBridgeWnd()->CreateImageList();
	m_mapEditorBarPtr->GetFenceWnd()->CreateImageList();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnToolsOptions() 
{
	mapEditorOptions.Modify();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateToolsOptions(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapEditorOptions::Modify()
{
	CMapEditorOptionsDialog mapEditorOptionsDialog;
	mapEditorOptionsDialog.SetIsBZM( bSaveAsBZM );
	mapEditorOptionsDialog.SetParameter( szGameParameters );
	if ( mapEditorOptionsDialog.DoModal() == IDOK )
	{
		if ( bSaveAsBZM != mapEditorOptionsDialog.IsBZM() )
		{
			bSaveAsBZM = mapEditorOptionsDialog.IsBZM();
			if ( g_frameManager.GetTemplateEditorFrame() )
			{
				g_frameManager.GetTemplateEditorFrame()->SetMapModified();
			}
		}
		szGameParameters = mapEditorOptionsDialog.GetParameter();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnUpdateToolsRunGame(CCmdUI* pCmdUI) 
{
	OSVERSIONINFO osVersionInfo;
	osVersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	::GetVersionEx( &osVersionInfo );
	bool bEnable = false;
	if ( osVersionInfo.dwMajorVersion > 4 )
	{
		std::string szGameFolder = szStartDirectory + "game.exe";
		NStr::ToLower( szGameFolder );
		bEnable = NFile::IsFileExist( szGameFolder.c_str() );
	}
	pCmdUI->Enable( bEnable );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string CTemplateEditorFrame::SetNameForFlag( SMapObjectInfo *pFlagObjectInfo )
{
	const std::string szFlagPrefix = "Flag_";
	const std::string szPartyNeutral = "neutral";
	NI_ASSERT_T( pFlagObjectInfo != 0, NStr::Format( "Invalid MapObjectInfo: %x", pFlagObjectInfo) );
	int nPlayerIndexFound = -1;
	if ( pFlagObjectInfo )
	{
		const SGDBObjectDesc *pDesc = GetSingleton<IObjectsDB>()->GetDesc( pFlagObjectInfo->szName.c_str() );
		if ( pDesc && pDesc->eGameType == SGVOGT_FLAG )
		{
			if ( pFlagObjectInfo->szName.find( szFlagPrefix ) == 0 )
			{
				std::string szLocalGeneralPartyName;
				if ( ( pFlagObjectInfo->nPlayer >= 0 ) && ( pFlagObjectInfo->nPlayer < m_unitCreationInfo.mutableUnits.size() ) )
				{
					const std::string szPartyName = m_unitCreationInfo.mutableUnits[pFlagObjectInfo->nPlayer].szPartyName;
					for ( int nPartyIndex = 0; nPartyIndex < ucHelper.partyDependentInfo.size(); ++nPartyIndex )
					{
						if ( ucHelper.partyDependentInfo[nPartyIndex].szPartyName == szPartyName )
						{
							szLocalGeneralPartyName = ucHelper.partyDependentInfo[nPartyIndex].szGeneralPartyName;
							NStr::ToLower( szLocalGeneralPartyName );
							break;
						}
					}
				}
				if ( szLocalGeneralPartyName.empty() )
				{
					szLocalGeneralPartyName = szPartyNeutral;	
				}
				pFlagObjectInfo->szName = szFlagPrefix + szLocalGeneralPartyName;
			}
		}
	}
	return pFlagObjectInfo->szName;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CTemplateEditorFrame::SetPlayerForFlag( SMapObjectInfo *pFlagObjectInfo )
{
	const std::string szFlagPrefix = "Flag_";
	const std::string szFlagNeutral = "Flag_neutral";
	NI_ASSERT_T( pFlagObjectInfo != 0, NStr::Format( "Invalid MapObjectInfo: %x", pFlagObjectInfo) );
	int nPlayerIndexFound = -1;
	if ( pFlagObjectInfo )
	{
		const SGDBObjectDesc *pDesc = GetSingleton<IObjectsDB>()->GetDesc( pFlagObjectInfo->szName.c_str() );
		if ( pDesc && pDesc->eGameType == SGVOGT_FLAG )
		{
			if ( pFlagObjectInfo->szName.find( szFlagPrefix ) == 0 )
			{
				std::string szGeneralPartyName = pFlagObjectInfo->szName.substr( szFlagPrefix.size() );
				NStr::ToLower( szGeneralPartyName );
				
				if ( !ucHelper.IsInitialized() )
				{
					ucHelper.Initialize();	
				}

				for ( int nPlayerIndex = 0; nPlayerIndex < ( currentMapInfo.diplomacies.size() - 1 ); ++nPlayerIndex )
				{
					const std::string szPartyName = m_unitCreationInfo.mutableUnits[nPlayerIndex].szPartyName;
					
					for ( int nPartyIndex = 0; nPartyIndex < ucHelper.partyDependentInfo.size(); ++nPartyIndex )
					{
						if ( ucHelper.partyDependentInfo[nPartyIndex].szPartyName == szPartyName )
						{
							std::string szLocalGeneralPartyName = ucHelper.partyDependentInfo[nPartyIndex].szGeneralPartyName;
							NStr::ToLower( szLocalGeneralPartyName );
							if ( szGeneralPartyName == szLocalGeneralPartyName )
							{
								nPlayerIndexFound = nPlayerIndex;
							}
							break;
						}
					}
					if ( nPlayerIndexFound >= 0 )
					{
						break;
					}
				}
				if ( nPlayerIndexFound >= 0 )
				{
					pFlagObjectInfo->nPlayer = nPlayerIndexFound;
				}
				else
				{
					nPlayerIndexFound = currentMapInfo.diplomacies.size() - 1;
					pFlagObjectInfo->nPlayer = nPlayerIndexFound;
					pFlagObjectInfo->szName = szFlagNeutral;
				}
			}
		}
	}
	return nPlayerIndexFound;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::ResetPlayersForFlags()
{
	std::set<SMapObject*> flags;
	for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
	{
		SMapObject *pMapObject = it->first;
		if ( pMapObject->pDesc->eGameType == SGVOGT_FLAG )
		{
			flags.insert( pMapObject );
		}
	}

	std::vector<SMapObjectInfo> objectsToAdd;
	objectsToAdd.reserve( flags.size() );
	for ( std::set<SMapObject*>::iterator it = flags.begin(); it != flags.end(); ++it )
	{
		SMapObject *pMapObject = ( *it );
		SEditorObjectItem *pEditorObjectItem = m_objectsAI[pMapObject];

		SMapObjectInfo mapObjectInfo;
		mapObjectInfo.szName = pMapObject->pDesc->szKey;
		
		WORD wDir = 0;
		CVec3 vPos = VNULL3;
		pMapObject->GetPlacement( &vPos, &wDir );
		mapObjectInfo.vPos = vPos;
		mapObjectInfo.nDir = wDir;
		Vis2AI( &( mapObjectInfo.vPos ) );

		mapObjectInfo.nPlayer = pEditorObjectItem->nPlayer;
		mapObjectInfo.nScriptID = pEditorObjectItem->nScriptID;
		mapObjectInfo.fHP = pMapObject->fHP;
		
		SetNameForFlag( &mapObjectInfo );
		objectsToAdd.push_back( mapObjectInfo );
		RemoveObject( pMapObject );
	}
	
	for ( std::vector<SMapObjectInfo>::iterator it = objectsToAdd.begin(); it != objectsToAdd.end(); ++it )
	{
		AddObjectByAI( *it );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTemplateEditorFrame::OnToolsRunGame() 
{
	std::string szGamePath = szStartDirectory + "game.exe";
	NStr::ToLower( szGamePath );
	std::string szMapsFolder = szStartDirectory + "data\\maps\\";
	NStr::ToLower( szMapsFolder );
			
	if ( !NeedSaveChanges() )
	{
		return;
	}

	bool bNotLoadMap = false;
	if ( NFile::IsFileExist( szGamePath.c_str() ) )
	{
		std::string szCommandLine;
		
		if ( m_currentMapName.find( szMapsFolder ) == 0 )
		{
			const std::string szShortMapName = m_currentMapName.substr( szMapsFolder.size() ) + ".xml";
			if ( szShortMapName.empty() || ( szShortMapName.find( ' ' ) != std::string::npos ) )
			{
				szCommandLine = mapEditorOptions.szGameParameters;
				bNotLoadMap = true;
			}
			else
			{
				std::string szName;
				if ( mapEditorOptions.bSaveAsBZM )
				{
					szName = m_currentMapName + ".bzm";
				}
				else
				{
					szName = m_currentMapName + ".xml";
				}
				if ( !NFile::IsFileExist( szName.c_str() )  )
				{
					szCommandLine = mapEditorOptions.szGameParameters;
					bNotLoadMap = true;
				}
				else
				{
					if ( mapEditorOptions.szGameParameters.empty() )
					{
						szCommandLine = NStr::Format( "-%s", szShortMapName.c_str() );
					}
					else
					{
						szCommandLine = NStr::Format( "%s -%s", mapEditorOptions.szGameParameters.c_str(), szShortMapName.c_str() );
					}
				}
			}
		}
		else
		{
			szCommandLine = mapEditorOptions.szGameParameters;
			bNotLoadMap = true;
		}

		if ( bNotLoadMap )
		{
			CString strMessage;
			strMessage.LoadString( IDS_MAP_NAME_INVALID );
			CString strTitle;
			strTitle.LoadString( IDR_EDITORTYPE );
			std::string szMessage = NStr::Format( strMessage, m_currentMapName.c_str(),  szMapsFolder.c_str() );
			if ( MessageBox( szMessage.c_str(), strTitle, MB_ICONEXCLAMATION | MB_YESNO ) == IDNO )
			{
				return;
			}
		}

		char pszCommandLine[0xFFF];
		strcpy( pszCommandLine, "\"" );
		strcat( pszCommandLine, szGamePath.c_str() );
		strcat( pszCommandLine, "\"" );
		if ( !szCommandLine.empty() )
		{
			strcat( pszCommandLine, " " );
			strcat( pszCommandLine, szCommandLine.c_str() );
		}

		STARTUPINFO startinfo;
		PROCESS_INFORMATION procinfo;
		memset( &startinfo, 0, sizeof( STARTUPINFO ) );
		memset( &procinfo, 0, sizeof( PROCESS_INFORMATION ) );
		startinfo.cb = sizeof( startinfo );
		bool bRetVal = false;
		DWORD dwResult = 0;
		bRetVal = ::CreateProcess( 0, pszCommandLine, 0, 0, false, 0, 0, szStartDirectory.c_str(), &startinfo, &procinfo );
		dwResult = ::GetLastError();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTemplateEditorFrame::NeedUpdateStorages()
{
	CPtr<IObjectsDB> pIDB = GetSingleton<IObjectsDB>();
	bool bNeedUpdateStorages = false;
	if ( m_currentMovingObjectPtrAI )
	{
		if ( m_currentMovingObjectPtrAI->pDesc->eGameType == SGVOGT_BUILDING )
		{
			CGDBPtr<SBuildingRPGStats> pStats = dynamic_cast<const SBuildingRPGStats*>( pIDB->GetRPGStats( m_currentMovingObjectPtrAI->pDesc ) );
			if ( ( pStats->eType == SBuildingRPGStats::TYPE_MAIN_RU_STORAGE ) ||
					 ( pStats->eType == SBuildingRPGStats::TYPE_TEMP_RU_STORAGE ) )
			{
				bNeedUpdateStorages = true;
			}
		}
	}
	else
	{
		for ( std::vector<SMapObject*>::iterator it = m_currentMovingObjectsAI.begin(); it != m_currentMovingObjectsAI.end(); ++it )
		{
			if ( m_objectsAI.find( *it ) != m_objectsAI.end() )
			{
				if ( (*it)->pDesc->eGameType == SGVOGT_BUILDING )
				{
					CGDBPtr<SBuildingRPGStats> pStats = dynamic_cast<const SBuildingRPGStats*>( pIDB->GetRPGStats( (*it)->pDesc ) );
					if ( ( pStats->eType == SBuildingRPGStats::TYPE_MAIN_RU_STORAGE ) ||
							 ( pStats->eType == SBuildingRPGStats::TYPE_TEMP_RU_STORAGE ) )
					{
						bNeedUpdateStorages = true;
					}
				}
			}
		}
	}
	return bNeedUpdateStorages;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
	CWeightVector<int> testVector;
	testVector.push_back( 0, 2 );
	testVector.push_back( 1, 1 );
	testVector.push_back( 2, 1 );

	{
		std::vector<int> values;
		values.resize( testVector.size(), 0 );

		DWORD dwTime = GetTickCount();
		for ( int nIndex = 0; nIndex < 10000; ++nIndex )
		{
			const int nValue = testVector.GetRandom( true );
			values[nValue] += 1;
		}
		dwTime = GetTickCount() - dwTime;

		NStr::DebugTrace( "Binary Search: %d %d %d, time: %d\n", values[0], values[1], values[2], dwTime );
	}

	{
		std::vector<int> values;
		values.resize( testVector.size(), 0 );
	
		DWORD dwTime = GetTickCount();
		for ( int nIndex = 0; nIndex < 10000; ++nIndex )
		{
			const int nValue = testVector.GetRandom( false );
			values[nValue] += 1;
		}
		dwTime = GetTickCount() - dwTime;
		
		NStr::DebugTrace( "Binary Search: %d %d %d, time: %d\n", values[0], values[1], values[2], dwTime );
	}
/**/
/**
		std::set< IRefCount* > squads;
		for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
		{
			if ( GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj ) )
			{	
				squads.insert( GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj ) );
			}
		}

		progressDialog.IterateProgressPosition();
			
		//---------------------------------------------------------------------
		//		����� ������� ������� �� ������
		//---------------------------------------------------------------------
		CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
		for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator it = m_objectsAI.begin(); it != m_objectsAI.end(); ++it )
		{
			if ( !GetSingleton<IAIEditor>()->GetFormationOfUnit( it->first->pAIObj ) )
			{
				SMapObjectInfo tmpObj;
				SEditorObjectItem *pTmp = it->second;
				tmpObj.nScriptID = it->second->nScriptID;
				//tmpObj.szLogic = it->second->szBehavior;
				tmpObj.nDir = it->first->pVisObj->GetDirection();
				tmpObj.nPlayer = it->second->nPlayer;
				tmpObj.vPos = it->first->pVisObj->GetPosition();
				tmpObj.nFrameIndex = GetAnimationFrameIndex( it->first->pVisObj );
				Vis2AI( &tmpObj.vPos );
				tmpObj.szName = it->second->sDesc.szKey;
				tmpObj.fHP = it->first->fHP;
				SMapObjectInfo::SLinkInfo link;
				link.nLinkID = GetSingleton<IAIEditor>()->AIToLink( it->first->pAIObj ); 

				// � ������� ������ ��� ����� � �� ������������ ������� �� ������� ��������� 
				if( it->second->pLink )
				{
					if( it->second->pLink->IsValid() )
					{
						link.nLinkWith = GetSingleton<IAIEditor>()->AIToLink( it->second->pLink->pAIObj );
					}
				}
				
				tmpObj.link = link;		
				if ( pTmp->bScenarioUnit )
				{
					AddMapObjectInfo( currentMapInfo.scenarioObjects, tmpObj );
				}
				else
				{
					AddMapObjectInfo( currentMapInfo.objects, tmpObj );
				}
				//currentMapInfo.objects.push_back( tmpObj );
			}
		}

		progressDialog.IterateProgressPosition();
		
		//---------------------------------------------------------------------
		//			 ������ ������ 
		//---------------------------------------------------------------------
		//CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
		for( std::set< IRefCount* >::iterator it = squads.begin(); it != squads.end(); ++it )
		{
			// ������� ���� �� ������ ��������� �� ������ 
			IRefCount **pUnits;
			int nLength;
			GetSingleton<IAIEditor>()->GetUnitsInFormation( (*it), &pUnits, &nLength);	
			SEditorObjectItem *tmpEditiorObj =	m_objectsAI[ FindByAI( pUnits[0] ) ];

			SMapObjectInfo tmpObj;
			tmpObj.nScriptID = tmpEditiorObj->nScriptID ; 
			tmpObj.nDir = GetSingleton<IAIEditor>()->GetDir( *it );
			tmpObj.nPlayer = tmpEditiorObj->nPlayer;
			//tmpObj.szLogic = tmpEditiorObj->szBehavior;


			tmpObj.vPos = CVec3( GetSingleton<IAIEditor>()->GetCenter( *it ).x, GetSingleton<IAIEditor>()->GetCenter( *it ).y, 0) ;
			tmpObj.nFrameIndex = 0;

			int numId = GetSingleton<IAIEditor>()->GetUnitDBID( *it );
			tmpObj.szName = pODB->GetDesc( numId )->szKey;//FindByAI( *it )->pDesc->szKey;//tmpEditiorObj->sDesc.szKey;
			tmpObj.fHP = FindByAI( pUnits[0] )->fHP;
			SMapObjectInfo::SLinkInfo link;
			link.nLinkID = GetSingleton<IAIEditor>()->AIToLink( *it );

			// � ������� ������ ��� ����� � �� ������������ ������� �� ������� ��������� 
			if( tmpEditiorObj->pLink )
			{
				if( tmpEditiorObj->pLink->IsValid() )
				{
					link.nLinkWith = GetSingleton<IAIEditor>()->AIToLink( tmpEditiorObj->pLink->pAIObj );
				}
			}
			
			tmpObj.link = link;
			if ( tmpEditiorObj->bScenarioUnit )
			{
				AddMapObjectInfo( currentMapInfo.scenarioObjects, tmpObj );
			}
			else
			{
				AddMapObjectInfo( currentMapInfo.objects, tmpObj );
			}
			//currentMapInfo.objects.push_back( tmpObj );	
		}
/**/
