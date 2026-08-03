#ifndef __CAMPAIGN_TREE_ITEM_H__
#define __CAMPAIGN_TREE_ITEM_H__

#include "TreeItem.h"
#include "../Main/rpgstats.h"

class CCampaignTreeRootItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CCampaignTreeRootItem );
public:
	CCampaignTreeRootItem() { bStaticElements = true; nItemType = E_CAMPAIGN_ROOT_ITEM; InitDefaultValues(); }
	~CCampaignTreeRootItem() {}
	
	virtual void InitDefaultValues();
};

class CCampaignCommonPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CCampaignCommonPropsItem );
public:
	CCampaignCommonPropsItem() { nItemType = E_CAMPAIGN_COMMON_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CCampaignCommonPropsItem() {};
	
	const char* GetHeaderText() { return values[0].value; }
	const char* GetSubHeaderText() { return values[1].value; }
	const char* GetMapImage() { return values[2].value; }
	const char* GetIntroMovie() { return values[3].value; }
	const char* GetOutroMovie() { return values[4].value; }
	const char* GetInterfaceMusic() { return values[5].value; }
	const char *GetPlayerSideName() { return values[6].value; }
	
	void SetHeaderText( const char *pszName ) { values[0].value = pszName; }
	void SetSubHeaderText( const char *pszName ) { values[1].value = pszName; }
	void SetMapImage( const char *pszName ) { values[2].value = pszName; }
	void SetIntroMovie( const char *pszName ) { values[3].value = pszName; }
	void SetOutroMovie( const char *pszName ) { values[4].value = pszName; }
	void SetInterfaceMusic( const char *pszName ) { values[5].value = pszName; }
	void SetPlayerSideName( const char *pszVal ) { values[6].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
};

class CCampaignChaptersItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CCampaignChaptersItem );
public:
	CCampaignChaptersItem() { nItemType = E_CAMPAIGN_CHAPTERS_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CCampaignChaptersItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CCampaignChapterPropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CCampaignChapterPropsItem );
public:
	CCampaignChapterPropsItem() { bStaticElements = true; nItemType = E_CAMPAIGN_CHAPTER_PROPS_ITEM; InitDefaultValues(); nImageIndex = 0; }
	~CCampaignChapterPropsItem() {};
	
	const char* GetChapterName() { return values[0].value; }
	CVec2 GetChapterPosition() { CVec2 res(values[1].value, values[2].value); return res; }
	bool GetChapterVisibleFlag() { return values[3].value; }
	bool GetChapterSecretFlag() { return values[4].value; }
	
	void SetChapterName( const char *pszVal ) { values[0].value = pszVal; }
	void SetChapterPosition( const CVec2 &vVal ) { values[1].value = vVal.x; values[2].value = vVal.y; }
	void SetChapterVisibleFlag( bool nVal ) { values[3].value = nVal; }
	void SetChapterSecretFlag( bool nVal ) { values[4].value = nVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

class CCampaignTemplatesItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CCampaignTemplatesItem );
public:
	CCampaignTemplatesItem() { nItemType = E_CAMPAIGN_TEMPLATES_ITEM; InitDefaultValues(); nImageIndex = 7; }
	~CCampaignTemplatesItem() {};
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void MyLButtonClick();
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyRButtonClick();
};

class CCampaignTemplatePropsItem : public CTreeItem
{
	OBJECT_NORMAL_METHODS( CCampaignTemplatePropsItem );
public:
	CCampaignTemplatePropsItem() { bStaticElements = true; nItemType = E_CAMPAIGN_TEMPLATE_PROPS_ITEM; InitDefaultValues(); nImageIndex = 3; }
	~CCampaignTemplatePropsItem() {};
	
	const char* GetTemplateName() { return values[0].value; }

	void SetTemplateName( const char *pszVal ) { values[0].value = pszVal; }
	
	virtual void InitDefaultValues();
	virtual void MyKeyDown( int nChar );
	virtual void UpdateItemValue( int nItemId, const CVariant &value );
	virtual void MyLButtonClick();
	virtual void MyRButtonClick();
};

#endif		//__CAMPAIGN_TREE_ITEM_H__
