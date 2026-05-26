#include "stdafx.h"
#include <io.h>
#include "..\Common\StingrayCompat.h"

#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"
#include "..\Main\rpgstats.h"

#include "editor.h"
#include "BuildCompose.h"			//для компоновки картинки в текстуру
#include "TreeDockWnd.h"
#include "PropView.h"
#include "TreeItem.h"
#include "CampaignFrm.h"
#include "ImageView.h"
#include "CampaignTreeItem.h"
#include "SpriteCompose.h"
#include "GameWnd.h"
#include "frames.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CCampaignFrame

IMPLEMENT_DYNCREATE(CCampaignFrame, CImageFrame)

BEGIN_MESSAGE_MAP(CCampaignFrame, CImageFrame)
	//{{AFX_MSG_MAP(CCampaignFrame)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCampaignFrame construction/destruction

CCampaignFrame::CCampaignFrame()
{
	szComposerName = "Campaign Editor";
	szExtension = "*.cgc";
	szComposerSaveName = "Campaign_Composer_Project";
	nTreeRootItemID = E_CAMPAIGN_ROOT_ITEM;
	nFrameType = CFrameManager::E_CAMPAIGN_FRAME;
	pWndView = new CImageView;
	pActiveChapter = 0;
	szAddDir = "scenarios\\campaigns\\";
	
	m_nCompressedFormat = GFXPF_DXT3;
	m_nLowFormat = GFXPF_ARGB4444;
}

CCampaignFrame::~CCampaignFrame()
{
}

int CCampaignFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CImageFrame::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_frameManager.AddFrame( this );

	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CCampaignFrame message handlers

void CCampaignFrame::FillRPGStats( SCampaignStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName )
{
	CCampaignCommonPropsItem *pCommonProps = static_cast<CCampaignCommonPropsItem *> ( pRootItem->GetChildItem( E_CAMPAIGN_COMMON_PROPS_ITEM ) );
	std::string szTemp, szTT;
	szTemp = szPrefix + pCommonProps->GetHeaderText();
	rpgStats.szHeaderText = szTemp;
	szTemp = szPrefix + pCommonProps->GetSubHeaderText();
	rpgStats.szSubheaderText = szTemp;
	szTemp = szPrefix + pCommonProps->GetMapImage();
	rpgStats.szMapImage = szTemp;

	{
		szTemp = pCommonProps->GetMapImage();
		szTemp += ".tga";
		MakeFullPath( GetDirectory( pszProjectName ).c_str(), szTemp.c_str(), szTT );
		rpgStats.mapImageRect = GetImageSize( szTT.c_str() );
	}

	rpgStats.szIntroMovie = pCommonProps->GetIntroMovie();
	rpgStats.szOutroMovie = pCommonProps->GetOutroMovie();
	rpgStats.szInterfaceMusic = pCommonProps->GetInterfaceMusic();
	rpgStats.szSideName = pCommonProps->GetPlayerSideName();
	rpgStats.szMODName = theApp.GetMODName();
	rpgStats.szMODVersion = theApp.GetMODVersion();
	
	CTreeItem *pChapters = pRootItem->GetChildItem( E_CAMPAIGN_CHAPTERS_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pChapters->GetBegin(); it!=pChapters->GetEnd(); ++it )
	{
		CCampaignChapterPropsItem *pChapterProps = static_cast<CCampaignChapterPropsItem *> ( it->GetPtr() );
		SCampaignStats::SChapter chapter;
		chapter.szChapter = pChapterProps->GetChapterName();
		chapter.vPosOnMap = pChapterProps->GetChapterPosition();
		chapter.bVisible = pChapterProps->GetChapterVisibleFlag();
		chapter.bSecret = pChapterProps->GetChapterSecretFlag();
		rpgStats.chapters.push_back( chapter );
	}

	CTreeItem *pTemplates = pRootItem->GetChildItem( E_CAMPAIGN_TEMPLATES_ITEM );
	for ( CTreeItem::CTreeItemList::const_iterator it=pTemplates->GetBegin(); it!=pTemplates->GetEnd(); ++it )
	{
		CCampaignTemplatePropsItem *pTemplateProps = static_cast<CCampaignTemplatePropsItem *> ( it->GetPtr() );
		rpgStats.templateMissions.push_back( pTemplateProps->GetTemplateName() );
	}
}

void CCampaignFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	NI_ASSERT( pRootItem != 0 );
	SCampaignStats rpgStats;
	if ( !bNewProjectJustCreated )
		FillRPGStats( rpgStats, pRootItem, pszProjectName );
	else
		GetRPGStats( rpgStats, pRootItem );
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CCampaignFrame::GetRPGStats( const SCampaignStats &rpgStats, CTreeItem *pRootItem )
{
	CCampaignCommonPropsItem *pCommonProps = static_cast<CCampaignCommonPropsItem *> ( pRootItem->GetChildItem( E_CAMPAIGN_COMMON_PROPS_ITEM ) );
	pCommonProps->SetHeaderText( rpgStats.szHeaderText.c_str() );
	pCommonProps->SetSubHeaderText( rpgStats.szSubheaderText.c_str() );
	pCommonProps->SetMapImage( rpgStats.szMapImage.c_str() );
	pCommonProps->SetIntroMovie( rpgStats.szIntroMovie.c_str() );
	pCommonProps->SetOutroMovie( rpgStats.szOutroMovie.c_str() );
	pCommonProps->SetInterfaceMusic( rpgStats.szInterfaceMusic.c_str() );
	pCommonProps->SetPlayerSideName( rpgStats.szSideName.c_str() );
	
	CTreeItem *pChapters = pRootItem->GetChildItem( E_CAMPAIGN_CHAPTERS_ITEM );
	NI_ASSERT( pChapters->GetChildsCount() == rpgStats.chapters.size() );
	int i = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pChapters->GetBegin(); it!=pChapters->GetEnd(); ++it )
	{
		CCampaignChapterPropsItem *pChapterProps = static_cast<CCampaignChapterPropsItem *> ( it->GetPtr() );
		pChapterProps->SetChapterName( rpgStats.chapters[i].szChapter.c_str() );
		pChapterProps->SetChapterPosition( rpgStats.chapters[i].vPosOnMap );
		pChapterProps->SetChapterVisibleFlag( rpgStats.chapters[i].bVisible );
		pChapterProps->SetChapterSecretFlag( rpgStats.chapters[i].bSecret );
		i++;
	}
	
	CTreeItem *pTemplates = pRootItem->GetChildItem( E_CAMPAIGN_TEMPLATES_ITEM );
	NI_ASSERT( pTemplates->GetChildsCount() == rpgStats.templateMissions.size() );
	i = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pTemplates->GetBegin(); it!=pTemplates->GetEnd(); ++it )
	{
		CCampaignTemplatePropsItem *pTemplateProps = static_cast<CCampaignTemplatePropsItem *> ( it->GetPtr() );
		pTemplateProps->SetTemplateName( rpgStats.templateMissions[i].c_str() );
		i++;
	}
}

void CCampaignFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	SCampaignStats rpgStats;
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	GetRPGStats( rpgStats, pRootItem );

	if ( szProjectFileName.size() > 0 )
	{
		//загрузим и отобразим картинку на экране
		CCampaignCommonPropsItem *pCommonProps = static_cast<CCampaignCommonPropsItem *> ( pRootItem->GetChildItem( E_CAMPAIGN_COMMON_PROPS_ITEM ) );
		std::string szMapFileName, szTemp;
		szTemp = pCommonProps->GetMapImage();
		szTemp += ".tga";
		MakeFullPath( GetDirectory( szProjectFileName.c_str() ).c_str(), szTemp.c_str(), szMapFileName );
		LoadImageTexture( szMapFileName.c_str() );
	}
}

bool CCampaignFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_CAMPAIGN_ROOT_ITEM );
	
	{
		//validation
		CCampaignCommonPropsItem *pCommonProps = static_cast<CCampaignCommonPropsItem *> ( pRootItem->GetChildItem( E_CAMPAIGN_COMMON_PROPS_ITEM ) );
		std::string szErrorMsg;
		std::string szTemp;
		szTemp = pCommonProps->GetHeaderText();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify header text reference before exporting.\n";
		szTemp = pCommonProps->GetSubHeaderText();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify subheader text reference before exporting.\n";
		szTemp = pCommonProps->GetMapImage();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify map image reference before exporting.\n";
		szTemp = pCommonProps->GetIntroMovie();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify intro movie reference before exporting.\n";
		szTemp = pCommonProps->GetOutroMovie();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify outro movie reference before exporting.\n";
		szTemp = pCommonProps->GetInterfaceMusic();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify interface music reference before exporting.\n";
		szTemp = pCommonProps->GetPlayerSideName();
		if ( szTemp.empty() )
			szErrorMsg = "You should specify player side before exporting.\n";
		CTreeItem *pChapters = pRootItem->GetChildItem( E_CAMPAIGN_CHAPTERS_ITEM );
		if ( pChapters->GetBegin() == pChapters->GetEnd() )
			szErrorMsg = "You should specify some chapter references before exporting.\n";
		for ( CTreeItem::CTreeItemList::const_iterator it = pChapters->GetBegin(); it != pChapters->GetEnd(); ++it )
		{
			CCampaignChapterPropsItem *pChapterProps = static_cast<CCampaignChapterPropsItem *> ( it->GetPtr() );
			szTemp = pChapterProps->GetChapterName();
			if ( szTemp.empty() )
				szErrorMsg = "You should specify all chapter references before exporting.\n";
		}
		CTreeItem *pTemplates = pRootItem->GetChildItem( E_CAMPAIGN_TEMPLATES_ITEM );
		if ( pTemplates->GetBegin() == pTemplates->GetEnd() )
			szErrorMsg = "You should specify some template references before exporting.\n";
		for ( CTreeItem::CTreeItemList::const_iterator it = pTemplates->GetBegin(); it != pTemplates->GetEnd(); ++it )
		{
			CCampaignTemplatePropsItem *pTemplateProps = static_cast<CCampaignTemplatePropsItem *> ( it->GetPtr() );
			szTemp = pTemplateProps->GetTemplateName();
			if ( szTemp.empty() )
				szErrorMsg = "You should specify all template references before exporting.\n";
		}
		if ( !szErrorMsg.empty() )
		{
			AfxMessageBox( szErrorMsg.c_str() );
			return false;
		}
	}
	szPrefix = szAddDir + szPrevExportFileName.substr( 0, szPrevExportFileName.rfind('\\') + 1 );
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	
	//Скопирую все данные в экспорт директорию
	szPrefix = theApp.GetDestDir() + szPrefix;
	CCampaignCommonPropsItem *pCommonProps = static_cast<CCampaignCommonPropsItem *> ( pRootItem->GetChildItem( E_CAMPAIGN_COMMON_PROPS_ITEM ) );
	std::string szTemp, szSource, szResult, szDir;
	szDir = GetDirectory( pszProjectName );
	szTemp = pCommonProps->GetHeaderText();
	szTemp += ".txt";
	if ( szTemp.length() > 0 )
	{
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}
	
	szTemp = pCommonProps->GetSubHeaderText();
	szTemp += ".txt";
	if ( szTemp.length() > 0 )
	{
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + szTemp;
		MyCopyFile( szSource.c_str(), szResult.c_str() );
	}

	szTemp = pCommonProps->GetMapImage();
	if ( szTemp.length() > 0 )
	{
		szTemp += ".tga";
		//Надо скомпоновать картинку, чтобы она загружалась из текстуры
		MakeFullPath( szDir.c_str(), szTemp.c_str(), szSource );
		szResult = szPrefix + pCommonProps->GetMapImage();
		if ( !ComposeImageToTexture( szSource.c_str(), szResult.c_str() ) )
			AfxMessageBox( "Error: ComposeImageToTexture() FAILED" );
	}

	szPrefix = "";
	return true;
}

FILETIME CCampaignFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime;
	minTime = GetFileChangeTime( pszResultFileName );
	return minTime;
}

void CCampaignFrame::SetActiveChapter( CCampaignChapterPropsItem *pChapter )
{
	pActiveChapter = pChapter;
	if ( pActiveChapter == 0 )
	{
		bShowKrest = false;
	}
	else
	{
		bShowKrest = true;
		vKrestPos = pActiveChapter->GetChapterPosition();
	}
	GFXDraw();
}

void CCampaignFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
}

void CCampaignFrame::SpecificClearBeforeBatchMode()
{
	bShowKrest = false;
	pActiveChapter = 0;
	pImageTexture = 0;
	vImageSize = VNULL2;
	GFXDraw();
}

void CCampaignFrame::OnLButtonDown(UINT nFlags, CPoint point) 
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

	if ( pActiveChapter )
	{
		pActiveChapter->SetChapterPosition( CVec2( point.x + m_wndHScrollBar.GetScrollPos(), point.y + m_wndVScrollBar.GetScrollPos() ) );
		vKrestPos = pActiveChapter->GetChapterPosition();
		pOIDockBar->SetItemProperty( pActiveChapter->GetItemName(), pActiveChapter );
		GFXDraw();
		SetChangedFlag( true );
	}

	CImageFrame::OnLButtonDown(nFlags, point);
}

void CCampaignFrame::OnLButtonUp(UINT nFlags, CPoint point)
{
	ReleaseCapture();
	CImageFrame::OnLButtonUp(nFlags, point);
}

void CCampaignFrame::OnMouseMove(UINT nFlags, CPoint point)
{
	if ( point.x < 0 )
		point.x = 0;
	if ( point.x > vImageSize.x )
		point.x = vImageSize.x;
	if ( point.y < 0 )
		point.y = 0;
	if ( point.y > vImageSize.y )
		point.y = vImageSize.y;

	if ( nFlags & MK_LBUTTON && pActiveChapter )
	{
		pActiveChapter->SetChapterPosition( CVec2( point.x + m_wndHScrollBar.GetScrollPos(), point.y + m_wndVScrollBar.GetScrollPos() ) );
		vKrestPos = pActiveChapter->GetChapterPosition();
		pOIDockBar->SetItemProperty( pActiveChapter->GetItemName(), pActiveChapter );
		GFXDraw();
		SetChangedFlag( true );
	}

	CImageFrame::OnMouseMove(nFlags, point);
}
