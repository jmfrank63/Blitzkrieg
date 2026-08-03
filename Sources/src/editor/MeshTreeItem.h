#ifndef __MESH_TREE_ITEM_H__
#define __MESH_TREE_ITEM_H__

#include "../GFX/GFX.h"
#include "../GFX/GFXHelper.h"
#include "../Main/rpgstats.h"
#include "TreeItem.h"

class CMeshTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshTreeRootItem );
public:
	CMeshTreeRootItem() { bStaticElements = true; nItemType = E_MESH_ROOT_ITEM; InitDefaultValues(); }
	~CMeshTreeRootItem() {}

	virtual void InitDefaultValues();
};

class CMeshCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshCommonPropsItem );
public:
	CMeshCommonPropsItem() { bStaticElements = true; nItemType = E_MESH_COMMON_PROPS_ITEM; nImageIndex = 5; InitDefaultValues(); }
	~CMeshCommonPropsItem() {};

	const char *GetMeshName() { return values[0].value; }
	EUnitRPGType GetMeshType();
	int GetAIClass();
	const char *GetPicture() { return values[3].value; }
	float GetHealth() { return values[4].value; }
	float GetRepairCost() { return values[5].value; }
	float GetCamouflage() { return values[6].value; }
	float GetSpeed() { return values[7].value; }
	float GetPassability() { return values[8].value; }
	float GetPullingPower() { return values[9].value; }
	float GetUninstallRotateTime() { return values[10].value; }
	float GetUninstallTransportTime() { return values[11].value; }
	float GetWeight() { return values[12].value; }
	int GetCrew() { return values[13].value; }
	int GetPassangers() { return values[14].value; }
	int GetPriority() { return values[15].value; }
	float GetRotateSpeed() { return values[16].value; }
	int GetBoundTileRadius() { return values[17].value; }
	float GetTurnRadius() { return values[18].value; }
	float GetSilhouette() { return values[19].value; }
	float GetAIPrice() { return values[20].value; }
	float GetSight() { return values[21].value; }
	float GetSightPower() { return values[22].value; }
	
/*
	const char* GetEffectFatalitySmokeName() { return values[23].value; }
	const char* GetEffectFatalitySoundName() { return values[24].value; }
	float GetEffectFatalitySoundMinDist() { return values[25].value; }
	float GetEffectFatalitySoundMaxDist() { return values[26].value; }
	int GetMoveCycleSoundStart() { return values[27].value; }
	int GetMoveCycleSoundStop() { return values[28].value; }
	const char* GetMoveSound() { return values[29].value; }
	const char* GetStopSound() { return values[30].value; }
*/
	
	void SetMeshName( const char *pszName ) { values[0].value = pszName; }
	void SetMeshType( int nType );
	void SetAIClass( int nVal );
	void SetPicture( const char *pszName ) { values[3].value = pszName; }
	void SetHealth( float fVal ) { values[4].value = fVal; }
	void SetRepairCost( float fVal ) { values[5].value = fVal; }
	void SetCamouflage( float fVal ) { values[6].value = fVal; }
	void SetSpeed( float fVal ) { values[7].value = fVal; }
	void SetPassability( float fVal ) { values[8].value = fVal; }
	void SetPullingPower( float fVal ) { values[9].value = fVal; }
	void SetUninstallRotateTime( float fVal ) { values[10].value = fVal; }
	void SetUninstallTransportTime( float fVal ) { values[11].value = fVal; }
	void SetWeight( float fVal ) { values[12].value = fVal; }
	void SetCrew( int nVal ) { values[13].value = nVal; }
	void SetPassangers( int nVal ) { values[14].value = nVal; }
	void SetPriority( int nVal ) { values[15].value = nVal; }
	void SetRotateSpeed( float fVal ) { values[16].value = fVal; }
	void SetBoundTileRadius( int nVal ) { values[17].value = nVal; }
	void SetTurnRadius( float fVal ) { values[18].value = fVal; }
	void SetSilhouette( float fVal ) { values[19].value = fVal; }
	void SetAIPrice( float fVal ) { values[20].value = fVal; }
	void SetSight( float fVal ) { values[21].value = fVal; }
	void SetSightPower( float fVal ) { values[22].value = fVal; }
		
/*
	void SetEffectFatalitySmokeName( const char *pszName ) { values[23].value = pszName; }
	void SetEffectFatalitySoundName( const char *pszName ) { values[24].value = pszName; }
	void SetEffectFatalitySoundMinDist( float fVal ) { values[25].value = fVal; }
	void SetEffectFatalitySoundMaxDist( float fVal ) { values[26].value = fVal; }
	void SetMoveCycleSoundStart( int nVal ) { values[27].value = nVal; }
	void SetMoveCycleSoundStop( int nVal ) { values[28].value = nVal; }
	void SetMoveSound( const char *pszName ) { values[29].value = pszName; }
	void SetStopSound( const char *pszName ) { values[30].value = pszName; }
*/

	virtual void InitDefaultValues();
};


class CMeshEffectsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshEffectsItem );
public:
	CMeshEffectsItem() { bStaticElements = true; nItemType = E_MESH_EFFECTS_ITEM; InitDefaultValues(); nImageIndex = 1; }
	~CMeshEffectsItem() {};

	const char* GetEffectDieselName() { return values[0].value; }
	const char* GetEffectSmokeName() { return values[1].value; }
	const char* GetEffectWheelDustName() { return values[2].value; }
	const char* GetEffectShootDustName() { return values[3].value; }
	const char* GetEffectFatalityName() { return values[4].value; }
	const char* GetEffectDisappearName() { return values[5].value; }
	
	const char* GetSoundStart() { return values[6].value; }
	const char* GetSoundCycle() { return values[7].value; }
	const char* GetSoundStop() { return values[8].value; }
	
	void SetEffectDieselName( const char *pszName ) { values[0].value = pszName; }
	void SetEffectSmokeName( const char *pszName ) { values[1].value = pszName; }
	void SetEffectWheelDustName( const char *pszName ) { values[2].value = pszName; }
	void SetEffectShootDustName( const char *pszName ) { values[3].value = pszName; }
	void SetEffectFatalityName( const char *pszName ) { values[4].value = pszName; }
	void SetEffectDisappearName( const char *pszName ) { values[5].value = pszName; }
	
	void SetSoundStart( const char *pszName ) { values[6].value = pszName; }
	void SetSoundCycle( const char *pszName ) { values[7].value = pszName; }
	void SetSoundStop( const char *pszName ) { values[8].value = pszName; }
	
	virtual void InitDefaultValues();
};

class CMeshSoundPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshSoundPropsItem );
public:
	CMeshSoundPropsItem() { nItemType = E_MESH_SOUND_PROPS_ITEM; nImageIndex = 0; InitDefaultValues(); }
	~CMeshSoundPropsItem() {};
	
	const char *GetSoundName() { return values[0].value; }
	float GetMinDistance() { return values[1].value; }
	float GetMaxDistance() { return values[2].value; }

	void SetSoundName( const char *pszVal ) { values[0].value = pszVal; }
	void SetMinDistance( float fVal ) { values[1].value = fVal; }
	void SetMaxDistance( float fVal ) { values[2].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CMeshAviaItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshAviaItem );
public:
	CMeshAviaItem() { nItemType = E_MESH_AVIA_ITEM; nImageIndex = 0; InitDefaultValues(); }
	~CMeshAviaItem() {};

	float GetMaxHeight() { return values[0].value; }
	float GetDivingAngle() { return values[1].value; }
	float GetClimbAngle() { return values[2].value; }
	float GetTiltAngle() { return values[3].value; }
	float GetTiltRatio() { return values[4].value; }
	
	void SetMaxHeight( float fVal ) { values[0].value = fVal; }
	void SetDivingAngle( float fVal ) { values[1].value = fVal; }
	void SetClimbAngle( float fVal ) { values[2].value = fVal; }
	void SetTiltAngle( float fVal ) { values[3].value = fVal; }
	void SetTiltRatio( float fVal ) { values[4].value = fVal; }
	
	virtual void InitDefaultValues();
};

class CMeshTrackItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshTrackItem );
public:
	CMeshTrackItem() { nItemType = E_MESH_TRACK_ITEM; nImageIndex = 0; InitDefaultValues(); }
	~CMeshTrackItem() {};
	
	bool GetLeaveTrackFlag() { return values[0].value; }
	float GetTrackWidth() { return values[1].value; }
	float GetTrackOffset() { return values[2].value; }
	float GetTrackStart() { return values[3].value; }
	float GetTrackEnd() { return values[4].value; }
	float GetTrackIntensity() { return values[5].value; }
	int GetTrackLifeTime() { return values[6].value; }
	
	void SetLeaveTrackFlag( bool bVal ) { values[0].value = bVal; }
	void SetTrackWidth( float fVal ) { values[1].value = fVal; }
	void SetTrackOffset( float fVal ) { values[2].value = fVal; }
	void SetTrackStart( float fVal ) { values[3].value = fVal; }
	void SetTrackEnd( float fVal ) { values[4].value = fVal; }
	void SetTrackIntensity( float fVal ) { values[5].value = fVal; }
	void SetTrackLifeTime( int nVal ) { values[6].value = nVal; }
	
	virtual void InitDefaultValues();
};

class CMeshDefencesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshDefencesItem );
public:
	CMeshDefencesItem() { bStaticElements = true; nItemType = E_MESH_DEFENCES_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CMeshDefencesItem() {};
	
	virtual void InitDefaultValues();
};

class CMeshDefencePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshDefencePropsItem );
public:
	CMeshDefencePropsItem() { nItemType = E_MESH_DEFENCE_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CMeshDefencePropsItem() {};
	
	int GetMinArmor() { return values[0].value; }
	int GetMaxArmor() { return values[1].value; }
	
	void SetMinArmor( int nVal ) { values[0].value = nVal; }
	void SetMaxArmor( int nVal ) { values[1].value = nVal; }
	
	virtual void InitDefaultValues();
};

class CMeshJoggingsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshJoggingsItem );
public:
	CMeshJoggingsItem() { bStaticElements = true; nItemType = E_MESH_JOGGINGS_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CMeshJoggingsItem() {};
	
	virtual void InitDefaultValues();
};

class CMeshJoggingPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshJoggingPropsItem );
public:
	CMeshJoggingPropsItem() { nItemType = E_MESH_JOGGING_PROPS_ITEM; nImageIndex = 0; InitDefaultValues(); }
	~CMeshJoggingPropsItem() {};
	
	float GetPeriod1() { return values[0].value; }
	float GetPeriod2() { return values[1].value; }
	float GetAmplitude1() { return values[2].value; }
	float GetAmplitude2() { return values[3].value; }
	float GetPhase1() { return values[4].value; }
	float GetPhase2() { return values[5].value; }
	
	void SetPeriod1( float fVal ) { values[0].value = fVal; }
	void SetPeriod2( float fVal ) { values[1].value = fVal; }
	void SetAmplitude1( float fVal ) { values[2].value = fVal; }
	void SetAmplitude2( float fVal ) { values[3].value = fVal; }
	void SetPhase1( float fVal ) { values[4].value = fVal; }
	void SetPhase2( float fVal ) { values[5].value = fVal; }
	
	virtual void InitDefaultValues();
};

class CMeshPlatformsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshPlatformsItem );
public:
	CMeshPlatformsItem() { bStaticElements = false; nItemType = E_MESH_PLATFORMS_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CMeshPlatformsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class CMeshPlatformPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshPlatformPropsItem );
public:
	CMeshPlatformPropsItem() { bStaticElements = true; nItemType = E_MESH_PLATFORM_PROPS_ITEM; nImageIndex = 5; InitDefaultValues(); }
	~CMeshPlatformPropsItem() {};
	
	const char *GetPlatformPartName() { return values[0].value; }
	const char *GetGunCarriageName1() { return values[1].value; }
	const char *GetGunCarriageName2() { return values[2].value; }
	float GetVerticalRotationSpeed() { return values[3].value; }
	float GetHorizontalRotationSpeed() { return values[4].value; }
	const char *GetRotationSound() { return values[5].value; }
	
	void SetVerticalRotationSpeed( float val ) { values[3].value = val; }
	void SetHorizontalRotationSpeed( float val ) { values[4].value = val; }
	void SetRotationSound( const char *pszVal ) { values[5].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyRButtonClick();
};

class CMeshGunsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshGunsItem );
public:
	CMeshGunsItem() { nItemType = E_MESH_GUNS_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CMeshGunsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class CMeshGunPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshGunPropsItem );
public:
	CMeshGunPropsItem() { nItemType = E_MESH_GUN_PROPS_ITEM; nImageIndex = 0; InitDefaultValues(); }
	~CMeshGunPropsItem() {};
	
	const char *GetShootPointName() { return values[0].value; }
	const char *GetShootPartName() { return values[1].value; }
	const char *GetWeaponName() { return values[2].value; }
	int GetPriority() { return values[3].value; }
	bool GetRecoilFlag() { return values[4].value; }
	int GetRecoilTime() { return values[5].value; }
	int GetRecoilShakeTime() { return values[6].value; }
	float GetRecoilShakeAngle() { return values[7].value; }
	int GetAmmoCount() { return values[8].value; }
	float GetReloadCost() { return values[9].value; }
	
	void SetWeaponName( const char *pszName ) { values[2].value = pszName; }
	void SetPriority( int nVal ) { values[3].value = nVal; }
	void SetRecoilFlag( bool bFlag ) { values[4].value = bFlag; }
	void SetRecoilTime( int nVal ) { values[5].value = nVal; }
	void SetRecoilShakeTime( int nVal ) { values[6].value = nVal; }
	void SetRecoilShakeAngle( float fVal ) { values[7].value = fVal; }
	void SetAmmoCount( int nVal ) { values[8].value = nVal; }
	void SetReloadCost( float fVal ) { values[9].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CMeshGraphicsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshGraphicsItem );
public:
	CMeshGraphicsItem() { bStaticElements = true; nItemType = E_MESH_GRAPHICS_ITEM; nImageIndex = 5; InitDefaultValues(); }
	~CMeshGraphicsItem() {};

	const char *GetCombatMeshName() { return values[0].value; }
	const char *GetInstallMeshName() { return values[1].value; }
	const char *GetTransMeshName() { return values[2].value; }
	const char *GetAliveSummerTexture() { return values[3].value; }
	const char *GetAliveWinterTexture() { return values[4].value; }
	const char *GetAliveAfrikaTexture() { return values[5].value; }
	const char *GetDeadSummerTexture() { return values[6].value; }
	const char *GetDeadWinterTexture() { return values[7].value; }
	const char *GetDeadAfrikaTexture() { return values[8].value; }
	
	void SetCombatMeshName( const char *val ) { values[0].value = val; }
	void SetInstallMeshName( const char *val ) { values[1].value = val; }
	void SetTransMeshName( const char *val ) { values[2].value = val; }
	void SetAliveSummerTexture( const char *val ) { values[3].value = val; }
	void SetAliveWinterTexture( const char *val ) { values[4].value = val; }
	void SetAliveAfrikaTexture( const char *val ) { values[5].value = val; }
	void SetDeadSummerTexture( const char *val ) { values[6].value = val; }
	void SetDeadWinterTexture( const char *val ) { values[7].value = val; }
	void SetDeadAfrikaTexture( const char *val ) { values[8].value = val; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CMeshDeathCratersItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshDeathCratersItem );
public:
	CMeshDeathCratersItem() { nItemType = E_MESH_DEATH_CRATERS_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CMeshDeathCratersItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyRButtonClick();
};

class CMeshDeathCraterPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshDeathCraterPropsItem );
public:
	CMeshDeathCraterPropsItem() { nItemType = E_MESH_DEATH_CRATER_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CMeshDeathCraterPropsItem() {};
	
	const char *GetCraterFileName() { return values[0].value; }
	
	void SetCraterFileName( const char *pszName ) { values[0].value = pszName; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class CMeshLocatorsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshLocatorsItem );
public:
	CMeshLocatorsItem() { bStaticElements = false; nItemType = E_MESH_LOCATORS_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CMeshLocatorsItem() {};
	
	virtual void InitDefaultValues();
};

interface IObjVisObj;
interface IGFXVertices;
class CMeshLocatorPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMeshLocatorPropsItem );

public:
	CPtr<IObjVisObj> pSprite;
	SGFXLineVertex lineVertices[2];
	int nLocatorID;

public:
	CMeshLocatorPropsItem() : nLocatorID( -1 ) { bStaticElements = true;  nItemType = E_MESH_LOCATOR_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CMeshLocatorPropsItem() {};

	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
};

#endif		//__MESH_TREE_ITEM_H__
