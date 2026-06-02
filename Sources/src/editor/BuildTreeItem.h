#ifndef __BUILDING_TREE_ITEM_H__
#define __BUILDING_TREE_ITEM_H__

#include "TreeItem.h"

class CBuildingTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingTreeRootItem );
public:
	CBuildingTreeRootItem() { bStaticElements = true; nItemType = E_BUILDING_ROOT_ITEM; InitDefaultValues(); }
	~CBuildingTreeRootItem() {}

	void ComposeAnimations( const char *pszProjectFileName, const char *pszResultingDir, const CVec2 &zeroPos, const CArray2D<BYTE> &pass, const CVec2 &vOrigin );
	FILETIME FindMaximalSourceTime( const char *pszProjectFileName );

	virtual void InitDefaultValues();
};

class CBuildingCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingCommonPropsItem );
public:
	CBuildingCommonPropsItem() { nItemType = E_BUILDING_COMMON_PROPS_ITEM; nImageIndex = 0; InitDefaultValues(); }
	~CBuildingCommonPropsItem() {};
	
	const char *GetName() { return values[0].value; }
	int GetBuildingType();
	int GetHealth() { return values[2].value; }
	float GetRepairCost() { return values[3].value; }
	int GetNumberOfRestSlots() { return values[4].value; }
	int GetNumberOfMedicalSlots() { return values[5].value; }
	const char *GetAmbientSound() { return values[6].value; }
	const char *GetCycledSound() { return values[7].value; }

	void SetName( const char *pszName ) { values[0].value = pszName; }
	void SetBuildingType( int nVal );
	void SetHealth( int nVal ) { values[2].value = nVal; }
	void SetRepairCost( float fVal ) { values[3].value = fVal; }
	void SetNumberOfRestSlots( int nVal ) { values[4].value = nVal; }
	void SetNumberOfMedicalSlots( int nVal ) { values[5].value = nVal; }
	void SetAmbientSound( const char *pszVal ) { values[6].value = pszVal; }
	void SetCycledSound( const char *pszVal ) { values[7].value = pszVal; }
	
	virtual void InitDefaultValues();
};

class CBuildingPassesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingPassesItem );
public:
	CBuildingPassesItem() { bStaticElements = false; nItemType = E_BUILDING_PASSES_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CBuildingPassesItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
};

class CBuildingPassPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingPassPropsItem );
public:
	CBuildingPassPropsItem() { bStaticElements = true; nItemType = E_BUILDING_PASS_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CBuildingPassPropsItem() {};
	
	int GetPassAIClass();
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CBuildingEntrancesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingEntrancesItem );
public:
	CBuildingEntrancesItem() { nItemType = E_BUILDING_ENTRANCES_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CBuildingEntrancesItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingEntrancePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingEntrancePropsItem );
public:
	CBuildingEntrancePropsItem() { bStaticElements = true; nItemType = E_BUILDING_ENTRANCE_PROPS_ITEM; nImageIndex = 4; InitDefaultValues(); }
	~CBuildingEntrancePropsItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingSlotsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingSlotsItem );
public:
	CBuildingSlotsItem() { nItemType = E_BUILDING_SLOTS_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CBuildingSlotsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
};

class CBuildingSlotPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingSlotPropsItem );
public:
	CBuildingSlotPropsItem() { bStaticElements = true; nItemType = E_BUILDING_SLOT_PROPS_ITEM; nImageIndex = 0; InitDefaultValues(); }
	~CBuildingSlotPropsItem() {};

	float GetConeDirection() { return values[0].value; }
	float GetConeAngle() { return values[1].value; }
	float GetSightMultiplier() { return values[2].value; }
	float GetCover() { return values[3].value; }
	const char *GetWeaponName() { return values[4].value; }
	int GetAmmo() { return values[5].value; }
	float GetRotationSpeed() { return values[6].value; }
	int GetPriority() { return values[7].value; }
	
	void SetConeDirection( float fVal ) { values[0].value = fVal; }
	void SetConeAngle( float fVal ) { values[1].value = fVal; }
	void SetSightMultiplier( float fVal ) { values[2].value = fVal; }
	void SetCover( float fVal ) { values[3].value = fVal; }
	void SetWeaponName( const char *pszVal ) { values[4].value = pszVal; }
	void SetAmmo( int nVal ) { values[5].value = nVal; }
	void SetRotationSpeed( float fVal ) { values[6].value = fVal; }
	void SetPriority( int nVal ) { values[7].value = nVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyKeyDown( int nChar );
};

class CBuildingFirePointsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingFirePointsItem );
public:
	CBuildingFirePointsItem() { nItemType = E_BUILDING_FIRE_POINTS_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CBuildingFirePointsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
};

class CBuildingFirePointPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingFirePointPropsItem );
public:
	CBuildingFirePointPropsItem() { bStaticElements = true; nItemType = E_BUILDING_FIRE_POINT_PROPS_ITEM; nImageIndex = 0; InitDefaultValues(); }
	~CBuildingFirePointPropsItem() {};
	
	float GetDirection() { return values[0].value; }
	const char *GetEffectName() { return values[1].value; }
	float GetVerticalAngle() { return values[2].value; }
	
	void SetDirection( float fVal ) { values[0].value = fVal; }
	void SetEffectName( const char *pszVal ) { values[1].value = pszVal; }
	void SetVerticalAngle( float fVal ) { values[2].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyKeyDown( int nChar );
};

class CBuildingGraphicsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingGraphicsItem );
public:
	CBuildingGraphicsItem() { bStaticElements = true; nItemType = E_BUILDING_GRAPHICS_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CBuildingGraphicsItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingSummerPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingSummerPropsItem );
public:
	CBuildingSummerPropsItem() { bStaticElements = true; nItemType = E_BUILDING_SUMMER_PROPS_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CBuildingSummerPropsItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingWinterPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingWinterPropsItem );
public:
	CBuildingWinterPropsItem() { bStaticElements = true; nItemType = E_BUILDING_WINTER_PROPS_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CBuildingWinterPropsItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingGraphicPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingGraphicPropsItem );
public:
	CBuildingGraphicPropsItem() { bStaticElements = true; nItemType = E_BUILDING_GRAPHIC1_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CBuildingGraphicPropsItem() {};
	
	const char *GetFileName() { return values[0].value; }
	const char *GetShadowFileName() { return values[1].value; }
	const char *GetNoiseFileName() { return values[2].value; }
	
	void SetFileName( const char *pszFileName ) { values[0].value = pszFileName; }
	void SetShadowName( const char *pszFileName ) { values[1].value = pszFileName; }
	void SetNoiseFileName( const char *pszFileName ) { values[2].value = pszFileName; }
	
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
};

class CBuildingGraphic1PropsItem : public CBuildingGraphicPropsItem
{
	OBJECT_NORMAL_METHODS( CBuildingGraphic1PropsItem );
public:
	CBuildingGraphic1PropsItem() { nItemType = E_BUILDING_GRAPHIC1_PROPS_ITEM; InitDefaultValues(); }
	~CBuildingGraphic1PropsItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingGraphic2PropsItem : public CBuildingGraphicPropsItem
{
	OBJECT_NORMAL_METHODS( CBuildingGraphic2PropsItem );
public:
	CBuildingGraphic2PropsItem() { nItemType = E_BUILDING_GRAPHIC2_PROPS_ITEM; InitDefaultValues(); }
	~CBuildingGraphic2PropsItem() {};

	virtual void InitDefaultValues();
};

class CBuildingGraphic3PropsItem : public CBuildingGraphicPropsItem
{
	OBJECT_NORMAL_METHODS( CBuildingGraphic3PropsItem );
public:
	CBuildingGraphic3PropsItem() { nItemType = E_BUILDING_GRAPHIC3_PROPS_ITEM; InitDefaultValues(); }
	~CBuildingGraphic3PropsItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingGraphicW1PropsItem : public CBuildingGraphicPropsItem
{
	OBJECT_NORMAL_METHODS( CBuildingGraphicW1PropsItem );
public:
	CBuildingGraphicW1PropsItem() { nItemType = E_BUILDING_GRAPHICW1_PROPS_ITEM; InitDefaultValues(); }
	~CBuildingGraphicW1PropsItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingGraphicW2PropsItem : public CBuildingGraphicPropsItem
{
	OBJECT_NORMAL_METHODS( CBuildingGraphicW2PropsItem );
public:
	CBuildingGraphicW2PropsItem() { nItemType = E_BUILDING_GRAPHICW2_PROPS_ITEM; InitDefaultValues(); }
	~CBuildingGraphicW2PropsItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingGraphicW3PropsItem : public CBuildingGraphicPropsItem
{
	OBJECT_NORMAL_METHODS( CBuildingGraphicW3PropsItem );
public:
	CBuildingGraphicW3PropsItem() { nItemType = E_BUILDING_GRAPHICW3_PROPS_ITEM; InitDefaultValues(); }
	~CBuildingGraphicW3PropsItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingDefencesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingDefencesItem );
public:
	CBuildingDefencesItem() { bStaticElements = true; nItemType = E_BUILDING_DEFENCES_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CBuildingDefencesItem() {};
	
	virtual void InitDefaultValues();
};

class CBuildingDefencePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingDefencePropsItem );
public:
	CBuildingDefencePropsItem() { nItemType = E_BUILDING_DEFENCE_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CBuildingDefencePropsItem() {};
	
	int GetMinArmor() { return values[0].value; }
	int GetMaxArmor() { return values[1].value; }
	float GetSilhouette() { return values[2].value; }
	
	void SetMinArmor( int nVal ) { values[0].value = nVal; }
	void SetMaxArmor( int nVal ) { values[1].value = nVal; }
	void SetSilhouette( float fVal ) { values[2].value = fVal; }
	
	virtual void InitDefaultValues();
};

class CBuildingDirExplosionsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingDirExplosionsItem );
public:
	CBuildingDirExplosionsItem() { bStaticElements = true; nItemType = E_BUILDING_DIR_EXPLOSIONS_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CBuildingDirExplosionsItem() {};

	const char *GetEffectName() { return values[0].value; }
	
	void SetEffectName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
};

class CBuildingDirExplosionPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingDirExplosionPropsItem );
public:
	CPtr<IObjVisObj> pSprite;
	CPtr<IObjVisObj> pHLine;

public:
	CBuildingDirExplosionPropsItem() { bStaticElements = true; nItemType = E_BUILDING_DIR_EXPLOSION_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CBuildingDirExplosionPropsItem() {};
	
	float GetDirection() { return values[0].value; }
	float GetVerticalAngle() { return values[1].value; }
	
	void SetDirection( float fVal ) { values[0].value = fVal; }
	void SetVerticalAngle( float fVal ) { values[1].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
};

class CBuildingSmokesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingSmokesItem );
public:
	CBuildingSmokesItem() { nItemType = E_BUILDING_SMOKES_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CBuildingSmokesItem() {};
	
	const char *GetEffectName() { return values[0].value; }
	
	void SetEffectName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
};

class CBuildingSmokePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBuildingSmokePropsItem );
public:
	CPtr<IObjVisObj> pSprite;
	CPtr<IObjVisObj> pHLine;
	
public:
	CBuildingSmokePropsItem() { bStaticElements = true; nItemType = E_BUILDING_SMOKE_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CBuildingSmokePropsItem() {};
	
	float GetDirection() { return values[0].value; }
	float GetVerticalAngle() { return values[1].value; }
	
	void SetDirection( float fVal ) { values[0].value = fVal; }
	void SetVerticalAngle( float fVal ) { values[1].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyKeyDown( int nChar );
};

#endif		//__BUILDING_TREE_ITEM_H__