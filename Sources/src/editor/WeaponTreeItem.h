#ifndef __WEAPON_TREE_ITEM_H__
#define __WEAPON_TREE_ITEM_H__

#include "TreeItem.h"
#include "..\Main\rpgstats.h"

class CWeaponTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponTreeRootItem );
public:
	CWeaponTreeRootItem() { bStaticElements = true; nItemType = E_WEAPON_ROOT_ITEM; InitDefaultValues(); }
	~CWeaponTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class CWeaponCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponCommonPropsItem );
public:
	CWeaponCommonPropsItem() { nItemType = E_WEAPON_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CWeaponCommonPropsItem() {};
	
	const char* GetWeaponName() { return values[0].value; }
	int GetDeltaAngle() { return values[1].value; }
	int GetAmmoPerShoot() { return values[2].value; }
	float GetAimingTime() { return values[3].value; }
	float GetDispersion() { return values[4].value; }
	float GetRangeMin() { return values[5].value; }
	float GetRangeMax() { return values[6].value; }
	int GetCeiling() { return values[7].value; }
	float GetRevealRadius() { return values[8].value; }
		
	void SetWeaponName( const char *pszName ) { values[0].value = pszName; }
	void SetDeltaAngle( int nCount ) { values[1].value = nCount; }
	void SetAmmoPerShoot( int nVal ) { values[2].value = nVal; }
	void SetAimingTime( float fVal ) { values[3].value = fVal; }
	void SetDispersion( float fVal ) { values[4].value = fVal; }
	void SetRangeMin( float fVal ) { values[5].value = fVal; }
	void SetRangeMax( float fVal ) { values[6].value = fVal; }
	void SetCeiling( int nVal ) { values[7].value = nVal; }
	void SetRevealRadius( float fVal ) { values[8].value = fVal; }
	
	virtual void InitDefaultValues();
};

class CWeaponShootTypesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponShootTypesItem );
public:
	CWeaponShootTypesItem() { nItemType = E_WEAPON_SHOOT_TYPES_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CWeaponShootTypesItem() {};

	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class CWeaponDamagePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponDamagePropsItem );
public:
	CWeaponDamagePropsItem() { bStaticElements = true; nItemType = E_WEAPON_DAMAGE_PROPS_ITEM; InitDefaultValues(); nImageIndex = 5; }
	~CWeaponDamagePropsItem() {};

	SWeaponRPGStats::SShell::ETrajectoryType GetTrajectoryType();
	int GetPiercingPower() { return values[1].value; }
	int GetRandomPiercing() { return values[2].value; }
	int GetDamagePower() { return values[3].value; }
	int GetRandomDamage() { return values[4].value; }
	float GetDamageArea() { return values[5].value; }
	float GetDamageArea2() { return values[6].value; }
	float GetSpeed() { return values[7].value; }
	bool GetTrackDamageFlag() { return values[8].value; }
	float GetDetonationPower() { return values[9].value; }
	float GetRateOfFire() { return values[10].value; }
	float GetRelaxTime() { return values[11].value; }
	SWeaponRPGStats::SShell::EDamageType GetDamageType();
	float GetTraceProbability() { return (float) values[13].value / 100.0f; }
	float GetTraceSpeed() { return values[14].value; }
	float GetBreakTrackProbability() { return values[15].value; }

	void SetTrajectoryType( int nVal );
	void SetPiercingPower( int nVal ) { values[1].value = nVal; }
	void SetRandomPiercing( int nVal ) { values[2].value = nVal; }
	void SetDamagePower( int nVal ) { values[3].value = nVal; }
	void SetRandomDamage( int nVal ) { values[4].value = nVal; }
	void SetDamageArea( float fVal ) { values[5].value = fVal; }
	void SetDamageArea2( float fVal ) { values[6].value = fVal; }
	void SetSpeed( float fVal ) { values[7].value = fVal; }
	void SetTrackDamageFlag( bool bFlag ) { values[8].value = bFlag; }
	void SetDetonationPower( float fVal ) { values[9].value = fVal; }
	void SetRateOfFire( float fVal ) { values[10].value = fVal; }
	void SetRelaxTime( float fVal ) { values[11].value = fVal; }
	void SetDamageType( int nVal );
	void SetTraceProbability( float fVal ) { values[13].value = fVal*100.0f; }
	void SetTraceSpeed( float fVal ) { values[14].value = fVal; }
	void SetBreakTrackProbability( float fVal ) { values[15].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class CWeaponEffectsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponEffectsItem );
public:
	CWeaponEffectsItem() { bStaticElements = true; nItemType = E_WEAPON_EFFECTS_ITEM; InitDefaultValues(); nImageIndex = 1; }
	~CWeaponEffectsItem() {};

	const char *GetHumanFireSound() { return values[0].value; }
	const char *GetEffectGunFire() { return values[1].value; }
	const char *GetEffectTrajectory() { return values[2].value; }
	const char *GetEffectHitDirect() { return values[3].value; }
	const char *GetEffectHitMiss() { return values[4].value; }
	const char *GetEffectHitReflect() { return values[5].value; }
	const char *GetEffectHitGround() { return values[6].value; }
	const char *GetEffectHitWater() { return values[7].value; }
	const char *GetEffectHitAir() { return values[8].value; }

	void SetHumanFireSound( const char *pszVal ) { values[0].value = pszVal; }
	void SetEffectGunFire( const char *pszVal ) { values[1].value = pszVal; }
	void SetEffectTrajectory( const char *pszVal ) { values[2].value = pszVal; }
	void SetEffectHitDirect( const char *pszVal ) { values[3].value = pszVal; }
	void SetEffectHitMiss( const char *pszVal ) { values[4].value = pszVal; }
	void SetEffectHitReflect( const char *pszVal ) { values[5].value = pszVal; }
	void SetEffectHitGround( const char *pszVal ) { values[6].value = pszVal; }
	void SetEffectHitWater( const char *pszVal ) { values[7].value = pszVal; }
	void SetEffectHitAir( const char *pszVal ) { values[8].value = pszVal; }
	
	virtual void InitDefaultValues();
};

class CWeaponSoundPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponSoundPropsItem );
public:
	CWeaponSoundPropsItem() { bStaticElements = true; nItemType = E_WEAPON_SOUND_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CWeaponSoundPropsItem() {};
	
	const char* GetShootSound() { return values[0].value; }
	const char* GetTrajectorySound() { return values[1].value; }
	const char* GetExplosionSound() { return values[2].value; }
	const char* GetMissSound() { return values[3].value; }
	const char* GetReflectSound() { return values[4].value; }
	
	void SetShootSound( const char *pszName ) { values[0].value = pszName; }
	void SetTrajectorySound( const char *pszName ) { values[1].value = pszName; }
	void SetExplosionSound( const char *pszName ) { values[2].value = pszName; }
	void SetMissSound( const char *pszName ) { values[3].value = pszName; }
	void SetReflectSound( const char *pszName ) { values[4].value = pszName; }
	
	virtual void InitDefaultValues() {}
	virtual void UpdateItemValue( int nItemId, const CVariant &value ) {}
};

class CWeaponEffectPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponEffectPropsItem );
public:
	CWeaponEffectPropsItem() { nItemType = E_WEAPON_EFFECT_PROPS_ITEM; InitDefaultValues(); nImageIndex = 5; }
	~CWeaponEffectPropsItem() {};
	
	const char *GetEffectName() { return values[0].value; }
	const char *GetSoundName() { return values[1].value; }
	float GetMinDistance() { return values[2].value; }
	float GetMaxDistance() { return values[3].value; }
	
	void SetEffectName( const char *pszVal ) { values[0].value = pszVal; }
	void SetSoundName( const char *pszVal ) { values[1].value = pszVal; }
	void SetMinDistance( float fVal ) { values[2].value = fVal; }
	void SetMaxDistance( float fVal ) { values[3].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CWeaponFlashPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponFlashPropsItem );
public:
	CWeaponFlashPropsItem() { nItemType = E_WEAPON_FLASH_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CWeaponFlashPropsItem() {};
	
	int GetFlashPower() { return values[0].value; }
	int GetFlashDuration() { return values[1].value; }
	
	void SetFlashPower( int nVal ) { values[0].value = nVal; }
	void SetFlashDuration( int nVal ) { values[1].value = nVal; }
	
	virtual void InitDefaultValues();
};

class CWeaponCratersItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponCratersItem );
public:
	CWeaponCratersItem() { nItemType = E_WEAPON_CRATERS_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CWeaponCratersItem() {};

	const char *GetDirectory() { return values[0].value; }

	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyRButtonClick();
};

class CWeaponCraterPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CWeaponCraterPropsItem );
public:
	CWeaponCraterPropsItem() { bComplexItem = true; nItemType = E_WEAPON_CRATER_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CWeaponCraterPropsItem() {};
	
	const char *GetCraterFileName() { return values[0].value; }
	
	void SetCraterFileName( const char *pszName ) { values[0].value = pszName; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

#endif		//__WEAPON_TREE_ITEM_H__
