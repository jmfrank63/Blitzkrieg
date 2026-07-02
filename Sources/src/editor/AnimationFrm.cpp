#include "stdafx.h"

#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"
#include "..\Formats\fmtAnimation.h"

#include "editor.h"
#include "PropView.h"
#include "TreeItem.h"
#include "TreeDockWnd.h"
#include "AnimationFrm.h"
#include "GameWnd.h"
#include "frames.h"
#include "SpriteCompose.h"
#include "localization.h"

#define ID_ALL_DIR_THUMB_ITEMS  2000
#define ID_SELECTED_THUMB_ITEMS 2001
#define ID_UNITS_SCROLLBAR			2233

static const int THUMB_LIST_WIDTH = 145;


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(CAnimationFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CAnimationFrame, CParentFrame)
	ON_WM_SETFOCUS()
	ON_WM_CREATE()
	ON_COMMAND(ID_RUN_BUTTON, OnRunButton)
	ON_COMMAND(ID_STOP_BUTTON, OnStopButton)
	ON_UPDATE_COMMAND_UI(ID_RUN_BUTTON, OnUpdateRunButton)
	ON_UPDATE_COMMAND_UI(ID_STOP_BUTTON, OnUpdateStopButton)
	ON_COMMAND(ID_FILE_EXPORT_ONLY_RPG_STATS, OnFileExportOnlyRpgStats)
	ON_UPDATE_COMMAND_UI(ID_FILE_EXPORT_ONLY_RPG_STATS, OnUpdateFileExportOnlyRpgStats)
	ON_UPDATE_COMMAND_UI(ID_IMPORT_ACK_FILE, OnUpdateImportAckFile)
	ON_UPDATE_COMMAND_UI(ID_EXPORT_ACK_FILE, OnUpdateExportAckFile)
END_MESSAGE_MAP()



CAnimationFrame::CAnimationFrame() : m_wndSelectedThumbItems( true )
{
	szComposerName = "Infantry Editor";
	szExtension = "*.unt";
	szComposerSaveName = "Unit_Composer_Project";
	nTreeRootItemID = E_ANIMATION_ROOT_ITEM;
	nFrameType = CFrameManager::E_ANIMATION_FRAME;
	szAddDir = "units\\humans\\";

	m_pActiveDirTreeItem = 0;
	m_pActiveAnimation = 0;
	bRunning = false;
	bComposed = false;
	bExportOnlyRPGStats = false;
	
	pWndView = new CAnimationView;

	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB1555;
}

CAnimationFrame::~CAnimationFrame()
{
}

int CAnimationFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CParentFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );

	if (!pWndView->Create(NULL, NULL,  WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}


	if ( !m_wndAllDirThumbItems.Create( 0, "All dir thumb items", WS_CHILD | WS_VISIBLE,
		CRect(0, 0, 0, 0), pWndView, ID_ALL_DIR_THUMB_ITEMS) )
	{
		TRACE0("Failed to create All Dir Thumb Items window\n");
		return -1;
	}
	
	if ( !m_wndSelectedThumbItems.Create( 0, "Selected thumb items", WS_CHILD | WS_VISIBLE,
		CRect(0, 0, 0, 0), pWndView, ID_SELECTED_THUMB_ITEMS) )
	{
		TRACE0("Failed to create Selected thumb items window\n");
		return -1;
	}

	DWORD dwStyle = WS_CHILD | SBS_HORZ | SBS_TOPALIGN;
	if ( !m_wndScrollBar.Create( dwStyle, CRect( 0, 600, 800, 20 ), pWndView, ID_UNITS_SCROLLBAR ) )
	{
		TRACE0("Failed to create ScrollBar\n");
		return -1;
	}
	
	GenerateProjectName();
	return 0;
}


void CAnimationFrame::ShowFrameWindows( int nCommand )
{
	if ( bRunning )
		OnStopButton();
	CParentFrame::ShowFrameWindows( nCommand );

	if ( nCommand == SW_SHOW )
	{
		g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );
		m_wndScrollBar.ShowScrollBar( FALSE );

		ICamera *pCamera = GetSingleton<ICamera>();
		pCamera->SetAnchor( CVec3(24*fWorldCellSize, 24*fWorldCellSize, 0) );
		pCamera->Update();
		
		IGFX *pGFX = GetSingleton<IGFX>();
		pGFX->SetViewTransform( pCamera->GetPlacement() );
	}
}

BOOL CAnimationFrame::Run()
{
	if ( !bRunning )
		return FALSE;

	GFXDraw();
	g_frameManager.GetGameWnd()->ValidateRect( 0 );
	return TRUE;
}

void CAnimationFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();

	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );

	ICamera* pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
  pSG->Draw( pCamera );
	
	pGFX->EndScene();
	pGFX->Flip();
}

void CAnimationFrame::ViewSizeChanged()
{
	CRect viewRect;
	pWndView->GetClientRect( viewRect );
	
	RECT rc;
	rc.left = viewRect.left;
	rc.top = viewRect.bottom - THUMB_LIST_WIDTH;
	rc.right = viewRect.right;
	rc.bottom = viewRect.bottom;
	if ( m_wndSelectedThumbItems.GetSafeHwnd() )
	{
		m_wndSelectedThumbItems.MoveWindow( &rc );
	}
	
	rc.bottom = rc.top;
	rc.top = viewRect.top;
	if ( m_wndAllDirThumbItems.GetSafeHwnd() )
	{
		m_wndAllDirThumbItems.MoveWindow( &rc );
	}
}

void CAnimationFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
}

void CAnimationFrame::SpecificClearBeforeBatchMode()
{
	m_wndAllDirThumbItems.SetActiveThumbItems( 0, 0 );
	m_wndSelectedThumbItems.SetActiveThumbItems( 0, 0 );
	m_pActiveDirTreeItem = 0;
	m_pActiveAnimation = 0;
	bComposed = false;
}

void CAnimationFrame::ClickOnThumbList( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
	{
		if ( m_pActiveAnimation )
			m_pActiveAnimation->SelectMeInTheTree();
	}
	else if ( nID == ID_SELECTED_THUMB_ITEMS )
	{
		int nSel = m_wndSelectedThumbItems.GetSelectedItemIndex();
		if ( nSel == -1 )
			return;

		CTreeItem *pTreeItem = (CTreeItem *) m_wndSelectedThumbItems.GetUserDataForItem( nSel );
		pTreeItem->SelectMeInTheTree();
	}
}

void CAnimationFrame::DoubleClickOnThumbList( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
	{
		if ( !m_pActiveAnimation )
			return;
		SetChangedFlag( true );
		bComposed = false;

		int nAllIndex = m_wndAllDirThumbItems.GetSelectedItemIndex();
		if ( nAllIndex == -1 )
			return;
		string szItemName = m_wndAllDirThumbItems.GetItemName( nAllIndex );
		int nImage = m_wndAllDirThumbItems.GetItemImageIndex( nAllIndex );

		int nNewItemIndex = m_wndSelectedThumbItems.InsertItemToEnd( szItemName.c_str(), nImage );
		NI_ASSERT( nNewItemIndex != -1 );
		
		CUnitFramePropsItem *pFrame = new CUnitFramePropsItem();
		pFrame->SetItemName( szItemName.c_str() );
		m_pActiveAnimation->AddChild( pFrame );
		
		NI_ASSERT( pFrame != 0 );
		m_wndSelectedThumbItems.SetUserDataForItem( nNewItemIndex, (long) pFrame );
	}
}

void CAnimationFrame::SelectItemInSelectedThumbList( DWORD dwData )
{
	int nIndex = m_wndSelectedThumbItems.GetItemIndexWithUserData( dwData );
	m_wndSelectedThumbItems.SelectItem( nIndex );
}

void CAnimationFrame::DeleteFrameInSelectedList( DWORD dwData )
{
	SetChangedFlag( true );
	bComposed = false;
	int nIndex = m_wndSelectedThumbItems.GetItemIndexWithUserData( dwData );
	m_wndSelectedThumbItems.DeleteItem( nIndex );
}

void CAnimationFrame::DeleteFrameInTree( int nID )
{
	if ( nID == ID_ALL_DIR_THUMB_ITEMS )
		return;

	SetChangedFlag( true );
	bComposed = false;

	int nSel = m_wndSelectedThumbItems.GetSelectedItemIndex();
	if ( nSel == -1 )
		return;

	DWORD dwData = m_wndSelectedThumbItems.GetUserDataForItem( nSel );
	ASSERT( dwData != 0 );

	CTreeItem *pFrame = (CTreeItem *) dwData;
	NI_ASSERT( pFrame->GetItemType() == E_UNIT_FRAME_PROPS_ITEM );
	pFrame->DeleteMeInParentTreeItem();

	m_wndSelectedThumbItems.SelectItem( nSel + 1 );
	
	m_wndSelectedThumbItems.DeleteItem( nSel );
}

void CAnimationFrame::FillRPGStats( SInfantryRPGStats &rpgStats, CTreeItem *pRootItem )
{
	CUnitCommonPropsItem *pCommonProps = static_cast<CUnitCommonPropsItem *> ( pRootItem->GetChildItem( E_UNIT_COMMON_PROPS_ITEM ) );
	rpgStats.szKeyName = pCommonProps->GetUnitName();
	rpgStats.type = (EUnitRPGType) pCommonProps->GetUnitType();

	rpgStats.fMaxHP = pCommonProps->GetHealth();
	rpgStats.nMinArmor = rpgStats.nMaxArmor = pCommonProps->GetArmor();
	rpgStats.fSight = pCommonProps->GetSight();
	rpgStats.fCamouflage = pCommonProps->GetCamouflage();
	rpgStats.fSpeed = pCommonProps->GetSpeed();
	rpgStats.fPassability = pCommonProps->GetPassability();
	rpgStats.bCanAttackUp = pCommonProps->GetAttackUpFlag();
	rpgStats.bCanAttackDown = pCommonProps->GetAttackDownFlag();
	rpgStats.fPrice = pCommonProps->GetAIPrice();
	rpgStats.fSightPower = pCommonProps->GetSightPower();
	
	CUnitAcksItem *pAcks = static_cast<CUnitAcksItem *> ( pRootItem->GetChildItem( E_UNIT_ACKS_ITEM ) );
	NI_ASSERT( pAcks != 0 );
	rpgStats.szAcksNames.resize( 2 );
	rpgStats.szAcksNames[0] = pAcks->GetAckName();
	rpgStats.szAcksNames[1] = pAcks->GetAckName2();

	CUnitActionsItem *pActions = static_cast<CUnitActionsItem*>( pRootItem->GetChildItem( E_UNIT_ACTIONS_ITEM ) );
	pActions->GetActions( &rpgStats );

	CUnitExposuresItem *pExposures = static_cast<CUnitExposuresItem*>( pRootItem->GetChildItem( E_UNIT_EXPOSURES_ITEM ) );
	pExposures->GetExposures( &rpgStats );
	
	rpgStats.fRotateSpeed = 0.0f;
	rpgStats.nPriority = 0;
	rpgStats.nUninstallRotate = 0;
	rpgStats.nUninstallTransport = 0;
	

	CUnitWeaponPropsItem *pWeaponProps = static_cast<CUnitWeaponPropsItem *> ( pRootItem->GetChildItem( E_UNIT_WEAPON_PROPS_ITEM ) );
	rpgStats.guns.resize( 2 );
	if ( strlen( pWeaponProps->GetWeaponName() ) == 0 )
		rpgStats.guns[0].szWeapon = "generic";
	else
		rpgStats.guns[0].szWeapon = pWeaponProps->GetWeaponName();
	rpgStats.guns[0].nAmmo = pWeaponProps->GetAmmoCount();
	rpgStats.guns[0].fReloadCost = pWeaponProps->GetReloadCost();
	
	CUnitGrenadePropsItem *pGrenadeProps = static_cast<CUnitGrenadePropsItem *> ( pRootItem->GetChildItem( E_UNIT_GRENADE_PROPS_ITEM ) );
	if ( string( "generic" ) == pGrenadeProps->GetGrenadeName() )
	{
		rpgStats.guns.resize( 1 );				//типа убиваю второй элемент, гранату
	}
	else
	{
		if ( strlen( pGrenadeProps->GetGrenadeName() ) > 0 && !( strlen( pGrenadeProps->GetGrenadeName() ) == 1 && pGrenadeProps->GetGrenadeName()[0] == '_' ) )
		{
			rpgStats.guns[1].szWeapon = pGrenadeProps->GetGrenadeName();
			rpgStats.guns[1].nAmmo = pGrenadeProps->GetAmmoCount();
			rpgStats.guns[1].fReloadCost = pGrenadeProps->GetReloadCost();
		}
		else
		{
			rpgStats.guns.resize( 1 );				//типа убиваю второй элемент, гранату
		}
	}

	CUnitAnimationsItem *pAnimsItem = static_cast<CUnitAnimationsItem *> ( pRootItem->GetChildItem( E_UNIT_ANIMATIONS_ITEM ) );
	NI_ASSERT( pAnimsItem != 0 );

	rpgStats.animtimes.resize( pAnimsItem->GetChildsCount() + 1 );		//CRAP +1
	CTreeItem::CTreeItemList::const_iterator animIt;
	int nFind = 0;
	for ( animIt=pAnimsItem->GetBegin(); animIt!=pAnimsItem->GetEnd(); ++animIt )
	{
		CUnitAnimationPropsItem *pAnimProps = (CUnitAnimationPropsItem *) animIt->GetPtr();
		rpgStats.animtimes[GetActionFromName( pAnimProps->GetItemName() )] = pAnimProps->GetFrameTime() * pAnimProps->GetChildsCount();

		std::string szAnimName = pAnimProps->GetItemName();
		if ( szAnimName == "Run" )
		{
			rpgStats.fRunSpeed = pAnimProps->GetAnimationSpeed();
			if ( nFind == 1 )
				break;
			nFind++;
		}

		if ( szAnimName == "Crawl" )
		{
			rpgStats.fCrawlSpeed = pAnimProps->GetAnimationSpeed();
			if ( nFind == 1 )
				break;
			nFind++;
		}
	}
	
	int nIndex = 0;
	rpgStats.animdescs.resize( ANIMATION_LAST_ANIMATION );
	for ( animIt=pAnimsItem->GetBegin(); animIt!=pAnimsItem->GetEnd(); ++animIt )
	{
		CUnitAnimationPropsItem *pAnimProps = (CUnitAnimationPropsItem *) animIt->GetPtr();
		SUnitBaseRPGStats::SAnimDesc desc;
		desc.nIndex = nIndex;
		desc.nAction = pAnimProps->GetActionFrame() * pAnimProps->GetFrameTime();
		desc.nLength = pAnimProps->GetChildsCount() * pAnimProps->GetFrameTime();
		desc.nAABB_A = -1;
		desc.nAABB_D = -1;
		rpgStats.animdescs[pAnimProps->GetAnimationType()].push_back( desc );
		nIndex++;
	}
}

void CAnimationFrame::InitDirNames()
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	CDirectoriesItem *pDirsItem = static_cast<CDirectoriesItem *> ( pRootItem->GetChildItem( E_UNIT_DIRECTORIES_ITEM ) );

	const char* szDirNames[] =
	{
		"RightUp\\",
		"Up\\",
		"LeftUp\\",
		"Left\\",
		"LeftDown\\",
		"Down\\",
		"RightDown\\",
		"Right\\",
	};

	int nSeason = 0;
	std::string szPrefix;
	std::string szName;
	for ( CTreeItem::CTreeItemList::const_iterator seasonIt=pDirsItem->GetBegin(); seasonIt!=pDirsItem->GetEnd(); ++seasonIt )
	{
		CUnitSeasonPropsItem *pSeasonProps = static_cast<CUnitSeasonPropsItem*> ( seasonIt->GetPtr() );
		switch ( nSeason )
		{
		case 0:
			szPrefix = "Summer\\";
			break;
		case 1:
			szPrefix = "Winter\\";
			break;
		case 2:
			szPrefix = "Africa\\";
			break;
		default:
			NI_ASSERT_T( 0, "Invalid number of seasons" );
		}

		int nDirIndex = 0;
		for ( CTreeItem::CTreeItemList::const_iterator it=pSeasonProps->GetBegin(); it!=pSeasonProps->GetEnd(); ++it )
		{
			CDirectoryPropsItem *pProps = static_cast<CDirectoryPropsItem *> ( it->GetPtr() );
			szName = szPrefix;
			szName += szDirNames[nDirIndex];
			pProps->SetDirName( szName.c_str() );
			nDirIndex++;
		}
		nSeason++;
	}

}

void CAnimationFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	ASSERT( !pDT->IsReading() );
	
	SInfantryRPGStats rpgStats;
	if ( !bNewProjectJustCreated )
		FillRPGStats( rpgStats, pRootItem );
	else
	{
		InitDirNames();
		GetRPGStats( rpgStats, pRootItem );
	}
	
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CAnimationFrame::GetRPGStats( const SInfantryRPGStats &rpgStats, CTreeItem *pRootItem )
{
	CUnitCommonPropsItem *pCommonProps = static_cast<CUnitCommonPropsItem *> ( pRootItem->GetChildItem( E_UNIT_COMMON_PROPS_ITEM ) );
	pCommonProps->SetUnitName( rpgStats.szKeyName.c_str() );
	pCommonProps->SetUnitType( rpgStats.type );
	
	pCommonProps->SetHealth( rpgStats.fMaxHP );
	pCommonProps->SetArmor( rpgStats.nMinArmor );
	pCommonProps->SetSight( rpgStats.fSight );
	pCommonProps->SetCamouflage( rpgStats.fCamouflage );
	pCommonProps->SetSpeed( rpgStats.fSpeed );
	pCommonProps->SetPassability( rpgStats.fPassability );
	pCommonProps->SetAttackUpFlag( rpgStats.bCanAttackUp );
	pCommonProps->SetAttackDownFlag( rpgStats.bCanAttackDown );
	pCommonProps->SetAIPrice( rpgStats.fPrice );
	pCommonProps->SetSightPower( rpgStats.fSightPower );
	
	int nFind = 0;
	CTreeItem *pAnimsItem = pRootItem->GetChildItem( E_UNIT_ANIMATIONS_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pAnimsItem->GetBegin(); it!=pAnimsItem->GetEnd(); ++it )
	{
		CUnitAnimationPropsItem *pProps = static_cast<CUnitAnimationPropsItem *> ( it->GetPtr() );
		std::string szAnimName = pProps->GetItemName();

		if ( szAnimName == "Run" )
		{
			pProps->SetAnimationSpeed( rpgStats.fRunSpeed );
			if ( nFind == 1 )
				break;
			nFind++;
		}

		if ( szAnimName == "Crawl" )
		{
			pProps->SetAnimationSpeed( rpgStats.fCrawlSpeed );
			if ( nFind == 1 )
				break;
			nFind++;
		}
	}

	CUnitAcksItem *pAcks = static_cast<CUnitAcksItem *> ( pRootItem->GetChildItem( E_UNIT_ACKS_ITEM ) );
	NI_ASSERT( pAcks != 0 );
	if ( rpgStats.szAcksNames.size() >= 1 )
		pAcks->SetAckName( rpgStats.szAcksNames[0].c_str() );
	if ( rpgStats.szAcksNames.size() >= 2 )
		pAcks->SetAckName2( rpgStats.szAcksNames[1].c_str() );

	CUnitActionsItem *pActions = static_cast<CUnitActionsItem*>( pRootItem->GetChildItem( E_UNIT_ACTIONS_ITEM ) );
	pActions->SetActions( &rpgStats );

	CUnitExposuresItem *pExposures = static_cast<CUnitExposuresItem*>( pRootItem->GetChildItem( E_UNIT_EXPOSURES_ITEM ) );
	pExposures->SetExposures( &rpgStats );
	
	if ( rpgStats.guns.size() > 0 )
	{
		CUnitWeaponPropsItem *pWeaponProps = static_cast<CUnitWeaponPropsItem *> ( pRootItem->GetChildItem( E_UNIT_WEAPON_PROPS_ITEM ) );
		pWeaponProps->SetWeaponName( rpgStats.guns[0].szWeapon.c_str() );
		pWeaponProps->SetAmmoCount( rpgStats.guns[0].nAmmo );
		pWeaponProps->SetReloadCost( rpgStats.guns[0].fReloadCost );
	}
	if ( rpgStats.guns.size() > 1 )
	{
		CUnitGrenadePropsItem *pGrenadeProps = static_cast<CUnitGrenadePropsItem *> ( pRootItem->GetChildItem( E_UNIT_GRENADE_PROPS_ITEM ) );
		pGrenadeProps->SetGrenadeName( rpgStats.guns[1].szWeapon.c_str() );
		pGrenadeProps->SetAmmoCount( rpgStats.guns[1].nAmmo );
		pGrenadeProps->SetReloadCost( rpgStats.guns[1].fReloadCost );
	}
}

void CAnimationFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	ASSERT( pDT->IsReading() );
	
	SInfantryRPGStats rpgStats;
	FillRPGStats( rpgStats, pRootItem );			//перед загрузкой инициализирую значени€ми по умолчанию
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	
	GetRPGStats( rpgStats, pRootItem );
}

void CAnimationFrame::OnFileExportOnlyRpgStats() 
{
	bExportOnlyRPGStats = true;
	OnFileExportFiles();
	bExportOnlyRPGStats = false;
}

void CAnimationFrame::OnUpdateFileExportOnlyRpgStats(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )			//≈сли уже был создан проект
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

void CAnimationFrame::OnUpdateImportAckFile(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
}

void CAnimationFrame::OnUpdateExportAckFile(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
}

bool CAnimationFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_ANIMATION_ROOT_ITEM );
	CAnimationTreeRootItem *pAnimRoot = (CAnimationTreeRootItem *) pRootItem;

	string szResDir = GetDirectory(pszResultFileName);
	if ( !bExportOnlyRPGStats )
		pAnimRoot->ComposeAnimations( pszProjectName, szResDir.c_str(), false, false );

	SaveRPGStats( pDT, pRootItem, pszProjectName );
	
	CLocalizationItem *pLocItem = static_cast<CLocalizationItem *> ( pRootItem->GetChildItem( E_LOCALIZATION_ITEM ) );
	NI_ASSERT( pLocItem != 0 );

	string szSrc, szRes;
	szRes = szResDir;
	szRes += "name.txt";
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pLocItem->GetLocalizationName(), szSrc );
	MyCopyFile( szSrc.c_str(), szRes.c_str() );

	szRes = szResDir;
	szRes += "desc.txt";
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pLocItem->GetLocalizationDesc(), szSrc );
	MyCopyFile( szSrc.c_str(), szRes.c_str() );
	
	szRes = szResDir;
	szRes += "stats.txt";
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pLocItem->GetLocalizationStats(), szSrc );
	MyCopyFile( szSrc.c_str(), szRes.c_str() );
	
	return true;
}

void CAnimationFrame::OnRunButton() 
{
	if ( !bComposed )
		if ( !ComposeAnimations() )
			return;
	
	if ( bRunning )
		return;
	bRunning = !bRunning;
	BeginWaitCursor();
	
	g_frameManager.GetGameWnd()->ShowWindow( SW_SHOW );
	m_wndAllDirThumbItems.ShowWindow( SW_HIDE );
	m_wndSelectedThumbItems.ShowWindow( SW_HIDE );
	
	m_wndScrollBar.ShowScrollBar();
	m_wndScrollBar.EnableScrollBar();
	SCROLLINFO info;
	info.fMask = SIF_PAGE|SIF_RANGE;
	info.nMin = 0;
	info.nPage = 14;
	info.nMax = 27;
	if ( !m_wndScrollBar.SetScrollInfo( &info ) )
		m_wndScrollBar.SetScrollRange( 0, 27 );
	m_wndScrollBar.SetScrollPos( 0 );


	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	NI_ASSERT( pTree != 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_ANIMATION_ROOT_ITEM );

	CTreeItem *pAnimsItem = pRootItem->GetChildItem( E_UNIT_ANIMATIONS_ITEM );
	NI_ASSERT( pAnimsItem != 0 );

	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();

	string szObjName = theApp.GetEditorTempResourceDir();
	GetSingleton<ICamera>()->Update();
	const CVec3 vCameraAnchor = GetSingleton<ICamera>()->GetAnchor();
	int nStartX = vCameraAnchor.x - 650;
	int nStartY = vCameraAnchor.y + 110;
	const int nSdvig = 25;
	units.clear();

	CTreeItem::CTreeItemList::const_iterator it = pAnimsItem->GetBegin();
	for ( ; it!=pAnimsItem->GetEnd(); ++it )
	{
		if ( (*it)->GetChildsCount() == 0 )
			continue;
		CUnitAnimationPropsItem *pAnimProps = static_cast<CUnitAnimationPropsItem *> ( it->GetPtr() );

		int x = nStartX;
		int y = nStartY;
		
		{
			SUnitObject singleUnit;
			std::vector<std::string> szAnimNames;
			
			for ( int i=0; i<pAnimProps->GetNumberOfDirections(); i++ )
			{
				CPtr<IObjVisObj> pObj = static_cast<IObjVisObj *> ( pVOB->BuildObject( (szObjName + "\\1").c_str(), 0, SGVOT_SPRITE ) );
				singleUnit.pUnit = pObj;
				singleUnit.vPos = CVec3(x, y, 0);
				units.push_back( singleUnit );
				pObj->SetPosition( singleUnit.vPos );
				pObj->SetDirection( i*65536/pAnimProps->GetNumberOfDirections() );
				pSG->AddObject( pObj, SGVOGT_UNIT );

				string szName = (*it)->GetItemName();
				NStr::ToLower( szName );
				szAnimNames.push_back( szName );

				pObj = static_cast<IObjVisObj *> ( pVOB->BuildObject( (szObjName + "\\1w").c_str(), 0, SGVOT_SPRITE ) );
				singleUnit.pUnit = pObj;
				singleUnit.vPos = CVec3(x + 240, y + 240, 0);
				units.push_back( singleUnit );
				pObj->SetPosition( singleUnit.vPos );
				pObj->SetDirection( i*65536/pAnimProps->GetNumberOfDirections() );
				pSG->AddObject( pObj, SGVOGT_UNIT );
				
				szName = (*it)->GetItemName();
				NStr::ToLower( szName );
				szAnimNames.push_back( szName );
				
				pObj = static_cast<IObjVisObj *> ( pVOB->BuildObject( (szObjName + "\\1a").c_str(), 0, SGVOT_SPRITE ) );
				singleUnit.pUnit = pObj;
				singleUnit.vPos = CVec3(x + 480, y + 480, 0);
				units.push_back( singleUnit );
				pObj->SetPosition( singleUnit.vPos );
				pObj->SetDirection( i*65536/pAnimProps->GetNumberOfDirections() );
				pSG->AddObject( pObj, SGVOGT_UNIT );
				
				szName = (*it)->GetItemName();
				NStr::ToLower( szName );
				szAnimNames.push_back( szName );
				
				if ( pAnimProps->GetNumberOfDirections() == 4 )
				{
					x += nSdvig * 2;
					y += nSdvig * 2;
				}
				else
				{
					x += nSdvig;
					y += nSdvig;
				}
			}
			for ( int i=0; i<szAnimNames.size(); ++i )
				units[units.size()-szAnimNames.size()+i].pUnit->SetAnimation( GetActionFromName(szAnimNames[i]) );
		}
		
		nStartX += nSdvig * 2;
		nStartY -= nSdvig * 2;
	}
	EndWaitCursor();
}

void CAnimationFrame::UpdateUnitsCoordinates()
{
	int nPos = m_wndScrollBar.GetScrollPos() * 10;
	CVec3 vPos;
	vPos.z = 0;
	for ( int i=0; i<units.size(); ++i )
	{
		vPos.x = units[i].vPos.x - nPos;
		vPos.y = units[i].vPos.y - nPos;
		units[i].pUnit->SetPosition( vPos );
	}
	GFXDraw();
}

void CAnimationFrame::OnStopButton() 
{
	if ( !bRunning )
		return;
	
	bRunning = !bRunning;
	
	m_wndScrollBar.ShowScrollBar( FALSE );
	g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );
	m_wndAllDirThumbItems.ShowWindow( SW_SHOW );
	m_wndSelectedThumbItems.ShowWindow( SW_SHOW );

	units.clear();
	IScene *pSG = GetSingleton<IScene>();
	pSG->Clear();
}

void CAnimationFrame::OnUpdateRunButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//≈сли проект не был создан
	{
		pCmdUI->Enable( false );
		return;
	}

	if ( bRunning )
		pCmdUI->Enable( false );
	else
		pCmdUI->Enable( true );
}

void CAnimationFrame::OnUpdateStopButton(CCmdUI* pCmdUI) 
{
	if ( bRunning )
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

bool CAnimationFrame::CopyRuntimeAnimationsToTemp()
{
	if ( szPrevExportFileName.empty() )
		return false;

	string szExportFileName;
	if ( IsRelatedPath( szPrevExportFileName.c_str() ) )
		MakeFullPath( (theApp.GetDestDir() + szAddDir).c_str(), szPrevExportFileName.c_str(), szExportFileName );
	else
		szExportFileName = szPrevExportFileName;

	string szSourceDir = GetDirectory( szExportFileName.c_str() );
	if ( szSourceDir.empty() )
		return false;

	string szDestDir = theApp.GetEditorTempDir();
	string szPattern = szSourceDir + "*.*";
	WIN32_FIND_DATA ffData;
	HANDLE hFind = ::FindFirstFile( szPattern.c_str(), &ffData );
	if ( hFind == INVALID_HANDLE_VALUE )
		return false;

	bool bCopied = false;
	do
	{
		if ( ffData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			continue;

		string szFileName = ffData.cFileName;
		string szLowerName = szFileName;
		NStr::ToLower( szLowerName );
		const bool bSpriteFile =
			(szLowerName.size() >= 4 && szLowerName.substr( szLowerName.size() - 4 ) == ".san") ||
			(szLowerName.size() >= 6 && szLowerName.substr( szLowerName.size() - 6 ) == "_h.dds") ||
			(szLowerName.size() >= 6 && szLowerName.substr( szLowerName.size() - 6 ) == "_l.dds") ||
			(szLowerName.size() >= 6 && szLowerName.substr( szLowerName.size() - 6 ) == "_c.dds");
		if ( !bSpriteFile )
			continue;

		if ( MyCopyFile( (szSourceDir + szFileName).c_str(), (szDestDir + szFileName).c_str() ) )
			bCopied = true;
	}
	while ( ::FindNextFile( hFind, &ffData ) );

	::FindClose( hFind );
	return bCopied;
}
bool CAnimationFrame::ComposeAnimations()
{
	
	BeginWaitCursor();
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	NI_ASSERT( pTree != 0 );
	
	CTreeItem *pRootItem = pTree->GetRootItem();
	NI_ASSERT( pRootItem != 0 );
	
	NI_ASSERT( pRootItem->GetItemType() == E_ANIMATION_ROOT_ITEM );
	CAnimationTreeRootItem *pAnimRoot = (CAnimationTreeRootItem *) pRootItem;
	string szTempDir = theApp.GetEditorTempDir();
	bool result = pAnimRoot->ComposeAnimations( szProjectFileName.c_str(), szTempDir.c_str(), true, true );
	if ( !result )
		result = CopyRuntimeAnimationsToTemp();
	bComposed = result;
	EndWaitCursor();
	return result;
}

bool CAnimationFrame::LoadRuntimePreviewThumbnails( CDirectoryPropsItem *pDirPropsItem )
{
	if ( pDirPropsItem == 0 || szPrevExportFileName.empty() )
		return false;

	string szExportFileName;
	if ( IsRelatedPath( szPrevExportFileName.c_str() ) )
		MakeFullPath( (theApp.GetDestDir() + szAddDir).c_str(), szPrevExportFileName.c_str(), szExportFileName );
	else
		szExportFileName = szPrevExportFileName;

	string szRuntimeDir = GetDirectory( szExportFileName.c_str() );
	string szDDSName = szRuntimeDir + "1_h.dds";
	string szSANName = szRuntimeDir + "1.san";
	if ( _access( szDDSName.c_str(), 04 ) != 0 || _access( szSANName.c_str(), 04 ) != 0 )
		return false;

	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	CPtr<IDataStream> pDDSStream = OpenFileStream( szDDSName.c_str(), STREAM_ACCESS_READ );
	if ( pDDSStream == 0 )
		return false;

	CPtr<IDDSImage> pDDSImage = pIP->LoadDDSImage( pDDSStream );
	if ( pDDSImage == 0 )
		return false;

	CPtr<IImage> pTextureImage = pIP->Decompress( pDDSImage );
	if ( pTextureImage == 0 )
		return false;

	CPtr<IDataStream> pSANStream = OpenFileStream( szSANName.c_str(), STREAM_ACCESS_READ );
	if ( pSANStream == 0 )
		return false;

	SSpriteAnimationFormat animFormat;
	CPtr<IStructureSaver> pSaver = CreateStructureSaver( pSANStream, IStructureSaver::READ );
	CSaverAccessor saver = pSaver;
	DWORD dwSignature = 0;
	saver.Add( 127, &dwSignature );
	if ( dwSignature != 0 )
		return false;
	saver.Add( 1, &animFormat );

	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 || pTree->GetRootItem() == 0 )
		return false;

	CTreeItem *pAnimsItem = pTree->GetRootItem()->GetChildItem( E_UNIT_ANIMATIONS_ITEM );
	if ( pAnimsItem == 0 )
		return false;

	string szPreviewDir = theApp.GetEditorTempDir();
	const int nTextureX = pTextureImage->GetSizeX();
	const int nTextureY = pTextureImage->GetSizeY();
	int nAnimIndex = 0;
	bool bSavedAny = false;
	for ( CTreeItem::CTreeItemList::const_iterator animIt=pAnimsItem->GetBegin(); animIt!=pAnimsItem->GetEnd(); ++animIt, ++nAnimIndex )
	{
		if ( nAnimIndex >= animFormat.animations.size() )
			break;

		const SSpriteAnimationFormat::SSpriteAnimation &animation = animFormat.animations[nAnimIndex];
		if ( animation.dirs.empty() )
			continue;

		const SSpriteAnimationFormat::SSpriteAnimation::SDir &dir = animation.dirs[0];
		int nFrameIndex = 0;
		for ( CTreeItem::CTreeItemList::const_iterator frameIt=(*animIt)->GetBegin(); frameIt!=(*animIt)->GetEnd(); ++frameIt, ++nFrameIndex )
		{
			if ( nFrameIndex >= dir.frames.size() )
				break;
			const int nRectIndex = dir.frames[nFrameIndex];
			if ( nRectIndex < 0 || nRectIndex >= animation.rects.size() )
				continue;

			const SSpriteRect &spriteRect = animation.rects[nRectIndex];
			int nLeft = int( min( spriteRect.maps.x1, spriteRect.maps.x2 ) * nTextureX );
			int nRight = int( max( spriteRect.maps.x1, spriteRect.maps.x2 ) * nTextureX );
			int nTop = int( min( spriteRect.maps.y1, spriteRect.maps.y2 ) * nTextureY );
			int nBottom = int( max( spriteRect.maps.y1, spriteRect.maps.y2 ) * nTextureY );
			nLeft = max( 0, min( nLeft, nTextureX - 1 ) );
			nRight = max( nLeft + 1, min( nRight, nTextureX ) );
			nTop = max( 0, min( nTop, nTextureY - 1 ) );
			nBottom = max( nTop + 1, min( nBottom, nTextureY ) );

			RECT rc = { nLeft, nTop, nRight, nBottom };
			CPtr<IImage> pFrameImage = pIP->CreateImage( nRight - nLeft, nBottom - nTop );
			pFrameImage->Set( 0xff000000 );
			pFrameImage->CopyFrom( pTextureImage, &rc, 0, 0 );

			string szFrameFileName = (*frameIt)->GetItemName();
			szFrameFileName += ".tga";
			CPtr<IDataStream> pFrameStream = CreateFileStream( (szPreviewDir + szFrameFileName).c_str(), STREAM_ACCESS_WRITE );
			if ( pFrameStream != 0 && pIP->SaveImageAsTGA( pFrameStream, pFrameImage ) )
				bSavedAny = true;
		}
	}

	if ( !bSavedAny )
		return false;

	m_wndAllDirThumbItems.LoadAllImagesFromDir( pDirPropsItem->GetThumbItems(), pDirPropsItem->GetImageList(), szPreviewDir.c_str() );
	return !pDirPropsItem->GetThumbItems()->thumbDataList.empty();
}
void CAnimationFrame::SetActiveDirTreeItem( CDirectoryPropsItem *pDirPropsItem )
{
	m_pActiveDirTreeItem = pDirPropsItem;
	if ( m_pActiveDirTreeItem )
	{
		if ( !m_pActiveDirTreeItem->GetLoadedFlag() )
		{
			string szEditorDataDir = theApp.GetEditorDataDir();
			szEditorDataDir += "editor\\";

			m_wndAllDirThumbItems.LoadImageToImageList( m_pActiveDirTreeItem->GetImageList(), "invalid.tga", szEditorDataDir.c_str() );
			MakeFullPath( GetDirectory(szProjectFileName.c_str()).c_str(), m_pActiveDirTreeItem->GetDirName(), szEditorDataDir );
			m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pActiveDirTreeItem->GetThumbItems(), m_pActiveDirTreeItem->GetImageList(), szEditorDataDir.c_str() );
			if ( m_pActiveDirTreeItem->GetThumbItems()->thumbDataList.empty() )
				LoadRuntimePreviewThumbnails( m_pActiveDirTreeItem );
			m_pActiveDirTreeItem->SetLoadedFlag( true );
		}
		
		m_wndAllDirThumbItems.SetActiveThumbItems( m_pActiveDirTreeItem->GetThumbItems(), m_pActiveDirTreeItem->GetImageList() );
		m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pActiveDirTreeItem->GetThumbItems(), m_pActiveDirTreeItem->GetImageList() );
	}
	else
	{
		NI_ASSERT( 0 );
	}
}

void CAnimationFrame::ActiveDirNameChanged()
{
	SetChangedFlag( true );
	bComposed = false;

	if ( m_pActiveDirTreeItem )
	{
		string szDir = GetDirectory( szProjectFileName.c_str() );
		string szFull;
		bool bRes = MakeFullPath( szDir.c_str(), m_pActiveDirTreeItem->GetDirName(), szFull );

		if ( bRes )
			m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pActiveDirTreeItem->GetThumbItems(), m_pActiveDirTreeItem->GetImageList(), szFull.c_str() );
		else
			m_wndAllDirThumbItems.LoadAllImagesFromDir( m_pActiveDirTreeItem->GetThumbItems(), m_pActiveDirTreeItem->GetImageList(), m_pActiveDirTreeItem->GetDirName() );

		m_wndAllDirThumbItems.SetActiveThumbItems( m_pActiveDirTreeItem->GetThumbItems(), m_pActiveDirTreeItem->GetImageList() );
		m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pActiveDirTreeItem->GetThumbItems(), m_pActiveDirTreeItem->GetImageList() );
	}
}

void CAnimationFrame::SetActiveAnimItem( CUnitAnimationPropsItem *pAnimation )
{
	if ( pAnimation == m_pActiveAnimation )
		return;

	m_pActiveAnimation = pAnimation;
	if ( m_pActiveAnimation )
	{
		m_wndSelectedThumbItems.SetActiveThumbItems( m_pActiveAnimation->GetThumbItems(), 0 );
		if ( m_pActiveDirTreeItem )
			m_wndSelectedThumbItems.LoadImageIndexFromThumbs( m_pActiveDirTreeItem->GetThumbItems(), m_pActiveDirTreeItem->GetImageList() );
	
		if ( !m_pActiveAnimation->GetLoadedFlag() )
		{
			NI_ASSERT( m_wndSelectedThumbItems.GetThumbsCount() == m_pActiveAnimation->GetChildsCount() );
			CTreeItem::CTreeItemList::const_iterator it;
			int i = 0;
			for ( it=m_pActiveAnimation->GetBegin(); it!=m_pActiveAnimation->GetEnd(); ++it )
			{
				m_wndSelectedThumbItems.SetUserDataForItem( i, (DWORD) it->GetPtr() );
				i++;
			}
			m_pActiveAnimation->SetLoadedFlag( true );
		}
	}
}

BOOL CAnimationFrame::SpecificTranslateMessage( MSG *pMsg )
{
	if ( bRunning )
	{
		if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE )
		{
			OnStopButton();
			return true;
		}
	}

	switch ( pMsg->message )
	{
		case WM_THUMB_LIST_DBLCLK:
			DoubleClickOnThumbList( pMsg->wParam );
			return true;

		case WM_THUMB_LIST_DELETE:
			DeleteFrameInTree( pMsg->wParam );
			return true;
	}
	
	return false;
}

LRESULT CAnimationFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( message == WM_THUMB_LIST_SELECT )
	{
		ClickOnThumbList( wParam );
		return true;
	}
	
	return CParentFrame::WindowProc(message, wParam, lParam);
}

int CAnimationFrame::DisplayAcksMenu()
{
	POINT point;
	GetCursorPos( &point );
	
	CMenu menu;
	menu.LoadMenu( IDR_ACK_MENU );
	CMenu *popup = menu.GetSubMenu( 0 );
	return popup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, g_frameManager.GetActiveFrame() );
}

FILETIME CAnimationFrame::FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem )
{
	FILETIME maxTime, current;
	maxTime = GetFileChangeTime( pszProjectName );

	CAnimationTreeRootItem *pAnimRoot = static_cast<CAnimationTreeRootItem *>( pRootItem );
	current = pAnimRoot->FindMaximalSourceTime( pszProjectName );
	if ( current > maxTime )
		maxTime = current;

	string szProjectDir = GetDirectory( pszProjectName );

	string szTempFileName = szProjectDir;
	szTempFileName += "name.txt";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current > maxTime )
		maxTime = current;
	
/*
	string szTempFileName = szProjectDir;
	szTempFileName += "desc.txt";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current > maxTime )
		maxTime = current;
	
	string szTempFileName = szProjectDir;
	szTempFileName += "stats.txt";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current > maxTime )
		maxTime = current;
*/

	return maxTime;
}

FILETIME CAnimationFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime, current;
	string szDestDir = GetDirectory( pszResultFileName );

	string szTempFileName = szDestDir;
	szTempFileName += "1.san";
	current = GetFileChangeTime( szTempFileName.c_str() );
	minTime = current;

	szTempFileName = szDestDir;
	szTempFileName += "1";
	current = GetTextureFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;

	szTempFileName = szDestDir;
	szTempFileName += "1w";
	current = GetTextureFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;

	szTempFileName = szDestDir;
	szTempFileName += "1a";
	current = GetTextureFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;

	szTempFileName = szDestDir;
	szTempFileName += "1.xml";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;

	szTempFileName = szDestDir;
	szTempFileName += "name.txt";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;

/*
	szTempFileName = szDestDir;
	szTempFileName += "desc.txt";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szTempFileName = szDestDir;
	szTempFileName += "stats.txt";
	current = GetFileChangeTime( szTempFileName.c_str() );
	if ( current < minTime )
		minTime = current;
*/

	return minTime;
}
