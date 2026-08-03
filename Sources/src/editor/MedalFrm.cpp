#include "StdAfx.h"
#include <io.h>
#include "../Common/LegacyUiCompat.h"

#include "../GFX/GFX.H"
#include "../Scene/Scene.h"
#include "../Anim/Animation.h"
#include "../Main/rpgstats.h"

#include "editor.h"
#include "BuildCompose.h"			//��� ���������� �������� � ��������
#include "TreeDockWnd.h"
#include "PropView.h"
#include "TreeItem.h"
#include "MedalFrm.h"
#include "MedalView.h"
#include "MedalTreeItem.h"
#include "GameWnd.h"
#include "frames.h"
#include "ImageView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(CMedalFrame, CImageFrame)

BEGIN_MESSAGE_MAP(CMedalFrame, CImageFrame)
	ON_WM_CREATE()
END_MESSAGE_MAP()


CMedalFrame::CMedalFrame()
{
	szComposerName = "Medal Editor";
	szExtension = "*.mdc";
	szComposerSaveName = "Medal_Composer_Project";
	nTreeRootItemID = E_MEDAL_ROOT_ITEM;
	nFrameType = CFrameManager::E_MEDAL_FRAME;
	pWndView = new CImageView;
	szAddDir = "medals\\";
	
	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB1555;
}

CMedalFrame::~CMedalFrame()
{
}

int CMedalFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CImageFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );

	/*if (!pWndView->Create(NULL, NULL,  WS_CHILD | WS_VISIBLE, 
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}*/

	return 0;
}


void CMedalFrame::FillRPGStats( SMedalStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName )
{
	CMedalCommonPropsItem *pCommonProps = static_cast<CMedalCommonPropsItem *> ( pRootItem->GetChildItem( E_MEDAL_COMMON_PROPS_ITEM ) );
	rpgStats.szHeaderText = szPrefix + pCommonProps->GetName();
	rpgStats.szDescriptionText = szPrefix + pCommonProps->GetDescText();
	rpgStats.szTexture = szPrefix + pCommonProps->GetTexture();
	
	std::string szTemp, szSource;
	szTemp = pCommonProps->GetTexture();
	szTemp += ".tga";
	MakeFullPath( GetDirectory( pszProjectName ).c_str(), szTemp.c_str(), szSource );
	rpgStats.mapImageRect = GetImageSize( szSource.c_str() );
}

void CMedalFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	NI_ASSERT( pRootItem != 0 );
	SMedalStats rpgStats;
	if ( !bNewProjectJustCreated )
		FillRPGStats( rpgStats, pRootItem, pszProjectName );
	else
		GetRPGStats( rpgStats, pRootItem );
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CMedalFrame::GetRPGStats( const SMedalStats &rpgStats, CTreeItem *pRootItem )
{
/*
CMedalCommonPropsItem *pCommonProps = static_cast<CMedalCommonPropsItem *> ( pRootItem->GetChildItem( E_MEDAL_COMMON_PROPS_ITEM ) );
rpgStats.szHeaderText = szPrefix + pCommonProps->GetName();
rpgStats.szDescriptionText = szPrefix + pCommonProps->GetDescText();
rpgStats.szTexture = szPrefix + pCommonProps->GetTexture();
	*/
	/*
	CMedalCommonPropsItem *pCommonProps = static_cast<CMedalCommonPropsItem *> ( pRootItem->GetChildItem( E_MEDAL_COMMON_PROPS_ITEM ) );
	*/
}

void CMedalFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	SMedalStats rpgStats;
	
	int nPos = szPrevExportFileName.find( szAddDir );
	if ( nPos != std::string::npos )
		szPrevExportFileName = szPrevExportFileName.substr( szAddDir.size() );
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	GetRPGStats( rpgStats, pRootItem );
	LoadImageTexture( (rpgStats.szTexture + ".tga").c_str() );
}

bool CMedalFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_MEDAL_ROOT_ITEM );

/*
	std::string szData = theApp.GetEditorDataDir();
	MakeSubRelativePath( szData.c_str(), pszResultFileName, szPrefix );
	if ( szPrefix.size() == 0 )
	{
		AfxMessageBox( "Error: The medal frame project should be exported to the game DATA directory" );
		return false;
	}
	szPrefix = szPrefix.substr( 0, szPrefix.rfind('\\') + 1 );
*/

	szPrefix = szAddDir + szPrevExportFileName.substr( 0, szPrevExportFileName.rfind('\\') + 1 );
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	szPrefix = "";

	CMedalCommonPropsItem *pCommonProps = static_cast<CMedalCommonPropsItem *> ( pRootItem->GetChildItem( E_MEDAL_COMMON_PROPS_ITEM ) );
	std::string szDir = GetDirectory( pszProjectName );
	std::string szResDir = GetDirectory( pszResultFileName ).c_str();
	std::string szSource, szResult;

	std::string szTemp = pCommonProps->GetTexture();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".tga";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szResDir + pCommonProps->GetTexture();
		if ( !ComposeImageToTexture( szSource.c_str(), szResult.c_str() ) )
			AfxMessageBox( "Error: ComposeImageToTexture() FAILED" );
	}

	szTemp = pCommonProps->GetName();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".txt";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szResDir + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}

	szTemp = pCommonProps->GetDescText();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".txt";
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szResDir + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}

	return true;
}

FILETIME CMedalFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_MEDAL_ROOT_ITEM );

	FILETIME minTime;
	minTime.dwHighDateTime = 0;
	minTime.dwLowDateTime = 0;
/*
	minTime.dwHighDateTime = -1;
	minTime.dwLowDateTime = -1;
	minTime = GetFileChangeTime( pszResultFileName );

	FILETIME current;
	std::string szDir = GetDirectory( pszResultFileName ).c_str();
	std::string szTemp = szDir + "desc.txt";
	current = GetFileChangeTime( szTemp.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szTemp = szDir + "1.tga";
	current = GetFileChangeTime( szTemp.c_str() );
	if ( current < minTime )
		minTime = current;
*/

	return minTime;
}

FILETIME CMedalFrame::FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_MEDAL_ROOT_ITEM );
	
	FILETIME maxTime;
	maxTime.dwHighDateTime = -1;
	maxTime.dwLowDateTime = -1;
/*
	maxTime.dwHighDateTime = 0;
	maxTime.dwLowDateTime = 0;
	FILETIME currentTime;
	CMedalCommonPropsItem *pCommonProps = static_cast<CMedalCommonPropsItem *> ( pRootItem->GetChildItem( E_MEDAL_COMMON_PROPS_ITEM ) );
	std::string szTemp, szSource, szDir;
	szDir = GetDirectory( pszProjectName );

	CMedalTextPropsItem *pTextProps = static_cast<CMedalTextPropsItem *> ( pRootItem->GetChildItem( E_MEDAL_TEXT_PROPS_ITEM ) );
	szTemp = pTextProps->GetDescText();
	szTemp += ".txt";
	MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
	currentTime = GetFileChangeTime( szSource.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	CMedalPicturePropsItem *pPictureProps = static_cast<CMedalPicturePropsItem *> ( pRootItem->GetChildItem( E_MEDAL_PICTURE_PROPS_ITEM ) );
	szTemp = pPictureProps->GetTexture();
	szTemp += ".tga";
	MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
	currentTime = GetFileChangeTime( szSource.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
*/
	return maxTime;
}

void CMedalFrame::UpdatePropView( CTreeItem *pTreeItem )
{
	CParentFrame::UpdatePropView( pTreeItem );
	if ( CMedalCommonPropsItem *pCommonProps = dynamic_cast<CMedalCommonPropsItem *> ( pTreeItem ) )
	{
		std::string szTexture = pCommonProps->GetTexture();
		LoadImageTexture( (szTexture + ".tga").c_str() );
	}
}

