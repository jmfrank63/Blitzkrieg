#include "StdAfx.h"
#include <io.h>
#include "../Common/LegacyUiCompat.h"

#include "../GFX/GFX.h"
#include "../GFX/GFXHelper.h"
#include "../Scene/Scene.h"
#include "../Anim/Animation.h"
#include "../Main/rpgstats.h"
#include "../Formats/fmtMesh.h"
#include "../Image/Image.h"

#include "editor.h"
#include "DirectionButtonDock.h"
#include "TreeDockWnd.h"
#include "PropView.h"
#include "MeshView.h"
#include "TreeItem.h"
#include "AnimTreeItem.h"
#include "MeshTreeItem.h"
#include "WeaponTreeItem.h"
#include "MeshFrm.h"
#include "GameWnd.h"
#include "frames.h"
#include "RefDlg.h"
#include "localization.h"
#include "SpriteCompose.h"

static const int MIN_OPACITY = 120;
static const int MAX_OPACITY = 255;


IMPLEMENT_DYNCREATE(CMeshFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CMeshFrame, CParentFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_SHOW_LOCATORS_INFO, OnShowLocatorsInfo)
	ON_UPDATE_COMMAND_UI(ID_SHOW_LOCATORS_INFO, OnUpdateShowLocatorsInfo)
	ON_UPDATE_COMMAND_UI(ID_IMPORT_ACK_FILE, OnUpdateImportAckFile)
	ON_UPDATE_COMMAND_UI(ID_EXPORT_ACK_FILE, OnUpdateExportAckFile)
	ON_COMMAND(ID_SHOW_DIRECTION_BUTTON, OnShowDirectionButton)
	ON_UPDATE_COMMAND_UI(ID_SHOW_DIRECTION_BUTTON, OnUpdateShowDirectionButton)
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()


CMeshFrame::CMeshFrame()
{
	szComposerName = "Unit Editor";
	szExtension = "*.msh";
	szComposerSaveName = "Mesh_Composer_Project";
	nTreeRootItemID = E_MESH_ROOT_ITEM;
	nFrameType = CFrameManager::E_MESH_FRAME;
	pWndView = new CMeshView;
	szAddDir = "units\\technics\\";

	pActiveLocator = 0;
	pDirectionButtonDockBar = 0;
	bShowLocators = false;
}

CMeshFrame::~CMeshFrame()
{
}

int CMeshFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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

void CMeshFrame::ShowFrameWindows( int nCommand )
{
	CParentFrame::ShowFrameWindows( nCommand );
	g_frameManager.GetGameWnd()->ShowWindow( SW_SHOW );
	if ( pDirectionButtonDockBar )
		theApp.ShowSECControlBar( pDirectionButtonDockBar, nCommand );

	ICamera *pCamera = GetSingleton<ICamera>();
	pCamera->SetAnchor( CVec3(16*fWorldCellSize, 16*fWorldCellSize, 0) );

	IScene *pSG = GetSingleton<IScene>();
	if ( nCommand == SW_SHOW )
	{
		if ( pCombatObject )
			pSG->AddObject( pCombatObject, SGVOGT_UNIT );

		if ( bShowLocators )
			while ( !pSG->ToggleShow( SCENE_SHOW_BBS ) )
				;
		else
			while ( pSG->ToggleShow( SCENE_SHOW_BBS ) )
				;
	}
	else
	{
		while ( pSG->ToggleShow( SCENE_SHOW_BBS ) )
			;
	}
}

BOOL CMeshFrame::SpecificTranslateMessage( MSG *pMsg )
{
	switch ( pMsg->message )
	{
		case WM_ANGLE_CHANGED:
			if ( !pCombatObject )
				return true;
			UpdateLocators();
			
/*
			float fPrev = pActiveFormation->fFormationDir;
			pActiveFormation->fFormationDir = pDirectionButtonDockBar->GetAngle();
			CalculateNewPositions( pActiveFormation->fFormationDir - fPrev );
			UpdateFormationDirection();
*/
			GFXDraw();
			return true;
	}
	
	return false;
}

/*
void CMeshFrame::UpdateItemType()
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	NI_ASSERT( pRootItem != 0 );
	
	CMeshCommonPropsItem *pCommonProps = static_cast<CMeshCommonPropsItem *> ( pRootItem->GetChildItem( E_MESH_COMMON_PROPS_ITEM ) );
	NI_ASSERT( pCommonProps != 0 );
	CTreeItem *pAviaItem = pCommonProps->GetChildItem( E_MESH_AVIA_ITEM );
	if ( IsAviation( pCommonProps->GetMeshType() ) )
	{
		if ( !pAviaItem )
		{
			pAviaItem = new CMeshAviaItem;
			pAviaItem->SetItemName( "Aviation property" );
			pCommonProps->AddChild( pAviaItem );
			pCommonProps->ExpandTreeItem( true );
		}
	}
	else
	{
		if ( pAviaItem )
			pCommonProps->RemoveChild( pAviaItem );
	}
}
*/

void CMeshFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, m_backgroundColor );
	pGFX->BeginScene();
	
	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );
	
	ICamera *pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
	pCamera->Update();
	if ( bShowLocators && pActiveLocator )
	{
		pSG->AddMeshPair2( pActiveLocator->lineVertices,
			2,
			sizeof( SGFXLineVertex ),
			SGFXLineVertex::format,
			0,
			0,
			GFXPT_LINELIST,
			0,
			3,
			true );
	}

	pSG->Draw( pCamera );
	pGFX->EndScene();
	pGFX->Flip();
}

void CMeshFrame::SelectLocator( CMeshLocatorPropsItem *pLoc )
{
	NI_ASSERT( pLoc != 0 );
	
	if ( pActiveLocator == pLoc )
		return;
	
	if ( !bShowLocators )
	{
		pActiveLocator = pLoc;
		return;
	}
	
	if ( pActiveLocator )
	{
		pActiveLocator->pSprite->SetOpacity( MIN_OPACITY );
	}
	
	pActiveLocator = pLoc;
	pActiveLocator->pSprite->SetOpacity( MAX_OPACITY );
	UpdateActiveLocatorLine();
	GFXDraw();
}

void CMeshFrame::UpdateActiveLocatorLine()
{
	if ( !pActiveLocator )
		return;
	
	NI_ASSERT( pModelMatrix != 0 );
	const SHMatrix &matrix = pModelMatrix[ pActiveLocator->nLocatorID ];
	
	CVec3 vZ( 0, 0, 1.0f);
	CVec3 v;
	matrix.RotateVector( &v, vZ );
	v *= 100;

	CVec3 vPos3 = pActiveLocator->pSprite->GetPosition();
	pActiveLocator->lineVertices[0].Setup( vPos3, 0xff0000ff );
	
	vPos3 += v;
	pActiveLocator->lineVertices[1].Setup( vPos3, 0xff0000ff );
}

void CMeshFrame::SpecificInit()
{
	SpecificClearBeforeBatchMode();
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRoot = pTree->GetRootItem();
	CMeshGraphicsItem *pGraphicsItem = (CMeshGraphicsItem *) pRoot->GetChildItem( E_MESH_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	SetCombatMesh( pGraphicsItem->GetCombatMeshName(), szProjectFileName.c_str(), pRoot );
	SetInstallMesh( pGraphicsItem->GetInstallMeshName(), szProjectFileName.c_str(), pRoot );
	SetTransportableMesh( pGraphicsItem->GetTransMeshName(), szProjectFileName.c_str(), pRoot );
	UpdateLocatorVisibility();
	
	CMeshPlatformsItem *pPlatforms = static_cast<CMeshPlatformsItem *> ( pRoot->GetChildItem( E_MESH_PLATFORMS_ITEM ) );
	NI_ASSERT( pPlatforms != 0 );
	if ( pPlatforms->GetChildsCount() == 0 )
	{
		CTreeItem *pItem = new CMeshPlatformPropsItem;
		pItem->SetItemName( "Base" );
		pPlatforms->AddChild( pItem );
	}
}

void CMeshFrame::SpecificClearBeforeBatchMode()
{
	pActiveLocator = 0;
	pDirectionButtonDockBar->SetAngle( 0 );
	pModelMatrix = 0;

	IScene *pSG = GetSingleton<IScene>();
	if ( pCombatObject )
	{
		pSG->RemoveObject( pCombatObject );
		pCombatObject = 0;
	}
	if ( pInstallObject )
	{
		pSG->RemoveObject( pInstallObject );
		pInstallObject = 0;
	}
	if ( pTransObject )
	{
		pSG->RemoveObject( pTransObject );
		pTransObject = 0;
	}
	pSG->Clear();
}

bool operator< ( const SAnimationFormat &a, const SAnimationFormat &b )
{
	return a.nType < b.nType;
}

struct SMyGunner
{
	int nIndex;
	string szName;

	bool operator < ( const SMyGunner &a ) { return szName < a.szName; }
	bool operator < ( const SMyGunner &a ) const { return szName < a.szName; }
};

void CMeshFrame::FillRPGStats( SMechUnitRPGStats &rpgStats, CTreeItem *pRootItem, const char *pszProjectName )
{
	SSkeletonFormat skeleton;
	SAABBFormat aabb;											// axis-aligned bounding box
	std::vector<SAABBFormat> aabb_as;
	std::vector<SAABBFormat> aabb_ds;
	rpgStats.animdescs.resize( ANIMATION_LAST_ANIMATION );

	CMeshGraphicsItem *pGraphicsItem = (CMeshGraphicsItem *) pRootItem->GetChildItem( E_MESH_GRAPHICS_ITEM );
	CTreeItem *pDeathCratersItem = pGraphicsItem->GetChildItem( E_MESH_DEATH_CRATERS_ITEM );
	NI_ASSERT( pDeathCratersItem != 0 );
	for ( CTreeItem::CTreeItemList::const_iterator it=pDeathCratersItem->GetBegin(); it!=pDeathCratersItem->GetEnd(); ++it )
	{
		CMeshDeathCraterPropsItem *pProps = static_cast<CMeshDeathCraterPropsItem *>( it->GetPtr() );
		rpgStats.deathCraters.push_back( pProps->GetCraterFileName() );
	}
	
	if ( pszProjectName )
	{
		string szRelName, szFullName, szDir;
		bool bRes = true;
		szRelName = pGraphicsItem->GetCombatMeshName();
		if ( IsRelatedPath( szRelName.c_str() ) )
		{
			szDir = GetDirectory( pszProjectName );
			MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
		}
		else
			szFullName = szRelName;
		std::vector<SAnimationFormat> animations;
		
		CPtr<IDataStream> pStream = OpenFileStream( szFullName.c_str(), STREAM_ACCESS_READ );
		NI_ASSERT( pStream != 0 );
		if ( pStream == 0 )
		{
			AfxMessageBox( "Error: Can not load combat mechanics file, aborting" );
			return;
		}
		
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
		CSaverAccessor saver = pSaver;
		
		saver.Add( 1, &skeleton );
		saver.Add( 3, &animations );
		saver.Add( 4, &aabb );
		saver.Add( 5, &aabb_as );
		saver.Add( 6, &aabb_ds );

		{
			for ( int i=0; i<animations.size(); i++ )
			{
				SUnitBaseRPGStats::SAnimDesc desc;
				desc.nIndex = i;
				desc.nLength = animations[i].GetLength();
				desc.nAction = animations[i].GetActionTime();
				desc.nAABB_A = animations[i].nAABB_AIndex;
				desc.nAABB_D = animations[i].nAABB_DIndex;
				rpgStats.animdescs[animations[i].nType].push_back( desc );
			}
		}
		
		if ( pInstallObject )
		{
			szRelName = pGraphicsItem->GetInstallMeshName();
			if ( IsRelatedPath( szRelName.c_str() ) )
			{
				szDir = GetDirectory( pszProjectName );
				MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
			}
			else
				szFullName = szRelName;
			
			pStream = OpenFileStream( szFullName.c_str(), STREAM_ACCESS_READ );
			if ( pStream )
			{
				pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
				saver = pSaver;
				
				saver.Add( 3, &animations );
				if ( aabb_as.size() == 0 )
					saver.Add( 5, &aabb_as );
				if ( aabb_ds.size() == 0 )
					saver.Add( 6, &aabb_ds );
				
				{
					for ( int i=0; i<animations.size(); i++ )
					{
						SUnitBaseRPGStats::SAnimDesc desc;
						desc.nIndex = i;
						desc.nLength = animations[i].GetLength();
						desc.nAction = animations[i].GetActionTime();
						desc.nAABB_A = animations[i].nAABB_AIndex;
						desc.nAABB_D = animations[i].nAABB_DIndex;
						rpgStats.animdescs[animations[i].nType].push_back( desc );
					}
				}
			}
		}
		
		if ( pTransObject )
		{
			szRelName = pGraphicsItem->GetTransMeshName();
			if ( IsRelatedPath( szRelName.c_str() ) )
			{
				szDir = GetDirectory( pszProjectName );
				MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
			}
			else
				szFullName = szRelName;
			
			pStream = OpenFileStream( szFullName.c_str(), STREAM_ACCESS_READ );
			if ( pStream != 0 )
			{
				pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
				saver = pSaver;
				saver.Add( 3, &animations );
				if ( aabb_as.size() == 0 )
					saver.Add( 5, &aabb_as );
				if ( aabb_ds.size() == 0 )
					saver.Add( 6, &aabb_ds );
				
				{
					for ( int i=0; i<animations.size(); i++ )
					{
						SUnitBaseRPGStats::SAnimDesc desc;
						desc.nIndex = i;
						desc.nLength = animations[i].GetLength();
						desc.nAction = animations[i].GetActionTime();
						desc.nAABB_A = animations[i].nAABB_AIndex;
						desc.nAABB_D = animations[i].nAABB_DIndex;
						rpgStats.animdescs[animations[i].nType].push_back( desc );
					}
				}
			}
		}
	}

	{
		rpgStats.vAABBCenter.x = aabb.vCenter.x;
		rpgStats.vAABBCenter.y = aabb.vCenter.y;

		rpgStats.vAABBHalfSize.x = aabb.vHalfSize.x;
		rpgStats.vAABBHalfSize.y = aabb.vHalfSize.y;

		for ( int i=0; i<aabb_as.size(); i++ )
		{
			SUnitBaseRPGStats::SAABBDesc desc;
			desc.vCenter.x = aabb_as[i].vCenter.x;
			desc.vCenter.y = aabb_as[i].vCenter.y;
			desc.vHalfSize.x = aabb_as[i].vHalfSize.x;
			desc.vHalfSize.y = aabb_as[i].vHalfSize.y;
			rpgStats.aabb_as.push_back( desc );
		}

		for ( int i=0; i<aabb_ds.size(); i++ )
		{
			SUnitBaseRPGStats::SAABBDesc desc;
			desc.vCenter.x = aabb_ds[i].vCenter.x;
			desc.vCenter.y = aabb_ds[i].vCenter.y;
			desc.vHalfSize.x = aabb_ds[i].vHalfSize.x;
			desc.vHalfSize.y = aabb_ds[i].vHalfSize.y;
			rpgStats.aabb_ds.push_back( desc );
		}
	}

	pModelMatrix = 0;
	if ( pCombatObject )
	{
		CVec3 vOldPosition = pCombatObject->GetPosition();
		int nOldDirection = pCombatObject->GetDirection();
		pCombatObject->SetPosition( CVec3(0, 0, 0) );
		pCombatObject->SetDirection( 0 );
		
		IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pCombatObject->GetAnimation() );
		pModelMatrix = pMeshAnim->GetMatrices( MONE );

		pCombatObject->SetPosition( vOldPosition );
		pCombatObject->SetDirection( nOldDirection );
	}

	CMeshCommonPropsItem *pCommonProps = static_cast<CMeshCommonPropsItem *> ( pRootItem->GetChildItem( E_MESH_COMMON_PROPS_ITEM ) );
	rpgStats.szKeyName = pCommonProps->GetMeshName();
	
	rpgStats.type = pCommonProps->GetMeshType();
	rpgStats.aiClass = (EAIClass) pCommonProps->GetAIClass();
	
	rpgStats.fMaxHP = pCommonProps->GetHealth();
	rpgStats.fRepairCost = pCommonProps->GetRepairCost();
	rpgStats.fSight = pCommonProps->GetSight();
	rpgStats.fCamouflage = pCommonProps->GetCamouflage();
	rpgStats.fSpeed = pCommonProps->GetSpeed();
	rpgStats.fPassability = pCommonProps->GetPassability();
	rpgStats.fTowingForce = pCommonProps->GetPullingPower();
	rpgStats.fUninstallRotate = pCommonProps->GetUninstallRotateTime();
	rpgStats.fUninstallTransport = pCommonProps->GetUninstallTransportTime();
	rpgStats.fWeight = pCommonProps->GetWeight();
	
	rpgStats.nCrew = pCommonProps->GetCrew();
	rpgStats.nPassangers = pCommonProps->GetPassangers();
	rpgStats.nPriority = pCommonProps->GetPriority();
	rpgStats.fRotateSpeed = pCommonProps->GetRotateSpeed();
	rpgStats.nBoundTileRadius = pCommonProps->GetBoundTileRadius();
	rpgStats.fTurnRadius = pCommonProps->GetTurnRadius();
	rpgStats.fSmallAABBCoeff = pCommonProps->GetSilhouette();
	rpgStats.fPrice = pCommonProps->GetAIPrice();
	rpgStats.fSightPower = pCommonProps->GetSightPower();
	
/*
	for ( int i=0; i<rpgStats.acknowledgements.size(); i++ )
	{
		rpgStats.acknowledgements[i].clear();
	}
*/

	CUnitAcksItem *pAcks = static_cast<CUnitAcksItem *> ( pRootItem->GetChildItem( E_UNIT_ACKS_ITEM ) );
	NI_ASSERT( pAcks != 0 );
	rpgStats.szAcksNames.resize( 1 );
	rpgStats.szAcksNames[0] = pAcks->GetAckName();

/*
	for ( CTreeItem::CTreeItemList::const_iterator ext=pAcks->GetBegin(); ext!=pAcks->GetEnd(); ++ext )
	{
		CUnitAckTypesItem *pAckType = static_cast<CUnitAckTypesItem *> ( ext->GetPtr() );
		for ( CTreeItem::CTreeItemList::const_iterator it=pAckType->GetBegin(); it!=pAckType->GetEnd(); ++it )
		{
			CUnitAckTypePropsItem *pProps = static_cast<CUnitAckTypePropsItem *> ( it->GetPtr() );
			std::pair<float,std::string> p;
			p.first = pProps->GetProbability();
			p.second = pProps->GetSoundName();
			
			rpgStats.acknowledgements[pAckType->GetAckType()].push_back( p );
		}
	}
*/
	
	CMeshEffectsItem *pEffects = static_cast<CMeshEffectsItem *> ( pRootItem->GetChildItem( E_MESH_EFFECTS_ITEM ) );
	NI_ASSERT( pEffects != 0 );
	rpgStats.szEffectDiesel = pEffects->GetEffectDieselName();
	rpgStats.szEffectSmoke = pEffects->GetEffectSmokeName();
	rpgStats.szEffectWheelDust = pEffects->GetEffectWheelDustName();
	rpgStats.szEffectShootDust = pEffects->GetEffectShootDustName();
	rpgStats.szEffectFatality = pEffects->GetEffectFatalityName();
	rpgStats.szEffectDisappear = pEffects->GetEffectDisappearName();
	
	rpgStats.szSoundMoveStart = pEffects->GetSoundStart();
	rpgStats.szSoundMoveStop = pEffects->GetSoundStop();
	rpgStats.szSoundMoveCycle = pEffects->GetSoundCycle();

	CMeshAviaItem *pAviaProps = static_cast<CMeshAviaItem *>( pCommonProps->GetChildItem( E_MESH_AVIA_ITEM ) );
	if ( pAviaProps != 0 )
	{
		rpgStats.fMaxHeight = pAviaProps->GetMaxHeight();
		rpgStats.fDivingAngle = pAviaProps->GetDivingAngle();
		rpgStats.fClimbAngle = pAviaProps->GetClimbAngle();
		rpgStats.fTiltAngle = pAviaProps->GetTiltAngle();
		rpgStats.fTiltRatio = pAviaProps->GetTiltRatio();
	}
	
	CMeshTrackItem *pTrackProps = static_cast<CMeshTrackItem *>( pCommonProps->GetChildItem( E_MESH_TRACK_ITEM ) );
	if ( pTrackProps != 0 )
	{
		rpgStats.bLeavesTracks = pTrackProps->GetLeaveTrackFlag();
		rpgStats.fTrackWidth = pTrackProps->GetTrackWidth();
		rpgStats.fTrackOffset = pTrackProps->GetTrackOffset();
		rpgStats.fTrackStart = pTrackProps->GetTrackStart();
		rpgStats.fTrackEnd = pTrackProps->GetTrackEnd();
		rpgStats.fTrackIntensity = pTrackProps->GetTrackIntensity();
		rpgStats.nTrackLifetime = pTrackProps->GetTrackLifeTime();
	}

	CUnitActionsItem *pActions = static_cast<CUnitActionsItem*>( pRootItem->GetChildItem( E_UNIT_ACTIONS_ITEM ) );
	pActions->GetActions( &rpgStats );

	CUnitExposuresItem *pExposures = static_cast<CUnitExposuresItem*>( pRootItem->GetChildItem( E_UNIT_EXPOSURES_ITEM ) );
	pExposures->GetExposures( &rpgStats );
	
	CTreeItem *pDefencesItem = pRootItem->GetChildItem( E_MESH_DEFENCES_ITEM );
	for ( int i=0; i<6; i++ )
	{
		CMeshDefencePropsItem *pDefProps = static_cast<CMeshDefencePropsItem *> ( pDefencesItem->GetChildItem( E_MESH_DEFENCE_PROPS_ITEM, i ) );
		int nIndex = 0;
		if ( string( "Left" ) == pDefProps->GetItemName() )
			nIndex = RPG_LEFT;
		else if ( string( "Right" ) == pDefProps->GetItemName() )
			nIndex = RPG_RIGHT;
		else if ( string( "Top" ) == pDefProps->GetItemName() )
			nIndex = RPG_TOP;
		else if ( string( "Bottom" ) == pDefProps->GetItemName() )
			nIndex = RPG_BOTTOM;
		else if ( string( "Front" ) == pDefProps->GetItemName() )
			nIndex = RPG_FRONT;
		else if ( string( "Back" ) == pDefProps->GetItemName() )
			nIndex = RPG_BACK;
		
		rpgStats.armors[ nIndex ].fMin = pDefProps->GetMinArmor();
		rpgStats.armors[ nIndex ].fMax = pDefProps->GetMaxArmor();
	}
	
	CTreeItem *pJoggingsItem = pRootItem->GetChildItem( E_MESH_JOGGINGS_ITEM );
	for ( int i=0; i<3; i++ )
	{
		CMeshJoggingPropsItem *pJogProps = static_cast<CMeshJoggingPropsItem *> ( pJoggingsItem->GetChildItem( E_MESH_JOGGING_PROPS_ITEM, i ) );
		SMechUnitRPGStats::SJoggingParams *pJog = 0;
		if ( i == 0 )
			pJog = &rpgStats.jx;
		else if ( i == 1 )
			pJog = &rpgStats.jy;
		else if ( i == 2 )
			pJog = &rpgStats.jz;
		
		pJog->fPeriod1 = pJogProps->GetPeriod1();
		pJog->fPeriod2 = pJogProps->GetPeriod2();
		pJog->fAmp1 = pJogProps->GetAmplitude1();
		pJog->fAmp2 = pJogProps->GetAmplitude2();
		pJog->fPhase1 = pJogProps->GetPhase1();
		pJog->fPhase2 = pJogProps->GetPhase2();
	}
	
/*
	CMesh3DPointsItem *p3DPointsItem = (CMesh3DPointsItem *) pRootItem->GetChildItem( E_MESH_3DPOINTS_ITEM );
	ASSERT( p3DPointsItem != 0 );
	
	for ( CTreeItem::CTreeItemList::const_iterator it=p3DPointsItem->GetBegin(); it!=p3DPointsItem->GetEnd(); ++it )
	{
		CMesh3DPointPropsItem *pPointProps = (CMesh3DPointPropsItem *) it->GetPtr();
		if ( pPointProps->GetMeshPointType() == CMesh3DPointsItem::E_DAMAGE_POINT )
			rpgStats.damagePoints.push_back( pPointProps->GetMeshPointIndex() );
		else if ( pPointProps->GetMeshPointType() == CMesh3DPointsItem::E_EXHAUST_POINT )
			rpgStats.exhaustPoints.push_back( pPointProps->GetMeshPointIndex() );
		else if ( pPointProps->GetMeshPointType() == CMesh3DPointsItem::E_TOW_POINT )
			rpgStats.nTowPoint = pPointProps->GetMeshPointIndex();
		else if ( pPointProps->GetMeshPointType() == CMesh3DPointsItem::E_ENTRANCE_POINT )
			rpgStats.nEntrancePoint = pPointProps->GetMeshPointIndex();
	}
*/

	int nMyAmmoIndex = -1;
	int nNumNodes = 0;
	std::vector<const char*> allNamesVector( nNumNodes );
	if ( pInstallObject )
	{
		IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pInstallObject->GetAnimation() );
		IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );
		nNumNodes = pMeshAnim->GetNumNodes();
		if ( nNumNodes != 0 )
		{
			allNamesVector.resize( nNumNodes );
			NI_ASSERT( pMeshAnimEdit != 0 );
			pMeshAnimEdit->GetAllNodeNames( &(allNamesVector[0]), nNumNodes );
		}
	}
	{
		for ( int i=0; i<nNumNodes; i++ )
		{
			const char *pszTemp = "LExhaust";
			if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
			{
				rpgStats.exhaustPoints.push_back( i );
				continue;
			}
		}
	}
	nNumNodes = 0;
	allNamesVector.resize( 0 );
	if ( pTransObject )
	{
		IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pTransObject->GetAnimation() );
		IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );
		nNumNodes = pMeshAnim->GetNumNodes();
		if ( nNumNodes != 0 )
		{
			allNamesVector.resize( nNumNodes );
			NI_ASSERT( pMeshAnimEdit != 0 );
			pMeshAnimEdit->GetAllNodeNames( &(allNamesVector[0]), nNumNodes );
		}
	}
	{
		for ( int i=0; i<nNumNodes; i++ )
		{
			const char *pszTemp = "LExhaust";
			if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
			{
				rpgStats.exhaustPoints.push_back( i );
				continue;
			}
		}
	}
	nNumNodes = 0;
	allNamesVector.resize( 0 );
	if ( pCombatObject )
	{
		IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pCombatObject->GetAnimation() );
		IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );
		nNumNodes = pMeshAnim->GetNumNodes();
		if ( nNumNodes != 0 )
		{
			allNamesVector.resize( nNumNodes );
			NI_ASSERT( pMeshAnimEdit != 0 );
			pMeshAnimEdit->GetAllNodeNames( &(allNamesVector[0]), nNumNodes );
		}
	}

	{
		for ( int i=0; i<nNumNodes; i++ )
		{
			const char *pszTemp = 0;
/*
			pszTemp = "LExplosion";
			if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
			{
				rpgStats.damagePoints.push_back( i );
			}
*/

			pszTemp = "LExhaust";
			if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
			{
				rpgStats.exhaustPoints.push_back( i );
				continue;
			}

			pszTemp = "LPeople";
			if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
			{
				rpgStats.nEntrancePoint = i;
				continue;
			}

			pszTemp = "LGunner";
			if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
			{
				rpgStats.peoplePointIndices.push_back( i );
			}

			pszTemp = "LAmmo";
			if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
			{
				nMyAmmoIndex = i;
				continue;
			}

			pszTemp = "LShootDust";
			if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
			{
				rpgStats.nShootDustPoint = i;
				continue;
			}
		}
	}

	rpgStats.guns.clear();					//������� �����
	CMeshPlatformsItem *pPlatforms = static_cast<CMeshPlatformsItem *> ( pRootItem->GetChildItem( E_MESH_PLATFORMS_ITEM ) );
	rpgStats.platforms.resize( pPlatforms->GetChildsCount() );
	int nPlatformIndex = 0;
	
	for ( CTreeItem::CTreeItemList::const_iterator it=pPlatforms->GetBegin(); it!=pPlatforms->GetEnd(); ++it )
	{
		CMeshPlatformPropsItem *pPlatformProps = static_cast<CMeshPlatformPropsItem *> ( it->GetPtr() );
		rpgStats.platforms[nPlatformIndex].fVerticalRotationSpeed = pPlatformProps->GetVerticalRotationSpeed();
		rpgStats.platforms[nPlatformIndex].fHorizontalRotationSpeed = pPlatformProps->GetHorizontalRotationSpeed();
		if ( nNumNodes != 0 )
		{
			string szPartName = pPlatformProps->GetPlatformPartName();
			int i = 0;
			for ( ; i<nNumNodes; i++ )
			{
				if ( allNamesVector[i] == szPartName )
					break;
			}

			if ( i == nNumNodes )
			{
				rpgStats.platforms[nPlatformIndex].constraint.fMin = 0;
				rpgStats.platforms[nPlatformIndex].constraint.fMax = 0;
				rpgStats.platforms[nPlatformIndex].nModelPart = -1;
			}
			else
			{
				rpgStats.platforms[nPlatformIndex].nModelPart = i;
				if ( pszProjectName )
				{
					for ( int i=0; i<skeleton.nodes.size(); i++ )
					{
						if ( skeleton.nodes[i].nIndex == rpgStats.platforms[nPlatformIndex].nModelPart )
						{
							rpgStats.platforms[nPlatformIndex].constraint.fMin = skeleton.nodes[i].constraint.fMin;
							rpgStats.platforms[nPlatformIndex].constraint.fMax = skeleton.nodes[i].constraint.fMax;
						}
					}
				}
			}


			rpgStats.platforms[nPlatformIndex].dwGunCarriageParts = 0xffff0000;
			rpgStats.platforms[nPlatformIndex].constraintVertical.fMin = 0;
			rpgStats.platforms[nPlatformIndex].constraintVertical.fMax = 0;
			for ( int k=0; k<2; k++ )
			{
				if ( k == 0 )
					szPartName = pPlatformProps->GetGunCarriageName1();
				else
					szPartName = pPlatformProps->GetGunCarriageName2();
				i = 0;
				for ( ; i<nNumNodes; i++ )
				{
					if ( allNamesVector[i] == szPartName )
						break;
				}
				
				if ( i == nNumNodes )
				{
					rpgStats.platforms[nPlatformIndex].dwGunCarriageParts |= 255 << (8 * k);
				}
				else
				{
					rpgStats.platforms[nPlatformIndex].dwGunCarriageParts |= i << (8 * k);
					if ( pszProjectName )
					{
						for ( int a=0; a<skeleton.nodes.size(); a++ )
						{
							if ( skeleton.nodes[a].nIndex == i )
							{
								if ( rpgStats.platforms[nPlatformIndex].constraintVertical.fMin == 0 )
									rpgStats.platforms[nPlatformIndex].constraintVertical.fMin = skeleton.nodes[a].constraint.fMin;
								if ( rpgStats.platforms[nPlatformIndex].constraintVertical.fMax == 0 )
									rpgStats.platforms[nPlatformIndex].constraintVertical.fMax = skeleton.nodes[a].constraint.fMax;
							}
						}
					}
				}
			}
		}

		CMeshGunsItem *pGuns = static_cast<CMeshGunsItem *> ( pPlatformProps->GetChildItem( E_MESH_GUNS_ITEM ) );
		{
			CTreeItem::CTreeItemList::const_iterator it=pGuns->GetBegin();
			for ( ; it!=pGuns->GetEnd(); ++it )
			{
				CMeshGunPropsItem *pGunProps = static_cast<CMeshGunPropsItem *> ( it->GetPtr() );
				SMechUnitRPGStats::SGun gun;
				if ( nNumNodes != 0 )
				{
					string szPointName = pGunProps->GetShootPointName();
					int i = 0;
					for ( ; i<nNumNodes; i++ )
					{
						if ( allNamesVector[i] == szPointName )
							break;
					}
					if ( i == nNumNodes )
						gun.nShootPoint = -1;
					else
					{
						gun.nShootPoint = i;

						if ( pModelMatrix && gun.nShootPoint != -1 )
						{
							CVec3 vRes;
							pModelMatrix[ gun.nShootPoint ].RotateVector( &vRes, CVec3(0, 0, 1) );
							float alpha = atan2( vRes.y, vRes.x );
							alpha += (float) 3 * FP_PI2;
							if ( alpha > FP_2PI )
								alpha -= FP_2PI;
							gun.wDirection = alpha * 65535 / FP_2PI;
						}
					}

					string szPartName = pGunProps->GetShootPartName();
					for ( i=0; i<nNumNodes; i++ )
					{
						if ( allNamesVector[i] == szPartName )
							break;
					}
					if ( i == nNumNodes )
					{
						gun.fRecoilLength = 0;
						gun.nModelPart = -1;
					}
					else
					{
						gun.nModelPart = i;
						if ( pszProjectName )
						{
							for ( int i=0; i<skeleton.nodes.size(); i++ )
							{
								if ( skeleton.nodes[i].nIndex == gun.nModelPart )
								{
									gun.fRecoilLength = fabs( skeleton.nodes[i].constraint.fMax - skeleton.nodes[i].constraint.fMin );
								}
							}
						}
					}
				}
				else
					gun.nShootPoint = -1;
				
				gun.szWeapon = pGunProps->GetWeaponName();
				gun.nPriority = pGunProps->GetPriority();
				gun.bRecoil = pGunProps->GetRecoilFlag();
				gun.recoilTime = pGunProps->GetRecoilTime();
				gun.nRecoilShakeTime = pGunProps->GetRecoilShakeTime();
				gun.fRecoilShakeAngle = pGunProps->GetRecoilShakeAngle();
				gun.nAmmo = pGunProps->GetAmmoCount();
				gun.fReloadCost = pGunProps->GetReloadCost();
				rpgStats.guns.push_back( gun );
			}
		}
		if ( nPlatformIndex == 0 )
			rpgStats.platforms[nPlatformIndex].nFirstGun = 0;
		else
			rpgStats.platforms[nPlatformIndex].nFirstGun = rpgStats.platforms[nPlatformIndex-1].nFirstGun + rpgStats.platforms[nPlatformIndex-1].nNumGuns;
		rpgStats.platforms[nPlatformIndex].nNumGuns = rpgStats.guns.size() - rpgStats.platforms[nPlatformIndex].nFirstGun;

		nPlatformIndex++;
	}
	
	if ( pModelMatrix )
	{
		if ( rpgStats.nEntrancePoint != -1 )
		{
			SHMatrix entranceMatrix = pModelMatrix[ rpgStats.nEntrancePoint ];
			CVec3 vEntranceTrans = entranceMatrix.GetTrans3();
			rpgStats.vEntrancePoint.x = vEntranceTrans.x;
			rpgStats.vEntrancePoint.y = vEntranceTrans.y;
		}
		for ( int i=0; i<rpgStats.peoplePointIndices.size(); ++i )
		{
			SHMatrix matrix = pModelMatrix[ rpgStats.peoplePointIndices[i] ];
			CVec3 v3 = matrix.GetTrans3();
			CVec2 v2( v3.x, v3.y );
			rpgStats.vPeoplePoints.push_back( v2 );
		}
		if ( nMyAmmoIndex != -1 )
		{
			SHMatrix matrix = pModelMatrix[ nMyAmmoIndex ];
			CVec3 v3 = matrix.GetTrans3();
			rpgStats.vAmmoPoint.x = v3.x;
			rpgStats.vAmmoPoint.y = v3.y;
		}
	}
	
	rpgStats.vGunners.resize( 3 );
	for ( int k=0; k<3; k++ )
	{
		CPtr<IObjVisObj> pObject;
		if ( k == 0 )
			pObject = pCombatObject;
		else if ( k == 1 )
			pObject = pInstallObject;
		else if ( k == 2 )
			pObject = pTransObject;

		if ( pObject == 0 )
			continue;

		std::vector< SMyGunner > gunners;
		SMyGunner oneGunner;

		const SHMatrix *pLocalMatrix = 0;
		{
			CVec3 vOldPosition = pObject->GetPosition();
			int nOldDirection = pObject->GetDirection();
			pObject->SetPosition( CVec3(0, 0, 0) );
			pObject->SetDirection( 0 );
			
			IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pObject->GetAnimation() );
			pLocalMatrix = pMeshAnim->GetMatrices( MONE );
			
			pObject->SetPosition( vOldPosition );
			pObject->SetDirection( nOldDirection );
		}

		nNumNodes = 0;
		{
			IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pObject->GetAnimation() );
			IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );
			nNumNodes = pMeshAnim->GetNumNodes();
			if ( nNumNodes != 0 )
			{
				allNamesVector.resize( nNumNodes );
				NI_ASSERT( pMeshAnimEdit != 0 );
				pMeshAnimEdit->GetAllNodeNames( &(allNamesVector[0]), nNumNodes );
			}

			for ( int i=0; i<nNumNodes; i++ )
			{
				const char *pszTemp = "LGunner";
				if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
				{
					oneGunner.szName = allNamesVector[i];
					oneGunner.nIndex = i; 
					gunners.push_back( oneGunner );
					continue;
				}

				pszTemp = "LTowingPoint";
				if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
				{
					rpgStats.nTowPoint = i;

					SHMatrix towMatrix = pLocalMatrix[ rpgStats.nTowPoint ];
					CVec3 vTowTrans = towMatrix.GetTrans3();
					rpgStats.vTowPoint.x = vTowTrans.x;
					rpgStats.vTowPoint.y = vTowTrans.y;
					continue;
				}

				pszTemp = "LFrontWheel";
				if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
				{
					SHMatrix wheelMatrix = pLocalMatrix[ i ];
					CVec3 vWheelTrans = wheelMatrix.GetTrans3();
					rpgStats.vFrontWheel.x = vWheelTrans.x;
					rpgStats.vFrontWheel.y = vWheelTrans.y;
					continue;
				}

				pszTemp = "LBackWheel";
				if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
				{
					SHMatrix wheelMatrix = pLocalMatrix[ i ];
					CVec3 vWheelTrans = wheelMatrix.GetTrans3();
					rpgStats.vBackWheel.x = vWheelTrans.x;
					rpgStats.vBackWheel.y = vWheelTrans.y;
					continue;
				}

				pszTemp = "LHookPoint";
				if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
				{
					SHMatrix hookMatrix = pLocalMatrix[ i ];
					CVec3 vHookTrans = hookMatrix.GetTrans3();
					rpgStats.vHookPoint.x = vHookTrans.x;
					rpgStats.vHookPoint.y = vHookTrans.y;
					continue;
				}

				pszTemp = "LFatalitySmoke";
				if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
				{
					rpgStats.nFatalitySmokePoint = i;
					continue;
				}

				pszTemp = "LSmoke";
				if ( !strncmp( allNamesVector[i], pszTemp, strlen(pszTemp) ) )
				{
					rpgStats.damagePoints.push_back( i );
					continue;
				}
			}
		}
		
		std::sort( gunners.begin(), gunners.end() );
		if ( pLocalMatrix )
		{
			for ( int i=0; i<gunners.size(); i++ )
			{
				SHMatrix matrix = pLocalMatrix[ gunners[i].nIndex ];
				CVec3 v3 = matrix.GetTrans3();
				CVec2 v2( v3.x, v3.y );
				NI_ASSERT( gunners[i].szName.size() > 8 )
					std::string szTemp = gunners[i].szName.c_str() + 7;		//7 == sizeof "LGunner"
				if ( szTemp[0] == '0' )
					rpgStats.vGunners[0].push_back( v2 );
				else if ( szTemp[0] == '1' )
					rpgStats.vGunners[1].push_back( v2 );
				else if ( szTemp[0] == '2' )
					rpgStats.vGunners[2].push_back( v2 );
			}
		}
	}
	
	IObjectsDB *pDB = GetSingleton<IObjectsDB>();
	for ( int p=0; p<rpgStats.platforms.size(); p++ )
	{
		SMechUnitRPGStats::SPlatform &platform = rpgStats.platforms[p];
		if ( platform.nModelPart == -1 )
			continue;		//�� ������ �������
		if ( platform.constraintVertical.fMax != 0 )
			continue;

		bool bBalisticType = false;
		int nShootPointIndex = 0;
		for ( int g=platform.nFirstGun; g<platform.nFirstGun + platform.nNumGuns; g++ )
		{
			SMechUnitRPGStats::SGun &gun = rpgStats.guns[g];
			gun.RetrieveShortcuts( pDB );
			for ( int s=0; s<gun.pWeapon->shells.size(); s++ )
			{
				if ( gun.pWeapon->shells[s].trajectory == SWeaponRPGStats::SShell::TRAJECTORY_HOWITZER ||
					gun.pWeapon->shells[s].trajectory == SWeaponRPGStats::SShell::TRAJECTORY_CANNON )
				{
					bBalisticType = true;
					nShootPointIndex = gun.nShootPoint;
					break;
				}
			}
			if ( bBalisticType )
				break;
		}
		
		NI_ASSERT_T( nShootPointIndex >= 0, "Invalid shoot point index (-1)" );
		if ( bBalisticType && nShootPointIndex >= 0 /*&& platform.constraintVertical.fMax == 0*/ )
		{
			NI_ASSERT( pModelMatrix != 0 );
			const SHMatrix &matrix = pModelMatrix[ nShootPointIndex ];
			CVec3 vZ( 0, 0, 1.0f);
			CVec3 v;
			matrix.RotateVector( &v, vZ );

			double d = sqrt( v.x*v.x + v.y*v.y );
			double alpha = atan2( fabs(v.z), d );
			NI_ASSERT_T( alpha >= 0, "Error while calculation angle (angle < 0)" );

			platform.constraintVertical.fMin = alpha;
			platform.constraintVertical.fMax = alpha;
		}
	}
}

void CMeshFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	ASSERT( !pDT->IsReading() );
	
	SMechUnitRPGStats rpgStats;
	if ( !bNewProjectJustCreated )
		FillRPGStats( rpgStats, pRootItem, pszProjectName );
	else
		GetRPGStats( rpgStats, pRootItem );
	
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CMeshFrame::GetRPGStats( const SMechUnitRPGStats &rpgStats, CTreeItem *pRootItem )
{
	CMeshGraphicsItem *pGraphicsItem = (CMeshGraphicsItem *) pRootItem->GetChildItem( E_MESH_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	CTreeItem *pDeathCratersItem = pGraphicsItem->GetChildItem( E_MESH_DEATH_CRATERS_ITEM );
	NI_ASSERT( pDeathCratersItem != 0 );
	pDeathCratersItem->RemoveAllChilds();
	for ( int i=0; i<rpgStats.deathCraters.size(); i++ )
	{
		CMeshDeathCraterPropsItem *pProps = new CMeshDeathCraterPropsItem;
		pProps->SetItemName( rpgStats.deathCraters[i].c_str() );
		pProps->SetCraterFileName( rpgStats.deathCraters[i].c_str() );
		pDeathCratersItem->AddChild( pProps );
	}

	CMeshCommonPropsItem *pCommonProps = static_cast<CMeshCommonPropsItem *> ( pRootItem->GetChildItem( E_MESH_COMMON_PROPS_ITEM ) );
	pCommonProps->SetMeshName( rpgStats.szKeyName.c_str() );
	
	pCommonProps->SetMeshType( rpgStats.type );
	pCommonProps->SetAIClass( rpgStats.aiClass );
	
	pCommonProps->SetHealth( rpgStats.fMaxHP );
	pCommonProps->SetRepairCost( rpgStats.fRepairCost );
	pCommonProps->SetSight( rpgStats.fSight );
	pCommonProps->SetCamouflage( rpgStats.fCamouflage );
	pCommonProps->SetSpeed( rpgStats.fSpeed );
	pCommonProps->SetPassability( rpgStats.fPassability );
	pCommonProps->SetPullingPower( rpgStats.fTowingForce );
	pCommonProps->SetUninstallRotateTime( rpgStats.fUninstallRotate );
	pCommonProps->SetUninstallTransportTime( rpgStats.fUninstallTransport );
	pCommonProps->SetWeight( rpgStats.fWeight );
	
	pCommonProps->SetCrew( rpgStats.nCrew );
	pCommonProps->SetPassangers( rpgStats.nPassangers );
	pCommonProps->SetPriority( rpgStats.nPriority );
	pCommonProps->SetRotateSpeed( rpgStats.fRotateSpeed );
	pCommonProps->SetBoundTileRadius( rpgStats.nBoundTileRadius );
	pCommonProps->SetTurnRadius( rpgStats.fTurnRadius );
	pCommonProps->SetSilhouette( rpgStats.fSmallAABBCoeff );
	pCommonProps->SetAIPrice( rpgStats.fPrice );
	pCommonProps->SetSightPower( rpgStats.fSightPower );
	
	CUnitAcksItem *pAcks = static_cast<CUnitAcksItem *> ( pRootItem->GetChildItem( E_UNIT_ACKS_ITEM ) );
	NI_ASSERT( pAcks != 0 );
	if ( rpgStats.szAcksNames.size() > 0 )
		pAcks->SetAckName( rpgStats.szAcksNames[0].c_str() );

	CMeshEffectsItem *pEffects = static_cast<CMeshEffectsItem *> ( pRootItem->GetChildItem( E_MESH_EFFECTS_ITEM ) );
	NI_ASSERT( pEffects != 0 );
	pEffects->SetEffectDieselName( rpgStats.szEffectDiesel.c_str() );
	pEffects->SetEffectSmokeName( rpgStats.szEffectSmoke.c_str() );
	pEffects->SetEffectWheelDustName( rpgStats.szEffectWheelDust.c_str() );
	pEffects->SetEffectShootDustName( rpgStats.szEffectShootDust.c_str() );
	pEffects->SetEffectFatalityName( rpgStats.szEffectFatality.c_str() );
	pEffects->SetEffectDisappearName( rpgStats.szEffectDisappear.c_str() );
	
	pEffects->SetSoundStart( rpgStats.szSoundMoveStart.c_str() );
	pEffects->SetSoundCycle( rpgStats.szSoundMoveCycle.c_str() );
	pEffects->SetSoundStop( rpgStats.szSoundMoveStop.c_str() );

	const char *pTemp = strstr( rpgStats.szSoundMoveStop.c_str(), "cycle" );
	if ( pTemp )
	{
		pEffects->SetSoundCycle( rpgStats.szSoundMoveStop.c_str() );
		pEffects->SetSoundStop( rpgStats.szSoundMoveCycle.c_str() );
		SetChangedFlag( true );
	}
	
	CMeshAviaItem *pAviaProps = static_cast<CMeshAviaItem *>( pCommonProps->GetChildItem( E_MESH_AVIA_ITEM ) );
	if ( pAviaProps != 0 )
	{
		pAviaProps->SetMaxHeight( rpgStats.fMaxHeight );
		pAviaProps->SetDivingAngle( rpgStats.fDivingAngle );
		pAviaProps->SetClimbAngle( rpgStats.fClimbAngle );
		pAviaProps->SetTiltAngle( rpgStats.fTiltAngle );
		pAviaProps->SetTiltRatio( rpgStats.fTiltRatio );
	}

	CMeshTrackItem *pTrackProps = static_cast<CMeshTrackItem *>( pCommonProps->GetChildItem( E_MESH_TRACK_ITEM ) );
	if ( pTrackProps != 0 )
	{
		pTrackProps->SetLeaveTrackFlag( rpgStats.bLeavesTracks );
		pTrackProps->SetTrackWidth( rpgStats.fTrackWidth );
		pTrackProps->SetTrackOffset( rpgStats.fTrackOffset );
		pTrackProps->SetTrackStart( rpgStats.fTrackStart );
		pTrackProps->SetTrackEnd( rpgStats.fTrackEnd );
		pTrackProps->SetTrackIntensity( rpgStats.fTrackIntensity );
		pTrackProps->SetTrackLifeTime( rpgStats.nTrackLifetime );
	}
	
	CUnitActionsItem *pActionsItem = static_cast<CUnitActionsItem *>( pRootItem->GetChildItem( E_UNIT_ACTIONS_ITEM ) );
	pActionsItem->SetActions( &rpgStats );
	
	CUnitExposuresItem *pExposures = static_cast<CUnitExposuresItem*>( pRootItem->GetChildItem( E_UNIT_EXPOSURES_ITEM ) );
	pExposures->SetExposures( &rpgStats );
	
	CTreeItem *pDefencesItem = pRootItem->GetChildItem( E_MESH_DEFENCES_ITEM );
	for ( int i=0; i<6; i++ )
	{
		CMeshDefencePropsItem *pDefProps = static_cast<CMeshDefencePropsItem *> ( pDefencesItem->GetChildItem( E_MESH_DEFENCE_PROPS_ITEM, i ) );
		int nIndex = 0;
		if ( string( "Left" ) == pDefProps->GetItemName() )
			nIndex = RPG_LEFT;
		else if ( string( "Right" ) == pDefProps->GetItemName() )
			nIndex = RPG_RIGHT;
		else if ( string( "Top" ) == pDefProps->GetItemName() )
			nIndex = RPG_TOP;
		else if ( string( "Bottom" ) == pDefProps->GetItemName() )
			nIndex = RPG_BOTTOM;
		else if ( string( "Front" ) == pDefProps->GetItemName() )
			nIndex = RPG_FRONT;
		else if ( string( "Back" ) == pDefProps->GetItemName() )
			nIndex = RPG_BACK;

		pDefProps->SetMinArmor( rpgStats.armors[nIndex].fMin );
		pDefProps->SetMaxArmor( rpgStats.armors[nIndex].fMax );
	}
	
	CTreeItem *pJoggingsItem = pRootItem->GetChildItem( E_MESH_JOGGINGS_ITEM );
	for ( int i=0; i<3; i++ )
	{
		CMeshJoggingPropsItem *pJogProps = static_cast<CMeshJoggingPropsItem *> ( pJoggingsItem->GetChildItem( E_MESH_JOGGING_PROPS_ITEM, i ) );
		const SMechUnitRPGStats::SJoggingParams *pJog = 0;
		if ( i == 0 )
			pJog = &rpgStats.jx;
		else if ( i == 1 )
			pJog = &rpgStats.jy;
		else if ( i == 2 )
			pJog = &rpgStats.jz;

		pJogProps->SetPeriod1( pJog->fPeriod1 );
		pJogProps->SetPeriod2( pJog->fPeriod2 );
		pJogProps->SetAmplitude1( pJog->fAmp1 );
		pJogProps->SetAmplitude2( pJog->fAmp2 );
		pJogProps->SetPhase1( pJog->fPhase1 );
		pJogProps->SetPhase2( pJog->fPhase2 );
	}

	CMeshPlatformsItem *pPlatforms = static_cast<CMeshPlatformsItem *> ( pRootItem->GetChildItem( E_MESH_PLATFORMS_ITEM ) );
	int nPlatformIndex = 0;
	int nGunIndex = 0;
	for ( CTreeItem::CTreeItemList::const_iterator it=pPlatforms->GetBegin(); it!=pPlatforms->GetEnd(); ++it )
	{
		CMeshPlatformPropsItem *pPlatformProps = static_cast<CMeshPlatformPropsItem *> ( it->GetPtr() );
		pPlatformProps->SetVerticalRotationSpeed( rpgStats.platforms[nPlatformIndex].fVerticalRotationSpeed );
		pPlatformProps->SetHorizontalRotationSpeed( rpgStats.platforms[nPlatformIndex].fHorizontalRotationSpeed );
		CMeshGunsItem *pGuns = static_cast<CMeshGunsItem *> ( pPlatformProps->GetChildItem( E_MESH_GUNS_ITEM ) );
		for ( CTreeItem::CTreeItemList::const_iterator it=pGuns->GetBegin(); it!=pGuns->GetEnd(); ++it )
		{
			CMeshGunPropsItem *pGunProps = static_cast<CMeshGunPropsItem *> ( it->GetPtr() );
			const SMechUnitRPGStats::SGun &gun = rpgStats.guns[nGunIndex];
			pGunProps->SetWeaponName( gun.szWeapon.c_str() );
			pGunProps->SetPriority( gun.nPriority );
			pGunProps->SetRecoilFlag( gun.bRecoil );
			pGunProps->SetRecoilTime( gun.recoilTime );
			pGunProps->SetRecoilShakeTime( gun.nRecoilShakeTime );
			pGunProps->SetRecoilShakeAngle( gun.fRecoilShakeAngle );
			pGunProps->SetAmmoCount( gun.nAmmo );
			pGunProps->SetReloadCost( gun.fReloadCost );
			nGunIndex++;
		}
		nPlatformIndex++;
	}
}

void CMeshFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	ASSERT( pDT->IsReading() );
	
	SMechUnitRPGStats rpgStats;
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	
	GetRPGStats( rpgStats, pRootItem );
}

bool CMeshFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	SaveRPGStats( pDT, pRootItem, pszProjectName );

	CMeshGraphicsItem *pGraphicsItem = (CMeshGraphicsItem *) pRootItem->GetChildItem( E_MESH_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	string szRelName, szFullName, szDir;
	bool bRes = true;
	szRelName = pGraphicsItem->GetCombatMeshName();
	if ( IsRelatedPath( szRelName.c_str() ) )
	{
		szDir = GetDirectory( pszProjectName );
		MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
	}
	else
		szFullName = szRelName;
	string szFilesSourceDir = GetDirectory( szFullName.c_str() );
	
	string szDestDir = GetDirectory( pszResultFileName );
	{
		string szMask = "*.mod";
		vector<string> files;
		NFile::EnumerateFiles( szFilesSourceDir.c_str(), szMask.c_str(), NFile::CGetAllFiles( &files ), false );

		for ( int i=0; i<files.size(); i++ )
		{
			int nPos = files[i].rfind( '\\' );
			if ( nPos != string::npos )
			{
				string szResFileName = szDestDir + files[i].substr( nPos + 1 );
				MyCopyFile( files[i].c_str(), szResFileName.c_str() );
			}
			else
			{
				AfxMessageBox( NStr::Format( "Error: Can not find \"\\\" in the string %s\nCan not copy .mod file", files[i].c_str() ) );
			}
		}
	}

	string szResFileName;
	szRelName = pGraphicsItem->GetAliveSummerTexture();
	if ( szRelName.size() > 0 )
	{
		if ( IsRelatedPath( szRelName.c_str() ) )
			MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
		else
			szFullName = szRelName;
		szResFileName = szDestDir;
		szResFileName += "1";
		ConvertAndSaveImage( szFullName.c_str(), szResFileName.c_str() );
	}

	szRelName = pGraphicsItem->GetAliveWinterTexture();
	if ( szRelName.size() > 0 )
	{
		if ( IsRelatedPath( szRelName.c_str() ) )
			MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
		else
			szFullName = szRelName;
		szResFileName = szDestDir;
		szResFileName += "1w";
		ConvertAndSaveImage( szFullName.c_str(), szResFileName.c_str() );
	}
	
	szRelName = pGraphicsItem->GetAliveAfrikaTexture();
	if ( szRelName.size() > 0 )
	{
		if ( IsRelatedPath( szRelName.c_str() ) )
			MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
		else
			szFullName = szRelName;
		szResFileName = szDestDir;
		szResFileName += "1a";
		ConvertAndSaveImage( szFullName.c_str(), szResFileName.c_str() );
	}

	szRelName = pGraphicsItem->GetDeadSummerTexture();
	if ( szRelName.size() > 0 )
	{
		if ( IsRelatedPath( szRelName.c_str() ) )
			MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
		else
			szFullName = szRelName;
		szResFileName = szDestDir;
		szResFileName += "2";
		ConvertAndSaveImage( szFullName.c_str(), szResFileName.c_str() );
	}
	
	szRelName = pGraphicsItem->GetDeadWinterTexture();
	if ( szRelName.size() > 0 )
	{
		if ( IsRelatedPath( szRelName.c_str() ) )
			MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
		else
			szFullName = szRelName;
		szResFileName = szDestDir;
		szResFileName += "2w";
		ConvertAndSaveImage( szFullName.c_str(), szResFileName.c_str() );
	}
	
	szRelName = pGraphicsItem->GetDeadAfrikaTexture();
	if ( szRelName.size() > 0 )
	{
		if ( IsRelatedPath( szRelName.c_str() ) )
			MakeFullPath( szDir.c_str(), szRelName.c_str(), szFullName );
		else
			szFullName = szRelName;
		szResFileName = szDestDir;
		szResFileName += "2a";
		ConvertAndSaveImage( szFullName.c_str(), szResFileName.c_str() );
	}
	
	{
		szFullName = szDir + "icon.tga";

		do
		{
			IImageProcessor *pIP = GetSingleton<IImageProcessor>();
			
			CPtr<IDataStream> pStream = OpenFileStream( szFullName.c_str(), STREAM_ACCESS_READ );
			if ( !pStream )
			{
				break;
			}
			CPtr<IImage> pImage = pIP->LoadImage( pStream );
			if ( !pImage )
			{
				AfxMessageBox( (std::string( "Error: can not load image from stream: ") + szFullName).c_str() );
				break;
			}
			
			{
				const int nSizeX = 64;
				const int nSizeY = 64;
				CPtr<IImage> pSmallImage = pIP->CreateScaleBySize( pImage, nSizeX, nSizeY, ISM_LANCZOS3 );
				CPtr<IImage> p64Image = pIP->CreateImage( 64, 64 );
				SColor col;
				col.r = col.g = col.b = 146;
				col.a = 0;
				p64Image->Set( col );
				RECT rc;
				rc.left = 0;
				rc.top = 0;
				rc.right = 64;
				rc.bottom = 64;
				p64Image->CopyFromAB( pSmallImage, &rc, 0, 0 );
	
				szResFileName = szDestDir;
				szResFileName += "icon.tga";
				CPtr<IDataStream> pSaveStream = CreateFileStream( szResFileName.c_str(), STREAM_ACCESS_WRITE );
				if ( pStream )
					pIP->SaveImageAsTGA( pSaveStream, p64Image );
				else
					AfxMessageBox( (std::string( "Error: can not create icon stream: ") + szResFileName).c_str() );
			}

			{
				const int nSizeX = 90;
				const int nSizeY = 90;
				if ( pImage->GetSizeX() != nSizeX || pImage->GetSizeY() != nSizeY )
					pImage = pIP->CreateScaleBySize( pImage, nSizeX, nSizeY, ISM_LANCZOS3 );
				
				CPtr<IImage> p128Image = pIP->CreateImage( 128, 128 );
				p128Image->Set( 0 );
				RECT rc;
				rc.left = 0;
				rc.top = 0;
				rc.right = 90;
				rc.bottom = 90;
				p128Image->CopyFrom( pImage, &rc, 0, 0 );
				
				szResFileName = szDestDir;
				szResFileName += "icon";
				SaveCompressedTexture( p128Image, szResFileName.c_str() );
			}
		} while( 0 );
	}
	
	{
		szFullName = szDir + "icon512.tga";
		szResFileName = szDestDir;
		szResFileName += "icon512";
		ConvertAndSaveImage( szFullName.c_str(), szResFileName.c_str() );
	}

	for ( int i=0; i<3; i++ )
	{
		std::string szTemp;
		switch ( i )
		{
		case 0:
			szTemp = "1p";
			break;
		case 1:
			szTemp = "1pw";
			break;
		case 2:
			szTemp = "1pa";
			break;
		default:
			NI_ASSERT( 0 );				//WTF?
		}

		szResFileName = szDestDir;
		szResFileName += szTemp;
		szTemp = szDir + szTemp;
		szTemp += ".tga";
		ConvertAndSaveImage( szTemp.c_str(), szResFileName.c_str() );
	}

	CLocalizationItem *pLocItem = static_cast<CLocalizationItem *> ( pRootItem->GetChildItem( E_LOCALIZATION_ITEM ) );
	NI_ASSERT( pLocItem != 0 );
	
	string szSrc, szRes;
	szRes = szDestDir;
	szRes += "name.txt";
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pLocItem->GetLocalizationName(), szSrc );
	MyCopyFile( szSrc.c_str(), szRes.c_str() );
	
	szRes = szDestDir;
	szRes += "desc.txt";
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pLocItem->GetLocalizationDesc(), szSrc );
	MyCopyFile( szSrc.c_str(), szRes.c_str() );
	
	szRes = szDestDir;
	szRes += "stats.txt";
	MakeFullPath( GetDirectory(pszProjectName).c_str(), pLocItem->GetLocalizationStats(), szSrc );
	MyCopyFile( szSrc.c_str(), szRes.c_str() );

	return true;
}

void CMeshFrame::LoadGunPointPropsComboBox( SProp *pPointProp )
{
	pPointProp->szStrings.clear();
	if ( !pCombatObject )
	{
		pPointProp->szStrings.push_back("NA" );
		pPointProp->value ="NA";
		return;
	}

	IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pCombatObject->GetAnimation() );
	IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );
	NI_ASSERT( pMeshAnimEdit != 0 );
	int nNumLocators = pMeshAnimEdit->GetNumLocators();
	if ( nNumLocators == 0 )
	{
		pPointProp->szStrings.push_back( "NA" );
		pPointProp->value = "NA";
		return;
	}

	std::vector<const char*> namesVector( nNumLocators );
	pMeshAnimEdit->GetAllLocatorNames( &(namesVector[0]), nNumLocators );
	bool bExist = false;
	string szVal = pPointProp->value;
	for ( int i=0; i<nNumLocators; i++ )
	{
		char *pszTemp = 0;

		pszTemp = "LMainGun";
		if ( !strncmp( namesVector[i], pszTemp, strlen(pszTemp) ) )
		{
			pPointProp->szStrings.push_back( namesVector[i] );
			if ( szVal == namesVector[i] )
				bExist = true;
			continue;
		}

		pszTemp = "LMachineGun";
		if ( !strncmp( namesVector[i], pszTemp, strlen(pszTemp) ) )
		{
			pPointProp->szStrings.push_back( namesVector[i] );
			if ( szVal == namesVector[i] )
				bExist = true;
			continue;
		}
	}

	pPointProp->szStrings.push_back( "NA" );
	if ( !bExist )
		pPointProp->value = "NA";
}

void CMeshFrame::LoadGunPartPropsComboBox( SProp *pPointProp )
{
	pPointProp->szStrings.clear();
	if ( !pCombatObject )
	{
		pPointProp->szStrings.push_back( "NA" );
		pPointProp->value = "NA";
		return;
	}
	
	IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pCombatObject->GetAnimation() );
	int nNumNodes = pMeshAnim->GetNumNodes();
	if ( nNumNodes == 0 )
	{
		pPointProp->szStrings.push_back( "NA" );
		pPointProp->value = "NA";
		return;
	}
	
	IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );
	NI_ASSERT( pMeshAnimEdit != 0 );
	std::vector<const char*> allNamesVector( nNumNodes );
	pMeshAnimEdit->GetAllNodeNames( &(allNamesVector[0]), nNumNodes );
	
	int nNumLocators = pMeshAnimEdit->GetNumLocators();
	std::vector<const char*> locatorNamesVector( nNumLocators );
	if ( nNumLocators != 0 )
		pMeshAnimEdit->GetAllLocatorNames( &(locatorNamesVector[0]), nNumLocators );
	
	std::vector<string> partNamesVector;
	for ( int i=0; i<nNumNodes; i++ )
	{
		int k = 0;
		string szAll = allNamesVector[i];
		for ( ; k<nNumLocators; k++ )
		{
			string szLocator = locatorNamesVector[k];
			if ( szAll == szLocator )
				break;
		}
		if ( k == nNumLocators )
			partNamesVector.push_back( szAll );
	}
	
	bool bExist = false;
	string szVal = pPointProp->value;
	std::string szTemp = "GunCarriage";
	for ( int i=0; i<partNamesVector.size(); i++ )
	{
		std::string szName = partNamesVector[i];
		if ( partNamesVector[i][0] != 'L' && szName.compare( 0, szTemp.size(), szTemp ) )
		{
			pPointProp->szStrings.push_back( partNamesVector[i] );
			if ( szVal == partNamesVector[i] )
				bExist = true;
		}
	}

	pPointProp->szStrings.push_back( "NA" );
	if ( !bExist )
		pPointProp->value = "NA";
}

void CMeshFrame::LoadGunCarriagePropsComboBox( SProp *pPointProp )
{
	pPointProp->szStrings.clear();
	if ( !pCombatObject )
	{
		pPointProp->szStrings.push_back( "NA" );
		pPointProp->value = "NA";
		return;
	}
	
	IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pCombatObject->GetAnimation() );
	int nNumNodes = pMeshAnim->GetNumNodes();
	if ( nNumNodes == 0 )
	{
		pPointProp->szStrings.push_back( "NA" );
		pPointProp->value = "NA";
		return;
	}
	
	IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );
	NI_ASSERT( pMeshAnimEdit != 0 );
	std::vector<const char*> allNamesVector( nNumNodes );
	pMeshAnimEdit->GetAllNodeNames( &(allNamesVector[0]), nNumNodes );
	
	pPointProp->szStrings.clear();
	std::string szTemp = "GunCarriage";
	for ( int i=0; i<nNumNodes; i++ )
	{
		std::string szName = allNamesVector[i];
		if ( !szName.compare( 0, szTemp.size(), szTemp ) )
			pPointProp->szStrings.push_back( allNamesVector[i] );
	}

	if ( pPointProp->szStrings.empty() )
		pPointProp->value = "NA";
	pPointProp->szStrings.push_back( "NA" );
}

void CMeshFrame::LoadPlatformPropsComboBox( SProp *pPointProp )
{
	pPointProp->szStrings.clear();
	if ( !pCombatObject )
	{
		pPointProp->szStrings.push_back( "NA" );
		pPointProp->value = "NA";
		return;
	}
	
	IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pCombatObject->GetAnimation() );
	int nNumNodes = pMeshAnim->GetNumNodes();
	if ( nNumNodes == 0 )
	{
		pPointProp->szStrings.push_back( "NA" );
		pPointProp->value = "NA";
		return;
	}

	IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );
	NI_ASSERT( pMeshAnimEdit != 0 );
	std::vector<const char*> allNamesVector( nNumNodes );
	if ( nNumNodes == 0 )
	{
		pPointProp->szStrings.push_back( "NA" );
		pPointProp->value = "NA";
		return;
	}
	pMeshAnimEdit->GetAllNodeNames( &(allNamesVector[0]), nNumNodes );
	
	int nNumLocators = pMeshAnimEdit->GetNumLocators();
	std::vector<const char*> locatorNamesVector( nNumLocators );
	if ( nNumLocators != 0 )
		pMeshAnimEdit->GetAllLocatorNames( &(locatorNamesVector[0]), nNumLocators );
	
	std::vector<string> partNamesVector;
	for ( int i=0; i<nNumNodes; i++ )
	{
		int k = 0;
		string szAll = allNamesVector[i];
		for ( ; k<nNumLocators; k++ )
		{
			string szLocator = locatorNamesVector[k];
			if ( szAll == szLocator )
				break;
		}
		if ( k == nNumLocators )
			partNamesVector.push_back( szAll );
	}
	
	bool bExist = false;
	string szVal = pPointProp->value;
	for ( int i=0; i<partNamesVector.size(); i++ )
	{
		pPointProp->szStrings.push_back( partNamesVector[i] );
		if ( szVal == pPointProp->szStrings[i] )
			bExist = true;
	}
	if ( !bExist )
		pPointProp->value = "NA";
	pPointProp->szStrings.push_back( "NA" );
}

void CMeshFrame::SetCombatMesh( const char *pszMeshName, const char *pszProjectName, CTreeItem *pRootItem )
{
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	if ( pCombatObject )
	{
		pSG->RemoveObject( pCombatObject );
		GFXDraw();
	}

	pActiveLocator = 0;
	CTreeItem *pLocatorsItem = pRootItem->GetChildItem( E_MESH_LOCATORS_ITEM );
	NI_ASSERT( pLocatorsItem != 0 );
	pLocatorsItem->RemoveAllChilds();

	CMeshGraphicsItem *pGraphicsItem = 0;
	if ( !pRootItem )
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		pRootItem = pTree->GetRootItem();
	}
	pGraphicsItem = static_cast<CMeshGraphicsItem *>( pRootItem->GetChildItem( E_MESH_GRAPHICS_ITEM ) );
		
	{
		string szMeshFullName, szDir;
		if ( IsRelatedPath( pszMeshName ) )
		{
			szDir = GetDirectory( pszProjectName );
			MakeFullPath( szDir.c_str(), pszMeshName, szMeshFullName );
		}
		else
			szMeshFullName = pszMeshName;
		
		string szTempModFile = theApp.GetEditorTempDir();
		NFile::CreatePath( szTempModFile.c_str() );
		szTempModFile += "Unit.mod";
		if ( !CopyFile( szMeshFullName.c_str(), szTempModFile.c_str(), FALSE ) )
			return;
		
		szDir = GetDirectory( szMeshFullName.c_str() );
		string szTgaName = szDir;
		if ( GetFileAttributes( ( szTgaName + pGraphicsItem->GetAliveSummerTexture() ).c_str() ) != -1 )
			szTgaName += pGraphicsItem->GetAliveSummerTexture();
		else if ( GetFileAttributes( ( szTgaName + pGraphicsItem->GetAliveWinterTexture() ).c_str() ) != -1 )
			szTgaName += pGraphicsItem->GetAliveWinterTexture();
		else if ( GetFileAttributes( ( szTgaName + pGraphicsItem->GetAliveAfrikaTexture() ).c_str() ) != -1 )
			szTgaName += pGraphicsItem->GetAliveAfrikaTexture();
		else
			return;
		string szTempTgaFile = theApp.GetEditorTempDir();
		szTempTgaFile += "Unit";
		SaveTexture8888( szTgaName.c_str(), szTempTgaFile.c_str() );
	}
	string szTempModFile = theApp.GetEditorTempResourceDir();
	szTempModFile += "\\Unit";
	pCombatObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( szTempModFile.c_str(), 0, SGVOT_MESH ) );
	if ( !pCombatObject )
		return;
	
	pCombatObject->SetPosition( CVec3(12*fWorldCellSize, 12*fWorldCellSize, 0) );
	pCombatObject->SetDirection( 0 );
	pSG->AddObject( pCombatObject, SGVOGT_UNIT );
	NI_ASSERT( pCombatObject != 0 );
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	if ( pTree )
	{
		IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pCombatObject->GetAnimation() );
		pModelMatrix = 0;
		{
			CVec3 vOldPosition = pCombatObject->GetPosition();
			int nOldDirection = pCombatObject->GetDirection();
			pCombatObject->SetPosition( CVec3(0, 0, 0) );
			pCombatObject->SetDirection( 0 );
			
			pModelMatrix = pMeshAnim->GetMatrices( MONE );
			
			pCombatObject->SetPosition( vOldPosition );
			pCombatObject->SetDirection( nOldDirection );
		}

		int nNumNodes = 0;
		std::vector<const char*> allNamesVector( nNumNodes );
		IMeshAnimationEdit *pMeshAnimEdit = dynamic_cast<IMeshAnimationEdit *> ( pMeshAnim );
		nNumNodes = pMeshAnim->GetNumNodes();
		if ( nNumNodes != 0 )
		{
			allNamesVector.resize( nNumNodes );
			NI_ASSERT( pMeshAnimEdit != 0 );
			pMeshAnimEdit->GetAllNodeNames( &(allNamesVector[0]), nNumNodes );

			for ( int i=0; i<nNumNodes; i++ )
			{
				SHMatrix matrix = pModelMatrix[ i ];
				CVec3 vTrans = matrix.GetTrans3();
				vTrans += CVec3( 12*fWorldCellSize, 12*fWorldCellSize, 0 );

				IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
				CPtr<IObjVisObj> pObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( "editor\\locator\\1", 0, SGVOT_SPRITE ) );
				NI_ASSERT( pObject != 0 );
				
				pObject->SetPosition( vTrans );
				pObject->SetDirection( 0 );
				pObject->SetAnimation( 0 );
				if ( bShowLocators )
					pObject->SetOpacity( MIN_OPACITY );
				else
					pObject->SetOpacity( 0 );
				pSG->AddObject( pObject, SGVOGT_OBJECT );

				CMeshLocatorPropsItem *pLocProps = new CMeshLocatorPropsItem;
				pLocProps->nLocatorID = i;
				pLocProps->pSprite = pObject;
				pLocProps->SetItemName( allNamesVector[i] );
				
				pLocatorsItem->AddChild( pLocProps );
			}
		}
	}
	
	GFXDraw();
}

void CMeshFrame::UpdateLocators()
{
/*
ICamera *pCamera = GetSingleton<ICamera>();
CVec3 vPos = pCamera->GetAnchor();
pCamera->Update();
vPos = pCamera->GetAnchor();
pSG->Update( )
	*/
	
	IScene *pSG = GetSingleton<IScene>();
	
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	IMeshVisObj *pMeshObject = static_cast<IMeshVisObj*>( pCombatObject.GetPtr() );
	IMeshAnimation *pMeshAnim = static_cast<IMeshAnimation *> ( pCombatObject->GetAnimation() );
	GetSingleton<IGameTimer>()->Update( timeGetTime() - 5 );
	DWORD dwTime = GetSingleton<IGameTimer>()->GetGameTime();
	dwTime += 10;

	pCombatObject->SetDirection( pDirectionButtonDockBar->GetIntAngle() );
	pCombatObject->Update( dwTime );

/*
	{
		CVec3 vOldPosition = pCombatObject->GetPosition();
		pCombatObject->SetPosition( CVec3(0, 0, 0) );
		pSG->MoveObject( pCombatObject, CVec3(0, 0, 0) );
		pCombatObject->Update( dwTime );
		pCombatObject->SetPosition( vOldPosition );
		pSG->MoveObject( pCombatObject, vOldPosition );
		pCombatObject->Update( dwTime );
	}

	GFXDraw();
	
	SHMatrix initMatrix = pMeshObject->GetPlacement();
	pModelMatrix = pMeshAnim->GetMatrices( initMatrix );
*/

	CTreeItem *pLocatorsItem = pRootItem->GetChildItem( E_MESH_LOCATORS_ITEM );
	CTreeItem::CTreeItemList::const_iterator it = pLocatorsItem->GetBegin();
	int nNumNodes = pMeshAnim->GetNumNodes();
	if ( nNumNodes != 0 )
	{
		for ( int i=0; i<nNumNodes; i++ )
		{
			SHMatrix matrix = pModelMatrix[ i ];
			CVec3 vTrans = matrix.GetTrans3();
			CMeshLocatorPropsItem *pLocProps = static_cast<CMeshLocatorPropsItem *> ( it->GetPtr() );
			pLocProps->pSprite->SetPosition( vTrans );
			pSG->MoveObject( pLocProps->pSprite, vTrans );
			pLocProps->pSprite->Update( dwTime );
			++it;
		}
	}

	UpdateActiveLocatorLine();
	GFXDraw();
}

void CMeshFrame::OnShowLocatorsInfo() 
{
	bShowLocators = !bShowLocators;
	UpdateLocatorVisibility();
}

void CMeshFrame::UpdateLocatorVisibility()
{
	IScene *pSG = GetSingleton<IScene>();
	if ( bShowLocators )
		while ( !pSG->ToggleShow( SCENE_SHOW_BBS ) )
			;
	else
		while ( pSG->ToggleShow( SCENE_SHOW_BBS ) )
			;

	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	CTreeItem *pRootItem = pTree->GetRootItem();
	
	CTreeItem *pLocatorsItem = pRootItem->GetChildItem( E_MESH_LOCATORS_ITEM );
	CTreeItem::CTreeItemList::const_iterator it = pLocatorsItem->GetBegin();
	for ( ; it!=pLocatorsItem->GetEnd(); ++it )
	{
		CMeshLocatorPropsItem *pLocProps = static_cast<CMeshLocatorPropsItem *> ( it->GetPtr() );
		if ( bShowLocators )
			pLocProps->pSprite->SetOpacity( MIN_OPACITY );
		else
			pLocProps->pSprite->SetOpacity( 0 );
	}
	
	if ( bShowLocators && pActiveLocator )
	{
		pActiveLocator->pSprite->SetOpacity( MAX_OPACITY );
		UpdateActiveLocatorLine();
	}
	GFXDraw();
}

void CMeshFrame::OnUpdateShowLocatorsInfo(CCmdUI* pCmdUI) 
{
	CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
	pCmdUI->Enable( pTree != 0 );
}

void CMeshFrame::OnUpdateImportAckFile(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
}

void CMeshFrame::OnUpdateExportAckFile(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( true );
}

void CMeshFrame::SetInstallMesh( const char *pszMeshName, const char *pszProjectName, CTreeItem *pRootItem )
{
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	if ( pInstallObject )
	{
		pSG->RemoveObject( pInstallObject );
		GFXDraw();
	}
	
	CMeshGraphicsItem *pGraphicsItem = 0;
	if ( pRootItem )
		pGraphicsItem = static_cast<CMeshGraphicsItem *>( pRootItem->GetChildItem( E_MESH_GRAPHICS_ITEM ) );
	else
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRoot = pTree->GetRootItem();
		pGraphicsItem = static_cast<CMeshGraphicsItem *>( pRoot->GetChildItem( E_MESH_GRAPHICS_ITEM ) );
	}

	{
		string szMeshFullName, szDir;
		if ( IsRelatedPath( pszMeshName ) )
		{
			szDir = GetDirectory( pszProjectName );
			MakeFullPath( szDir.c_str(), pszMeshName, szMeshFullName );
		}
		else
			szMeshFullName = pszMeshName;
		szDir = GetDirectory( szMeshFullName.c_str() );
		
		string szTempModFile = theApp.GetEditorTempDir();
		szTempModFile += "2.mod";
		if ( !CopyFile( szMeshFullName.c_str(), szTempModFile.c_str(), FALSE ) )
			return;
		
		szDir = GetDirectory( szMeshFullName.c_str() );
		string szTgaName = szDir;
		if ( GetFileAttributes( ( szTgaName + pGraphicsItem->GetAliveSummerTexture() ).c_str() ) != -1 )
			szTgaName += pGraphicsItem->GetAliveSummerTexture();
		else if ( GetFileAttributes( ( szTgaName + pGraphicsItem->GetAliveWinterTexture() ).c_str() ) != -1 )
			szTgaName += pGraphicsItem->GetAliveWinterTexture();
		else if ( GetFileAttributes( ( szTgaName + pGraphicsItem->GetAliveAfrikaTexture() ).c_str() ) != -1 )
			szTgaName += pGraphicsItem->GetAliveAfrikaTexture();
		else
			return;
		string szTempTgaFile = theApp.GetEditorTempDir();
		szTempTgaFile += "2";
		SaveTexture8888( szTgaName.c_str(), szTempTgaFile.c_str() );
	}
	string szTempModFile = theApp.GetEditorTempResourceDir();
	szTempModFile += "\\2";
	pInstallObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( szTempModFile.c_str(), 0, SGVOT_MESH ) );
	if ( !pInstallObject )
		return;
	
	pInstallObject->SetPosition( CVec3(15*fWorldCellSize, 15*fWorldCellSize, 0) );
	pInstallObject->SetDirection( 0 );
	pSG->AddObject( pInstallObject, SGVOGT_UNIT );
	GFXDraw();
}

void CMeshFrame::SetTransportableMesh( const char *pszMeshName, const char *pszProjectName, CTreeItem *pRootItem )
{
	IVisObjBuilder *pVOB = GetSingleton<IVisObjBuilder>();
	IScene *pSG = GetSingleton<IScene>();
	if ( pTransObject )
	{
		pSG->RemoveObject( pTransObject );
		GFXDraw();
	}
	
	CMeshGraphicsItem *pGraphicsItem = 0;
	if ( pRootItem )
		pGraphicsItem = static_cast<CMeshGraphicsItem *>( pRootItem->GetChildItem( E_MESH_GRAPHICS_ITEM ) );
	else
	{
		CETreeCtrl *pTree = pTreeDockBar->GetTreeWithIndex( 0 );
		CTreeItem *pRoot = pTree->GetRootItem();
		pGraphicsItem = static_cast<CMeshGraphicsItem *>( pRoot->GetChildItem( E_MESH_GRAPHICS_ITEM ) );
	}

	{
		string szMeshFullName, szDir;
		if ( IsRelatedPath( pszMeshName ) )
		{
			szDir = GetDirectory( pszProjectName );
			MakeFullPath( szDir.c_str(), pszMeshName, szMeshFullName );
		}
		else
			szMeshFullName = pszMeshName;
		szDir = GetDirectory( szMeshFullName.c_str() );

		string szTempModFile = theApp.GetEditorTempDir();
		szTempModFile += "3.mod";
		if ( !CopyFile( szMeshFullName.c_str(), szTempModFile.c_str(), FALSE ) )
			return;
		
		szDir = GetDirectory( szMeshFullName.c_str() );
		string szTgaName = szDir;
		if ( GetFileAttributes( ( szTgaName + pGraphicsItem->GetAliveSummerTexture() ).c_str() ) != -1 )
			szTgaName += pGraphicsItem->GetAliveSummerTexture();
		else if ( GetFileAttributes( ( szTgaName + pGraphicsItem->GetAliveWinterTexture() ).c_str() ) != -1 )
			szTgaName += pGraphicsItem->GetAliveWinterTexture();
		else if ( GetFileAttributes( ( szTgaName + pGraphicsItem->GetAliveAfrikaTexture() ).c_str() ) != -1 )
			szTgaName += pGraphicsItem->GetAliveAfrikaTexture();
		else
			return;
		string szTempTgaFile = theApp.GetEditorTempDir();
		szTempTgaFile += "3";
		SaveTexture8888( szTgaName.c_str(), szTempTgaFile.c_str() );

		if ( !CopyFile( szTgaName.c_str(), szTempTgaFile.c_str(), FALSE ) )
			return;
	}
	string szTempModFile = theApp.GetEditorTempResourceDir();
	szTempModFile += "\\3";
	pTransObject = static_cast<IObjVisObj *> ( pVOB->BuildObject( szTempModFile.c_str(), 0, SGVOT_MESH ) );
	if ( !pTransObject )
		return;
	
	pTransObject->SetPosition( CVec3(18*fWorldCellSize, 18*fWorldCellSize, 0) );
	pTransObject->SetDirection( 0 );
	pSG->AddObject( pTransObject, SGVOGT_UNIT );
	GFXDraw();
}

bool CMeshFrame::LoadFramePreExportData( const char *pszProjectFile, CTreeItem *pRootItem )
{
	CMeshGraphicsItem *pGraphicsItem = (CMeshGraphicsItem *) pRootItem->GetChildItem( E_MESH_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	SetCombatMesh( pGraphicsItem->GetCombatMeshName(), pszProjectFile, pRootItem );
	SetInstallMesh( pGraphicsItem->GetInstallMeshName(), pszProjectFile, pRootItem );
	SetTransportableMesh( pGraphicsItem->GetTransMeshName(), pszProjectFile, pRootItem );
	return true;
}

FILETIME CMeshFrame::FindMaximalSourceTime( const char *pszProjectName, CTreeItem *pRootItem )
{
	FILETIME maxTime, currentTime;
	maxTime.dwHighDateTime = 0;
	maxTime.dwLowDateTime = 0;

	CMeshGraphicsItem *pGraphicsItem = (CMeshGraphicsItem *) pRootItem->GetChildItem( E_MESH_GRAPHICS_ITEM );
	NI_ASSERT( pGraphicsItem != 0 );
	string szRelName, szFullName;
	string szProjectDir = GetDirectory( pszProjectName );
	bool bRes = true;
	szRelName = pGraphicsItem->GetCombatMeshName();
	if ( IsRelatedPath( szRelName.c_str() ) )
		MakeFullPath( szProjectDir.c_str(), szRelName.c_str(), szFullName );
	else
		szFullName = szRelName;
	maxTime = GetFileChangeTime( szFullName.c_str() );
	
	szRelName = pGraphicsItem->GetInstallMeshName();
	if ( IsRelatedPath( szRelName.c_str() ) )
		MakeFullPath( szProjectDir.c_str(), szRelName.c_str(), szFullName );
	else
		szFullName = szRelName;
	currentTime = GetFileChangeTime( szFullName.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szRelName = pGraphicsItem->GetTransMeshName();
	if ( IsRelatedPath( szRelName.c_str() ) )
		MakeFullPath( szProjectDir.c_str(), szRelName.c_str(), szFullName );
	else
		szFullName = szRelName;
	currentTime = GetFileChangeTime( szFullName.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szRelName = pGraphicsItem->GetAliveSummerTexture();
	if ( IsRelatedPath( szRelName.c_str() ) )
		MakeFullPath( szProjectDir.c_str(), szRelName.c_str(), szFullName );
	else
		szFullName = szRelName;
	currentTime = GetFileChangeTime( szFullName.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szRelName = pGraphicsItem->GetAliveWinterTexture();
	if ( IsRelatedPath( szRelName.c_str() ) )
		MakeFullPath( szProjectDir.c_str(), szRelName.c_str(), szFullName );
	else
		szFullName = szRelName;
	currentTime = GetFileChangeTime( szFullName.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szRelName = pGraphicsItem->GetAliveAfrikaTexture();
	if ( IsRelatedPath( szRelName.c_str() ) )
		MakeFullPath( szProjectDir.c_str(), szRelName.c_str(), szFullName );
	else
		szFullName = szRelName;
	currentTime = GetFileChangeTime( szFullName.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szRelName = pGraphicsItem->GetDeadSummerTexture();
	if ( IsRelatedPath( szRelName.c_str() ) )
		MakeFullPath( szProjectDir.c_str(), szRelName.c_str(), szFullName );
	else
		szFullName = szRelName;
	currentTime = GetFileChangeTime( szFullName.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szRelName = pGraphicsItem->GetDeadWinterTexture();
	if ( IsRelatedPath( szRelName.c_str() ) )
		MakeFullPath( szProjectDir.c_str(), szRelName.c_str(), szFullName );
	else
		szFullName = szRelName;
	currentTime = GetFileChangeTime( szFullName.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	szRelName = pGraphicsItem->GetDeadAfrikaTexture();
	if ( IsRelatedPath( szRelName.c_str() ) )
		MakeFullPath( szProjectDir.c_str(), szRelName.c_str(), szFullName );
	else
		szFullName = szRelName;
	currentTime = GetFileChangeTime( szFullName.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;

	szRelName = szProjectDir + "icon.tga";
	currentTime = GetFileChangeTime( szFullName.c_str() );
	if ( currentTime > maxTime )
		maxTime = currentTime;
	
	return maxTime;
}

FILETIME CMeshFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime, current;
	string szDestDir = GetDirectory( pszResultFileName );
	string szFullName;
	szFullName = szDestDir;
	szFullName += "1.xml";
	minTime = GetFileChangeTime( szFullName.c_str() );

	szFullName = szDestDir;
	szFullName += "1.mod";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szFullName = szDestDir;
	szFullName += "2.mod";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szFullName = szDestDir;
	szFullName += "3.mod";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;

	szFullName = szDestDir;
	szFullName += "1.tga";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szFullName = szDestDir;
	szFullName += "1w.tga";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;
		
	szFullName = szDestDir;
	szFullName += "1a.tga";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;

	szFullName = szDestDir;
	szFullName += "2.tga";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szFullName = szDestDir;
	szFullName += "2w.tga";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;
		
	szFullName = szDestDir;
	szFullName += "2a.tga";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;
	
	szFullName = szDestDir;
	szFullName += "icon.tga";
	current = GetFileChangeTime( szFullName.c_str() );
	if ( current < minTime )
		minTime = current;

	return minTime;
}
void CMeshFrame::OnShowDirectionButton()
{
	SwitchDockerVisible( pDirectionButtonDockBar );
}
void CMeshFrame::OnUpdateShowDirectionButton(CCmdUI* pCmdUI) 
{
	UpdateShowMenu( pCmdUI, pDirectionButtonDockBar );
}
void CMeshFrame::OnRButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	
	CParentFrame::OnRButtonDown(nFlags, point);
}

