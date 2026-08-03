#ifndef __3DROAD_TREE_ITEM_H__
#define __3DROAD_TREE_ITEM_H__

#include "TreeItem.h"
#include "../Formats/fmtVSO.h"

class C3DRoadTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( C3DRoadTreeRootItem );
public:
	C3DRoadTreeRootItem() { bStaticElements = true; nItemType = E_3DROAD_ROOT_ITEM; InitDefaultValues(); }
	~C3DRoadTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class C3DRoadCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( C3DRoadCommonPropsItem );
public:
	C3DRoadCommonPropsItem() { nItemType = E_3DROAD_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~C3DRoadCommonPropsItem() {};
	
	int GetBottomWidth() { return values[0].value; }
	bool HasBorders() { return values[1].value; }
	float GetBorderRelativeWidth() { return values[2].value; }
	int GetPriority() { return values[3].value; }
	float GetPassability() { return values[4].value; }
	int GetPassForInfantry() { return values[5].value; }
	int GetPassForWheels() { return values[6].value; }
	int GetPassForHalfTracks() { return values[7].value; }
	int GetPassForTracks() { return values[8].value; }
	int GetRoadType();
	int GetMinimapCenterColor() { return values[10].value; }
	int GetMinimapBorderColor() { return values[11].value; }
	BYTE GetSoilParams();
	
	void SetBottomWidth( int nVal ) { values[0].value = nVal; }
	void SetBorderRelativeWidth( float fVal ) { values[2].value = fVal; }
	void SetPriority( int nVal ) { values[3].value = nVal; }
	void SetPassability( float fVal ) { values[4].value = fVal; }
	void SetPassForInfantry( bool bVal ) { values[5].value = bVal; }
	void SetPassForWheels( bool bVal ) { values[6].value = bVal; }
	void SetPassForHalfTracks( bool bVal ) { values[7].value = bVal; }
	void SetPassForTracks( bool bVal ) { values[8].value = bVal; }
	void SetRoadType( int nVal );
	void SetMinimapCenterColor( int dwCol ) { values[10].value = dwCol; }
	void SetMinimapBorderColor( int dwCol ) { values[11].value = dwCol; }
	void SetSoilParams( BYTE nVal ) { values[12].value = ( ( nVal & SVectorStripeObjectDesc::ESP_DUST ) != 0x0 ); values[13].value = ( ( nVal & SVectorStripeObjectDesc::ESP_TRACE ) != 0x0 ); }

	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class C3DRoadLayerPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( C3DRoadLayerPropsItem );
public:
	C3DRoadLayerPropsItem() { nItemType = E_3DROAD_LAYER_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~C3DRoadLayerPropsItem() {};
	
	int GetCenterOpacity() { return values[0].value; }
	int GetBorderOpacity() { return values[1].value; }
	float GetTextureStep() { return values[2].value; }
	std::string GetTexture() { return values[3].value; }
	
	void SetCenterOpacity( int nVal ) { values[0].value = nVal; }
	void SetBorderOpacity( int nVal ) { values[1].value = nVal; }
	void SetTextureStep( float fVal ) { values[2].value = fVal; }
	void SetTexture( const char *pszVal ) { values[3].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

#endif		//__3DROAD_TREE_ITEM_H__
