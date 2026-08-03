#ifndef __MINE_TREE_ITEM_H__
#define __MINE_TREE_ITEM_H__

#include "TreeItem.h"
#include "..\Main\rpgstats.h"

class CMineTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMineTreeRootItem );
public:
	CMineTreeRootItem() { bStaticElements = true; nItemType = E_MINE_ROOT_ITEM; InitDefaultValues(); }
	~CMineTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class CMineCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMineCommonPropsItem );
public:
	CMineCommonPropsItem() { nItemType = E_MINE_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CMineCommonPropsItem() {};
	
	const char* GetMineName() { return values[0].value; }
	int GetWeight() { return values[1].value; }
	
	void SetMineName( const char *pszName ) { values[0].value = pszName; }
	void SetWeight( int nVal ) { values[1].value = nVal; }
	
	virtual void InitDefaultValues();
};

#endif		//__MINE_TREE_ITEM_H__
