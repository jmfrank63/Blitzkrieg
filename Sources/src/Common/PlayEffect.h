#ifndef __PLAY_EFFECT_H__
#define __PLAY_EFFECT_H__
#pragma ONCE
#include "Season.h"
inline const std::string* GetHitEffect( const SAINotifyHitInfo &hit, const SWeaponRPGStats::SShell &shell )
{
	switch ( hit.eHitType ) 
	{
		case SAINotifyHitInfo::EHT_HIT:			// direct hit
 			return &shell.szEffectHitDirect;
		case SAINotifyHitInfo::EHT_MISS:		// visual hit, but RPG miss
			return &shell.szEffectHitMiss;
		case SAINotifyHitInfo::EHT_REFLECT:	// visual hit, RPG hit, but absorbed
			return &shell.szEffectHitReflect;
	}
	return 0;
}
inline IEffectVisObj* PlayEffect( const std::string &szEffect, const CVec3 &vPos, 
																  const NTimer::STime &currTime, bool bOutbound, IScene *pScene, IVisObjBuilder *pVOB,
																	const NTimer::STime &timeAfterStart = 0, ESoundMixType eMixType = SFX_MIX_IF_TIME_EQUALS, 
																	ESoundAddMode eAddType = SAM_ADD_N_FORGET, ESoundCombatType eCombatType = ESCT_ASK_RPG )
{
	if ( szEffect.empty() )
		return 0;
	if ( IVisObj *pObj = pVOB->BuildObject(("effects\\effects\\" + szEffect).c_str(), 0, SGVOT_EFFECT) ) 
	{
		static_cast<IEffectVisObj*>(pObj)->SetStartTime( currTime );
		pObj->SetPlacement( vPos, 0 );
		const std::string &szSoundEffect = static_cast<IEffectVisObj*>(pObj)->GetSoundEffect();
		if ( !szSoundEffect.empty() ) 
			pScene->AddSound( szSoundEffect.c_str(), vPos, eMixType, eAddType, eCombatType, 1, 100, timeAfterStart );
		if ( bOutbound ) 
		{
			if ( pScene->AddOutboundObject(pObj, SGVOGT_EFFECT) == false )
				return 0;
		}
		else if ( pScene->AddObject(pObj, SGVOGT_EFFECT, 0) == false )
			return 0;
		return static_cast<IEffectVisObj*>( pObj );
	}
	return 0;
}
inline void SetCraterEffect( const std::string &szCreater, int nSeason, const CVec3 &vPos, int nPriority, IScene *pScene, IVisObjBuilder *pVOB )
{
	const std::string szName = szCreater + GetSeasonApp2( nSeason );
	IVisObj *pObj = pVOB->BuildObject( szName.c_str(), 0, SGVOT_SPRITE );
	if ( pObj == 0 ) 
		pObj = pVOB->BuildObject( szCreater.c_str(), 0, SGVOT_SPRITE );
	NI_ASSERT_SLOW_T( pObj != 0, NStr::Format("Can't create crater picture \"%s\"", szCreater.c_str()) );
	pObj->SetPlacement( vPos, 0 );
	checked_cast<IObjVisObj*>(pObj)->SetPriority( nPriority );
	pScene->AddCraterObject( pObj, SGVOGT_TERRAOBJ );
}
inline void SetFlashEffect( const SFlashEffect &flash, const NTimer::STime &currTime, const CVec3 &vPos, 
													  const DWORD dwColor, IScene *pScene, IVisObjBuilder *pVOB )
{
	NI_ASSERT_SLOW_T( flash.HasFlash(), "Empty flash passed" );
	const char *pszName = "effects\\sprites\\flash\\1";
	IVisObj *pObj = pVOB->BuildObject( pszName, 0, SGVOT_FLASH );
	NI_ASSERT_SLOW_T( pObj != 0, NStr::Format("Can't create flash \"%s\"", pszName) );
	pObj->SetPosition( vPos );
	static_cast<IFlashVisObj*>(pObj)->Setup( currTime, flash.nDuration, flash.nPower, dwColor );
	pScene->AddObject( pObj, SGVOGT_FLASH, 0 );
}
inline void MakeMatrixFromDirection( SHMatrix *pMatrix, const float fHDirection, const float fVDirection = 0 )
{
	CQuat quat = CQuat( -(FP_PI2 - fVDirection), V3_AXIS_X );
	quat *= CQuat( fHDirection, V3_AXIS_Z );
	pMatrix->Set( quat );
}
#endif // __PLAY_EFFECT_H__