#include "StdAfx.h"
#include <io.h>
#include "..\Common\LegacyUiCompat.h"

#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"

#include "editor.h"
#include "PropView.h"
#include "WeaponView.h"
#include "WeaponFrm.h"
#include "TreeItem.h"
#include "WeaponTreeItem.h"
#include "GameWnd.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


IMPLEMENT_DYNCREATE(CWeaponFrame, CParentFrame)

BEGIN_MESSAGE_MAP(CWeaponFrame, CParentFrame)
	ON_WM_CREATE()
END_MESSAGE_MAP()


CWeaponFrame::CWeaponFrame()
{
	szComposerName = "Weapon Editor";
	szExtension = "*.wpn";
	szComposerSaveName = "Weapon_Composer_Project";
	nTreeRootItemID = E_WEAPON_ROOT_ITEM;
	nFrameType = CFrameManager::E_WEAPON_FRAME;
	pWndView = new CWeaponView;
	szAddDir = "weapons\\";
}

CWeaponFrame::~CWeaponFrame()
{
}

int CWeaponFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
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


bool CWeaponFrame::ExportFrameData( IDataTree *pDT, const char *pszProjectName, const char *pszResultFileName, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	NI_ASSERT( pRootItem->GetItemType() == E_WEAPON_ROOT_ITEM );
	SaveRPGStats( pDT, pRootItem, pszProjectName );

	return true;
}

void CWeaponFrame::GFXDraw()
{
	pGFX->Clear( 0, 0, GFXCLEAR_TARGET | GFXCLEAR_ZBUFFER, 0x80808080 );
	pGFX->BeginScene();

	GetSingleton<IGameTimer>()->Update( timeGetTime() );
	pGFX->SetShadingEffect( 2 );
	pGFX->SetTexture( 0, 0 );

	ICamera*	pCamera = GetSingleton<ICamera>();
	IScene *pSG = GetSingleton<IScene>();
	IGameTimer *pTimer = GetSingleton<IGameTimer>();
  pSG->Draw( pCamera );
	
	pGFX->EndScene();
	pGFX->Flip();
}

void CWeaponFrame::FillRPGStats( SWeaponRPGStats &rpgStats, CTreeItem *pRootItem )
{
	CWeaponCommonPropsItem *pCommonProps = static_cast<CWeaponCommonPropsItem *> ( pRootItem->GetChildItem( E_WEAPON_COMMON_PROPS_ITEM ) );
	rpgStats.szKeyName = pCommonProps->GetWeaponName();
	rpgStats.wDeltaAngle = pCommonProps->GetDeltaAngle();
	rpgStats.nAmmoPerBurst = pCommonProps->GetAmmoPerShoot();
	rpgStats.fDispersion = pCommonProps->GetDispersion();
	rpgStats.fRangeMin = pCommonProps->GetRangeMin();
	rpgStats.fRangeMax = pCommonProps->GetRangeMax();
	rpgStats.nCeiling = pCommonProps->GetCeiling();
	rpgStats.fAimingTime = pCommonProps->GetAimingTime();
	rpgStats.fRevealRadius = pCommonProps->GetRevealRadius();
	
	CTreeItem *pShootTypesItem = pRootItem->GetChildItem( E_WEAPON_SHOOT_TYPES_ITEM );
	CTreeItem::CTreeItemList::const_iterator it = pShootTypesItem->GetBegin();
	for ( ; it!=pShootTypesItem->GetEnd(); ++it )
	{
		SWeaponRPGStats::SShell damage;
		CWeaponDamagePropsItem *pDamageItem = (CWeaponDamagePropsItem *) it->GetPtr();
		
		damage.trajectory = pDamageItem->GetTrajectoryType();
		damage.nPiercing = pDamageItem->GetPiercingPower();
		damage.nPiercingRandom = pDamageItem->GetRandomPiercing();
		damage.fDamagePower = pDamageItem->GetDamagePower();
		damage.nDamageRandom = pDamageItem->GetRandomDamage();
		damage.fArea = pDamageItem->GetDamageArea();
		damage.fArea2 = pDamageItem->GetDamageArea2();
		damage.fSpeed = pDamageItem->GetSpeed();
		damage.fDetonationPower = pDamageItem->GetDetonationPower();
		damage.fFireRate = pDamageItem->GetRateOfFire();
		damage.fRelaxTime = pDamageItem->GetRelaxTime();
		damage.eDamageType = pDamageItem->GetDamageType();
		damage.fTraceProbability = pDamageItem->GetTraceProbability();
		damage.fTraceSpeedCoeff = pDamageItem->GetTraceSpeed();
		damage.fBrokeTrackProbability = pDamageItem->GetBreakTrackProbability();
		
		CTreeItem *pCratersItem = pDamageItem->GetChildItem( E_WEAPON_CRATERS_ITEM );
		for ( CTreeItem::CTreeItemList::const_iterator in=pCratersItem->GetBegin(); in!=pCratersItem->GetEnd(); ++in )
		{
			CWeaponCraterPropsItem *pProps = static_cast<CWeaponCraterPropsItem *> ( in->GetPtr() );
			damage.szCraters.push_back( pProps->GetCraterFileName() );
		}

		if ( pDamageItem->GetTrackDamageFlag() )
			damage.specials.SetData( 0 );
		else
			damage.specials.RemoveData( 0 );

		CWeaponEffectsItem *pEffects = static_cast<CWeaponEffectsItem *> ( pDamageItem->GetChildItem( E_WEAPON_EFFECTS_ITEM ) );
		NI_ASSERT( pEffects != 0 );
		damage.szFireSound = pEffects->GetHumanFireSound();
		damage.szEffectGunFire = pEffects->GetEffectGunFire();
		damage.szEffectTrajectory = pEffects->GetEffectTrajectory();
		damage.szEffectHitDirect = pEffects->GetEffectHitDirect();
		damage.szEffectHitMiss = pEffects->GetEffectHitMiss();
		damage.szEffectHitReflect = pEffects->GetEffectHitReflect();
		damage.szEffectHitGround = pEffects->GetEffectHitGround();
		damage.szEffectHitWater = pEffects->GetEffectHitWater();
		damage.szEffectHitAir = pEffects->GetEffectHitAir();

		for ( int i=0; i<2; i++ )
		{
			SFlashEffect *pEffect = 0;
			if ( i == 0 )
				pEffect = &damage.flashFire;
			else if ( i == 1 )
				pEffect = &damage.flashExplosion;

			CWeaponFlashPropsItem *pFlashProps = static_cast<CWeaponFlashPropsItem *>( pDamageItem->GetChildItem( E_WEAPON_FLASH_PROPS_ITEM, i ) );
			pEffect->nPower = pFlashProps->GetFlashPower();
			pEffect->nDuration = pFlashProps->GetFlashDuration();
		}

		if ( it == pShootTypesItem->GetBegin() )
			rpgStats.shells[0] = damage;
		else
			rpgStats.shells.push_back( damage );
	}
}

void CWeaponFrame::SaveRPGStats( IDataTree *pDT, CTreeItem *pRootItem, const char *pszProjectName )
{
	NI_ASSERT( pRootItem != 0 );
	SWeaponRPGStats rpgStats;
	if ( !bNewProjectJustCreated )
		FillRPGStats( rpgStats, pRootItem );
	else
		GetRPGStats( rpgStats, pRootItem );

	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
}

void CWeaponFrame::GetRPGStats( const SWeaponRPGStats &rpgStats, CTreeItem *pRootItem )
{
	CWeaponCommonPropsItem *pCommonProps = static_cast<CWeaponCommonPropsItem *> ( pRootItem->GetChildItem( E_WEAPON_COMMON_PROPS_ITEM ) );
	pCommonProps->SetWeaponName( rpgStats.szKeyName.c_str() );
	pCommonProps->SetDeltaAngle( rpgStats.wDeltaAngle );
	pCommonProps->SetAmmoPerShoot( rpgStats.nAmmoPerBurst );
	pCommonProps->SetDispersion( rpgStats.fDispersion );
	pCommonProps->SetRangeMin( rpgStats.fRangeMin );
	pCommonProps->SetRangeMax( rpgStats.fRangeMax );
	pCommonProps->SetCeiling( rpgStats.nCeiling );
	pCommonProps->SetAimingTime( rpgStats.fAimingTime );
	pCommonProps->SetRevealRadius( rpgStats.fRevealRadius );
	
	CTreeItem *pShootTypesItem = pRootItem->GetChildItem( E_WEAPON_SHOOT_TYPES_ITEM );
	NI_ASSERT( rpgStats.shells.size() >= pShootTypesItem->GetChildsCount() );
	CTreeItem::CTreeItemList::const_iterator it = pShootTypesItem->GetBegin();
	
	int k = 0;
	for ( ; it!=pShootTypesItem->GetEnd(); ++it )
	{
		const SWeaponRPGStats::SShell &damage = rpgStats.shells[k];
		CWeaponDamagePropsItem *pDamageItem = (CWeaponDamagePropsItem *) it->GetPtr();
		
		pDamageItem->SetTrajectoryType( damage.trajectory );
		pDamageItem->SetPiercingPower( damage.nPiercing );
		pDamageItem->SetRandomPiercing( damage.nPiercingRandom );
		pDamageItem->SetDamagePower( damage.fDamagePower );
		pDamageItem->SetRandomDamage( damage.nDamageRandom );
		pDamageItem->SetDamageArea( damage.fArea );
		pDamageItem->SetDamageArea2( damage.fArea2 );
		pDamageItem->SetSpeed( damage.fSpeed );
		pDamageItem->SetDetonationPower( damage.fDetonationPower );
		pDamageItem->SetTrackDamageFlag( damage.specials.GetData( 0 ) );
		pDamageItem->SetRateOfFire( damage.fFireRate );
		pDamageItem->SetRelaxTime( damage.fRelaxTime );
		pDamageItem->SetDamageType( damage.eDamageType );
		pDamageItem->SetTraceProbability( damage.fTraceProbability );
		pDamageItem->SetTraceSpeed( damage.fTraceSpeedCoeff );
		pDamageItem->SetBreakTrackProbability( damage.fBrokeTrackProbability );
		
		CTreeItem *pCratersItem = pDamageItem->GetChildItem( E_WEAPON_CRATERS_ITEM );
		pCratersItem->RemoveAllChilds();
		for ( int i=0; i<damage.szCraters.size(); i++ )
		{
			CWeaponCraterPropsItem *pProps = new CWeaponCraterPropsItem;
			std::string szTemp = damage.szCraters[i].c_str();
			pProps->SetItemName( damage.szCraters[i].c_str() );
			pProps->SetCraterFileName( damage.szCraters[i].c_str() );
			pCratersItem->AddChild( pProps );
		}
		
		CWeaponEffectsItem *pEffects = static_cast<CWeaponEffectsItem *> ( pDamageItem->GetChildItem( E_WEAPON_EFFECTS_ITEM ) );
		NI_ASSERT( pEffects != 0 );
		pEffects->SetHumanFireSound( damage.szFireSound.c_str() );
		pEffects->SetEffectGunFire( damage.szEffectGunFire.c_str() );
		pEffects->SetEffectTrajectory( damage.szEffectTrajectory.c_str() );
		pEffects->SetEffectHitDirect( damage.szEffectHitDirect.c_str() );
		pEffects->SetEffectHitMiss( damage.szEffectHitMiss.c_str() );
		pEffects->SetEffectHitReflect( damage.szEffectHitReflect.c_str() );
		pEffects->SetEffectHitGround( damage.szEffectHitGround.c_str() );
		pEffects->SetEffectHitWater( damage.szEffectHitWater.c_str() );
		pEffects->SetEffectHitAir( damage.szEffectHitAir.c_str() );

		for ( int i=0; i<2; i++ )
		{
			const SFlashEffect *pEffect = 0;
			if ( i == 0 )
				pEffect = &damage.flashFire;
			else if ( i == 1 )
				pEffect = &damage.flashExplosion;
			
			CWeaponFlashPropsItem *pFlashProps = static_cast<CWeaponFlashPropsItem *>( pDamageItem->GetChildItem( E_WEAPON_FLASH_PROPS_ITEM, i ) );
			pFlashProps->SetFlashPower( pEffect->nPower );
			pFlashProps->SetFlashDuration( pEffect->nDuration );
		}
		
		k++;
	}
}

void CWeaponFrame::LoadRPGStats( IDataTree *pDT, CTreeItem *pRootItem )
{
	NI_ASSERT( pRootItem != 0 );
	SWeaponRPGStats rpgStats;
	
	CTreeAccessor tree = pDT;
	tree.Add( "RPG", &rpgStats );
	GetRPGStats( rpgStats, pRootItem );
}

FILETIME CWeaponFrame::FindMinimalExportFileTime( const char *pszResultFileName, CTreeItem *pRootItem )
{
	FILETIME minTime;
	minTime = GetFileChangeTime( pszResultFileName );
	return minTime;
}
