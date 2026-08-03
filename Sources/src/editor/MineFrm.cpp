#include "StdAfx.h"
#include <io.h>
#include "../Common/LegacyUiCompat.h"

#include "../GFX/GFX.H"
#include "../Scene/Scene.h"
#include "../Anim/Animation.h"
#include "../Main/rpgstats.h"

#include "editor.h"
#include "TreeDockWnd.h"
#include "PropView.h"
#include "TreeItem.h"
#include "MineFrm.h"
#include "MineView.h"
#include "GameWnd.h"
#include "frames.h"
#include "BuildCompose.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(CMineFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CMineFrame, CParentFrame)
	ON_WM_CREATE()
END_MESSAGE_MAP()


CMineFrame::CMineFrame()
{
	szComposerName = "Mine Editor";
	szExtension = "*.mcp";
	szComposerSaveName = "Mine_Composer_Project";
	nTreeRootItemID = E_MINE_ROOT_ITEM;
	nFrameType = CFrameManager::E_MINE_FRAME;
	pWndView = new CMineView;
	szAddDir = "objects\\simpleobjects\\common\\summer\\mine\\";
	
	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB1555;
}

CMineFrame::~CMineFrame()
{
}

int CMineFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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

	return 0;
}


void CMineFrame::FillRPGStats( SMineRPGStats &rpgStats, CTreeItem *pRootItem )
{
	CMineCommonPropsItem *pCommonProps = static_cast<CMineCommonPropsItem *> ( pRootItem->GetChildItem( E_MINE_COMMON_PROPS_ITEM ) );
	rpgStats.szKeyName = pCommonProps->GetMineName();
	rpgStats.fWeight = pCommonProps->GetWeight();
	rpgStats.szFlagModel = "1";
	rpgStats.szWeapon = pCommonProps->GetMineName();
}

void CMineFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	NI_ASSERT( pRootItem != 0 );
	SMineRPGStats rpgStats;
	if ( !bNewProjectJustCreated )
		FillRPGStats( rpgStats, pRootItem );
	else
		GetRPGStats( rpgStats, pRootItem );

	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CMineFrame::GetRPGStats( const SMineRPGStats &rpgStats, CTreeItem *pRootItem )
{
	CMineCommonPropsItem *pCommonProps = static_cast<CMineCommonPropsItem *> ( pRootItem->GetChildItem( E_MINE_COMMON_PROPS_ITEM ) );
	pCommonProps->SetMineName( rpgStats.szKeyName.c_str() );
	pCommonProps->SetWeight( rpgStats.fWeight );
}

void CMineFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	SMineRPGStats rpgStats;
	FillRPGStats( rpgStats, pRootItem );			//����� ��������� ������������� ���������� �� ���������

	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	GetRPGStats( rpgStats, pRootItem );
}

bool CMineFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_MINE_ROOT_ITEM );
	
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	
	string szPictureName, szShadowName, szResultName;
	szPictureName = GetDirectory( pszProjectName );
	szPictureName += "1.tga";
	szShadowName = GetDirectory( pszProjectName );
	szShadowName += "1s.tga";
	szResultName = GetDirectory( pszResultFileName );
	szResultName += "1";
	ComposeSingleObject( szPictureName.c_str(), szShadowName.c_str(), szResultName.c_str(), VNULL2 );
	return true;
}

FILETIME CMineFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime;
	minTime = GetFileChangeTime( pszResultFileName );
	return minTime;
}
