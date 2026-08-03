#include "StdAfx.h"
#include <io.h>
#include "../Common/LegacyUiCompat.h"

#include "../GFX/GFX.H"
#include "../Scene/Scene.h"
#include "../Scene/PFX.h"
#include "../Formats/fmtEffect.h"

#include "editor.h"
#include "PropView.h"
#include "TreeItem.h"
#include "ParticleTreeItem.h"
#include "ParticleView.h"
#include "ParticleFrm.h"
#include "GameWnd.h"
#include "frames.h"
#include "SetDirDialog.h"
#include "KeyFrameDock.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(CParticleFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CParticleFrame, CParentFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_RUN_BUTTON, OnRunButton)
	ON_COMMAND(ID_STOP_BUTTON, OnStopButton)
	ON_UPDATE_COMMAND_UI(ID_STOP_BUTTON, OnUpdateStopButton)
	ON_UPDATE_COMMAND_UI(ID_RUN_BUTTON, OnUpdateRunButton)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_TILEPOS, OnUpdateStatusBar)
	ON_COMMAND(ID_GET_PARTICLE_INFO, OnGetParticleInfo)
	ON_COMMAND(ID_BUTTON_CAMERA, OnButtonCamera)
	ON_COMMAND(ID_PARTICLE_SOURCE, OnSwitchParticleSourceType)
	ON_UPDATE_COMMAND_UI(ID_PARTICLE_SOURCE, OnUpdateParticleSource)
	ON_UPDATE_COMMAND_UI(ID_GET_PARTICLE_INFO, OnUpdateGetParticleInfo)
	ON_COMMAND(ID_EDIT_SETBACKGROUNDCOLOR, OnEditSetbackgroundcolor)
	ON_UPDATE_COMMAND_UI(ID_BUTTON_CAMERA, OnUpdateButtonCamera)
	ON_COMMAND(ID_SHOW_FUNCTION, OnShowFunctionFrame)
	ON_UPDATE_COMMAND_UI(ID_SHOW_FUNCTION, OnUpdateShowFunctionFrame)
END_MESSAGE_MAP()


CParticleFrame::CParticleFrame()
{
	szComposerName = "Particle Editor";
	szExtension = "*.pcp";
	szComposerSaveName = "Particle_Composer_Project";
	nTreeRootItemID = E_PARTICLE_ROOT_ITEM;
	nFrameType = CFrameManager::E_PARTICLE_FRAME;
	pWndView = new CParticleView;
	szAddDir = "effects\\particles\\";
	
	pKeyFrameDockBar = 0;
	m_fNumberOfParticles = 0;
	m_fMaxSize = 0;
	m_fAverageSize = 0;
	m_fAverageCount = 0;
	bHorizontalCamera = false;
	bRunning = false;
	bComplexSource = false;

	m_nCompressedFormat = GFXPF_DXT5;
	m_nLowFormat = GFXPF_ARGB4444;
}

CParticleFrame::~CParticleFrame()
{
}

int CParticleFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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

void CParticleFrame::SetKeyFrameDockBar( CKeyFrameDockWnd *pWnd )
{
	pKeyFrameDockBar = pWnd;
	pKeyFrameDockBar->ClearControl();

	pTreeDockBar->SetKeyFrameDockWnd( pWnd );
}


void CParticleFrame::GetParticleInfo()
{
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	pVOB->Clear();

	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	CreateEffectDescriptionFile();
	string szDir = theApp.GetEditorTempResourceDir();
	szDir += "\\temp";
	
	CPtr<IEffectVisObj> pEffect = static_cast<IEffectVisObj*>( pVOB->BuildObject( szDir.c_str(), 0, SGVOT_EFFECT ) );
	IParticleSource **ppParticleSource = 0;
	int nNumEffects = 0;
	pEffect->GetParticleEffects( &ppParticleSource, &nNumEffects, true );
	if ( nNumEffects > 0 )
	{
		IParticleSourceWithInfo *pParticleSourceWithInfo = dynamic_cast<IParticleSourceWithInfo*> ( ppParticleSource[nNumEffects-1] );
		SParticleSourceInfo sourceInfo;
		pParticleSourceWithInfo->GetInfo( sourceInfo );
		m_fNumberOfParticles = sourceInfo.fMaxCount;
		m_fAverageCount = sourceInfo.fAverageCount;
		m_fAverageSize = sourceInfo.fAverageSize;
		m_fMaxSize = sourceInfo.fMaxSize;
	}
}

void CParticleFrame::OnGetParticleInfo() 
{
	GetParticleInfo();
}

void CParticleFrame::UpdateCamera()
{
	if ( bHorizontalCamera )
		SetHorizontalCamera();
	else
		SetDefaultCamera();
	if ( pGFX )
		GFXDraw();
}

void CParticleFrame::OnButtonCamera() 
{
	bHorizontalCamera = !bHorizontalCamera;

/*
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	if ( bHorizontalCamera )
		pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR)->SetButtonStyle( 10, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR)->SetButtonStyle( 10, TBBS_CHECKBOX );
*/
	
	UpdateCamera();
}

void CParticleFrame::OnUpdateStatusBar(CCmdUI* pCmdUI) 
{
	CString text;

	int nControlIndex = theApp.GetMainFrame()->m_wndStatusBar.CommandToIndex( ID_INDICATOR_TILEPOS );
	text.Format( "Max particles %g", m_fNumberOfParticles );
	theApp.GetMainFrame()->m_wndStatusBar.SetPaneText( nControlIndex, text );
	
	nControlIndex = theApp.GetMainFrame()->m_wndStatusBar.CommandToIndex( ID_INDICATOR_COORDS );
	text.Format( "Size %g", m_fMaxSize );
	theApp.GetMainFrame()->m_wndStatusBar.SetPaneText( nControlIndex, text );
	
	nControlIndex = theApp.GetMainFrame()->m_wndStatusBar.CommandToIndex( ID_INDICATOR_CONTROL );
	text.Format( "Average size %g", m_fAverageSize );
	theApp.GetMainFrame()->m_wndStatusBar.SetPaneText( nControlIndex, text );
	
	nControlIndex = theApp.GetMainFrame()->m_wndStatusBar.CommandToIndex( ID_INDICATOR_OBJECTTYPE );
	text.Format( "Average count %g", m_fAverageCount );
	theApp.GetMainFrame()->m_wndStatusBar.SetPaneText( nControlIndex, text );
}

void CParticleFrame::ShowFrameWindows( int nCommand )
{
	if ( bRunning )
		OnStopButton();
	CParentFrame::ShowFrameWindows( nCommand );
	
	if ( pKeyFrameDockBar )
		theApp.ShowSECControlBar( pKeyFrameDockBar, nCommand );
	if ( nCommand == SW_SHOW )
	{
		g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );
		ICamera *pCamera = GetSingleton<ICamera>();
		pCamera->SetAnchor( CVec3(0, 0, 0) );
		UpdateCamera();
	}

	IParticleManager *pPM = GetSingleton<IParticleManager>();
	if ( nCommand == SW_SHOW )
		pPM->SetShareMode( SDSM_RELOAD );
	else
		pPM->SetShareMode( SDSM_SHARE );
}

BOOL CParticleFrame::Run()
{
	if ( !bRunning )
		return FALSE;

	GFXDraw();
	g_frameManager.GetGameWnd()->ValidateRect( 0 );
	return TRUE;
}

void CParticleFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();

	GetSingleton<IGameTimer>()->Update( timeGetTime() );

	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );

	
	ICamera*	pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
  pSG->Draw( pCamera );
	
	pGFX->EndScene();
	pGFX->Flip();
}

void CParticleFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
}

void CParticleFrame::SpecificClearBeforeBatchMode()
{
	pKeyFrameDockBar->ClearControl();
}

static void CopyFramesListToKeyTrack( CTrack &track, const CFramesList &framesList, float multy = 1.0f )
{
	int i = 0;
	for ( CFramesList::const_iterator it=framesList.begin(); it!=framesList.end(); ++it )
	{
		track.AddKey( it->first * 1000, it->second * multy );
		i++;
	}

	if ( i == 1 )
	{
		track.AddKey( 1000, framesList.front().second * multy );
	}
}

static void CopyFramesListToAngleTrack( CTrack &track, const CFramesList &framesList )
{
	int i = 0;
	float fValue = 0;
	for ( CFramesList::const_iterator it=framesList.begin(); it!=framesList.end(); ++it )
	{
		fValue = it->second/360.0f*2*PI;
		track.AddKey( it->first * 1000, fValue );
		i++;
	}

	if ( i == 1 )
	{
		track.AddKey( 1000, fValue );
	}
}

void CParticleFrame::FillRPGStats( SParticleSourceData &particleSetup, CTreeItem *pRoot )
{
	NI_ASSERT( pRoot->GetItemType() == E_PARTICLE_ROOT_ITEM );
	CParticleCommonPropsItem *pCommonPropsItem = static_cast<CParticleCommonPropsItem *> ( pRoot->GetChildItem( E_PARTICLE_COMMON_PROPS_ITEM ) );
	
	particleSetup.fGravity = pCommonPropsItem->GetGravity();
	particleSetup.nLifeTime = pCommonPropsItem->GetLifeTime();
	particleSetup.vDirection = pCommonPropsItem->GetDirectionVector();
	particleSetup.vWind = pCommonPropsItem->GetWindVector();
	particleSetup.fRadialWind = pCommonPropsItem->GetWindPower();
	particleSetup.nAreaType = pCommonPropsItem->GetAreaType();

	float fScale = 1.0f;

	{
		CParticleSourcePropItems *pSourceProps = static_cast<CParticleSourcePropItems *> ( pRoot->GetChildItem( E_PARTICLE_SOURCE_PROP_ITEMS ) );
		particleSetup.nTextureDX = pSourceProps->GetTextureXSize();
		particleSetup.nTextureDY = pSourceProps->GetTextureYSize();
		particleSetup.szTextureName = pSourceProps->GetTextureFileName();

		CParticleGenerateSpeedItem *pGenSpeedItem = static_cast<CParticleGenerateSpeedItem *> ( pSourceProps->GetChildItem( E_PARTICLE_GENERATE_SPEED_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackBeginSpeed, pGenSpeedItem->framesList, fScale );
		CParticleRandSpeedItem *pRandomSpeedItem = static_cast<CParticleRandSpeedItem *> ( pSourceProps->GetChildItem( E_PARTICLE_RAND_SPEED_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackBeginSpeedRandomizer, pRandomSpeedItem->framesList, fScale );
		CParticleGenerateAreaItem *pGenAreaItem = static_cast<CParticleGenerateAreaItem *> ( pSourceProps->GetChildItem( E_PARTICLE_GENERATE_AREA_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackGenerateArea, pGenAreaItem->framesList, fScale );
		CParticleGenerateAngleItem *pGenAngleItem = static_cast<CParticleGenerateAngleItem *> ( pSourceProps->GetChildItem( E_PARTICLE_GENERATE_ANGLE_ITEM ) );
		CopyFramesListToAngleTrack( particleSetup.trackBeginAngleRandomizer, pGenAngleItem->framesList );
		CParticleGenerateDensityItem *pGenDensityItem = static_cast<CParticleGenerateDensityItem *> ( pSourceProps->GetChildItem( E_PARTICLE_GENERATE_DENSITY_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackDensity, pGenDensityItem->framesList );
		
		CParticleGenerateLifeItem *pGenLifeItem = static_cast<CParticleGenerateLifeItem *> ( pSourceProps->GetChildItem( E_PARTICLE_GENERATE_LIFE_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackLife, pGenLifeItem->framesList );
		CParticleRandLifeItem *pRandomLifeItem = static_cast<CParticleRandLifeItem *> ( pSourceProps->GetChildItem( E_PARTICLE_RAND_LIFE_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackLifeRandomizer, pRandomLifeItem->framesList, fScale );
		CParticleGenerateSpinItem *pGenSpinItem = static_cast<CParticleGenerateSpinItem *> ( pSourceProps->GetChildItem( E_PARTICLE_GENERATE_SPIN_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackGenerateSpin, pGenSpinItem->framesList );
		CParticleGenerateRandomSpinItem *pGenRandomSpinItem = static_cast<CParticleGenerateRandomSpinItem *> ( pSourceProps->GetChildItem( E_PARTICLE_GENERATE_RANDOM_SPIN_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackGenerateSpinRandomizer, pGenRandomSpinItem->framesList );
		CParticleGenerateOpacityItem *pGenOpacityItem = static_cast<CParticleGenerateOpacityItem *> ( pSourceProps->GetChildItem( E_PARTICLE_GENERATE_OPACITY_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackGenerateOpacity, pGenOpacityItem->framesList );
	}

	CTreeItem *pParticlePropsItem = pRoot->GetChildItem( E_PARTICLE_PROP_ITEMS );
	{
		CParticleSpinItem *pSpinItem = static_cast<CParticleSpinItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_SPIN_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackSpin, pSpinItem->framesList );
		CParticleWeightItem *pWeightItem = static_cast<CParticleWeightItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_WEIGHT_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackWeight, pWeightItem->framesList, fScale );
		CParticleSpeedItem *pSpeedItem = static_cast<CParticleSpeedItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_SPEED_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackSpeed, pSpeedItem->framesList );
		CParticleCRandomSpeedItem *pRandomSpeedItem = static_cast<CParticleCRandomSpeedItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_C_RANDOM_SPEED_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackSpeedRnd, pRandomSpeedItem->framesList );
		CParticleSizeItem *pSizeItem = static_cast<CParticleSizeItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_SIZE_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackSize, pSizeItem->framesList, fScale );
		CParticleOpacityItem *pOpacityItem = static_cast<CParticleOpacityItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_OPACITY_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackOpacity, pOpacityItem->framesList );
		CParticleTextureFrameItem *pTextureFrameItem = static_cast<CParticleTextureFrameItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_TEXTURE_FRAME_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackTextureFrame, pTextureFrameItem->framesList );
	}

	particleSetup.InitIntegrals();
}

void CParticleFrame::FillRPGStats2( SSmokinParticleSourceData &particleSetup, CTreeItem *pRoot )
{
	NI_ASSERT( pRoot->GetItemType() == E_PARTICLE_ROOT_ITEM );
	CParticleCommonPropsItem *pCommonPropsItem = static_cast<CParticleCommonPropsItem *> ( pRoot->GetChildItem( E_PARTICLE_COMMON_PROPS_ITEM ) );
	
	particleSetup.fGravity = pCommonPropsItem->GetGravity();
	particleSetup.nLifeTime = pCommonPropsItem->GetLifeTime();
	particleSetup.vDirection = pCommonPropsItem->GetDirectionVector();
	particleSetup.vWind = pCommonPropsItem->GetWindVector();
	particleSetup.fRadialWind = pCommonPropsItem->GetWindPower();
	particleSetup.nAreaType = pCommonPropsItem->GetAreaType();
	
	float fScale = 1.0f;
	
	{
		CParticleComplexSourceItem *pComplexSourceProps = static_cast<CParticleComplexSourceItem *> ( pRoot->GetChildItem( E_PARTICLE_COMPLEX_SOURCE_ITEM ) );
		particleSetup.szParticleEffectName = pComplexSourceProps->GetParticleName();
		if ( particleSetup.szParticleEffectName.empty() )
		{
			AfxMessageBox( "Error: the complex particle source has no effect, using default (flame)" );
			particleSetup.szParticleEffectName = "Effects\\particles\\flame";
		}
		
		CParticleGenerateSpeedItem *pGenSpeedItem = static_cast<CParticleGenerateSpeedItem *> ( pComplexSourceProps->GetChildItem( E_PARTICLE_GENERATE_SPEED_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackBeginSpeed, pGenSpeedItem->framesList, fScale );
		CParticleRandSpeedItem *pRandomSpeedItem = static_cast<CParticleRandSpeedItem *> ( pComplexSourceProps->GetChildItem( E_PARTICLE_RAND_SPEED_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackBeginSpeedRandomizer, pRandomSpeedItem->framesList, fScale );
		CParticleGenerateAreaItem *pGenAreaItem = static_cast<CParticleGenerateAreaItem *> ( pComplexSourceProps->GetChildItem( E_PARTICLE_GENERATE_AREA_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackGenerateArea, pGenAreaItem->framesList, fScale );
		CParticleGenerateAngleItem *pGenAngleItem = static_cast<CParticleGenerateAngleItem *> ( pComplexSourceProps->GetChildItem( E_PARTICLE_GENERATE_ANGLE_ITEM ) );
		CopyFramesListToAngleTrack( particleSetup.trackBeginAngleRandomizer, pGenAngleItem->framesList );
		CParticleGenerateDensityItem *pGenDensityItem = static_cast<CParticleGenerateDensityItem *> ( pComplexSourceProps->GetChildItem( E_PARTICLE_GENERATE_DENSITY_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackDensity, pGenDensityItem->framesList );
	}
	
	{
		CTreeItem *pParticlePropsItem = pRoot->GetChildItem( E_PARTICLE_COMPLEX_ITEM );
		CParticleWeightItem *pWeightItem = static_cast<CParticleWeightItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_WEIGHT_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackWeight, pWeightItem->framesList, fScale );
		CParticleSpeedItem *pSpeedItem = static_cast<CParticleSpeedItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_SPEED_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackSpeed, pSpeedItem->framesList );
		CParticleCRandomSpeedItem *pRandomSpeedItem = static_cast<CParticleCRandomSpeedItem *> ( pParticlePropsItem->GetChildItem( E_PARTICLE_C_RANDOM_SPEED_ITEM ) );
		CopyFramesListToKeyTrack( particleSetup.trackSpeedRnd, pRandomSpeedItem->framesList );
	}

	particleSetup.InitIntegrals();
}

void CParticleFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	NI_ASSERT( !pDT->IsReading() );
	CTreeAccessor tree = pDT;

	
	if ( bComplexSource )
	{
		SSmokinParticleSourceData particleSetup;
		if ( bNewProjectJustCreated )
		{
			particleSetup.trackBeginAngleRandomizer.AddKey( 0, 0 );
			particleSetup.trackBeginSpeed.AddKey( 0, 0 );
			particleSetup.trackBeginSpeedRandomizer.AddKey( 0, 0 );
			particleSetup.trackDensity.AddKey( 0, 0 );
			particleSetup.trackGenerateArea.AddKey( 0, 0 );
			particleSetup.trackSpeed.AddKey( 0, 0 );
			particleSetup.trackSpeedRnd.AddKey( 0, 0 );
			particleSetup.trackWeight.AddKey( 0, 0 );
			GetRPGStats2( particleSetup, pRootItem );
		}
		else
			FillRPGStats2( particleSetup, pRootItem );
		tree.Add( "KeyData", &particleSetup );
	}
	else
	{
		SParticleSourceData particleSetup;
		if ( bNewProjectJustCreated )
		{
			particleSetup.trackBeginAngleRandomizer.AddKey( 0, 0 );
			particleSetup.trackBeginSpeed.AddKey( 0, 0 );
			particleSetup.trackBeginSpeedRandomizer.AddKey( 0, 0 );
			particleSetup.trackDensity.AddKey( 0, 0 );
			particleSetup.trackGenerateArea.AddKey( 0, 0 );
			particleSetup.trackGenerateOpacity.AddKey( 0, 0 );
			particleSetup.trackGenerateSpin.AddKey( 0, 0 );
			particleSetup.trackGenerateSpinRandomizer.AddKey( 0, 0 );
			particleSetup.trackLife.AddKey( 0, 0 );
			particleSetup.trackLifeRandomizer.AddKey( 0, 0 );
			particleSetup.trackOpacity.AddKey( 0, 0 );
			particleSetup.trackSize.AddKey( 0, 0 );
			particleSetup.trackSpeed.AddKey( 0 ,0 );
			particleSetup.trackSpeedRnd.AddKey( 0, 0 );
			particleSetup.trackSpin.AddKey( 0, 0 );
			particleSetup.trackTextureFrame.AddKey( 0, 0 );
			particleSetup.trackWeight.AddKey( 0, 0 );					
			GetRPGStats( particleSetup, pRootItem );
		}
		else
			FillRPGStats( particleSetup, pRootItem );
		tree.Add( "KeyData", &particleSetup );
	}
}

void CParticleFrame::GetRPGStats( const SParticleSourceData &particleSetup, CTreeItem *pRootItem )
{
	bComplexSource = false;

	{
		CParticleSourcePropItems *pProps = static_cast<CParticleSourcePropItems *> ( pRootItem->GetChildItem( E_PARTICLE_SOURCE_PROP_ITEMS ) );
		pProps->SetTextureFileName( particleSetup.szTextureName.c_str() );
	}
	
}

void CParticleFrame::GetRPGStats2( const SSmokinParticleSourceData &particleSetup, CTreeItem *pRootItem )
{
	bComplexSource = true;
	
	{
		CParticleComplexSourceItem *pProps = static_cast<CParticleComplexSourceItem *> ( pRootItem->GetChildItem( E_PARTICLE_COMPLEX_SOURCE_ITEM ) );
		pProps->SetParticleName( particleSetup.szParticleEffectName.c_str() );
	}
	
}

struct SSourceType
{
	bool bComplexParticleSource;			//��� ���������, ���� true, �� ������� particle source
	SSourceType() : bComplexParticleSource( false ) {}
	virtual int STDCALL operator&( IDataTree &ss );
};

int SSourceType::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "ComplexParticleSource", &bComplexParticleSource );
	return 0;
}

void CParticleFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	ASSERT( pDT->IsReading() );
	
	CTreeAccessor tree = pDT;
	SSourceType sourceType;
	tree.Add( "KeyData", &sourceType );

	if ( sourceType.bComplexParticleSource )
	{
		SSmokinParticleSourceData smokinSourceData;
		tree.Add( "KeyData", &smokinSourceData );
		GetRPGStats2( smokinSourceData, pRootItem );
	}
	else
	{
		SParticleSourceData sourceData;
		tree.Add( "KeyData", &sourceData );
		GetRPGStats( sourceData, pRootItem );
	}
	UpdateSourceTypeTB();
}

bool CParticleFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	SaveRPGStats( pDT, pRootItem, pszProjectName );
	return true;
}

void CParticleFrame::CreateEffectDescriptionFile()
{
	string szDir = theApp.GetEditorTempDir();
	{
		CPtr<IDataStorage> pStorage = CreateStorage( szDir.c_str(), STREAM_ACCESS_WRITE, STORAGE_TYPE_FILE );
		CPtr<IDataStream> pXMLStream = pStorage->CreateStream( "particle.xml", STREAM_ACCESS_WRITE );
		ASSERT( pXMLStream != 0 );
		if ( pXMLStream == 0 )
			return;
		CPtr<IDataTree> pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE );
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		if ( !pTree )
			return;
		CTreeItem *pRoot = pTree->GetRootItem();
		if ( !pRoot )
			return;
		CParticleCommonPropsItem *pCommonPropsItem = static_cast<CParticleCommonPropsItem *> ( pRoot->GetChildItem( E_PARTICLE_COMMON_PROPS_ITEM ) );
		SaveRPGStats( pDT, pRoot, szProjectFileName.c_str() );
		
		if ( bComplexSource )
		{
			SSmokinParticleEffectDesc smokinParticleEffectDesc;
			string szName = theApp.GetEditorTempResourceDir();
			szName += "\\particle";
			smokinParticleEffectDesc.szPath = szName;
			smokinParticleEffectDesc.nStart = 0;
			smokinParticleEffectDesc.nDuration = pCommonPropsItem->GetLifeTime() * 2;
			smokinParticleEffectDesc.vPos = pCommonPropsItem->GetPositionVector();
			smokinParticleEffectDesc.fScale = pCommonPropsItem->GetScaleFactor();
			
			SEffectDesc effDesc;
			effDesc.smokinParticles.push_back( smokinParticleEffectDesc );
			
			pXMLStream = pStorage->CreateStream( "temp.xml", STREAM_ACCESS_WRITE );
			pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE, "effect" );
			CTreeAccessor tree = pDT;
			tree.Add( "effect", &effDesc );
		}
		else
		{
			SParticleEffectDesc particleEffectDesc;
			string szName = theApp.GetEditorTempResourceDir();
			szName += "\\particle";
			particleEffectDesc.szPath = szName;
			particleEffectDesc.nStart = 0;
			particleEffectDesc.nDuration = pCommonPropsItem->GetLifeTime() * 2;
			particleEffectDesc.vPos = pCommonPropsItem->GetPositionVector();
			particleEffectDesc.fScale = pCommonPropsItem->GetScaleFactor();
			
			SEffectDesc effDesc;
			effDesc.particles.push_back( particleEffectDesc );
			
			pXMLStream = pStorage->CreateStream( "temp.xml", STREAM_ACCESS_WRITE );
			pDT = CreateDataTreeSaver( pXMLStream, IDataTree::WRITE, "effect" );
			CTreeAccessor tree = pDT;
			tree.Add( "effect", &effDesc );
		}
	}
}

void CParticleFrame::OnRunButton() 
{
	if ( bRunning )
		return;
	bRunning = !bRunning;
	
	g_frameManager.GetGameWnd()->ShowWindow( SW_SHOW );
	CreateEffectDescriptionFile();

	IScene *pSG = GetSingleton<IScene>();
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	pVOB->Clear();
	pSG->Clear();
	IParticleManager *pPM = GetSingleton<IParticleManager>();
	pPM->Clear();
	
	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	string szDir = theApp.GetEditorTempResourceDir();
	szDir += "\\temp";

	pEffect = static_cast<IEffectVisObj*>( pVOB->BuildObject( szDir.c_str(), 0, SGVOT_EFFECT ) );
	pEffect->SetPlacement( CVec3(0, 0, 0), 0 );
	pEffect->SetStartTime( GetSingleton<IGameTimer>()->GetGameTime() );
	pSG->AddObject( pEffect, SGVOGT_EFFECT );
}

void CParticleFrame::OnStopButton() 
{
	if ( !bRunning )
		return;

	bRunning = !bRunning;

	g_frameManager.GetGameWnd()->ShowWindow( SW_HIDE );

	IScene *pSG = GetSingleton<IScene>();
	pSG->Clear();
}

void CParticleFrame::OnUpdateRunButton(CCmdUI* pCmdUI) 
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

void CParticleFrame::OnUpdateStopButton(CCmdUI* pCmdUI) 
{
	if ( bRunning )
	{
		if ( pEffect->IsFinished(GetSingleton<IGameTimer>()->GetGameTime()) )  
		{
			OnStopButton();
			pCmdUI->Enable( false );
		}
		pCmdUI->Enable( true );
	}
	else
		pCmdUI->Enable( false );
}

BOOL CParticleFrame::SpecificTranslateMessage( MSG *pMsg )
{
	if ( bRunning )
	{
		if ( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE )
		{
			OnStopButton();
			return true;
		}
	}
	
	return false;
}

void CParticleFrame::OnSwitchParticleSourceType()
{
	bComplexSource = !bComplexSource;
	UpdateSourceType();
}

void CParticleFrame::UpdateSourceType( bool bOnlyDelete )
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( !pTree )
		return;
	CTreeItem *pRootItem = pTree->GetRootItem();
	/*if ( !bOnlyDelete )
	{
		pRootItem->RemoveAllChilds();
		pRootItem->CreateDefaultChilds();
		pRootItem->InsertChildItems();
	}*/

	if ( bComplexSource )
	{
		CTreeItem *pProps = pRootItem->GetChildItem( E_PARTICLE_SOURCE_PROP_ITEMS );
		pProps->ExpandTreeItem( false );//DeleteMeInParentTreeItem();
		pProps = pRootItem->GetChildItem( E_PARTICLE_PROP_ITEMS );
		pProps->ExpandTreeItem( false );//DeleteMeInParentTreeItem();
		pProps = pRootItem->GetChildItem( E_PARTICLE_COMPLEX_SOURCE_ITEM );
		pProps->ExpandTreeItem( true );//DeleteMeInParentTreeItem();
		pProps = pRootItem->GetChildItem( E_PARTICLE_COMPLEX_ITEM );
		pProps->ExpandTreeItem( true );//DeleteMeInParentTreeItem();
	}
	else
	{
		CTreeItem *pProps = pRootItem->GetChildItem( E_PARTICLE_COMPLEX_SOURCE_ITEM );
		pProps->ExpandTreeItem( false );//DeleteMeInParentTreeItem();
		pProps = pRootItem->GetChildItem( E_PARTICLE_COMPLEX_ITEM );
		pProps->ExpandTreeItem( false );//DeleteMeInParentTreeItem();
		pProps = pRootItem->GetChildItem( E_PARTICLE_SOURCE_PROP_ITEMS );
		pProps->ExpandTreeItem( true );//DeleteMeInParentTreeItem();
		pProps = pRootItem->GetChildItem( E_PARTICLE_PROP_ITEMS );
		pProps->ExpandTreeItem( true );//DeleteMeInParentTreeItem();
	}
}

void CParticleFrame::UpdateSourceTypeTB()
{
	SECToolBarManager* pToolBarMgr = theApp.GetMainFrame()->GetControlBarManager();
	SECCustomToolBar *pToolBar = pToolBarMgr->ToolBarFromID(AFX_IDW_TOOLBAR+12);
	int nIndex = pToolBar->CommandToIndex( ID_PARTICLE_SOURCE );
	if ( bComplexSource )
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX | TBBS_CHECKED );
	else
		pToolBar->SetButtonStyle( nIndex, TBBS_CHECKBOX );
}

void CParticleFrame::OnUpdateParticleSource(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}

void CParticleFrame::OnUpdateGetParticleInfo(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}

void CParticleFrame::OnUpdateButtonCamera(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}
void CParticleFrame::OnShowFunctionFrame()
{
	SwitchDockerVisible( pKeyFrameDockBar );
}
void CParticleFrame::OnUpdateShowFunctionFrame(CCmdUI* pCmdUI) 
{
	UpdateShowMenu( pCmdUI, pKeyFrameDockBar );
}
