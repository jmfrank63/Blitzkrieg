#ifndef __MEDAL_TREE_ITEM_H__
#define __MEDAL_TREE_ITEM_H__

#include "TreeItem.h"
#include "../Main/rpgstats.h"

class CMedalTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMedalTreeRootItem );
public:
	CMedalTreeRootItem() { bStaticElements = true; nItemType = E_MEDAL_ROOT_ITEM; InitDefaultValues(); }
	~CMedalTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class CMedalCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMedalCommonPropsItem );
public:
	CMedalCommonPropsItem() { nItemType = E_MEDAL_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CMedalCommonPropsItem() {};
	
	const char* GetName() { return values[0].value; }
	const char* GetDescText() { return values[1].value; }
	const char* GetTexture() { return values[2].value; }
	
	void SetName( const char *pszName ) { values[0].value = pszName; }
	const char* SetDescText( const char *pszName ) { values[1].value = pszName; }
	const char* SetTexture( const char *pszName ) { values[2].value = pszName; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CMedalPicturePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMedalPicturePropsItem );
public:
	CMedalPicturePropsItem() { nItemType = E_MEDAL_PICTURE_PROPS_ITEM; InitDefaultValues(); nImageIndex = 1; }
	~CMedalPicturePropsItem() {};
	
	const char* GetTexture() { return values[0].value; }
	CVec2 GetPosition() { return CVec2( values[1].value, values[2].value ); }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

class CMedalTextPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMedalTextPropsItem );
public:
	CMedalTextPropsItem() { nItemType = E_MEDAL_TEXT_PROPS_ITEM; InitDefaultValues(); nImageIndex = 2; }
	~CMedalTextPropsItem() {};
	
	const char* GetDescText() { return values[0].value; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};

#endif		//__MEDAL_TREE_ITEM_H__
