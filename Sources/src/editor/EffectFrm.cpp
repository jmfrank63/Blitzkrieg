#include "StdAfx.h"
#include <io.h>
#include "..\Common\LegacyUiCompat.h"

#include "..\Scene\Scene.h"
#include "..\Formats\fmtEffect.h"
#include "..\StreamIO\DataTreeXML.h"
#include "..\Anim\Animation.h"
#include "..\Scene\PFX.h"

#include "editor.h"
#include "DirectionButtonDock.h"
#include "PropView.h"
#include "EffectView.h"
#include "TreeItem.h"
#include "EffTreeItem.h"
#include "EffectFrm.h"
#include "GameWnd.h"
#include "frames.h"
#include "RefDlg.h"

static const int THUMB_LIST_WIDTH = 145;
static char BASED_CODE szEffectComposeFilter[] = "Effect Compose Project Files (*.eff)|*.eff||";


IMPLEMENT_DYNCREATE(CEffectFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CEffectFrame, CParentFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_RUN_BUTTON, OnRunButton)
	ON_COMMAND(ID_STOP_BUTTON, OnStopButton)
	ON_UPDATE_COMMAND_UI(ID_STOP_BUTTON, OnUpdateStopButton)
	ON_UPDATE_COMMAND_UI(ID_RUN_BUTTON, OnUpdateRunButton)
	ON_UPDATE_COMMAND_UI(ID_INTERPOLATE_TREE_ITEM, OnUpdateInterpolateTreeItem)
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_BUTTON_CAMERA, OnButtonCamera)
	ON_UPDATE_COMMAND_UI(ID_BUTTON_CAMERA, OnUpdateButtonCamera)
	ON_COMMAND(ID_SHOW_DIRECTION_BUTTON, OnShowDirectionButton)
	ON_UPDATE_COMMAND_UI(ID_SHOW_DIRECTION_BUTTON, OnUpdateShowDirectionButton)
END_MESSAGE_MAP()


CEffectFrame::CEffectFrame()
{
	szComposerName = "Effect Editor";
	szExtension = "*.eff";
	szComposerSaveName = "Effect_Composer_Project";
	nTreeRootItemID = E_EFFECT_ROOT_ITEM;
	nFrameType = CFrameManager::E_EFFECT_FRAME;
	szAddDir = "effects\\effects\\";

	bRunning = false;
	pWndView = new CEffectView;
	pDirectionButtonDockBar = 0;
	bHorizontalCamera = false;
}

CEffectFrame::~CEffectFrame()
{
}

int CEffectFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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
	
	GenerateProjectName();
	return 0;
}

void CEffectFrame::ShowFrameWindows( int nCommand )
{
	if ( bRunning )
		OnStopButton();
	
	if ( pDirectionButtonDockBar )
		theApp.ShowSECControlBar( pDirectionButtonDockBar, nCommand );

	CParentFrame::ShowFrameWindows( nCommand );
	if ( nCommand == SW_SHOW )
	{
		g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );
		ICamera *pCamera = GetSingleton<ICamera>();
		pCamera->SetAnchor( CVec3(0, 0, 0) );
	}
	
	IParticleManager *pPM = GetSingleton<IParticleManager>();
	if ( nCommand == SW_SHOW )
		pPM->SetShareMode( SDSM_RELOAD );
	else
		pPM->SetShareMode( SDSM_SHARE );
}

void CEffectFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
}

void CEffectFrame::SpecificClearBeforeBatchMode()
{
	pDirectionButtonDockBar->SetAngle( ToRadian(45.0f) );
}

void CEffectFrame::UpdateEffectAngle()
{
	if ( !bRunning )
		return;

	float fAlpha = -pDirectionButtonDockBar->GetAngle();
	fAlpha += ToRadian( 45.0f );			//�������� 45 ��������
	if ( fAlpha >= FP_2PI )
		fAlpha -= FP_2PI;
	float fTemp = 1.0/sqrt(2);
	CQuat quat( fAlpha, CVec3(-fTemp, fTemp, 0 ) );
	SHMatrix matrix;
	matrix.Set( quat );
	pRunningEffect->SetEffectDirection( matrix );
}

BOOL CEffectFrame::Run()
{
	if ( !bRunning )
		return FALSE;
	
	GFXDraw();
	g_frameManager.GetGameWnd()->ValidateRect( 0 );
	return TRUE;
}

void CEffectFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();
	
	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );
	
	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
  pSG->Draw( pCamera );
	
	pGFX->EndScene();
	pGFX->Flip();
}

struct SSourceType
{
	bool bComplexParticleSource;			//��� ���������, ���� true, �� ������� particle source
	SSourceType() : bComplexParticleSource( false ) {}
	virtual int STDCALL operator&( IDataTree &ss );
};
/*
int SSourceType::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "ComplexParticleSource", &bComplexParticleSource );
	return 0;
}
*/
void CEffectFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	ASSERT( !pDT->IsReading() );
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_EFFECT_ROOT_ITEM );
	
	CTreeItem *pAnimsItem = 0, *pFuncParticles = 0, *pTreeItem = 0;
	pTreeItem = pRootItem->GetChildItem( E_EFFECT_ANIMATIONS_ITEM );
	NI_ASSERT( pTreeItem != 0 );
	pAnimsItem = pTreeItem;
	
	pTreeItem = pRootItem->GetChildItem( E_EFFECT_FUNC_PARTICLES_ITEM );
	NI_ASSERT( pTreeItem != 0 );
	pFuncParticles = pTreeItem;

/*
	if ( pAnimsItem->GetChildsCount() + pFuncParticles->GetChildsCount() == 0 )			//��������, ������ ���� � ������ � func ��������
		return;			//x3
*/

	SEffectDesc effDesc;
	CEffectCommonPropsItem *pCommonTreeItem = static_cast<CEffectCommonPropsItem *> ( pRootItem->GetChildItem( E_EFFECT_COMMON_PROPS_ITEM ) );
	effDesc.szSound = pCommonTreeItem->GetSoundName();
	CTreeItem::CTreeItemList::const_iterator it;
	
	for ( it=pAnimsItem->GetBegin(); it!=pAnimsItem->GetEnd(); ++it )
	{
		CEffectAnimationPropsItem *pAnimProps = (CEffectAnimationPropsItem *) it->GetPtr();
		SSpriteEffectDesc spriteEffect;
		
		string szName = "Effects\\sprites\\";
		szName += pAnimProps->GetItemName();
		spriteEffect.szPath = szName;
		spriteEffect.nStart = pAnimProps->GetBeginTime();
		spriteEffect.nRepeat = pAnimProps->GetRepeatCount();
		spriteEffect.vPos = pAnimProps->GetPosition();
		
		effDesc.sprites.push_back( spriteEffect );
	}
	
	for ( it=pFuncParticles->GetBegin(); it!=pFuncParticles->GetEnd(); ++it )
	{
		CEffectFuncPropsItem *pFuncProps = (CEffectFuncPropsItem *) it->GetPtr();
		string szName = "Effects\\particles\\";
		szName += pFuncProps->GetItemName();
		std::string szFullName = theApp.GetEditorDataDir();
		szFullName += szName;
		szFullName += ".xml";
		
		CPtr<IDataStream> pXMLStream = OpenFileStream( szFullName.c_str(), STREAM_ACCESS_READ );
		if ( !pXMLStream )
		{
			szFullName = "Error: Can not open stream: " + szFullName;
			AfxMessageBox( szFullName.c_str() );
			continue;
		}
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::READ );
		CTreeAccessor tree = pDT;
		
		SSourceType sourceType;
		tree.Add( "KeyData", &sourceType );
		
		if ( sourceType.bComplexParticleSource )
		{
			SSmokinParticleEffectDesc complexParticleEffect;
			complexParticleEffect.szPath = szName;
			complexParticleEffect.nStart = pFuncProps->GetBeginTime();
			complexParticleEffect.nDuration = pFuncProps->GetDuration();
			complexParticleEffect.vPos = pFuncProps->GetPosition();
			complexParticleEffect.fScale = pFuncProps->GetScaleFactor();
			effDesc.smokinParticles.push_back( complexParticleEffect );
		}
		else
		{
			SParticleEffectDesc particleEffect;
			particleEffect.szPath = szName;
			particleEffect.nStart = pFuncProps->GetBeginTime();
			particleEffect.nDuration = pFuncProps->GetDuration();
			particleEffect.vPos = pFuncProps->GetPosition();
			particleEffect.fScale = pFuncProps->GetScaleFactor();
			effDesc.particles.push_back( particleEffect );
		}
	}

	CTreeAccessor tree = pDT;
	tree.Add( "effect", &effDesc );
}

bool CEffectFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	return true;
}

void CEffectFrame::OnRunButton() 
{
	if ( bRunning )
		return;
	bRunning = !bRunning;

	g_frameManager.GetGameWnd()->ShowWindow( SW_SHOW );

	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	pSG->Clear();
	pVOB->Clear();

	string szDir = theApp.GetEditorTempDir();
	{
		CPtr<IDataStorage> pStorage = CreateStorage( szDir.c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
		CPtr<IDataStream> pXMLStream = pStorage->CreateStream( "test.xml", STREAM_ACCESS_WRITE );
		ASSERT( pXMLStream != 0 );
		if ( pXMLStream == 0 )
			return;
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE, "effect" );
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRoot = pTree->GetRootItem();
		SaveRPGStats( pDT, pRoot, szProjectFileName.c_str() );
	}

	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	szDir = theApp.GetEditorTempResourceDir();
	szDir += "\\test";
	IEffectVisObj *pEffect = static_cast<IEffectVisObj*>( pVOB->BuildObject( szDir.c_str(), 0, SGVOT_EFFECT ) );
	pEffect->SetPlacement( CVec3(0, 0, 0), 0 );
	pEffect->SetStartTime( GetSingleton<IGameTimer>()->GetGameTime() );
	pSG->AddObject( pEffect, SGVOGT_EFFECT );
	pRunningEffect = pEffect;
	UpdateEffectAngle();
}

void CEffectFrame::OnStopButton() 
{
	if ( !bRunning )
		return;

	bRunning = !bRunning;
	pRunningEffect = 0;

	g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );

	IScene *pSG = GetSingleton<IScene>();
	pSG->Clear();
}

void CEffectFrame::OnUpdateRunButton(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree == 0 )			//���� ������ �� ��� ������
	{
		pCmdUI->Enable( false );
		return;
	}

	if ( bRunning )
		pCmdUI->Enable( false );
	else
		pCmdUI->Enable( true );
}

void CEffectFrame::OnUpdateStopButton(CCmdUI* pCmdUI) 
{
	if ( bRunning )
	{
		if ( pRunningEffect->IsFinished(GetSingleton<IGameTimer>()->GetGameTime()) )  
		{
			OnStopButton();
			pCmdUI->Enable( false );
		}
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

BOOL CEffectFrame::SpecificTranslateMessage( MSG *pMsg )
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
		case WM_ANGLE_CHANGED:
			UpdateEffectAngle();
			return true;
	}

	return false;
}

int CEffectFrame::GetLastParticleEffectLifeTime()
{
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	pVOB->Clear();
	
	string szDir = theApp.GetEditorTempDir();
	{
		CPtr<IDataStorage> pStorage = CreateStorage( szDir.c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
		CPtr<IDataStream> pXMLStream = pStorage->CreateStream( "test.xml", STREAM_ACCESS_WRITE );
		NI_ASSERT( pXMLStream != 0 );
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE, "effect" );
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRoot = pTree->GetRootItem();
		SaveRPGStats( pDT, pRoot, szProjectFileName.c_str() );
	}
	
	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	szDir = theApp.GetEditorTempResourceDir();
	szDir += "\\test";

	CPtr<IEffectVisObj> pEffect = static_cast<IEffectVisObj*>( pVOB->BuildObject( szDir.c_str(), 0, SGVOT_EFFECT ) );
	IParticleSource **ppParticleSource = 0;
	int nNumEffects = 0;
	pEffect->GetParticleEffects( &ppParticleSource, &nNumEffects, true );
	NTimer::STime time = ppParticleSource[nNumEffects-1]->GetEffectLifeTime();
	return time;
}

void CEffectFrame::OnUpdateInterpolateTreeItem(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
}

void CEffectFrame::OnLButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();

	CParentFrame::OnLButtonDown(nFlags, point);
}

void CEffectFrame::OnRButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	
	CParentFrame::OnRButtonDown(nFlags, point);
}

FILETIME CEffectFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime;
	minTime = GetFileChangeTime( pszResultFileName );
	return minTime;
}

void CEffectFrame::OnButtonCamera() 
{
	bHorizontalCamera = !bHorizontalCamera;
	UpdateCamera();
}

void CEffectFrame::UpdateCamera()
{
	if ( bHorizontalCamera )
		SetHorizontalCamera();
	else
		SetDefaultCamera();
	if ( pGFX )
		GFXDraw();
}

void CEffectFrame::OnUpdateButtonCamera(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}
void CEffectFrame::OnShowDirectionButton()
{
	SwitchDockerVisible( pDirectionButtonDockBar );
}
void CEffectFrame::OnUpdateShowDirectionButton(CCmdUI* pCmdUI) 
{
	UpdateShowMenu( pCmdUI, pDirectionButtonDockBar );
}
