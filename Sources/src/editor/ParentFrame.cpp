#include "StdAfx.h"

#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\Anim\Animation.h"
#include "..\image\image.h"
#include "..\Main\RPGStats.h"
#include "..\Formats\fmtUnitCreation.h"
#include "..\AILogic\UnitCreation.h"
#include "..\RandomMapGen\MapInfo_types.h"

#include "frames.h"
#include "editor.h"
#include "gamewnd.h"
#include "ParentFrame.h"
#include "TreeDockWnd.h"
#include "PropView.h"
#include "TreeItem.h"
#include "RefDlg.h"

#include "BrowseDialog.h"
#include "SetDirDialog.h"
#include "PictureOptions.h"
#include "BatchModeDialog.h"
#include "ProgressDialog.h"
#include "MODDialog.h"
#include "SpriteCompose.h"

#include <direct.h>

#include "..\RandomMapGen\Resource_Types.h"


static const int TRANSACTION_LIMIT = 100;
static char BASED_CODE szComposerFilter[] =
"Resource editor files|*.unt;*.gui;*.spt;*.eff;*.obt;*.msh;*.wpn;*.bld;*.til"
";*.rdc;*.fnc;*.pcp;*.trc;*.scp;*.mcp;*.bdg;*.mip;*.chc;*.cgc;*.3rd;*.3rv;*.mdc|"
"Infantry files (*.unt)|*.unt|"
"Sprite files (*.spt)|*.spt|"
"Effect files (*.eff)|*.eff|"
"Object files (*.obt)|*.obt|"
"Mesh files (*.msh)|*.msh|"
"Weapon files (*.wpn)|*.wpn|"
"Building files (*.bld)|*.bld|"
"Tileset files (*.til)|*.til|"
"Fence files (*.fnc)|*.fnc|"
"Trench files (*.trc)|*.trc|"
"Building files (*.bld)|*.bld|"
"Particle files (*.pcp)|*.pcp|"
"Squad files (*.scp)|*.scp|"
"Mine files (*.mcp)|*.mcp|"
"Bridge files (*.bdg)|*.bdg|"
"Mission files (*.mip)|*.mip|"
"Chapter files (*.chc)|*.chc|"
"Campaign files (*.cgc)|*.cgc|"
"3D Road files (*.3rd)|*.3rd|"
"3D River files (*.3rv)|*.3rv|"
"Medal files (*.mdc)|*.mdc|";

static const std::string szConfigFileName = "gamma.cfg";
static const int NO_CONFIG_FILE = -8;

IMPLEMENT_DYNCREATE(CParentFrame, SECWorksheet)

BEGIN_MESSAGE_MAP(CParentFrame, SECWorksheet)
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
	ON_WM_SETFOCUS()
	ON_WM_CREATE()
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_EXPORT_PAK, OnExportPak)
	ON_COMMAND(ID_RUN_GAME, OnRunGame)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_COMMAND(ID_FILE_SAVE_PROJECT_AS, OnFileSaveProjectAs)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_EXPORTFILES, OnFileExportFiles)
	ON_COMMAND(ID_FILE_CREATENEWPROJECT, OnFileCreateNewProject)
	ON_UPDATE_COMMAND_UI(ID_FILE_EXPORTFILES, OnUpdateFileExportFiles)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateFileSave)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_PROJECT_AS, OnUpdateSaveProjectAs)
	ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE, OnUpdateCloseFile)
	ON_WM_SETCURSOR()
	ON_COMMAND(ID_FILE_SETDIRECTORIES, OnFileSetdirectories)
	ON_COMMAND(ID_SET_PICTURE_OPTIONS, OnSetPictureOptions)
	ON_COMMAND(ID_FILE_BATCH_MODE, OnFileBatchMode)
	ON_COMMAND(ID_EDIT_SETBACKGROUNDCOLOR, OnEditSetbackgroundcolor)
	ON_UPDATE_COMMAND_UI(ID_INSERT_TREE_ITEM, OnUpdateInsertTreeItem)
	ON_UPDATE_COMMAND_UI(ID_MENU_DELETE_TREE_ITEM, OnUpdateDeleteTreeItem)
	ON_COMMAND(ID_VIEW_ADVANCED_TOOLBAR, OnViewAdvancedToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ADVANCED_TOOLBAR, OnUpdateViewAdvancedToolbar)
	ON_COMMAND(ID_SHOW_TREE, OnShowTree)
	ON_UPDATE_COMMAND_UI(ID_SHOW_TREE, OnUpdateShowTree)
	ON_COMMAND(ID_SHOW_OBJECT_INSPECTOR, OnShowOI)
	ON_UPDATE_COMMAND_UI(ID_SHOW_OBJECT_INSPECTOR, OnUpdateShowOI)
	ON_COMMAND(ID_MOD_SETTINGS, OnMODSettings)
	ON_COMMAND(ID_VIEW_EXPANDALL, OnExpandTree)

	ON_COMMAND(ID_EDITORS_UNITEDITOR, OnUnitEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_UNITEDITOR, OnUpdateUnitEditor)
	ON_COMMAND(ID_EDITORS_INFANTRYEDITOR, OnInfantryEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_INFANTRYEDITOR, OnUpdateInfantryEditor)
	ON_COMMAND(ID_EDITORS_SQUADEDITOR, OnSquadEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_SQUADEDITOR, OnUpdateSquadEditor)
	ON_COMMAND(ID_EDITORS_WEAPONEDITOR, OnWeaponEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_WEAPONEDITOR, OnUpdateWeaponEditor)
	ON_COMMAND(ID_EDITORS_MINEEDITOR, OnMineEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_MINEEDITOR, OnUpdateMineEditor)

	ON_COMMAND(ID_EDITORS_PARTICLEEDITOR, OnParticleEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_PARTICLEEDITOR, OnUpdateParticleEditor)
	ON_COMMAND(ID_EDITORS_SPRITEEDITOR, OnSpriteEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_SPRITEEDITOR, OnUpdateSpriteEditor)
	ON_COMMAND(ID_EDITORS_EFFECTEDITOR, OnEffectEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_EFFECTEDITOR, OnUpdateEffectEditor)

	ON_COMMAND(ID_EDITORS_BUILDINGEDITOR, OnBuildingEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_BUILDINGEDITOR, OnUpdateBuildingEditor)
	ON_COMMAND(ID_EDITORS_OBJECTEDITOR, OnObjectEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_OBJECTEDITOR, OnUpdateObjectEditor)
	ON_COMMAND(ID_EDITORS_FENCEEDITOR, OnFenceEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_FENCEEDITOR, OnUpdateFenceEditor)
	ON_COMMAND(ID_EDITORS_BRIDGEEDITOR, OnBridgeEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_BRIDGEEDITOR, OnUpdateBridgeEditor)
	ON_COMMAND(ID_EDITORS_TRENCHEDITOR, OnTrenchEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_TRENCHEDITOR, OnUpdateTrenchEditor)

	ON_COMMAND(ID_EDITORS_MISSIONEDITOR, OnMissionEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_MISSIONEDITOR, OnUpdateMissionEditor)
	ON_COMMAND(ID_EDITORS_CHAPTEREDITOR, OnChapterEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_CHAPTEREDITOR, OnUpdateChapterEditor)
	ON_COMMAND(ID_EDITORS_CAMPAIGNEDITOR, OnCampaignEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_CAMPAIGNEDITOR, OnUpdateCampaignEditor)
	ON_COMMAND(ID_EDITORS_MEDALEDITOR, OnMedalEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_MEDALEDITOR, OnUpdateMedalEditor)

	ON_COMMAND(ID_EDITORS_TERRAINEDITOR, OnTerrainEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_TERRAINEDITOR, OnUpdateTerrainEditor)
	ON_COMMAND(ID_EDITORS_ROADEDITOR, OnRoadEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_ROADEDITOR, OnUpdateRoadEditor)
	ON_COMMAND(ID_EDITORS_RIVEREDITOR, OnRiverEditor)
	ON_UPDATE_COMMAND_UI(ID_EDITORS_RIVEREDITOR, OnUpdateRiverEditor)

END_MESSAGE_MAP()

enum ETransactionTypes
{
	E_UNKNOWN,
	E_SAVE,
	E_IMPORT,
	E_EXPORT,
	E_BATCH_EXPORT,
	E_BATCH_SAVE,
};

CParentFrame::CParentFrame()
{
	pTreeDockBar = 0;
	pOIDockBar = 0;
	pToolBar = 0;
	bChanged = false;
	bNewProjectJustCreated = false;
	m_backgroundColor = 0x80808080;
	pWndView = 0;
	nTreeRootItemID = -1;
	bDefaultExportName = true;
	nFrameType = CFrameManager::E_UNKNOWN_FRAME;
	szExportExtension = ".xml";
	m_fBrightness = m_fContrast = m_fGamma = 0;
	m_nCompressedFormat = -1;
	m_nLowFormat = -1;
	m_nHighFormat = GFXPF_ARGB8888;
	m_nCompressedShadowFormat = GFXPF_DXT5;
	m_nLowShadowFormat = GFXPF_ARGB4444;
	m_nHighShadowFormat = GFXPF_ARGB8888;
	bTreeExpand = true;
}

CParentFrame::~CParentFrame()
{
	if ( pWndView )
	{
		delete pWndView;
		pWndView = 0;
	}
}

void CParentFrame::SaveRegisterData()
{
	std::string szVar;
/*
	szVar = szComposerName;
	szVar += " Local Source Directory";
	theApp.MyWriteProfileString( "", szVar.c_str(), szLocalSourceDir.c_str() );
	
	szVar = szComposerName;
	szVar += " Local Destination Directory";
	theApp.MyWriteProfileString( "", szVar.c_str(), szLocalDestDir.c_str() );
*/
	
	szVar = szComposerName;
	szVar += " Background Color";
	theApp.MyWriteProfileInt( "", szVar.c_str(), m_backgroundColor );
}

void CParentFrame::LoadRegisterData()
{
	std::string szVar;
/*
	szVar = szComposerName;
	szVar += " Local Source Directory";
	szLocalSourceDir = theApp.MyGetProfileString( "", szVar.c_str(), "" );

	szVar = szComposerName;
	szVar += " Local Destination Directory";
	szLocalDestDir = theApp.MyGetProfileString( "", szVar.c_str(), "" );
*/
	
	szVar = szComposerName;
	szVar += " Background Color";
	m_backgroundColor = theApp.MyGetProfileInt( "", szVar.c_str(), 0x80808080 );
}

void CParentFrame::ShowFrameWindows( int nCommand )
{
	pWndView->ShowWindow( nCommand );
	if ( pTreeDockBar )
		theApp.ShowSECControlBar( pTreeDockBar, nCommand );
	if ( pOIDockBar )
		theApp.ShowSECControlBar( pOIDockBar, nCommand );
	if ( pToolBar )
		theApp.ShowSECToolBar( pToolBar, nCommand );
	
	if ( nCommand == SW_SHOW )
	{
		g_frameManager.GetGameWnd()->SetParent( this );
		g_frameManager.GetGameWnd()->SetWindowPos( &wndTop, 0, 0, GAME_SIZE_X, GAME_SIZE_Y, SWP_HIDEWINDOW );
	}
	else
		g_frameManager.GetGameWnd()->ShowWindow( nCommand );

	IScene *pSG = GetSingleton<IScene>();
	IAnimationManager *pAM = GetSingleton<IAnimationManager>();
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	IMeshManager *pMM = GetSingleton<IMeshManager>();
	
	if ( nCommand == SW_SHOW )
	{
		pAM->SetShareMode( SDSM_RELOAD );
		pTM->SetShareMode( SDSM_RELOAD );
		pMM->SetShareMode( SDSM_RELOAD );
		
		ICamera *pCamera = GetSingleton<ICamera>();
		pCamera->SetAnchor( CVec3(8*fWorldCellSize, 8*fWorldCellSize, 0) );
	}
	else
	{
		pSG->Clear();
		pAM->SetShareMode( SDSM_SHARE );
		pTM->SetShareMode( SDSM_SHARE );
		pMM->SetShareMode( SDSM_SHARE );
	}
	
	ComputeCaption();
}

void CParentFrame::SetChangedFlag( bool bFlag )
{
	if ( bChanged != bFlag )
	{
		bChanged = bFlag;
		ComputeCaption();
	}
}

void CParentFrame::ClearPropView()
{
	pOIDockBar->ClearControl();
}	

BOOL CParentFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
	if (pWndView->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;
	
	return SECWorksheet::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

BOOL CParentFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	
	if( !SECWorksheet::PreCreateWindow(cs) )
		return FALSE;
	
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0);
	
	return TRUE;
}

BOOL CParentFrame::PreTranslateMessage( MSG* pMsg )
{
/*
	if ( pMsg->hwnd == pOIDockBar->GetSafeHwnd() )
	{
		return SECWorksheet::PreTranslateMessage(pMsg);
	}
*/

	switch ( pMsg->message )
	{
		case WM_USERCHANGEPARAM:
			SetChangedFlag( true );
			return true;

/*
		case WM_KEYDOWN:
		{
			if ( pOIDockBar )
			{
				CWnd *pWnd = GetFocus();
				if ( pWnd && pOIDockBar->IsChild( pWnd ) )
				{
					pWnd->PostMessage( WM_KEYDOWN, pMsg->wParam, pMsg->lParam );
					return FALSE;
				}
			}
		}
*/
			
	}

	if ( SpecificTranslateMessage( pMsg ) )
		return TRUE;

	return SECWorksheet::PreTranslateMessage(pMsg);
}

void CParentFrame::SetOIDockBar( CPropView *pWnd )
{
	pOIDockBar = pWnd;
	pTreeDockBar->SetPropView( pWnd );
}

BOOL CParentFrame::SaveFrame( bool bUnlock )
{
	if ( bChanged )
	{
		string szErr = szComposerName;
		szErr += " ";
		szErr += szProjectFileName;
		szErr += " has beed modified, save it?";
		int res = AfxMessageBox( szErr.c_str(), MB_YESNOCANCEL );
		if ( res == IDYES )
		{
			OnFileSave();
			if ( bUnlock )
			{
				UnLockFile();
			}
			return FALSE;
		}
		else if ( res == IDCANCEL )
			return TRUE;
		else
		{
			if ( bUnlock )
			{
				UnLockFile();
			}
			return FALSE;
		}
	}
	
	if ( bUnlock )
	{
		UnLockFile();
	}
	return FALSE;
}

void CParentFrame::UpdatePropView( CTreeItem *pTreeItem )
{
	pOIDockBar->ClearControl();
	if ( pTreeItem )
		pOIDockBar->SetItemProperty( pTreeItem->GetItemName(), pTreeItem );
}

void CParentFrame::ComputeCaption()
{
	NI_ASSERT( szComposerName.size() > 0 );
	if ( !pTreeDockBar )
		return;
	
	string text = szComposerName;
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree )
	{
		text += " - ";
		text += szProjectFileName;
		if ( bChanged)
			text += " *";
	}
	SetTitle( "" );
	SetWindowText( text.c_str() );
	/*
	SetTitle( text.c_str() );
	SetWindowText( "" );
	*/
}

void CParentFrame::GenerateProjectName()
{
	bool bFound = false;
	szProjectFileName = theApp.GetSourceDir() + szAddDir;
	szProjectFileName += "current";
	szProjectFileName += szExtension.c_str() + 1;
	if ( _access( szProjectFileName.c_str(), 00 ) )
		return;

	for ( int k=0; k<10; k++ )
	{
		for ( int i=0; i<10; i++ )
		{
			szProjectFileName = theApp.GetSourceDir() + szAddDir;
			szProjectFileName += NStr::Format( "current%d%d", k, i );
			szProjectFileName += szExtension.c_str() + 1;
			if ( _access( szProjectFileName.c_str(), 00 ) )
			{
				bFound = true;
				break;
			}
		}
		
		if ( bFound )
			break;
	}
}

CETreeCtrl *CParentFrame::CreateTrees()
{
	CETreeCtrl *pResult = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( !pResult )
	{
		string szTreeName = szComposerName.substr( 0, szComposerName.rfind( ' ' ) );
		szTreeName += " Tree";
		pResult = pTreeDockBar->AddTree( szTreeName.c_str(), 1000, true );
		pResult->LoadImageList( IDB_TREEBMP );
	}
	else
		pResult->GetRootItem()->RemoveAllChilds();
	pResult->CreateRootItem( nTreeRootItemID );

	return pResult;
}

void CParentFrame::SaveFrameOwnData( IDataTree *pDT )
{
	pDT->StartChunk( "own_data" );
	CTreeAccessor tree = pDT;
	
	tree.Add( "export_file_name", &szPrevExportFileName );
		
	pDT->FinishChunk();
}

void CParentFrame::LoadFrameOwnData( IDataTree *pDT )
{
	pDT->StartChunk( "own_data" );
	CTreeAccessor tree = pDT;
	
	string szPrevExportDir;
	tree.Add( "export_dir", &szPrevExportDir );
	if ( szPrevExportDir.size() > 0 )
	{
		szPrevExportFileName = szPrevExportDir;
		szPrevExportFileName += "1.xml";
	}
	else
		tree.Add( "export_file_name", &szPrevExportFileName );
	
	pDT->FinishChunk();
}

void CParentFrame::OnSetFocus(CWnd* pOldWnd) 
{
	g_frameManager.SetActiveFrame( this );
	SECWorksheet::OnSetFocus(pOldWnd);
	pWndView->SetFocus();
}

void CParentFrame::OnFileCreateNewProject() 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )			//Если уже был создан проект
	{
		if ( SaveFrame( true ) )
			return;						//нажали cancel
	}
	
	GenerateProjectName();
	string szTempFileName = szProjectFileName.substr( szProjectFileName.rfind('\\') + 1 );
	string szNewProjectName;
	string szTitle = "Create New ";
	szTitle += szComposerName;
	szTitle += "Project";
	
	if ( !ShowFileDialog( szNewProjectName, (theApp.GetSourceDir() + szAddDir).c_str(), szTitle.c_str(), FALSE, szExtension.c_str(), szTempFileName.c_str(), szComposerFilter ) )
		return;
	
	UnLockFile();
	szProjectFileName = szNewProjectName;
	if ( !LockFile() )
	{
		AfxMessageBox( "Can not lock file, aborting" );
		return;
	}
	
	szPrevExportFileName = "";
	pTree = CreateTrees();
	
	bNewProjectJustCreated = true;
	OnFileSave();				//сразу сохраняем проект
	bNewProjectJustCreated = false;
	SetChangedFlag( false );
	ComputeCaption();
	IScene *pSG = GetSingleton<IScene>();
	pSG->Clear();
	pOIDockBar->ClearControl();
	SpecificInit();
	::ShowFirstChildElementInPropertyView( pTree, pOIDockBar );
}

void CParentFrame::OnFileOpen() 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )			//Если уже был создан проект
	{
		if ( SaveFrame( true ) )
			return;						//нажали cancel
		pOIDockBar->ClearControl();
	}
	
	std::string szNewProjectFile;
	string szTitle = "Open ";
	szTitle += szComposerName;
	szTitle += " Project";
	if ( !ShowFileDialog( szNewProjectFile, (theApp.GetSourceDir() + szAddDir).c_str(), szTitle.c_str(), TRUE, szExtension.c_str(), 0, szComposerFilter ) )
		return;
	
	CParentFrame *pFrame = g_frameManager.ActivateFrameByExtension( szNewProjectFile.c_str() );
	if ( pFrame == 0 )
	{
		int nRes = AfxMessageBox( "Error: unknown file extension, do you want to try opening this file in current editor?", MB_YESNO );
		if ( nRes == IDYES )
			pFrame = g_frameManager.GetActiveFrame();
	}

	if ( pFrame )
		pFrame->LoadComposerFile( szNewProjectFile.c_str() );

	MSG msg;
	PeekMessage( &msg, GetSafeHwnd(), WM_MOUSEMOVE, WM_RBUTTONUP, PM_REMOVE );
}

void CParentFrame::LoadComposerFile( const char *pszFileName )
{
	UnLockFile();
	szProjectFileName = pszFileName;
	if ( !LockFile() )
		return;
	szPrevExportFileName = "";
	CETreeCtrl *pTree = CreateTrees();
	CTreeItem *pRootItem = pTree->GetRootItem();
	
	{
		CPtr<IDataStream> pXMLStream = OpenFileStream( szProjectFileName.c_str(), STREAM_ACCESS_READ );
		if ( !pXMLStream )
		{
			AfxMessageBox( "Unable to open file!" );
			UnLockFile();
			pTreeDockBar->DeleteTree( 0 );
			return;
		}
		
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ, szComposerSaveName.c_str() );
		try
		{
			pTreeDockBar->LoadTrees( pDT );
		}
		catch (...)
		{
			AfxMessageBox( "Wrong file format!" );
			UnLockFile();
			pTreeDockBar->DeleteTree( 0 );
			return;
		}
		theApp.AddToRecentFileList( pszFileName );
		IScene *pSG = GetSingleton<IScene>();
		pSG->Clear();
		pOIDockBar->ClearControl();
		SpecificInit();
		LoadFrameOwnData( pDT );
		LoadRPGStats( pDT, pRootItem );
		
		LoadTransactions( pDT );
	}
	
	SetChangedFlag( false );
	ComputeCaption();
	::ShowFirstChildElementInPropertyView( pTree, pOIDockBar );
}

void CParentFrame::OnFileSave()
{
	FILETIME time;
	if ( m_szOldProjectName.size() == 0 )
		m_szOldProjectName = szProjectFileName;
	time = GetFileChangeTime( m_szOldProjectName.c_str() );
	
	{
		std::string szTemp = GetDirectory( szProjectFileName.c_str() );
		szTemp += "backup.tmp";
		DeleteFile( szTemp.c_str() );
		MoveFile( szProjectFileName.c_str(), szTemp.c_str() );
	}

	CPtr<IDataStream> pXMLStream = OpenFileStream( szProjectFileName.c_str(), STREAM_ACCESS_WRITE );
	ASSERT( pXMLStream != 0 );
	if ( pXMLStream == 0 )
		return;
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE, szComposerSaveName.c_str() );
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRoot = pTree->GetRootItem();
	
	SaveFrameOwnData( pDT );
	SaveRPGStats( pDT, pRoot, szProjectFileName.c_str() );
	SaveTransactions( &time, pDT, szProjectFileName.c_str(), E_SAVE );
	if ( g_frameManager.GetActiveFrameType() != CFrameManager::E_GUI_FRAME )
		pTreeDockBar->SaveTrees( pDT );
	else
		SpecificSave( pDT );
	m_szOldProjectName = "";
	SetChangedFlag( false );
}

void CParentFrame::OnFileSaveProjectAs()
{
	std::string szNewProjectFile;
	string szTitle = "Save ";
	szTitle += szComposerName;
	szTitle += " Project As";

	std::string szOldProjectName = szProjectFileName;
	GenerateProjectName();
	string szTempFileName = szProjectFileName.substr( szProjectFileName.rfind('\\') + 1 );
	
	if ( !ShowFileDialog( szNewProjectFile, (theApp.GetSourceDir()+szAddDir).c_str(), szTitle.c_str(), FALSE, szExtension.c_str(), szTempFileName.c_str(), szComposerFilter ) )
	{
		szProjectFileName = szOldProjectName;
		return;
	}
	szProjectFileName = szOldProjectName;
	
	/*
	{
		std::string szTemp = GetDirectory( szProjectFileName.c_str() );
		szTemp += "backup.tmp";
		CopyFile( szProjectFileName.c_str(), szTemp.c_str() );
		}
	*/
	
	std::string szDirFrom = GetDirectory( szProjectFileName.c_str() );
	std::string szDirTo = GetDirectory( szNewProjectFile.c_str() );
	if ( szDirFrom != szDirTo )
	{
		string szMask = "*.*";
		vector<string> dirs;
		NFile::EnumerateFiles( szDirFrom.c_str(), szMask.c_str(), NFile::CGetAllDirectoriesRelative( szDirFrom.c_str(), &dirs ), true );
		for ( int i=0; i<dirs.size(); i++ )
		{
			std::string szRes = szDirTo + dirs[i];
			CreateDirectory( szRes.c_str(), NULL );
		}

		vector<string> files;
		NFile::EnumerateFiles( szDirFrom.c_str(), szMask.c_str(), NFile::CGetAllFilesRelative( szDirFrom.c_str(), &files ), true );
		for ( int i=0; i<files.size(); i++ )
		{
			std::string szFrom = szDirFrom + files[i];
			std::string szTo = szDirTo + files[i];
			CopyFile( szFrom.c_str(), szTo.c_str(), FALSE );
		}
	}

	szProjectFileName = szNewProjectFile;
	m_szOldProjectName = szOldProjectName;
	OnFileSave();
	ComputeCaption();
}

string CParentFrame::GetExportFileName()
{
	if ( bDefaultExportName )
		return "1.xml";
	else
		return "";
}

bool CParentFrame::ReadConfigFile( const char *pszDirectory, bool bBatchMode )
{
	std::string szConfigFile = pszDirectory;
	szConfigFile += szConfigFileName;		//gamma.cfg
	
	CPtr<IDataStream> pConfigStream = OpenFileStream( szConfigFile.c_str(), STREAM_ACCESS_READ );
	if ( !pConfigStream )
	{
		std::string szTempDir = pszDirectory;
		if ( szTempDir.size() <= 3 )
		{
			m_fBrightness = 0;
			m_fContrast = 0;
			m_fGamma = 0;
			if ( bBatchMode )
				return false;

			CString szErr;
			szErr.Format( "Error: Can not find config file %s\nPossibly you need to change source directory,\nor create config file for such types of projects\nDo you want to create default config file for this composer module?", szConfigFileName.c_str() );
			int nRes = AfxMessageBox( szErr, MB_OKCANCEL );
			if ( nRes == IDOK )
			{
				WriteConfigFile( false, false );
				return true;
			}
			else
				return false;
		}

		szTempDir = szTempDir.substr( 0, szTempDir.size() - 1 );
		szTempDir = GetDirectory( szTempDir.c_str() );
		return ReadConfigFile( szTempDir.c_str(), bBatchMode );
	}
	
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pConfigStream, IDataTree::READ );
	CTreeAccessor saver = pDT;
	saver.Add( "Brightness", &m_fBrightness );
	saver.Add( "Contrast", &m_fContrast );
	saver.Add( "Gamma", &m_fGamma );
	return true;
}

bool CParentFrame::WriteConfigFile( bool bAsk, bool bCurrentProjectOnly )
{
	std::string szConfigFile;
	if ( !bCurrentProjectOnly )
		szConfigFile = theApp.GetSourceDir() + szAddDir + szConfigFileName;
	else
		szConfigFile = GetDirectory( szProjectFileName.c_str() ) + szConfigFileName;

	if ( bAsk && _access( szConfigFile.c_str(), 04 ) )
	{
		CString szErr;
		szErr.Format( "Error: Can not find file\n%s\nDo you want to create new config file?", szConfigFile.c_str() );
		int nRes = AfxMessageBox( szErr, MB_YESNO );
		if ( nRes == IDNO )
			return false;
	}
	
	CPtr<IDataStream> pConfigStream = CreateFileStream( szConfigFile.c_str(), STREAM_ACCESS_WRITE );
	if ( pConfigStream != 0 )
	{
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pConfigStream, IDataTree::WRITE );
		CTreeAccessor saver = pDT;
		saver.Add( "Brightness", &m_fBrightness );
		saver.Add( "Contrast", &m_fContrast );
		saver.Add( "Gamma", &m_fGamma );
	}
	else
	{
		AfxMessageBox( "Unable to create config file!" );
		return false;
	}
	return true;
}

void CParentFrame::OnFileExportFiles() 
{
	if ( !ReadConfigFile( GetDirectory( szProjectFileName.c_str() ).c_str(), false ) )
		return;

	string szExportFileName;
	if ( !szPrevExportFileName.empty() )
	{
		if ( IsRelatedPath( szPrevExportFileName.c_str() ) )
			MakeFullPath( (theApp.GetDestDir()+szAddDir).c_str(), szPrevExportFileName.c_str(), szExportFileName );
		else
			szExportFileName = szPrevExportFileName;
	}
	else
	{
		int nRes = GetFileAttributes( (theApp.GetDestDir() + szAddDir).c_str() );
		if ( nRes == -1 )
		{
			mkdir( (theApp.GetDestDir() + szAddDir).c_str() );
		}

		szExportFileName = theApp.GetDestDir() + szAddDir;
		std::string szSource = theApp.GetSourceDir() + szAddDir;
		nRes = GetFileAttributes( szSource.c_str() );
		if ( nRes != -1 )
		{
			std::string szRes;
			if ( MakeRelativePath( szSource.c_str(), GetDirectory(szProjectFileName.c_str()).c_str(), szRes ) )
				szExportFileName += szRes;
		}
		szExportFileName += GetExportFileName();
	}

	CBrowseDialog brs;
	brs.SetFileName( szExportFileName.c_str() );
	brs.SetFilter( szXMLFilter.c_str() );
	brs.SetExtension( szExportExtension.c_str() );

	string szExportDir = GetDirectory( szExportFileName.c_str() );
	string szShortExportFileName = szExportFileName.substr( szExportFileName.rfind('\\') + 1 );
	string szTitle = "Export ";
	szTitle += szComposerName;
	szTitle += " Files";
	string szRelFileName;
	brs.SetTitle( szTitle.c_str() );
	
	int nRes = 0;
	while ( !nRes )
	{
		if ( brs.DoModal() != IDOK )
			return;

		szExportFileName = brs.GetFileName();
		CPtr<IDataStorage> pStorage = CreateStorage( GetDirectory( szExportFileName.c_str()).c_str(), STREAM_ACCESS_WRITE );

		nRes = MakeSubRelativePath( (theApp.GetDestDir() + szAddDir).c_str(), szExportFileName.c_str(), szRelFileName );
		if ( !nRes )
		{
			szRelFileName = szExportFileName;
			int nBox = AfxMessageBox( "This project is exporting to incorrect directory.\nThe export directory should be sub path relative to 'Export Directory' settings\nDo you want to continue export to that file?\n\n", MB_OKCANCEL );
			if ( nBox == IDOK )
				break;
		}
	}

/*
	int nRes = 0;
	while ( !nRes )
	{
		if ( !ShowFileDialog( szExportFileName, szExportDir.c_str(), szTitle.c_str(), FALSE, szExportExtension.c_str(), szShortExportFileName.c_str(), szXMLFilter.c_str() ) )
			return;
		
		nRes = MakeSubRelativePath( (theApp.GetDestDir() + szAddDir).c_str(), szExportFileName.c_str(), szRelFileName );
		if ( !nRes )
		{
			szRelFileName = szExportFileName;
			int nBox = AfxMessageBox( "This project is exporting to incorrect directory.\nThe export directory should be sub path relative to 'Export Directory' settings\nDo you want to continue export to that file?\n\n"
				"Задаваемая Export директория для проекта должна быть вложенной\nотносительно настроек редактора. Иначе будут баги при batch mode\nПродолжить экспорт в эту директорию?", MB_OKCANCEL );
			if ( nBox == IDOK )
				break;
		}
	}
*/

	if ( szPrevExportFileName != szRelFileName )
	{
		szPrevExportFileName = szRelFileName;
		SetChangedFlag( true );
	}
	
	BeginWaitCursor();
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	NI_ASSERT( pTree != 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	
	bool bErr = false;
	CPtr<IDataTree> pDT;
	if ( nFrameType == CFrameManager::E_SPRITE_FRAME || nFrameType == CFrameManager::E_TILESET_FRAME )
	{
		pDT = 0;
	}
	else
	{
		CPtr<IDataStream> pXMLStream = CreateFileStream( szExportFileName.c_str(), STREAM_ACCESS_WRITE );
		NI_ASSERT( pXMLStream != 0 );
		if ( pXMLStream == 0 )
		{
			bErr = true;
			string szErr = "Error: can not create stream: ";
			szErr += szExportFileName;
			AfxMessageBox( szErr.c_str() );
		}
		else
		{
			if ( nFrameType == CFrameManager::E_EFFECT_FRAME )
				pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE, "effect" );
			else
				pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE );
			FILETIME time;
			m_szOldProjectName = szProjectFileName;
			time = GetFileChangeTime( m_szOldProjectName.c_str() );
			SaveTransactions( &time, pDT, szExportFileName.c_str(), E_EXPORT );
		}
	}

	if ( !bErr )
	{
		if ( ExportFrameData( pDT, szProjectFileName.c_str(), szExportFileName.c_str(), pRootItem ) == false )
			AfxMessageBox( "Error while exporting project" );
	}
	EndWaitCursor();
}

void CParentFrame::OnUpdateFileExportFiles(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )			//Если уже был создан проект
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

void CParentFrame::OnUpdateFileSave(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )			//Если уже был создан проект
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

void CParentFrame::OnUpdateSaveProjectAs(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )			//Если уже был создан проект
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

void CParentFrame::OnUpdateCloseFile(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree != 0 )			//Если уже был создан проект
		pCmdUI->Enable( true );
	else
		pCmdUI->Enable( false );
}

void CParentFrame::OnUpdateViewAdvancedToolbar(CCmdUI* pCmdUI) 
{
	if ( pToolBar )
	{
		if ( pToolBar->IsVisible() )
			pCmdUI->SetCheck( 1 );
		else
			pCmdUI->SetCheck( 0 );
		pCmdUI->Enable( 1 );
	}
	else
		pCmdUI->Enable( 0 );
}

void CParentFrame::OnViewAdvancedToolbar() 
{
	if ( !pToolBar )
		return;
	bool bVis = pToolBar->IsVisible();
	int nCommand = bVis ? SW_HIDE : SW_SHOW;
	theApp.ShowSECControlBar( pToolBar, nCommand );
}

void CParentFrame::OnFileClose() 
{
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree )
	{
		if ( SaveFrame( true ) )
			return;						//нажали cancel

		pTreeDockBar->DeleteTree( 0 );
		pOIDockBar->ClearControl();
		SpecificClearBeforeBatchMode();
		szProjectFileName = "";
		ComputeCaption();
	}
}

BOOL CParentFrame::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	SetCursor( LoadCursor(0, IDC_ARROW ) );
	return SECWorksheet::OnSetCursor(pWnd, nHitTest, message);
}

void CParentFrame::OnFileSetdirectories() 
{
	CSetDirDialog dlg;
	dlg.SetSourceDir( theApp.GetSourceDir().c_str() );
	dlg.SetExecDir( theApp.GetExecDir().c_str() );
	dlg.SetExecArgs( theApp.GetExecArgs().c_str() );
	if ( dlg.DoModal() == IDOK )
	{
		theApp.SetSourceDir( dlg.GetSourceDir() );
		theApp.SetExecDir( dlg.GetExecDir() );
		theApp.SetExecArgs( dlg.GetExecArgs() );
		theApp.SaveRegisterData();
	}
}

void CParentFrame::OnSetPictureOptions() 
{
	CPictureOptions dlg;
	dlg.SetBrightness( m_fBrightness );
	dlg.SetContrast( m_fContrast );
	dlg.SetGamma( m_fGamma );
	if ( dlg.DoModal() != IDOK )
		return;
	
	m_fBrightness = dlg.GetBrightness();
	m_fContrast = dlg.GetContrast();
	m_fGamma = dlg.GetGamma();
	
	WriteConfigFile( false /*true*/, dlg.GetCurrentProjectOnly() );
}

bool CParentFrame::ConvertAndSaveImage( const char *pszSrc, const char *pszDest )
{
	IImageProcessor *pIP = GetImageProcessor();
	CPtr<IDataStream> pStream = OpenFileStream( pszSrc, STREAM_ACCESS_READ );
	if ( !pStream )
		return false;
	CPtr<IImage> pImage = pIP->LoadImage( pStream );
	NI_ASSERT( pImage != 0 );
	if ( !pImage )
		return false;

	SaveCompressedTexture( pImage, pszDest );
	return true;
}

void CParentFrame::OnEditSetbackgroundcolor() 
{
	CColorDialog dlg( m_backgroundColor );
	
	dlg.m_cc.Flags |= CC_FULLOPEN;
	if ( dlg.m_cc.lpCustColors )
	{
		dlg.m_cc.lpCustColors[0] = m_backgroundColor;
	}
	if ( IDOK != dlg.DoModal() )
		return;
	m_backgroundColor = COLORREF2GFXColor( dlg.GetColor() );
	theApp.SaveRegisterData();
	
	GFXDraw();
}

void CParentFrame::OnFileBatchMode() 
{
	CBatchModeDialog dlg;
	dlg.SetSourceDir( (theApp.GetSourceDir() + szAddDir).c_str() );
	dlg.SetDestDir( (theApp.GetDestDir() + szAddDir).c_str() );
	std::string szMask = "current";
	szMask += szExtension.c_str() + 1;
	dlg.SetSearchMask( szMask.c_str() );
	if ( dlg.DoModal() == IDOK )
	{
		RunBatchExporter( dlg.GetSourceDir(), dlg.GetDestDir(), dlg.GetSearchMask(), dlg.GetForceModeFlag(), dlg.GetOpenAndSaveFlag() );
	}
}

void CParentFrame::RunBatchExporter( const char *pszSourceDir, const char *pszDestDir, const char *pszMask, bool bForceFlag, bool bOpenSave )
{
	vector<string> files, errorFiles, noConfigFiles;

	NFile::EnumerateFiles( pszSourceDir, pszMask, NFile::CGetAllFiles( &files ), true );

	BeginWaitCursor();
	CProgressDialog progressDialog;
	progressDialog.Init( files.size() );
	progressDialog.Create( IDD_PROGRESS_DIALOG, this );
	progressDialog.ShowWindow( SW_SHOW );
	szProjectFileName = "";

	for ( int i=0; i<files.size(); i++ )
	{
		progressDialog.SetProjectName( files[i].c_str() );
		int nRes = ExportSingleFile( files[i].c_str(), pszDestDir, bForceFlag, bOpenSave );
		if ( nRes == NO_CONFIG_FILE )
			noConfigFiles.push_back( files[i] );
		else if ( nRes )
			errorFiles.push_back( files[i] );
		progressDialog.SetPosition( i );
	}

	progressDialog.DestroyWindow();
	EndWaitCursor();

	if ( errorFiles.size() > 0 )
	{
		CString szErrorStr;
		szErrorStr.Format( "Can not export project files total count %d\n", errorFiles.size() );
		for ( int i=0; i<errorFiles.size(); i++ )
		{
			szErrorStr += errorFiles[i].c_str();
			szErrorStr += '\n';
		}
		
		AfxMessageBox( szErrorStr );
	}

	if ( noConfigFiles.size() > 0 )
	{
		CString szErrorStr;
		szErrorStr.Format( "Following %d projects do not have config file (%s)\n", noConfigFiles.size(), szConfigFileName.c_str() );
		for ( int i=0; i<noConfigFiles.size(); i++ )
		{
			szErrorStr += noConfigFiles[i].c_str();
			szErrorStr += '\n';
		}
		
		AfxMessageBox( szErrorStr );
	}

	GetSingleton<IScene>()->Clear();
	SpecificClearBeforeBatchMode();
}

int CParentFrame::ExportSingleFile( const char *pszFileName, const char *pszDestDir, bool bForceFlag, bool bOpenSave )
{
	SpecificClearBeforeBatchMode();
	int nResult = 0;

	if ( !bOpenSave )
	{
		if ( !ReadConfigFile( GetDirectory( pszFileName ).c_str(), true ) )
			nResult = NO_CONFIG_FILE;

		CPtr<CTreeItem> pRootItem = static_cast<CTreeItem *> ( GetCommonFactory()->CreateObject( nTreeRootItemID ) );
		CPtr<IDataStream> pXMLStream = CreateFileStream( pszFileName, STREAM_ACCESS_READ );
		if ( !pXMLStream )
			return -1;
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ, szComposerSaveName.c_str() );
		if ( !pDT )
			return -2;
		
		LoadFrameOwnData( pDT );
		LoadTransactions( pDT );
		pRootItem->operator&( *pDT );
		pRootItem->DeleteNullChilds();
		pRootItem->CreateDefaultChilds();
		
		string szExportFileName;
		if ( szPrevExportFileName.size() == 0 )
			return -3;
		if ( IsRelatedPath( szPrevExportFileName.c_str() ) )
			MakeFullPath( pszDestDir, szPrevExportFileName.c_str(), szExportFileName );
		else
			return -4;
		
		CPtr<IDataStorage> pStorage = CreateStorage( GetDirectory(szExportFileName.c_str()).c_str(), STREAM_ACCESS_WRITE );
		if ( !bForceFlag )
		{
			FILETIME sourceTime = FindMaximalSourceTime( pszFileName, pRootItem );
			if ( sourceTime.dwHighDateTime == 0 && sourceTime.dwLowDateTime == 0 )
				return -5;
			FILETIME projectTime = GetFileChangeTime( pszFileName );
			if ( projectTime > sourceTime )
				sourceTime = projectTime;
			
			FILETIME exportTime = FindMinimalExportFileTime( szExportFileName.c_str(), pRootItem );
			if ( exportTime < sourceTime )
				bForceFlag = true;
		}
		if ( !bForceFlag )
			return nResult;
		
		LoadRPGStats( pDT, pRootItem );
		if ( !LoadFramePreExportData( pszFileName, pRootItem ) )
			return -6;

		CPtr<IDataStream> pExportStream = CreateFileStream( szExportFileName.c_str(), STREAM_ACCESS_WRITE );
		NI_ASSERT( pExportStream != 0 );
		CPtr<IDataTree> pExportDT = CreateDataTreeSaver( pExportStream, IDataTree::WRITE );
		NI_ASSERT( pExportDT != 0 );
		FILETIME time;
		m_szOldProjectName = pszFileName;
		time = GetFileChangeTime( m_szOldProjectName.c_str() );
		SaveTransactions( &time, pExportDT, szExportFileName.c_str(), E_BATCH_EXPORT );

		if ( !ExportFrameData( pExportDT, pszFileName, szExportFileName.c_str(), pRootItem ) )
			return -7;
	}
	
	else
	{
		{
			std::string szTemp = GetDirectory( pszFileName );
			szTemp += "backup.tmp";
			DeleteFile( szTemp.c_str() );
			CopyFile( pszFileName, szTemp.c_str(), FALSE );
		}

		CETreeCtrl *pTree = CreateTrees();
		CPtr<CTreeItem> pRootItem = pTree->GetRootItem();
		
		{
			szProjectFileName = pszFileName;
			CPtr<IDataStream> pXMLStream = OpenFileStream( pszFileName, STREAM_ACCESS_READ );
			CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ, szComposerSaveName.c_str() );
			pTreeDockBar->LoadTrees( pDT );
			pOIDockBar->ClearControl();
			SpecificInit();
			LoadFrameOwnData( pDT );
			LoadRPGStats( pDT, pRootItem );
/*
			if ( !LoadFramePreExportData( pszFileName, pRootItem ) )
				return -6;
*/

/*
			LoadFrameOwnData( pDT );
			SpecificInit();
			pRootItem->operator&( *pDT );
			pRootItem->DeleteNullChilds();
			pRootItem->CreateDefaultChilds();
			LoadRPGStats( pDT, pRootItem );
*/
		}

		FILETIME time;
		m_szOldProjectName = pszFileName;
		time = GetFileChangeTime( m_szOldProjectName.c_str() );

		CPtr<IDataStream> pXMLStream = OpenFileStream( pszFileName, STREAM_ACCESS_WRITE );
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE, szComposerSaveName.c_str() );

		SaveTransactions( &time, pDT, pszFileName, E_BATCH_SAVE );

		pRootItem->operator &( *pDT );
		SaveFrameOwnData( pDT );
		SaveRPGStats( pDT, pRootItem, szProjectFileName.c_str() );
	}

	return nResult;
}

int CParentFrame::DisplayInsertMenu()
{
	POINT point;
	GetCursorPos( &point );
	
	CMenu menu;
	menu.LoadMenu( IDR_INSERT_TREE_ITEM_MENU );
	CMenu *popup = menu.GetSubMenu( 0 );
	int nRes = popup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, this );
	return nRes;
}

int CParentFrame::DisplayDeleteMenu()
{
	POINT point;
	GetCursorPos( &point );
	
	CMenu menu;
	menu.LoadMenu( IDR_DELETE_TREE_ITEM );
	CMenu *popup = menu.GetSubMenu( 0 );
	int nRes = popup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y, this );
	return nRes;
}

void CParentFrame::OnUpdateInsertTreeItem(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
}

void CParentFrame::OnUpdateDeleteTreeItem(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
}

bool CParentFrame::LockFile()
{
	NI_ASSERT( szProjectFileName.size() > 0 );
	
	std::string szDir = GetDirectory( szProjectFileName.c_str() );
	vector<string> files, errorFiles;
	
	std::string szUserName;
	{
		unsigned long nSize = 255;
		char temp[255];
		GetUserName( temp, &nSize );
		szUserName = temp;
	}

	NFile::EnumerateFiles( szDir.c_str(), "locked_*", NFile::CGetAllFiles( &files ), false );
	if ( !files.empty() )
	{
		if ( files.size() > 1 )
		{
			int nRes = AfxMessageBox( NStr::Format("The file %s is locked more than one time, do you want to remove all loked files?", szProjectFileName.c_str() ), MB_YESNO );
			if ( nRes == IDYES )
			{
				for ( int i=0; i<files.size(); i++ )
					remove( files[i].c_str() );
			}
			else
				return false;
		}
		else
		{
			string szTemp = files[0].c_str();
			szTemp = szTemp.substr( szTemp.rfind('\\') + 8 );
			if ( szUserName == szTemp )
				return true;			//файл уже залочен этим же юзером
			int nRes = AfxMessageBox( NStr::Format("The file %s is locked by %s, do you want to open it?", szProjectFileName.c_str(), szTemp.c_str() ), MB_YESNO );
			if ( nRes == IDNO )
				return false;
			else
				remove( files[0].c_str() );
		}
	}
	
	const std::string szLockedFile = NStr::Format( "locked_%s", szUserName );
	CPtr<IDataStream> pStream = CreateFileStream( (szDir + szLockedFile).c_str(), STREAM_ACCESS_WRITE );
	return true;
}

bool CParentFrame::UnLockFile()
{
	std::string szLockedFile = GetDirectory( szProjectFileName.c_str() );
	if ( szLockedFile.empty() )
		return false;
	unsigned long nSize = 255;
	char temp[255];
	GetUserName( temp, &nSize );
	szLockedFile += NStr::Format( "locked_%s", temp );
	return !remove( szLockedFile.c_str() );
}

int CParentFrame::STransaction::operator &( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "PreviousPath", &szSourceName );
	saver.Add( "PreviousDate", &szSourceDate );
	saver.Add( "PreviousTime", &szSourceTime );
	saver.Add( "PreviousOwner", &szSourceOwner );
	saver.Add( "Action", &szAction );
	saver.Add( "CurrentPath", &szDestName );
	saver.Add( "CurrentDate", &szDestDate );
	saver.Add( "CurrentTime", &szDestTime );
	saver.Add( "CurrentOwner", &szDestOwner );
	return 0;
}

void CParentFrame::LoadTransactions( IDataTree *pDT )
{
	transactions.clear();
	CTreeAccessor tree = pDT;
	tree.Add( "History", &transactions );
}

void CParentFrame::SaveTransactions( FILETIME *pFT, IDataTree *pDT, const char *pszDest, int nAction )
{
	STransaction trans;
	trans.szSourceName = m_szOldProjectName;
	if ( transactions.size() > 0 )
		trans.szSourceOwner = transactions.back().szDestOwner;
	SYSTEMTIME st;
	FileTimeToLocalFileTime( pFT, pFT );
	FileTimeToSystemTime( pFT, &st );
	trans.szSourceDate = NStr::Format( "%.2d.%.2d.%d", st.wDay, st.wMonth, st.wYear );
	trans.szSourceTime = NStr::Format( "%.2d:%.2d:%.2d", st.wHour, st.wMinute, st.wSecond );

	switch ( nAction )
	{
		case E_SAVE:
			trans.szAction = "Saved as";
			break;
		case E_IMPORT:
			trans.szAction = "Imported from excel as";
			break;
		case E_EXPORT:
			trans.szAction = "Exported as";
			break;
		case E_BATCH_EXPORT:
			trans.szAction = "Batch exported as";
			break;
		case E_BATCH_SAVE:
			trans.szAction = "Batch saved as";
			break;
		default:
			AfxMessageBox( "Error: unknown transaction" );
	}

	char temp[255];
	unsigned long nSize = 255;
	GetUserName( temp, &nSize );
	trans.szDestName = pszDest;
	trans.szDestOwner = temp;
	GetLocalTime( &st );
	trans.szDestDate = NStr::Format( "%.2d.%.2d.%d", st.wDay, st.wMonth, st.wYear );
	trans.szDestTime = NStr::Format( "%.2d:%.2d:%.2d", st.wHour, st.wMinute, st.wSecond );

	CTreeAccessor tree = pDT;

	if ( nAction == E_EXPORT || nAction == E_BATCH_EXPORT )
	{
		tree.Add( "History", &trans );
	}
	else
	{
		transactions.push_back( trans );
		if ( transactions.size() > TRANSACTION_LIMIT )
		{
			transactions.erase( transactions.begin(), transactions.begin() + transactions.size() - TRANSACTION_LIMIT );
		}
		tree.Add( "History", &transactions );
	}
}
void CParentFrame::OnExportPak() 
{
	std::string szCommandLine;
	if ( !ShowFileDialog( szCommandLine, theApp.GetEditorDir().c_str(), "Enter name for PAK-file to compress current MOD to", FALSE, "pak", NULL, "PAK files (*.pak)|*.pak|" ) )
		return;
	szCommandLine = " -9 -R -D \"" + szCommandLine + "\" *.*";
	ShellExecute( 0, "open", "zip.exe", szCommandLine.c_str(), (theApp.GetDestDir()).c_str(), SW_SHOWNORMAL );
}
void CParentFrame::OnRunGame() 
{
	std::string szMODPath = theApp.GetDestDir();
	int nPos = szMODPath.rfind( "\\", szMODPath.length() - 1 );
	if ( ShellExecute( 0, "open", (theApp.GetExecDir() + "\\game.exe").c_str(), (theApp.GetExecArgs() + "-mod\"" + szMODPath.substr( nPos + 1 ) + "\"").c_str(), theApp.GetExecDir().c_str(), SW_SHOWNORMAL ) != 0 )
		AfxMessageBox( "Unable to find zip.exe!\n Path to this file should be set in your PATH environment variable." );
}
void CParentFrame::OnHelp() 
{
	std::string szPath = theApp.GetEditorDir() + "reshelp.chm";
	ShellExecute( 0, "open", szPath.c_str(), NULL, NULL, SW_SHOWNORMAL );
}
void CParentFrame::SwitchDockerVisible( SECControlBar *pBar )
{
	if ( !pBar )
		return;
	bool bVis = pBar->IsVisible();
	int nCommand = bVis ? SW_HIDE : SW_SHOW;
	theApp.ShowSECControlBar( pBar, nCommand );
}
void CParentFrame::UpdateShowMenu( CCmdUI* pCmdUI, SECControlBar *pBar )
{
	if ( pBar )
	{
		if ( pBar->IsVisible() )
			pCmdUI->SetCheck( 1 );
		else
			pCmdUI->SetCheck( 0 );
		pCmdUI->Enable( 1 );
	}
	else
		pCmdUI->Enable( 0 );
}
void CParentFrame::OnShowOI()
{
	SwitchDockerVisible( pOIDockBar );
}
void CParentFrame::OnUpdateShowOI(CCmdUI* pCmdUI) 
{
	UpdateShowMenu( pCmdUI, pOIDockBar );
}
void CParentFrame::OnShowTree()
{
	SwitchDockerVisible( pTreeDockBar );
}
void CParentFrame::OnUpdateShowTree(CCmdUI* pCmdUI) 
{
	UpdateShowMenu( pCmdUI, pTreeDockBar );
}
void CParentFrame::OnMODSettings()
{
	CMODDialog dlg;
	dlg.mExportDir = theApp.GetCleanDestDir().c_str();
	dlg.mName = theApp.GetMODName().c_str();
	dlg.mVersion = theApp.GetMODVersion().c_str();
	dlg.mDesc = theApp.GetMODDesc().c_str();
	if ( dlg.DoModal() == IDOK )
	{
		BeginWaitCursor();
		theApp.SetDestDir( dlg.mExportDir );
		std::string szPath = theApp.GetDestDir().c_str();
		std::string szName = dlg.mName;
		std::string szVersion = dlg.mVersion;
		std::string szDesc = dlg.mDesc;
		theApp.WriteMODFile( szPath, szName, szVersion, szDesc );
		theApp.SetMODName( szName.c_str() );
		theApp.SetMODVersion( szVersion.c_str() );
		theApp.SetMODDesc( szDesc.c_str() );
		{
			CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
			pStorage->RemoveStorage( "MOD" );
			if ( CPtr<IDataStorage> pMODStorage = OpenStorage( (theApp.GetDestDir() + "*.pak").c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON ) )
				pStorage->AddStorage( pMODStorage, "MOD" );
			CPtr<IObjectsDB> pDB = GetSingleton<IObjectsDB>();
			pDB->LoadDB();
			GetSingleton<IGFX>()->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\medium" ) );
			CReferenceDialog::InitLists();
		}
		EndWaitCursor();
	}	
}
void CParentFrame::SwitchActiveFrame( int id )
{
	CParentFrame *pFrame = g_frameManager.GetFrame(	id );
	g_frameManager.SetActiveFrame( pFrame );
	pFrame->PostMessage( WM_SETFOCUS, 0, 0 );
}
void CParentFrame::UpdateFrameMenu( CCmdUI* pCmdUI, int id )
{
	if ( g_frameManager.GetActiveFrameType() == id )
		pCmdUI->SetCheck( 1 );
	else
		pCmdUI->SetCheck( 0 );
}
void CParentFrame::OnExpandTree()
{
	if ( CETreeCtrl *pETree = pTreeDockBar->GetTreeWithIndex( 0 ) )
		if ( CTreeItem *pTreeItem = pETree->GetRootItem() )
		{
			bTreeExpand = !bTreeExpand;
			for ( CTreeItem::CTreeItemList::const_iterator it = pTreeItem->GetBegin(); it != pTreeItem->GetEnd(); ++it )
				(*it)->ExpandTreeItem( bTreeExpand );
		}
}
void CParentFrame::OnUnitEditor()
{
	SwitchActiveFrame( CFrameManager::E_MESH_FRAME );
}
void CParentFrame::OnUpdateUnitEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_MESH_FRAME );
}
void CParentFrame::OnInfantryEditor()
{
	SwitchActiveFrame( CFrameManager::E_ANIMATION_FRAME );
}
void CParentFrame::OnUpdateInfantryEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_ANIMATION_FRAME );
}
void CParentFrame::OnSquadEditor()
{
	SwitchActiveFrame( CFrameManager::E_SQUAD_FRAME );
}
void CParentFrame::OnUpdateSquadEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_SQUAD_FRAME );
}
void CParentFrame::OnWeaponEditor()
{
	SwitchActiveFrame( CFrameManager::E_WEAPON_FRAME );
}
void CParentFrame::OnUpdateWeaponEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_WEAPON_FRAME );
}
void CParentFrame::OnMineEditor()
{
	SwitchActiveFrame( CFrameManager::E_MINE_FRAME );
}
void CParentFrame::OnUpdateMineEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_MINE_FRAME );
}
void CParentFrame::OnParticleEditor()
{
	SwitchActiveFrame( CFrameManager::E_PARTICLE_FRAME );
}
void CParentFrame::OnUpdateParticleEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_PARTICLE_FRAME );
}
void CParentFrame::OnSpriteEditor()
{
	SwitchActiveFrame( CFrameManager::E_SPRITE_FRAME );
}
void CParentFrame::OnUpdateSpriteEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_SPRITE_FRAME );
}
void CParentFrame::OnEffectEditor()
{
	SwitchActiveFrame( CFrameManager::E_EFFECT_FRAME );
}
void CParentFrame::OnUpdateEffectEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_EFFECT_FRAME );
}
void CParentFrame::OnBuildingEditor()
{
	SwitchActiveFrame( CFrameManager::E_BUILDING_FRAME );
}
void CParentFrame::OnUpdateBuildingEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_BUILDING_FRAME );
}
void CParentFrame::OnObjectEditor()
{
	SwitchActiveFrame( CFrameManager::E_OBJECT_FRAME );
}
void CParentFrame::OnUpdateObjectEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_OBJECT_FRAME );
}
void CParentFrame::OnFenceEditor()
{
	SwitchActiveFrame( CFrameManager::E_FENCE_FRAME );
}
void CParentFrame::OnUpdateFenceEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_FENCE_FRAME );
}
void CParentFrame::OnBridgeEditor()
{
	SwitchActiveFrame( CFrameManager::E_BRIDGE_FRAME );
}
void CParentFrame::OnUpdateBridgeEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_BRIDGE_FRAME );
}
void CParentFrame::OnTrenchEditor()
{
	SwitchActiveFrame( CFrameManager::E_TRENCH_FRAME );
}
void CParentFrame::OnUpdateTrenchEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_TRENCH_FRAME );
}
void CParentFrame::OnMissionEditor()
{
	SwitchActiveFrame( CFrameManager::E_MISSION_FRAME );
}
void CParentFrame::OnUpdateMissionEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_MISSION_FRAME );
}
void CParentFrame::OnChapterEditor()
{
	SwitchActiveFrame( CFrameManager::E_CHAPTER_FRAME );
}
void CParentFrame::OnUpdateChapterEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_CHAPTER_FRAME );
}
void CParentFrame::OnCampaignEditor()
{
	SwitchActiveFrame( CFrameManager::E_CAMPAIGN_FRAME );
}
void CParentFrame::OnUpdateCampaignEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_CAMPAIGN_FRAME );
}
void CParentFrame::OnMedalEditor()
{
	SwitchActiveFrame( CFrameManager::E_MEDAL_FRAME );
}
void CParentFrame::OnUpdateMedalEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_MEDAL_FRAME );
}
void CParentFrame::OnTerrainEditor()
{
	SwitchActiveFrame( CFrameManager::E_TILESET_FRAME );
}
void CParentFrame::OnUpdateTerrainEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_TILESET_FRAME );
}
void CParentFrame::OnRoadEditor()
{
	SwitchActiveFrame( CFrameManager::E_3DROAD_FRAME );
}
void CParentFrame::OnUpdateRoadEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_3DROAD_FRAME );
}
void CParentFrame::OnRiverEditor()
{
	SwitchActiveFrame( CFrameManager::E_3DRIVER_FRAME );
}
void CParentFrame::OnUpdateRiverEditor(CCmdUI* pCmdUI) 
{
	UpdateFrameMenu( pCmdUI, CFrameManager::E_3DRIVER_FRAME );
}
void CParentFrame::OnSaveObjects()
{
	std::string szSrc = theApp.GetSourceDir();
	std::string szTarget = theApp.GetDestDir();
	std::string szSourceData = theApp.GetEditorDataDir();
	BeginWaitCursor();
	std::map<std::string,bool> sides;
	std::map<std::string,bool> objectCounter;
	std::vector<std::string> files;
	NFile::EnumerateFiles( szSrc.c_str(), "*.xml", NFile::CGetAllFilesRelative( szSrc.c_str(), &files ), true );
	for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
	{
		CMapInfo mapInfo;
		std::string szPath = szSrc + (*it);
		NStr::ToLower( szPath );
		CPtr<IDataStream> pStreamXML = OpenFileStream( szPath.c_str(), STREAM_ACCESS_READ );
		CTreeAccessor saver = CreateDataTreeSaver( pStreamXML, IDataTree::READ );
		saver.AddTypedSuper( &mapInfo );
		for ( std::vector<SMapObjectInfo>::const_iterator it = mapInfo.objects.begin(); it != mapInfo.objects.end(); ++it )
			objectCounter[it->szName] = true;
		for ( std::vector<SMapObjectInfo>::const_iterator it = mapInfo.scenarioObjects.begin(); it != mapInfo.scenarioObjects.end(); ++it )
			objectCounter[it->szName] = true;
		for ( std::vector<SUnitCreation>::const_iterator it = mapInfo.unitCreation.units.begin(); it != mapInfo.unitCreation.units.end(); ++it )
		{
			std::string szName = it->aviation.szParadropSquadName;
			objectCounter[szName] = true;
			sides[it->szPartyName] = true;
			for ( std::vector<SUCAircraft>::const_iterator ait = it->aviation.aircrafts.begin(); ait != it->aviation.aircrafts.end(); ++ait )
				objectCounter[ait->szName] = true;
		}
	}
	files.resize( 0 );
	NFile::EnumerateFiles( szSrc.c_str(), "*.bzm", NFile::CGetAllFilesRelative( szSrc.c_str(), &files ), true );
	for ( std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it )
	{
		CMapInfo mapInfo;
		std::string szPath = szSrc + (*it);
		NStr::ToLower( szPath );
		CPtr<IDataStream> pStreamBZM = OpenFileStream( szPath.c_str(), STREAM_ACCESS_READ );
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBZM, IStructureSaver::READ );
		CSaverAccessor saver = pSaver;
		saver.Add( 1, &mapInfo );
		for ( std::vector<SMapObjectInfo>::const_iterator it = mapInfo.scenarioObjects.begin(); it != mapInfo.scenarioObjects.end(); ++it )
			objectCounter[it->szName] = true;
		for ( std::vector<SMapObjectInfo>::const_iterator it = mapInfo.objects.begin(); it != mapInfo.objects.end(); ++it )
			objectCounter[it->szName] = true;
		for ( std::vector<SUnitCreation>::const_iterator it = mapInfo.unitCreation.units.begin(); it != mapInfo.unitCreation.units.end(); ++it )
		{
			std::string szName = it->aviation.szParadropSquadName;
			objectCounter[szName] = true;
			sides[it->szPartyName] = true;
			for ( std::vector<SUCAircraft>::const_iterator ait = it->aviation.aircrafts.begin(); ait != it->aviation.aircrafts.end(); ++ait )
				objectCounter[ait->szName] = true;
		}
	}
	{
		CPtr<IDataStream> pXMLStream = OpenFileStream( ( szSourceData + "partys.xml" ).c_str(), STREAM_ACCESS_READ );
		if ( !pXMLStream )
			return;
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ );
		CTreeAccessor saver = pDT;
		std::vector<CUnitCreation::SPartyDependentInfo> partyInfos;
		saver.Add( "PartyInfo", &partyInfos );
		for ( std::vector<CUnitCreation::SPartyDependentInfo>::const_iterator it = partyInfos.begin(); it != partyInfos.end(); ++it )
			if ( sides.find( it->szPartyName ) != sides.end() )
			{
				objectCounter[it->szParatroopSoldierName] = true;
				objectCounter[it->szGunCrewSquad] = true;
				objectCounter[it->szHeavyMGSquad] = true;
				objectCounter[it->szResupplyEngineerSquad] = true;
			}
	}
	std::list<std::string> paths;
	{
		CPtr<IDataStream> pXMLStream = OpenFileStream( ( szSourceData + "objects.xml" ).c_str(), STREAM_ACCESS_READ );
		if ( !pXMLStream )
			return;
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ, "ObjectDB" );
		CTreeAccessor saver = pDT;
		std::vector<SObjectInfo> objectInfos;
		saver.Add( "Objects", &objectInfos );
		for ( std::vector<SObjectInfo>::const_iterator it = objectInfos.begin(); it != objectInfos.end(); ++it )
		{
			if ( objectCounter.find( it->szName ) == objectCounter.end() )
				continue;
			if ( it->szType == "squad" )
			{
				CPtr<IDataStream> pXMLStream = OpenFileStream( ( szSourceData + it->szPath + "\\1.xml" ).c_str(), STREAM_ACCESS_READ );
				if ( !pXMLStream )
					return;
				CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ, "base" );
				CTreeAccessor saver = pDT;		
				SSquadRPGStats stats;
				saver.Add( "RPG", &stats );
				for ( std::vector<std::string>::const_iterator it = stats.memberNames.begin(); it != stats.memberNames.end(); ++it )
					objectCounter[*it] = true;					
			}
		}
		for ( std::vector<SObjectInfo>::const_iterator it = objectInfos.begin(); it != objectInfos.end(); ++it )
		{
			if ( objectCounter.find( it->szName ) == objectCounter.end() )
				continue;
			paths.push_back( it->szPath );
		}
		for ( std::list<std::string>::const_iterator it = paths.begin(); it != paths.end(); ++it )
			MyCopyDir( szSourceData + (*it), szTarget + (*it) );
		EndWaitCursor();
	}
}
