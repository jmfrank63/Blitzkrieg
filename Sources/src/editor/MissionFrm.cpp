#include "StdAfx.h"
#include <io.h>
#include "..\Common\LegacyUiCompat.h"

#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"
#include "..\Main\rpgstats.h"
#include "MinimapCreation.h"
#include "..\RandomMapGen\MapInfo_types.h"

#include "editor.h"
#include "BuildCompose.h"			//��� ���������� �������� � ��������
#include "TreeDockWnd.h"
#include "PropView.h"
#include "TreeItem.h"
#include "MissionFrm.h"
#include "ImageView.h"
#include "MissionTreeItem.h"
#include "GameWnd.h"
#include "frames.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(CMissionFrame, CImageFrame)

BEGIN_MESSAGE_MAP(CMissionFrame, CImageFrame)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_COMMAND(ID_GENERATE_IMAGE, OnGenerateImage)
	ON_UPDATE_COMMAND_UI(ID_GENERATE_IMAGE, OnUpdateGenerateImage)
END_MESSAGE_MAP()


CMissionFrame::CMissionFrame()
{
	szComposerName = "Mission Editor";
	szExtension = "*.mip";
	szComposerSaveName = "Mission_Composer_Project";
	nTreeRootItemID = E_MISSION_ROOT_ITEM;
	nFrameType = CFrameManager::E_MISSION_FRAME;
	pWndView = new CImageView;
	pActiveObjective = 0;

	szAddDir = "scenarios\\";

	m_nCompressedFormat = GFXPF_DXT3;
	m_nLowFormat = GFXPF_ARGB1555;
}

CMissionFrame::~CMissionFrame()
{
}

int CMissionFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CImageFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );
	
	return 0;
}


void CMissionFrame::FillRPGStats( SMissionStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName )
{
	CMissionCommonPropsItem *pCommonProps = static_cast<CMissionCommonPropsItem *> ( pRootItem->GetChildItem( E_MISSION_COMMON_PROPS_ITEM ) );
	std::string szTemp, szTT;
	szTemp = szPrefix + pCommonProps->GetHeaderText();
	rpgStats.szHeaderText = szTemp;
	szTemp = szPrefix + pCommonProps->GetSubHeaderText();
	rpgStats.szSubheaderText = szTemp;
	szTemp = szPrefix + pCommonProps->GetDescText();
	rpgStats.szDescriptionText = szTemp;
	szTemp = szPrefix + "map";
	rpgStats.szMapImage = szTemp;
	
	for ( int k=0; k<2; k++ )
	{
		CTreeItem *pMusics = pRootItem->GetChildItem( E_MISSION_MUSICS_ITEM, k );
		std::vector<std::string> *pMusicVector = 0;
		if ( k == 0 )
			pMusicVector = &rpgStats.combatMusics;
		else if ( k == 1 )
			pMusicVector = &rpgStats.explorMusics;
		else
			NI_ASSERT( 0 );
		
		for ( CTreeItem::CTreeItemList::const_iterator it=pMusics->GetBegin(); it!=pMusics->GetEnd(); ++it )
		{
			CMissionMusicPropsItem *pMusicProps = static_cast<CMissionMusicPropsItem *> ( it->GetPtr() );
			pMusicVector->push_back( pMusicProps->GetMusicFileName() );
		}
	}
	
	{
		szTemp = "map";
		szTemp += ".tga";
		MakeFullPath( GetDirectory( pszProjectName ).c_str(), szTemp.c_str(), szTT );
		rpgStats.mapImageRect = GetImageSize( szTT.c_str() );
	}

	rpgStats.szTemplateMap = pCommonProps->GetTemplateMap();
	rpgStats.szFinalMap = pCommonProps->GetFinalMap();
	rpgStats.szSettingName = pCommonProps->GetSettingName();
	rpgStats.szMODName = theApp.GetMODName();
	rpgStats.szMODVersion = theApp.GetMODVersion();
	
	CTreeItem *pObjectives = pRootItem->GetChildItem( E_MISSION_OBJECTIVES_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pObjectives->GetBegin(); it!=pObjectives->GetEnd(); ++it )
	{
		CMissionObjectivePropsItem *pObjProps = static_cast<CMissionObjectivePropsItem *> ( it->GetPtr() );
		SMissionStats::SObjective objective;
		szTemp = szPrefix + pObjProps->GetObjectiveHeader();
		objective.szHeader = szTemp;
		szTemp = szPrefix + pObjProps->GetObjectiveText();
		objective.szDescriptionText = szTemp;
		objective.vPosOnMap = pObjProps->GetObjectivePosition();
		objective.bSecret = pObjProps->GetObjectiveSecretFlag();
		objective.nAnchorScriptID = pObjProps->GetObjectiveScriptID();
		rpgStats.objectives.push_back( objective );
	}
}

void CMissionFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	NI_ASSERT( pRootItem != 0 );
	SMissionStats rpgStats;
	if ( !bNewProjectJustCreated )
		FillRPGStats( rpgStats, pRootItem, pszProjectName );
	else
		GetRPGStats( rpgStats, pRootItem );
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CMissionFrame::GetRPGStats( const SMissionStats &rpgStats, CTreeItem *pRootItem )
{
	CMissionCommonPropsItem *pCommonProps = static_cast<CMissionCommonPropsItem *> ( pRootItem->GetChildItem( E_MISSION_COMMON_PROPS_ITEM ) );
	pCommonProps->SetHeaderText( rpgStats.szHeaderText.c_str() );
	pCommonProps->SetSubHeaderText( rpgStats.szSubheaderText.c_str() );
	pCommonProps->SetDescText( rpgStats.szDescriptionText.c_str() );
	pCommonProps->SetTemplateMap( rpgStats.szTemplateMap.c_str() );
	pCommonProps->SetFinalMap( rpgStats.szFinalMap.c_str() );
	pCommonProps->SetSettingName( rpgStats.szSettingName.c_str() );
	
	for ( int k=0; k<2; k++ )
	{
		CTreeItem *pMusics = pRootItem->GetChildItem( E_MISSION_MUSICS_ITEM, k );
		const std::vector<std::string> *pMusicVector = 0;
		if ( k == 0 )
			pMusicVector = &rpgStats.combatMusics;
		else if ( k == 1 )
			pMusicVector = &rpgStats.explorMusics;
		else
			NI_ASSERT( 0 );

		NI_ASSERT( pMusics->GetChildsCount() == pMusicVector->size() );
		int i = 0;
		for ( CTreeItem::CTreeItemList::const_iterator it=pMusics->GetBegin(); it!=pMusics->GetEnd(); ++it )
		{
			CMissionMusicPropsItem *pMusicProps = static_cast<CMissionMusicPropsItem *> ( it->GetPtr() );
			pMusicProps->SetMusicFileName( (*pMusicVector)[i].c_str() );
			i++;
		}
	}

	CTreeItem *pObjectives = pRootItem->GetChildItem( E_MISSION_OBJECTIVES_ITEM );
	NI_ASSERT( pObjectives->GetChildsCount() == rpgStats.objectives.size() );
	int i = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pObjectives->GetBegin(); it!=pObjectives->GetEnd(); ++it )
	{
		CMissionObjectivePropsItem *pObjProps = static_cast<CMissionObjectivePropsItem *> ( it->GetPtr() );
		pObjProps->SetObjeciveText( rpgStats.objectives[i].szHeader.c_str() );
		pObjProps->SetObjeciveText( rpgStats.objectives[i].szDescriptionText.c_str() );
		pObjProps->SetObjectivePosition( rpgStats.objectives[i].vPosOnMap );
		pObjProps->SetObjectiveSecretFlag( rpgStats.objectives[i].bSecret );
		pObjProps->SetObjectiveScriptID( rpgStats.objectives[i].nAnchorScriptID );
		i++;
	}
}

void CMissionFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	SMissionStats rpgStats;
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	GetRPGStats( rpgStats, pRootItem );
	
	if ( szProjectFileName.size() > 0 )
	{
		CString text;
		text.Format( "Generating map image..." );
		theApp.GetMainFrame()->m_wndStatusBar.SetWindowText( text );
		CMissionCommonPropsItem *pCommonProps = static_cast<CMissionCommonPropsItem *> ( pRootItem->GetChildItem( E_MISSION_COMMON_PROPS_ITEM ) );
		std::string szMapFileName, szTemp, szTemp1;
		szMapFileName = pCommonProps->GetFinalMap();
		if ( !szMapFileName.empty() )
		{
			szMapFileName = "Maps\\" + szMapFileName;
			if ( GetFileAttributes( (theApp.GetEditorDataDir() + szMapFileName + ".bzm").c_str() ) != -1 || GetFileAttributes( (theApp.GetEditorDataDir() + szMapFileName + ".xml").c_str() ) != -1 )
			{
				szTemp = "map";
				szTemp = GetDirectory( szProjectFileName.c_str() ) + szTemp;
				BeginWaitCursor();
				if ( GetFileAttributes( ( szTemp + "_h.dds" ).c_str() ) == -1 )
					CMinimapCreation::Create1Minimap( szMapFileName, szTemp );
				EndWaitCursor();
				szTemp1 = theApp.GetEditorTempDir();
				szTemp1 = szTemp1 + "map";
				MyCopyFile( (szTemp + "_h.dds").c_str(), (szTemp1 + "_h.dds").c_str() );
				szTemp1 = "editor\\temp\\";
				szTemp1 = szTemp1 + "map";
				LoadImageTexture( szTemp1.c_str() );
			}
		}
		text.Format( "Ready" );
		theApp.GetMainFrame()->m_wndStatusBar.SetWindowText( text );
	}
}

bool CMissionFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_MISSION_ROOT_ITEM );
	{
		CMissionCommonPropsItem *pCommonProps = static_cast<CMissionCommonPropsItem *> ( pRootItem->GetChildItem( E_MISSION_COMMON_PROPS_ITEM ) );
		std::string szErrorMsg;
		std::string szTemp;
		szTemp = pCommonProps->GetHeaderText();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify header text reference before exporting.\n";
		szTemp = pCommonProps->GetSubHeaderText();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify subheader text reference before exporting.\n";
		szTemp = pCommonProps->GetDescText();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify description text reference before exporting.\n";
		szTemp = pCommonProps->GetFinalMap();		
		if ( szTemp.empty() )
		{
			szTemp = pCommonProps->GetTemplateMap();
			if ( szTemp.empty() )
				szErrorMsg = "You should specify either template or final map reference before exporting.\n";
			else
			{
				szTemp = pCommonProps->GetSettingName();
				if ( szTemp.empty() )
					szErrorMsg = "You should specify setting reference before exporting.\n";
			}
		}
		CTreeItem *pMusics = pRootItem->GetChildItem( E_MISSION_MUSICS_ITEM, 0 );
		if ( pMusics->GetBegin() == pMusics->GetEnd() )
				szErrorMsg = "You should specify some combat music references before exporting.\n";
		else
			for ( CTreeItem::CTreeItemList::const_iterator it = pMusics->GetBegin(); it != pMusics->GetEnd(); ++it )
			{
				CMissionMusicPropsItem *pMusicProps = static_cast<CMissionMusicPropsItem *> ( it->GetPtr() );
				szTemp = pMusicProps->GetMusicFileName();
				if ( szTemp.empty() )
					szErrorMsg = "You should specify all combat music references before exporting.\n";
			}
		pMusics = pRootItem->GetChildItem( E_MISSION_MUSICS_ITEM, 0 );
		if ( pMusics->GetBegin() == pMusics->GetEnd() )
				szErrorMsg = "You should specify some exploration music references before exporting.\n";
		else
			for ( CTreeItem::CTreeItemList::const_iterator it = pMusics->GetBegin(); it != pMusics->GetEnd(); ++it )
			{
				CMissionMusicPropsItem *pMusicProps = static_cast<CMissionMusicPropsItem *> ( it->GetPtr() );
				szTemp = pMusicProps->GetMusicFileName();
				if ( szTemp.empty() )
					szErrorMsg = "You should specify all exploration music references before exporting.\n";
			}
		CTreeItem *pObjectives = pRootItem->GetChildItem( E_MISSION_OBJECTIVES_ITEM );
		if ( pObjectives->GetBegin() == pObjectives->GetEnd() )
				szErrorMsg = "You should specify some objectives before exporting.\n";
		for ( CTreeItem::CTreeItemList::const_iterator it=pObjectives->GetBegin(); it!=pObjectives->GetEnd(); ++it )
		{
			CMissionObjectivePropsItem *pObjProps = static_cast<CMissionObjectivePropsItem *> ( it->GetPtr() );
			szTemp = pObjProps->GetObjectiveHeader();
			if ( szTemp.empty() )
				szErrorMsg = "You should specify header text for all objectives before exporting.\n";
			szTemp = pObjProps->GetObjectiveText();
			if ( szTemp.empty() )
				szErrorMsg = "You should specify description text for all objectives before exporting.\n";
		}
		if ( !szErrorMsg.empty() )
		{
			AfxMessageBox( szErrorMsg.c_str() );
			return false;
		}
	}
	
	szPrefix = szAddDir + szPrevExportFileName.substr( 0, szPrevExportFileName.rfind('\\') + 1 );
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	
	szPrefix = theApp.GetDestDir() + szPrefix;
	CMissionCommonPropsItem *pCommonProps = static_cast<CMissionCommonPropsItem *> ( pRootItem->GetChildItem( E_MISSION_COMMON_PROPS_ITEM ) );
	std::string szTemp, szSource, szResult, szDir;
	szDir = GetDirectory( pszProjectName );
	szTemp = "map";
	if ( szTemp.length() > 0 )
	{
		MakeFullPath( szDir.c_str(), (szTemp + "_h.dds").c_str(), szSource );
		szResult = szPrefix + (szTemp + "_h.dds");
		MyCopyFile( szSource.c_str(), szResult.c_str() );
		MakeFullPath( szDir.c_str(), (szTemp + "_c.dds").c_str(), szSource );
		szResult = szPrefix + (szTemp + "_c.dds");
		MyCopyFile( szSource.c_str(), szResult.c_str() );
		MakeFullPath( szDir.c_str(), (szTemp + "_l.dds").c_str(), szSource );
		szResult = szPrefix + (szTemp + "_l.dds");
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}

	szTemp = pCommonProps->GetHeaderText();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".txt";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}

	szTemp = pCommonProps->GetSubHeaderText();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".txt";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}

	szTemp = pCommonProps->GetDescText();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".txt";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}
	
	szTemp = pCommonProps->GetFinalMap();
	if ( szTemp.length() > 0 )
	{
		std::string szMapName = theApp.GetEditorDataDir() + "maps\\" + szTemp;
		if ( GetFileAttributes( ( szMapName + ".bzm").c_str() ) == -1 )
		{
			CMapInfo mapInfo;
			CPtr<IDataStream> pXMLStream = OpenFileStream( (szMapName + ".xml").c_str(), STREAM_ACCESS_READ );
			if ( pXMLStream )
			{
				CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ );
				if ( pDT )
				{
					CTreeAccessor saver = pDT;
					saver.AddTypedSuper( &mapInfo );
					CPtr<IDataStream> pBZMStream = CreateFileStream( (szMapName + ".bzm").c_str(), STREAM_ACCESS_WRITE );
					if ( pBZMStream )
					{
						SQuickLoadMapInfo quickLoadMapInfo;
						quickLoadMapInfo.FillFromMapInfo( mapInfo );
						CPtr<IStructureSaver> pSS = CreateStructureSaver( pBZMStream, IStructureSaver::WRITE );
						CSaverAccessor saver = pSS;
						saver.Add( 1, &mapInfo );
						saver.Add( RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER, &quickLoadMapInfo );
					}
				}
			}	
		}
	}
	CTreeItem *pObjectives = pRootItem->GetChildItem( E_MISSION_OBJECTIVES_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pObjectives->GetBegin(); it!=pObjectives->GetEnd(); ++it )
	{
		CMissionObjectivePropsItem *pObjProps = static_cast<CMissionObjectivePropsItem *> ( it->GetPtr() );
		SMissionStats::SObjective objective;
		szTemp = pObjProps->GetObjectiveHeader();
		int nPos = szTemp.rfind('\\');
		if ( nPos != std::string::npos )
		{
			std::string szSubDir = szTemp.substr( 0, nPos + 1 );
			szSubDir = szPrefix + szSubDir;
			CreateStorage( szSubDir.c_str(), STREAM_ACCESS_WRITE );
		}
		if ( szTemp.length() > 0 )
		{
			szTemp += ".txt";
			MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
			szResult = szPrefix + szTemp;
			MyCopyFile( szSource.c_str(), szResult.c_str() );
		}
		
		szTemp = pObjProps->GetObjectiveText();
		nPos = szTemp.rfind('\\');
		if ( nPos != std::string::npos )
		{
			std::string szSubDir = szTemp.substr( 0, nPos + 1 );
			szSubDir = szPrefix + szSubDir;
			CreateStorage( szSubDir.c_str(), STREAM_ACCESS_WRITE );
		}
		if ( szTemp.length() > 0 )
		{
			szTemp += ".txt";
			MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
			szResult = szPrefix + szTemp;
			MyCopyFile( szSource.c_str(), szResult.c_str() );
		}
	}
	szPrefix = "";

	return true;
}

FILETIME CMissionFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_MISSION_ROOT_ITEM );

	FILETIME minTime, current;
	minTime.dwHighDateTime = -1;
	minTime.dwLowDateTime = -1;
	minTime = GetFileChangeTime( pszResultFileName );

	std::string szData = theApp.GetEditorDataDir();
	MakeSubRelativePath( szData.c_str(), pszResultFileName, szPrefix );
	szPrefix = szPrefix.substr( 0, szPrefix.rfind('\\') + 1 );
	
	szPrefix = theApp.GetEditorDataDir() + szPrefix;
	CMissionCommonPropsItem *pCommonProps = static_cast<CMissionCommonPropsItem *> ( pRootItem->GetChildItem( E_MISSION_COMMON_PROPS_ITEM ) );
	std::string szTemp, szResult;
	szTemp = pCommonProps->GetHeaderText();
	szTemp += ".txt";
	szResult = szPrefix + szTemp;
	current = GetFileChangeTime( szResult.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szTemp = pCommonProps->GetSubHeaderText();
	szTemp += ".txt";
	szResult = szPrefix + szTemp;
	current = GetFileChangeTime( szResult.c_str() );
	if ( current < minTime )
		minTime = current;

	szTemp = pCommonProps->GetDescText();
	szTemp += ".txt";
	szResult = szPrefix + szTemp;
	current = GetFileChangeTime( szResult.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szTemp = "map";
	szTemp += "_h.dds";
	szResult = szPrefix + szTemp;
	current = GetFileChangeTime( szResult.c_str() );
	if ( current < minTime )
		minTime = current;
	
	CTreeItem *pObjectives = pRootItem->GetChildItem( E_MISSION_OBJECTIVES_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pObjectives->GetBegin(); it!=pObjectives->GetEnd(); ++it )
	{
		CMissionObjectivePropsItem *pObjProps = static_cast<CMissionObjectivePropsItem *> ( it->GetPtr() );
		szTemp = pObjProps->GetObjectiveHeader();
		szTemp += ".txt";
		szResult = szPrefix + szTemp;
		current = GetFileChangeTime( szResult.c_str() );
		if ( current < minTime )
			minTime = current;

		szTemp = pObjProps->GetObjectiveText();
		szTemp += ".txt";
		szResult = szPrefix + szTemp;
		current = GetFileChangeTime( szResult.c_str() );
		if ( current < minTime )
			minTime = current;
	}
	
	return minTime;
}

FILETIME CMissionFrame::FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_MISSION_ROOT_ITEM );
	
	FILETIME maxTime, currentTime;
	maxTime.dwHighDateTime = 0;
	maxTime.dwLowDateTime = 0;

	CMissionCommonPropsItem *pCommonProps = static_cast<CMissionCommonPropsItem *> ( pRootItem->GetChildItem( E_MISSION_COMMON_PROPS_ITEM ) );
	std::string szTemp, szSource, szDir;
	szDir = GetDirectory( pszProjectName );

	szTemp = pCommonProps->GetHeaderText();
	szTemp += ".txt";
	MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
	currentTime = GetFileChangeTime( szSource.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szTemp = pCommonProps->GetSubHeaderText();
	szTemp += ".txt";
	MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
	currentTime = GetFileChangeTime( szSource.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szTemp = pCommonProps->GetDescText();
	szTemp += ".txt";
	MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
	currentTime = GetFileChangeTime( szSource.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szTemp = "map";
	szTemp += "_h.dds";
	MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
	currentTime = GetFileChangeTime( szSource.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	CTreeItem *pObjectives = pRootItem->GetChildItem( E_MISSION_OBJECTIVES_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pObjectives->GetBegin(); it!=pObjectives->GetEnd(); ++it )
	{
		CMissionObjectivePropsItem *pObjProps = static_cast<CMissionObjectivePropsItem *> ( it->GetPtr() );
		szTemp = pObjProps->GetObjectiveHeader();
		szTemp += ".txt";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		currentTime = GetFileChangeTime( szSource.c_str() );
		if ( currentTime > maxTime )
			maxTime = currentTime;

		szTemp = pObjProps->GetObjectiveText();
		szTemp += ".txt";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		currentTime = GetFileChangeTime( szSource.c_str() );
		if ( currentTime > maxTime )
			maxTime = currentTime;
	}

	return maxTime;
}

void CMissionFrame::SetActiveObjective( CMissionObjectivePropsItem *pObjective )
{
	pActiveObjective = pObjective;
	if ( pActiveObjective == 0 )
	{
		bShowKrest = false;
	}
	else
	{
		bShowKrest = true;
		vKrestPos = pActiveObjective->GetObjectivePosition();
	}

	GFXDraw();
}

void CMissionFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
}

void CMissionFrame::SpecificClearBeforeBatchMode()
{
	bShowKrest = false;
	pActiveObjective = 0;
	pImageTexture = 0;
	vImageSize = VNULL2;
	GFXDraw();
}

void CMissionFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
	SetCapture();
	if ( point.x < 0 )
		point.x = 0;
	if ( point.x > vImageSize.x )
		point.x = vImageSize.x;
	if ( point.y < 0 )
		point.y = 0;
	if ( point.y > vImageSize.y )
		point.y = vImageSize.y;

	if ( pActiveObjective )
	{
		pActiveObjective->SetObjectivePosition( CVec2( point.x + m_wndHScrollBar.GetScrollPos(), point.y + m_wndVScrollBar.GetScrollPos() ) );
		vKrestPos = pActiveObjective->GetObjectivePosition();
		pOIDockBar->SetItemProperty( pActiveObjective->GetItemName(), pActiveObjective );
		GFXDraw();
		SetChangedFlag( true );
	}
	
	CImageFrame::OnLButtonDown(nFlags, point);
}

void CMissionFrame::OnLButtonUp(UINT nFlags, CPoint point)
{
	ReleaseCapture();
	CImageFrame::OnLButtonUp(nFlags, point);
}

void CMissionFrame::OnMouseMove(UINT nFlags, CPoint point)
{
	if ( point.x < 0 )
		point.x = 0;
	if ( point.x > vImageSize.x )
		point.x = vImageSize.x;
	if ( point.y < 0 )
		point.y = 0;
	if ( point.y > vImageSize.y )
		point.y = vImageSize.y;

	if ( nFlags & MK_LBUTTON && pActiveObjective )
	{
		pActiveObjective->SetObjectivePosition( CVec2( point.x + m_wndHScrollBar.GetScrollPos(), point.y + m_wndVScrollBar.GetScrollPos() ) );
		vKrestPos = pActiveObjective->GetObjectivePosition();
		pOIDockBar->SetItemProperty( pActiveObjective->GetItemName(), pActiveObjective );
		GFXDraw();
		SetChangedFlag( true );
	}
	
	CImageFrame::OnMouseMove(nFlags, point);
}
void CMissionFrame::OnUpdateGenerateImage(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}
void CMissionFrame::OnGenerateImage() 
{
	CString text;
	text.Format( "Generating map image..." );
	theApp.GetMainFrame()->m_wndStatusBar.SetWindowText( text );
	if ( szProjectFileName.size() > 0 )
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		if ( pTree == 0 )
			return;
		CTreeItem *pRoot = pTree->GetRootItem();
		if ( pRoot == 0 )
			return;
		CMissionCommonPropsItem *pCommonProps = static_cast<CMissionCommonPropsItem *> ( pRoot->GetChildItem( E_MISSION_COMMON_PROPS_ITEM ) );
		std::string szMapFileName, szTemp, szTemp1;
		szMapFileName = pCommonProps->GetFinalMap();
		szMapFileName = "Maps\\" + szMapFileName;
		if ( GetFileAttributes( (theApp.GetEditorDataDir() + szMapFileName + ".bzm").c_str() ) != -1 || GetFileAttributes( (theApp.GetEditorDataDir() + szMapFileName + ".xml").c_str() ) != -1 )
		{
			szTemp = "map";
			szTemp = GetDirectory( szProjectFileName.c_str() ) + szTemp;
			BeginWaitCursor();
			CMinimapCreation::Create1Minimap( szMapFileName, szTemp );
			EndWaitCursor();
			szTemp1 = theApp.GetEditorTempDir();
			szTemp1 = szTemp1 + "map";
			MyCopyFile( (szTemp + "_h.dds").c_str(), (szTemp1 + "_h.dds").c_str() );
			szTemp1 = "editor\\temp\\";
			szTemp1 = szTemp1 + "map";
			LoadImageTexture( szTemp1.c_str() );
		}
		else
			AfxMessageBox( "No map file found!" ); 
	}
	text.Format( "Ready" );
	theApp.GetMainFrame()->m_wndStatusBar.SetWindowText( text );
}
