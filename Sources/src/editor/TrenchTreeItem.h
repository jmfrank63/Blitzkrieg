#ifndef __TRENCH_TREE_ITEM_H__
#define __TRENCH_TREE_ITEM_H__

#include "TreeItem.h"

class CTrenchTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTrenchTreeRootItem );
public:
	CTrenchTreeRootItem() { bStaticElements = true; nItemType = E_TRENCH_ROOT_ITEM; InitDefaultValues(); }
	~CTrenchTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class CTrenchCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTrenchCommonPropsItem );
public:
	CTrenchCommonPropsItem() { nItemType = E_TRENCH_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CTrenchCommonPropsItem() {};
	
	const char *GetTrenchName() { return values[0].value; }
	int GetTrenchHealth() { return values[1].value; }
	int GetTrenchRestSlots() { return values[2].value; }
	int GetTrenchMedicalSlots() { return values[3].value; }
	float GetTrenchCover() { return values[4].value; }
	
	void SetTrenchName( const char *pszVal ) { values[0].value = pszVal; }
	void SetTrenchHealth( int nVal ) { values[1].value = nVal; }
	void SetTrenchRestSlots( int nVal ) { values[2].value = nVal; }
	void SetTrenchMedicalSlots( int nVal ) { values[3].value = nVal; }
	void SetTrenchCover( float fVal ) { values[4].value = fVal; }

	virtual void InitDefaultValues();
};

class CTrenchSourcesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTrenchSourcesItem );
public:
	CTrenchSourcesItem() { nItemType = E_TRENCH_SOURCES_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CTrenchSourcesItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class CTrenchSourcePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTrenchSourcePropsItem );
public:
	int nTrenchIndex;

public:
	CTrenchSourcePropsItem() : nTrenchIndex( -1 ) { nItemType = E_TRENCH_SOURCE_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CTrenchSourcePropsItem() {};
	
	const char *GetFileName() { return values[0].value; }
	float GetCoverage() { return values[1].value; }
	
	virtual int operator&( IDataTree &ss );
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
};

class CTrenchDefencesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTrenchDefencesItem );
public:
	CTrenchDefencesItem() { bStaticElements = true; nItemType = E_TRENCH_DEFENCES_ITEM; nImageIndex = 2; InitDefaultValues(); }
	~CTrenchDefencesItem() {};
	
	virtual void InitDefaultValues();
};

class CTrenchDefencePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CTrenchDefencePropsItem );
public:
	CTrenchDefencePropsItem() { nItemType = E_TRENCH_DEFENCE_PROPS_ITEM; nImageIndex = 3; InitDefaultValues(); }
	~CTrenchDefencePropsItem() {};
	
	int GetMinArmor() { return values[0].value; }
	int GetMaxArmor() { return values[1].value; }
	
	void SetMinArmor( int nVal ) { values[0].value = nVal; }
	void SetMaxArmor( int nVal ) { values[1].value = nVal; }
	
	virtual void InitDefaultValues();
};

#endif		//__TRENCH_TREE_ITEM_H__
