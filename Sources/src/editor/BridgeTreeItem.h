#ifndef __BRIDGE_TREE_ITEM_H__
#define __BRIDGE_TREE_ITEM_H__

#include "../GFX/GFX.H"
#include "../Main/rpgstats.h"
#include "../Scene/scene.h"
#include "GridFrm.h"
#include "TreeItem.h"

class CBridgeTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeTreeRootItem );
public:
	CBridgeTreeRootItem() { bStaticElements = true; nItemType = E_BRIDGE_ROOT_ITEM; InitDefaultValues(); }
	~CBridgeTreeRootItem() {}
	
	virtual void InitDefaultValues();
	
private:
	bool SaveShadowFile( const string &szBridgeFileName, const string &szShadowFileName, const string &szTempShadow );
};

class CBridgeCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeCommonPropsItem );
public:
	CBridgeCommonPropsItem() { nItemType = E_BRIDGE_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CBridgeCommonPropsItem() {};
	
	const char *GetBridgeName() { return values[0].value; }
	bool GetDirection();					//if true then horizontal
	int GetHealth() { return values[2].value; }
	float GetRepairCost() { return values[3].value; }
	int GetPassForInfantry() { return values[4].value; }
	int GetPassForWheels() { return values[5].value; }
	int GetPassForHalfTracks() { return values[6].value; }
	int GetPassForTracks() { return values[7].value; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CBridgeDefencesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeDefencesItem );
public:
	CBridgeDefencesItem() { bStaticElements = true; nItemType = E_BRIDGE_DEFENCES_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CBridgeDefencesItem() {};
	
	virtual void InitDefaultValues();
};

class CBridgeDefencePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeDefencePropsItem );
public:
	CBridgeDefencePropsItem() { nItemType = E_BRIDGE_DEFENCE_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CBridgeDefencePropsItem() {};
	
	int GetMinArmor() { return values[0].value; }
	int GetMaxArmor() { return values[1].value; }
	float GetSilhouette() { return values[2].value; }
	
	void SetMinArmor( int nVal ) { values[0].value = nVal; }
	void SetMaxArmor( int nVal ) { values[1].value = nVal; }
	void SetSilhouette( float fVal ) { values[2].value = fVal; }
	
	virtual void InitDefaultValues();
};

class CBridgeStagePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeStagePropsItem );
public:
	CBridgeStagePropsItem() { bStaticElements = true; nItemType = E_BRIDGE_STAGE_PROPS_ITEM; InitDefaultValues(); nImageIndex = 2; }
	~CBridgeStagePropsItem() {};
	
	enum
	{
		E_WHOLE,
		E_DAMAGED,
		E_DESTROYED,
	};
	int GetActiveStage();

	virtual void InitDefaultValues();
};

class CBridgeCommonSpansItem : public CTreeItem
{
	int GetActiveStage();
public:
	virtual void MyKeyDown( int nChar );
};

class CBridgeBeginSpansItem : public CBridgeCommonSpansItem
{
	OBJECT_NORMAL_METHODS( CBridgeBeginSpansItem );
public:
	CBridgeBeginSpansItem() { nItemType = E_BRIDGE_BEGIN_SPANS_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CBridgeBeginSpansItem() {};
	
	virtual void InitDefaultValues();
};

class CBridgeCenterSpansItem : public CBridgeCommonSpansItem
{
	OBJECT_NORMAL_METHODS( CBridgeCenterSpansItem );
public:
	CBridgeCenterSpansItem() { nItemType = E_BRIDGE_CENTER_SPANS_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CBridgeCenterSpansItem() {};
	
	virtual void InitDefaultValues();
};

class CBridgeEndSpansItem : public CBridgeCommonSpansItem
{
	OBJECT_NORMAL_METHODS( CBridgeEndSpansItem );
public:
	CBridgeEndSpansItem() { nItemType = E_BRIDGE_END_SPANS_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CBridgeEndSpansItem() {};
	
	virtual void InitDefaultValues();
};

class CBridgePartsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgePartsItem );
public:
	int nSpanIndex;
public:
	CListOfTiles lockedTiles;
	CListOfTiles transeparences;
	CListOfTiles unLockedTiles;
	
	CBridgePartsItem() : nSpanIndex( -1 ) { bStaticElements = true; nItemType = E_BRIDGE_PARTS_ITEM; InitDefaultValues(); nImageIndex = 2; }
	~CBridgePartsItem() {};
	

	int GetActiveStage();
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual int operator&( IDataTree &ss );
};

class CBridgePartPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgePartPropsItem );
public:
	CPtr<IObjVisObj> pSprite;
	bool bLoaded;

	CBridgePartPropsItem() { bStaticElements = true; bLoaded = false; nItemType = E_BRIDGE_PART_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CBridgePartPropsItem() {};

	const char *GetSpriteName() { return values[0].value; }

	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CBridgeFirePointsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeFirePointsItem );
public:
	CBridgeFirePointsItem() { nItemType = E_BRIDGE_FIRE_POINTS_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CBridgeFirePointsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
};

class CBridgeFirePointPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeFirePointPropsItem );
public:
	CBridgeFirePointPropsItem() { bStaticElements = true; nItemType = E_BRIDGE_FIRE_POINT_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CBridgeFirePointPropsItem() {};
	
	float GetDirection() { return values[0].value; }
	const char *GetEffectName() { return values[1].value; }
	
	void SetDirection( float fVal ) { values[0].value = fVal; }
	void SetEffectName( const char *pszVal ) { values[1].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyKeyDown( int nChar );
};

class CBridgeDirExplosionsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeDirExplosionsItem );
public:
	CBridgeDirExplosionsItem() { bStaticElements = true; nItemType = E_BRIDGE_DIR_EXPLOSIONS_ITEM; nImageIndex = 5; InitDefaultValues(); }
	~CBridgeDirExplosionsItem() {};
	
	const char *GetEffectName() { return values[0].value; }
	
	void SetEffectName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
};

class CBridgeDirExplosionPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeDirExplosionPropsItem );
public:
	CPtr<IObjVisObj> pSprite;
	CPtr<IObjVisObj> pHLine;
	
public:
	CBridgeDirExplosionPropsItem() { bStaticElements = true; nItemType = E_BRIDGE_DIR_EXPLOSION_PROPS_ITEM; nImageIndex = 6; InitDefaultValues(); }
	~CBridgeDirExplosionPropsItem() {};
	
	float GetDirection() { return values[0].value; }
	
	void SetDirection( float fVal ) { values[0].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
};

class CBridgeSmokesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeSmokesItem );
public:
	CBridgeSmokesItem() { nItemType = E_BRIDGE_SMOKES_ITEM; nImageIndex = 7; InitDefaultValues(); }
	~CBridgeSmokesItem() {};
	
	const char *GetEffectName() { return values[0].value; }
	
	void SetEffectName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void MyLButtonClick();
};

class CBridgeSmokePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CBridgeSmokePropsItem );
public:
	CPtr<IObjVisObj> pSprite;
	CPtr<IObjVisObj> pHLine;
	
public:
	CBridgeSmokePropsItem() { bStaticElements = true; nItemType = E_BRIDGE_SMOKE_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CBridgeSmokePropsItem() {};
	
	float GetDirection() { return values[0].value; }
	
	void SetDirection( float fVal ) { values[0].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyKeyDown( int nChar );
};

#endif		//__BRIDGE_TREE_ITEM_H__
