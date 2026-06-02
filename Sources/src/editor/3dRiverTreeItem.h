#ifndef __3DRIVER_TREE_ITEM_H__
#define __3DRIVER_TREE_ITEM_H__

#include "TreeItem.h"

class C3DRiverTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( C3DRiverTreeRootItem );
public:
	C3DRiverTreeRootItem() { bStaticElements = true; nItemType = E_3DRIVER_ROOT_ITEM; InitDefaultValues(); }
	~C3DRiverTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class C3DRiverBottomLayerPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( C3DRiverBottomLayerPropsItem );
public:
	C3DRiverBottomLayerPropsItem() { nItemType = E_3DRIVER_BOTTOM_LAYER_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~C3DRiverBottomLayerPropsItem() {};
	
	int GetBottomWidth() { return values[0].value; }
	int GetCenterOpacity() { return values[1].value; }
	int GetBorderOpacity() { return values[2].value; }
	float GetTextureStep() { return values[3].value; }
	const char *GetTexture() { return values[4].value; }
	const char *GetAmbientSound() { return values[5].value; }
	
	void SetBottomWidth( int nVal ) { values[0].value = nVal; }
	void SetCenterOpacity( int nVal ) { values[1].value = nVal; }
	void SetBorderOpacity( int nVal ) { values[2].value = nVal; }
	void SetTextureStep( float fVal ) { values[3].value = fVal; }
	void SetTexture( const char *pszVal ) { values[4].value = pszVal; }
	void SetAmbientSound( const char *pszVal ) { values[5].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class C3DRiverLayersItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( C3DRiverLayersItem );
public:
	C3DRiverLayersItem() { nItemType = E_3DRIVER_LAYERS_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~C3DRiverLayersItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class C3DRiverLayerPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( C3DRiverLayerPropsItem );
public:
	C3DRiverLayerPropsItem() { nItemType = E_3DRIVER_LAYER_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~C3DRiverLayerPropsItem() {};
	
	int GetCenterOpacity() { return values[0].value; }
	int GetBorderOpacity() { return values[1].value; }
	float GetStreamSpeed() { return values[2].value; }
	float GetTextureStep() { return values[3].value; }
	bool GetAnimatedFlag() { return values[4].value; }
	const char *GetTexture() { return values[5].value; }
	float GetDisturbance() { return values[6].value; }
	
	void SetCenterOpacity( int nVal ) { values[0].value = nVal; }
	void SetBorderOpacity( int nVal ) { values[1].value = nVal; }
	void SetStreamSpeed( float fVal ) { values[2].value = fVal; }
	void SetTextureStep( float fVal ) { values[3].value = fVal; }
	void SetAnimatedFlag( bool bVal ) { values[4].value = bVal; }
	void SetTexture( const char *pszVal ) { values[5].value = pszVal; }
	void SetDisturbance( float fVal ) { values[6].value = fVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyRButtonClick();
};

#endif		//__3DRIVER_TREE_ITEM_H__