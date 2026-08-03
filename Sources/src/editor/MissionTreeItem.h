#ifndef __MISSION_TREE_ITEM_H__
#define __MISSION_TREE_ITEM_H__

#include "TreeItem.h"
#include "../Main/rpgstats.h"

class CMissionTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMissionTreeRootItem );
public:
	CMissionTreeRootItem() { bStaticElements = true; nItemType = E_MISSION_ROOT_ITEM; InitDefaultValues(); }
	~CMissionTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class CMissionCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMissionCommonPropsItem );
public:
	CMissionCommonPropsItem() { nItemType = E_MISSION_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CMissionCommonPropsItem() {};
	
	const char* GetHeaderText() { return values[0].value; }
	const char* GetSubHeaderText() { return values[1].value; }
	const char* GetDescText() { return values[2].value; }
	const char* GetTemplateMap() { return values[3].value; }
	const char* GetFinalMap() { return values[4].value; }
	const char *GetSettingName() { return values[5].value; }
	
	void SetHeaderText( const char *pszName ) { values[0].value = pszName; }
	void SetSubHeaderText( const char *pszName ) { values[1].value = pszName; }
	void SetDescText( const char *pszName ) { values[2].value = pszName; }
	void SetTemplateMap( const char *pszName ) { values[3].value = pszName; }
	void SetFinalMap( const char *pszName ) { values[4].value = pszName; }
	void SetSettingName( const char *pszVal ) { values[5].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
};

class CMissionMusicsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMissionMusicsItem );
public:
	CMissionMusicsItem() { nItemType = E_MISSION_MUSICS_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CMissionMusicsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyRButtonClick();
	virtual void MyLButtonClick();
};

class CMissionMusicPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMissionMusicPropsItem );
public:
	CMissionMusicPropsItem() { bStaticElements = true; nItemType = E_MISSION_MUSIC_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CMissionMusicPropsItem() {};
	
	const char* GetMusicFileName() { return values[0].value; }
	
	void SetMusicFileName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CMissionObjectivesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMissionObjectivesItem );
public:
	CMissionObjectivesItem() { nItemType = E_MISSION_OBJECTIVES_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CMissionObjectivesItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CMissionObjectivePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CMissionObjectivePropsItem );
public:
	CMissionObjectivePropsItem() { bStaticElements = true; nItemType = E_MISSION_OBJECTIVE_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CMissionObjectivePropsItem() {};
	
	const char* GetObjectiveHeader() { return values[0].value; }
	const char* GetObjectiveText() { return values[1].value; }
	CVec2 GetObjectivePosition() { CVec2 res(values[2].value, values[3].value); return res; }
	bool GetObjectiveSecretFlag() { return values[4].value; }
	int GetObjectiveScriptID() { return values[5].value; }

	void SetObjeciveHeader( const char *pszVal ) { values[0].value = pszVal; }
	void SetObjeciveText( const char *pszVal ) { values[1].value = pszVal; }
	void SetObjectivePosition( CVec2 vVal ) { values[2].value = vVal.x; values[3].value = vVal.y; }
	void SetObjectiveSecretFlag( bool nVal ) { values[4].value = nVal; }
	void SetObjectiveScriptID( int nVal ) { values[5].value = nVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

#endif		//__MISSION_TREE_ITEM_H__
