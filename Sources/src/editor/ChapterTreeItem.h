#ifndef __CHAPTER_TREE_ITEM_H__
#define __CHAPTER_TREE_ITEM_H__

#include "TreeItem.h"
#include "../Main/rpgstats.h"

class CChapterTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CChapterTreeRootItem );
public:
	CChapterTreeRootItem() { bStaticElements = true; nItemType = E_CHAPTER_ROOT_ITEM; InitDefaultValues(); }
	~CChapterTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class CChapterCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CChapterCommonPropsItem );
public:
	CChapterCommonPropsItem() { nItemType = E_CHAPTER_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CChapterCommonPropsItem() {};
	
	const char* GetHeaderText() { return values[0].value; }
	const char* GetSubHeaderText() { return values[1].value; }
	const char* GetDescText() { return values[2].value; }
	const char* GetMapImage() { return values[3].value; }
	const char* GetScriptFile() { return values[4].value; }
	const char* GetInterfaceMusic() { return values[5].value; }
	int GetSeason();
	const char *GetSettingName() { return values[7].value; }
	const char *GetContextName() { return values[8].value; }
	const char *GetPlayerSideName() { return values[9].value; }
	
	void SetHeaderText( const char *pszName ) { values[0].value = pszName; }
	void SetSubHeaderText( const char *pszName ) { values[1].value = pszName; }
	void SetDescText( const char *pszName ) { values[2].value = pszName; }
	void SetMapImage( const char *pszName ) { values[3].value = pszName; }
	void SetScriptFile( const char *pszName ) { values[4].value = pszName; }
	void SetInterfaceMusic( const char *pszName ) { values[5].value = pszName; }
	void SetSeason( int nSeason );
	void SetSettingName( const char *pszVal ) { values[7].value = pszVal; }
	void SetContextName( const char *pszVal ) { values[8].value = pszVal; }
	void SetPlayerSideName( const char *pszVal ) { values[9].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
};

/*
class CChapterMusicsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CChapterMusicsItem );
public:
	CChapterMusicsItem() { nItemType = E_CHAPTER_MUSICS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CChapterMusicsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
};

class CChapterMusicPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CChapterMusicPropsItem );
public:
	CChapterMusicPropsItem() { bStaticElements = true; nItemType = E_CHAPTER_MUSIC_PROPS_ITEM; InitDefaultValues(); nImageIndex = 4; }
	~CChapterMusicPropsItem() {};
	
	const char* GetMusicFileName() { return values[0].value; }
	bool GetMusicCombatFlag() { return values[1].value; }
	int GetMusicProbability() { return values[2].value; }
	
	void SetMusicFileName( const char *pszVal ) { values[0].value = pszVal; }
	void SetMusicCombatFlag( bool nVal ) { values[1].value = nVal; }
	void SetMusicProbability( int nVal ) { values[2].value = nVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
};
*/

class CChapterMissionsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CChapterMissionsItem );
public:
	CChapterMissionsItem() { nItemType = E_CHAPTER_MISSIONS_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CChapterMissionsItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CChapterMissionPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CChapterMissionPropsItem );
public:
	CChapterMissionPropsItem() { bStaticElements = true; nItemType = E_CHAPTER_MISSION_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CChapterMissionPropsItem() {};
	
	const char* GetMissionName() { return values[0].value; }
	CVec2 GetMissionPosition() { CVec2 res(values[1].value, values[2].value); return res; }
	
	void SetMissionName( const char *pszVal ) { values[0].value = pszVal; }
	void SetMissionPosition( CVec2 vVal ) { values[1].value = vVal.x; values[2].value = vVal.y; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CChapterPlacesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CChapterPlacesItem );
public:
	CChapterPlacesItem() { nItemType = E_CHAPTER_PLACES_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CChapterPlacesItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CChapterPlacePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CChapterPlacePropsItem );
public:
	CChapterPlacePropsItem() { bStaticElements = true; nItemType = E_CHAPTER_PLACE_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CChapterPlacePropsItem() {};
	
	CVec2 GetPosition() { CVec2 res(values[0].value, values[1].value); return res; }
	
	void SetPosition( CVec2 vVal ) { values[0].value = vVal.x; values[1].value = vVal.y; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

#endif		//__CHAPTER_TREE_ITEM_H__
