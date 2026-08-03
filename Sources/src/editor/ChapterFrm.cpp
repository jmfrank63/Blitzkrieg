#include "StdAfx.h"
#include <io.h>
#include "../Common/LegacyUiCompat.h"

#include "../GFX/GFX.H"
#include "../Scene/Scene.h"
#include "../Anim/Animation.h"
#include "../Main/rpgstats.h"

#include "editor.h"
#include "MainFrm.h"
#include "BuildCompose.h"			//��� ���������� �������� � ��������
#include "TreeDockWnd.h"
#include "PropView.h"
#include "TreeItem.h"
#include "ChapterFrm.h"
#include "ImageView.h"
#include "ChapterTreeItem.h"
#include "GameWnd.h"
#include "frames.h"


static const int zeroSizeX = 32;
static const int zeroSizeY = 32;
static const float zeroShiftX = 15.4f;
static const float zeroShiftY = 15.4f;


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(CChapterFrame, CImageFrame)

BEGIN_MESSAGE_MAP(CChapterFrame, CImageFrame)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_COMMAND(ID_SHOW_CROSSES, OnShowCrosses)
	ON_UPDATE_COMMAND_UI(ID_SHOW_CROSSES, OnUpdateShowCrosses)
END_MESSAGE_MAP()


CChapterFrame::CChapterFrame()
{
	szComposerName = "Chapter Editor";
	szExtension = "*.chc";
	szComposerSaveName = "Chapter_Composer_Project";
	nTreeRootItemID = E_CHAPTER_ROOT_ITEM;
	nFrameType = CFrameManager::E_CHAPTER_FRAME;
	pWndView = new CImageView;
	szAddDir = "scenarios\\";

	pActiveMission = 0;
	pActivePlaceHolder = 0;
	bEditCrosses = false;
	m_nCompressedFormat = GFXPF_DXT3;
	m_nLowFormat = GFXPF_ARGB4444;
}

CChapterFrame::~CChapterFrame()
{
}

int CChapterFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CImageFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );
	
	return 0;
}


void CChapterFrame::FillRPGStats( SChapterStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName )
{
	CChapterCommonPropsItem *pCommonProps = static_cast<CChapterCommonPropsItem *> ( pRootItem->GetChildItem( E_CHAPTER_COMMON_PROPS_ITEM ) );
	std::string szTemp, szTT;
	szTemp = szPrefix + pCommonProps->GetHeaderText();
	rpgStats.szHeaderText = szPrefix + pCommonProps->GetHeaderText();
	rpgStats.szSubheaderText = szPrefix + pCommonProps->GetSubHeaderText();
	rpgStats.szDescriptionText = szPrefix + pCommonProps->GetDescText();
	rpgStats.szMapImage = szPrefix + pCommonProps->GetMapImage();

	{
		szTemp = pCommonProps->GetMapImage();
		szTemp += ".tga";
		MakeFullPath( GetDirectory( pszProjectName ).c_str(), szTemp.c_str(), szTT );
		rpgStats.mapImageRect = GetImageSize( szTT.c_str() );
	}

	rpgStats.szScript = szPrefix + pCommonProps->GetScriptFile();
	rpgStats.szInterfaceMusic = szPrefix + pCommonProps->GetInterfaceMusic();
	rpgStats.nSeason = pCommonProps->GetSeason();

	rpgStats.szSettingName = pCommonProps->GetSettingName();
	rpgStats.szContextName = szPrefix + pCommonProps->GetContextName();
	rpgStats.szSideName = pCommonProps->GetPlayerSideName();
	rpgStats.szMODName = theApp.GetMODName();
	rpgStats.szMODVersion = theApp.GetMODVersion();

/*
	CTreeItem *pMusics = pRootItem->GetChildItem( E_CHAPTER_MUSICS_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pMusics->GetBegin(); it!=pMusics->GetEnd(); ++it )
	{
		CChapterMusicPropsItem *pMusicProps = static_cast<CChapterMusicPropsItem *> ( it->GetPtr() );
		SChapterStats::SMusic music;
		music.szMusic = pMusicProps->GetMusicFileName();
		music.fProbability = pMusicProps->GetMusicProbability();
		music.bCombat = pMusicProps->GetMusicCombatFlag();
		rpgStats.missionmusics.push_back( music );
	}
*/

	CTreeItem *pMissions = pRootItem->GetChildItem( E_CHAPTER_MISSIONS_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pMissions->GetBegin(); it!=pMissions->GetEnd(); ++it )
	{
		CChapterMissionPropsItem *pMissionProps = static_cast<CChapterMissionPropsItem *> ( it->GetPtr() );
		SChapterStats::SMission mission;
		mission.szMission = pMissionProps->GetMissionName();
		mission.vPosOnMap = pMissionProps->GetMissionPosition();
		rpgStats.missions.push_back( mission );
	}

	CTreeItem *pPlaceHolders = pRootItem->GetChildItem( E_CHAPTER_PLACES_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pPlaceHolders->GetBegin(); it!=pPlaceHolders->GetEnd(); ++it )
	{
		CChapterPlacePropsItem *pPlaceProps = static_cast<CChapterPlacePropsItem *> ( it->GetPtr() );
		SChapterStats::SPlaceHolder place;
		place.vPosOnMap = pPlaceProps->GetPosition();
		rpgStats.placeHolders.push_back( place );
	}
}

void CChapterFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	NI_ASSERT( pRootItem != 0 );
	SChapterStats rpgStats;
	if ( !bNewProjectJustCreated )
		FillRPGStats( rpgStats, pRootItem, pszProjectName );
	else
		GetRPGStats( rpgStats, pRootItem );
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CChapterFrame::GetRPGStats( const SChapterStats &rpgStats, CTreeItem *pRootItem )
{
	CChapterCommonPropsItem *pCommonProps = static_cast<CChapterCommonPropsItem *> ( pRootItem->GetChildItem( E_CHAPTER_COMMON_PROPS_ITEM ) );
	pCommonProps->SetHeaderText( rpgStats.szHeaderText.c_str() );
	pCommonProps->SetSubHeaderText( rpgStats.szSubheaderText.c_str() );
	pCommonProps->SetDescText( rpgStats.szDescriptionText.c_str() );
	pCommonProps->SetMapImage( rpgStats.szMapImage.c_str() );
	pCommonProps->SetScriptFile( rpgStats.szScript.c_str() );
	pCommonProps->SetInterfaceMusic( rpgStats.szInterfaceMusic.c_str() );
	pCommonProps->SetSeason( rpgStats.nSeason );
	pCommonProps->SetSettingName( rpgStats.szSettingName.c_str() );
	pCommonProps->SetContextName( rpgStats.szContextName.c_str() );
	pCommonProps->SetPlayerSideName( rpgStats.szSideName.c_str() );
	
/*
	CTreeItem *pMusics = pRootItem->GetChildItem( E_CHAPTER_MUSICS_ITEM );
	NI_ASSERT( pMusics->GetChildsCount() == rpgStats.missionmusics.size() );
	int i = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pMusics->GetBegin(); it!=pMusics->GetEnd(); ++it )
	{
		CChapterMusicPropsItem *pMusicProps = static_cast<CChapterMusicPropsItem *> ( it->GetPtr() );
		pMusicProps->SetMusicFileName( rpgStats.missionmusics[i].szMusic.c_str() );
		pMusicProps->SetMusicProbability( rpgStats.missionmusics[i].fProbability );
		pMusicProps->SetMusicCombatFlag( rpgStats.missionmusics[i].bCombat );
		i++;
	}
*/

	CTreeItem *pMissions = pRootItem->GetChildItem( E_CHAPTER_MISSIONS_ITEM );
	NI_ASSERT( pMissions->GetChildsCount() == rpgStats.missions.size() );
	int i = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pMissions->GetBegin(); it!=pMissions->GetEnd(); ++it )
	{
		CChapterMissionPropsItem *pMissionProps = static_cast<CChapterMissionPropsItem *> ( it->GetPtr() );
		pMissionProps->SetMissionName( rpgStats.missions[i].szMission.c_str() );
		pMissionProps->SetMissionPosition( rpgStats.missions[i].vPosOnMap );
		i++;
	}
	
	CTreeItem *pPlaces = pRootItem->GetChildItem( E_CHAPTER_PLACES_ITEM );
	NI_ASSERT( pPlaces->GetChildsCount() == rpgStats.placeHolders.size() );
	i = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pPlaces->GetBegin(); it!=pPlaces->GetEnd(); ++it )
	{
		CChapterPlacePropsItem *pPlaceProps = static_cast<CChapterPlacePropsItem *> ( it->GetPtr() );
		pPlaceProps->SetPosition( rpgStats.placeHolders[i].vPosOnMap );
		i++;
	}
}

void CChapterFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	SChapterStats rpgStats;
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	GetRPGStats( rpgStats, pRootItem );
	
	if ( szProjectFileName.size() > 0 )
	{
		CChapterCommonPropsItem *pCommonProps = static_cast<CChapterCommonPropsItem *> ( pRootItem->GetChildItem( E_CHAPTER_COMMON_PROPS_ITEM ) );
		std::string szMapFileName, szTemp;
		szTemp = pCommonProps->GetMapImage();
		szTemp += ".tga";
		MakeFullPath( GetDirectory( szProjectFileName.c_str() ).c_str(), szTemp.c_str(), szMapFileName );
		LoadImageTexture( szMapFileName.c_str() );
	}
}

bool CChapterFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_CHAPTER_ROOT_ITEM );
	{
		CChapterCommonPropsItem *pCommonProps = static_cast<CChapterCommonPropsItem *> ( pRootItem->GetChildItem( E_CHAPTER_COMMON_PROPS_ITEM ) );
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
		szTemp = pCommonProps->GetMapImage();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify map image reference before exporting.\n";
		szTemp = pCommonProps->GetScriptFile();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify script reference before exporting.\n";
		szTemp = pCommonProps->GetInterfaceMusic();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify interface music reference before exporting.\n";
		szTemp = pCommonProps->GetSettingName();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify setting reference before exporting.\n";
		szTemp = pCommonProps->GetSeason();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify season before exporting.\n";
		szTemp = pCommonProps->GetContextName();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify context reference before exporting.\n";
		szTemp = pCommonProps->GetPlayerSideName();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify player side before exporting.\n";
		CTreeItem *pMissions = pRootItem->GetChildItem( E_CHAPTER_MISSIONS_ITEM );
		if ( pMissions->GetBegin() == pMissions->GetEnd() )
			szErrorMsg = "You should specify some mission references before exporting.\n";
		for ( CTreeItem::CTreeItemList::const_iterator it = pMissions->GetBegin(); it != pMissions->GetEnd(); ++it )
		{
			CChapterMissionPropsItem *pMissionProps = static_cast<CChapterMissionPropsItem *> ( it->GetPtr() );
			szTemp = pMissionProps->GetMissionName();
			if ( szTemp.empty() )
				szErrorMsg = "You should specify all mission references before exporting.\n";
		}
		CTreeItem *pPlaceHolders = pRootItem->GetChildItem( E_CHAPTER_PLACES_ITEM );
		if ( pPlaceHolders->GetBegin() == pPlaceHolders->GetEnd() )
			szErrorMsg = "You should specify some placeholders before exporting.\n";
		if ( !szErrorMsg.empty() )
		{
			AfxMessageBox( szErrorMsg.c_str() );
			return false;
		}
	}

	szPrefix = szAddDir + szPrevExportFileName.substr( 0, szPrevExportFileName.rfind('\\') + 1 );
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	
	szPrefix = theApp.GetDestDir() + szPrefix;
	CChapterCommonPropsItem *pCommonProps = static_cast<CChapterCommonPropsItem *> ( pRootItem->GetChildItem( E_CHAPTER_COMMON_PROPS_ITEM ) );
	std::string szTemp, szSource, szResult, szDir;
	szDir = GetDirectory( pszProjectName );
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

	szTemp = pCommonProps->GetMapImage();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".tga";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + pCommonProps->GetMapImage();
		if ( !ComposeImageToTexture( szSource.c_str(), szResult.c_str() ) )
			AfxMessageBox( "Error: ComposeImageToTexture() FAILED" );
	}
	
	szTemp = pCommonProps->GetScriptFile();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".lua";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}

/*
	szTemp = pCommonProps->GetSettingName();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".xml";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
		}
*/

	szTemp = pCommonProps->GetContextName();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".xml";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}
	
/*
	CTreeItem *pMissions = pRootItem->GetChildItem( E_CHAPTER_MISSIONS_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pMissions->GetBegin(); it!=pMissions->GetEnd(); ++it )
	{
		CChapterMissionPropsItem *pMissionProps = static_cast<CChapterMissionPropsItem *> ( it->GetPtr() );
		szTemp = pMissionProps->GetMissionBonus();
		if ( szTemp.length() > 0 )
		{
			szTemp += ".txt";
			MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
			szResult = szPrefix + szTemp;
			MyCopyFile( szSource.c_str(), szResult.c_str() );
		}
	}
*/
	
	szPrefix = "";
	return true;
}

FILETIME CChapterFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
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
	CChapterCommonPropsItem *pCommonProps = static_cast<CChapterCommonPropsItem *> ( pRootItem->GetChildItem( E_CHAPTER_COMMON_PROPS_ITEM ) );
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
	
	szTemp = pCommonProps->GetMapImage();
	szTemp += ".tga";
	szResult = szPrefix + szTemp;
	current = GetFileChangeTime( szResult.c_str() );
	if ( current < minTime )
		minTime = current;

	szTemp = pCommonProps->GetScriptFile();
	szTemp += ".lua";
	szResult = szPrefix + szTemp;
	current = GetFileChangeTime( szResult.c_str() );
	if ( current < minTime )
		minTime = current;
	
/*
	szTemp = pCommonProps->GetSettingName();
	szTemp += ".xml";
	szResult = szPrefix + szTemp;
	current = GetFileChangeTime( szResult.c_str() );
	if ( current < minTime )
		minTime = current;
*/

	szPrefix = "";
	return minTime;
}

FILETIME CChapterFrame::FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_MISSION_ROOT_ITEM );
	
	FILETIME maxTime, currentTime;
	maxTime.dwHighDateTime = 0;
	maxTime.dwLowDateTime = 0;

	CChapterCommonPropsItem *pCommonProps = static_cast<CChapterCommonPropsItem *> ( pRootItem->GetChildItem( E_CHAPTER_COMMON_PROPS_ITEM ) );
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
	
	szTemp = pCommonProps->GetMapImage();
	szTemp += ".tga";
	MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
	currentTime = GetFileChangeTime( szSource.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szTemp = pCommonProps->GetScriptFile();
	szTemp += ".lua";
	MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
	currentTime = GetFileChangeTime( szSource.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
/*
	szTemp = pCommonProps->GetSettingName();
	szTemp += ".xml";
	MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
	currentTime = GetFileChangeTime( szSource.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
*/

	return maxTime;
}

void CChapterFrame::SetActiveMission( CChapterMissionPropsItem *pMission )
{
	pActiveMission = pMission;
	if ( pActiveMission == 0 )
	{
		bShowKrest = false;
	}
	else
	{
		SetActivePlaceHolder( 0 );
		bShowKrest = true;
		vKrestPos = pActiveMission->GetMissionPosition();
	}
	GFXDraw();
}

void CChapterFrame::SetActivePlaceHolder( CChapterPlacePropsItem *pPlace )
{
	pActivePlaceHolder = pPlace;
	if ( pActivePlaceHolder == 0 )
	{
		bShowKrest = false;
	}
	else
	{
		SetActiveMission( 0 );
		bShowKrest = true;
		vKrestPos = pActivePlaceHolder->GetPosition();
	}
	GFXDraw();
}

void CChapterFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
}

void CChapterFrame::SpecificClearBeforeBatchMode()
{
	bShowKrest = false;
	pActiveMission = 0;
	pActivePlaceHolder = 0;
	pImageTexture = 0;
	vImageSize = VNULL2;
	GFXDraw();
}

void CChapterFrame::FindActiveCross( const CPoint &point )
{
	pActiveMission = 0;
	pActivePlaceHolder = 0;
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	
	CChapterMissionsItem *pMissions = static_cast<CChapterMissionsItem *> ( pRootItem->GetChildItem( E_CHAPTER_MISSIONS_ITEM ) );
	for ( CTreeItem::CTreeItemList::const_iterator it=pMissions->GetBegin(); it!=pMissions->GetEnd(); ++it )
	{
		CChapterMissionPropsItem *pProps = static_cast<CChapterMissionPropsItem *> ( it->GetPtr() );
		CVec2 vPos = pProps->GetMissionPosition();
		CVec2 vBegin( vPos.x-zeroShiftX-m_wndHScrollBar.GetScrollPos(), vPos.y-zeroShiftY-m_wndVScrollBar.GetScrollPos() );
		if ( point.x >= vBegin.x && point.x <= vBegin.x + zeroSizeX && point.y >= vBegin.y && point.y <= vBegin.y + zeroSizeY )
		{
			pActiveMission = pProps;
			vCrossShift.x = point.x - vBegin.x - zeroShiftX;
			vCrossShift.y = point.y - vBegin.y - zeroShiftY;
			return;
		}
	}
	
	CChapterPlacesItem *pPlaces = static_cast<CChapterPlacesItem *> ( pRootItem->GetChildItem( E_CHAPTER_PLACES_ITEM ) );
	for ( CTreeItem::CTreeItemList::const_iterator it=pPlaces->GetBegin(); it!=pPlaces->GetEnd(); ++it )
	{
		CChapterPlacePropsItem *pProps = static_cast<CChapterPlacePropsItem *> ( it->GetPtr() );
		CVec2 vPos = pProps->GetPosition();
		CVec2 vBegin( vPos.x-zeroShiftX-m_wndHScrollBar.GetScrollPos(), vPos.y-zeroShiftY-m_wndVScrollBar.GetScrollPos() );
		if ( point.x >= vBegin.x && point.x <= vBegin.x + zeroSizeX && point.y >= vBegin.y && point.y <= vBegin.y + zeroSizeY )
		{
			pActivePlaceHolder = pProps;
			vCrossShift.x = point.x - vBegin.x - zeroShiftX;
			vCrossShift.y = point.y - vBegin.y - zeroShiftY;
			return;
		}
	}
}

void CChapterFrame::OnLButtonDown(UINT nFlags, CPoint point) 
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

	if ( bEditCrosses )
	{
		FindActiveCross( point );
	}
	else
	{
		if ( pActiveMission )
		{
			pActiveMission->SetMissionPosition( CVec2( point.x + m_wndHScrollBar.GetScrollPos(), point.y + m_wndVScrollBar.GetScrollPos() ) );
			vKrestPos = pActiveMission->GetMissionPosition();
			pOIDockBar->SetItemProperty( pActiveMission->GetItemName(), pActiveMission );
			GFXDraw();
			SetChangedFlag( true );
		}
		
		if ( pActivePlaceHolder )
		{
			pActivePlaceHolder->SetPosition( CVec2( point.x + m_wndHScrollBar.GetScrollPos(), point.y + m_wndVScrollBar.GetScrollPos() ) );
			vKrestPos = pActivePlaceHolder->GetPosition();
			pOIDockBar->SetItemProperty( pActivePlaceHolder->GetItemName(), pActivePlaceHolder );
			GFXDraw();
			SetChangedFlag( true );
		}
	}

	CImageFrame::OnLButtonDown(nFlags, point);
}

void CChapterFrame::OnLButtonUp(UINT nFlags, CPoint point)
{
	ReleaseCapture();
	if ( bEditCrosses )
	{
		pActiveMission = 0;
		pActivePlaceHolder = 0;
	}
	CImageFrame::OnLButtonUp(nFlags, point);
}

void CChapterFrame::OnMouseMove(UINT nFlags, CPoint point)
{
	if ( bEditCrosses )
	{
		if ( nFlags & MK_LBUTTON && pActiveMission )
		{
			CVec2 vPos( point.x + m_wndHScrollBar.GetScrollPos() - vCrossShift.x, point.y + m_wndVScrollBar.GetScrollPos() - vCrossShift.y );
			if ( vPos.x < 0 )
				vPos.x = 0;
			if ( vPos.x > vImageSize.x )
				vPos.x = vImageSize.x;
			if ( vPos.y < 0 )
				vPos.y = 0;
			if ( vPos.y > vImageSize.y )
				vPos.y = vImageSize.y;
			pActiveMission->SetMissionPosition( vPos );
			GFXDraw();
			SetChangedFlag( true );
		}

		if ( nFlags & MK_LBUTTON && pActivePlaceHolder )
		{
			CVec2 vPos( point.x + m_wndHScrollBar.GetScrollPos() - vCrossShift.x, point.y + m_wndVScrollBar.GetScrollPos() - vCrossShift.y );
			if ( vPos.x < 0 )
				vPos.x = 0;
			if ( vPos.x > vImageSize.x )
				vPos.x = vImageSize.x;
			if ( vPos.y < 0 )
				vPos.y = 0;
			if ( vPos.y > vImageSize.y )
				vPos.y = vImageSize.y;
			pActivePlaceHolder->SetPosition( vPos );
			GFXDraw();
			SetChangedFlag( true );
		}
	}
	else
	{
		if ( point.x < 0 )
			point.x = 0;
		if ( point.x > vImageSize.x )
			point.x = vImageSize.x;
		if ( point.y < 0 )
			point.y = 0;
		if ( point.y > vImageSize.y )
			point.y = vImageSize.y;

		if ( nFlags & MK_LBUTTON && pActiveMission )
		{
			pActiveMission->SetMissionPosition( CVec2( point.x + m_wndHScrollBar.GetScrollPos(), point.y + m_wndVScrollBar.GetScrollPos() ) );
			vKrestPos = pActiveMission->GetMissionPosition();
			pOIDockBar->SetItemProperty( pActiveMission->GetItemName(), pActiveMission );
			GFXDraw();
			SetChangedFlag( true );
		}
		
		if ( nFlags & MK_LBUTTON && pActivePlaceHolder )
		{
			pActivePlaceHolder->SetPosition( CVec2( point.x + m_wndHScrollBar.GetScrollPos(), point.y + m_wndVScrollBar.GetScrollPos() ) );
			vKrestPos = pActivePlaceHolder->GetPosition();
			pOIDockBar->SetItemProperty( pActivePlaceHolder->GetItemName(), pActivePlaceHolder );
			GFXDraw();
			SetChangedFlag( true );
		}
	}
	
	CImageFrame::OnMouseMove(nFlags, point);
}

void CChapterFrame::OnShowCrosses() 
{
	bEditCrosses = !bEditCrosses;
	pActiveMission = 0;
	pActivePlaceHolder = 0;
	
	/*	
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	if ( bEditCrosses )
		pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR)->SetButtonStyle( 0, TBBS_CHECKBOX | TBBS_CHECKED );
		else
		pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR)->SetButtonStyle( 0, TBBS_CHECKBOX );
	*/
	
	GFXDraw();
}

void CChapterFrame::OnUpdateShowCrosses(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}

void CChapterFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();
	
	pGFX->SetShadingEffect( 3 );
	SGFXRect2 rc;
	pGFX->SetTexture( 0, pImageTexture );
	rc.rect.x1 = vImagePos.x;
	rc.rect.y1 = vImagePos.y;
	rc.rect.x2 = rc.rect.x1 + vImageSize.x;
	rc.rect.y2 = rc.rect.y1 + vImageSize.y;
	rc.maps = rcImageMap;
	pGFX->SetupDirectTransform();
	pGFX->DrawRects( &rc, 1 );
	pGFX->RestoreTransform();
	
	if ( bEditCrosses )
	{
		pGFX->SetTexture( 0, pKrestTexture );
		rc.maps = CTRect<float> ( 0.0f, 0.0f, 1.0f, 1.0f );
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRootItem = pTree->GetRootItem();

		CChapterMissionsItem *pMissions = static_cast<CChapterMissionsItem *> ( pRootItem->GetChildItem( E_CHAPTER_MISSIONS_ITEM ) );
		for ( CTreeItem::CTreeItemList::const_iterator it=pMissions->GetBegin(); it!=pMissions->GetEnd(); ++it )
		{
			CChapterMissionPropsItem *pProps = static_cast<CChapterMissionPropsItem *> ( it->GetPtr() );
			CVec2 vPos = pProps->GetMissionPosition();
			CVec2 vBegin( vPos.x-zeroShiftX-m_wndHScrollBar.GetScrollPos(), vPos.y-zeroShiftY-m_wndVScrollBar.GetScrollPos() );
			rc.rect = CTRect<float> ( vBegin.x, vBegin.y, vBegin.x+zeroSizeX, vBegin.y+zeroSizeY );
			pGFX->SetupDirectTransform();
			pGFX->DrawRects( &rc, 1 );
			pGFX->RestoreTransform();
		}

		CChapterPlacesItem *pPlaces = static_cast<CChapterPlacesItem *> ( pRootItem->GetChildItem( E_CHAPTER_PLACES_ITEM ) );
		for ( CTreeItem::CTreeItemList::const_iterator it=pPlaces->GetBegin(); it!=pPlaces->GetEnd(); ++it )
		{
			CChapterPlacePropsItem *pProps = static_cast<CChapterPlacePropsItem *> ( it->GetPtr() );
			CVec2 vPos = pProps->GetPosition();
			CVec2 vBegin( vPos.x-zeroShiftX-m_wndHScrollBar.GetScrollPos(), vPos.y-zeroShiftY-m_wndVScrollBar.GetScrollPos() );
			rc.rect = CTRect<float> ( vBegin.x, vBegin.y, vBegin.x+zeroSizeX, vBegin.y+zeroSizeY );
			pGFX->SetupDirectTransform();
			pGFX->DrawRects( &rc, 1 );
			pGFX->RestoreTransform();
		}
	}
	else
	{
		if ( bShowKrest )
		{
			pGFX->SetTexture( 0, pKrestTexture );
			CVec2 vBegin( vKrestPos.x-zeroShiftX-m_wndHScrollBar.GetScrollPos(), vKrestPos.y-zeroShiftY-m_wndVScrollBar.GetScrollPos() );
			rc.rect = CTRect<float> ( vBegin.x, vBegin.y, vBegin.x+zeroSizeX, vBegin.y+zeroSizeY );
			rc.maps = CTRect<float> ( 0.0f, 0.0f, 1.0f, 1.0f );
			pGFX->SetupDirectTransform();
			pGFX->DrawRects( &rc, 1 );
			pGFX->RestoreTransform();
		}
	}
	
	pGFX->EndScene();
	pGFX->Flip();
}
