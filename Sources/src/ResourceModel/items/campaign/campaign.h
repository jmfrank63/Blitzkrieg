#pragma once
// Campaign sub-editor - project extension .cgc, project XML root tag
// "Campaign_Composer_Project". MFC source: Sources/src/editor/CampaignTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CCampaignTreeRootItem : public CStatsItem
{
public:
	CCampaignTreeRootItem() : CStatsItem( ETIT_CAMPAIGN_ROOT_ITEM, "Campaign_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CCampaignCommonPropsItem : public CStatsItem
{
public:
	CCampaignCommonPropsItem() : CStatsItem( ETIT_CAMPAIGN_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CCampaignChaptersItem : public CStatsItem
{
public:
	CCampaignChaptersItem() : CStatsItem( ETIT_CAMPAIGN_CHAPTERS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CCampaignChapterPropsItem : public CStatsItem
{
public:
	CCampaignChapterPropsItem() : CStatsItem( ETIT_CAMPAIGN_CHAPTER_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CCampaignTemplatesItem : public CStatsItem
{
public:
	CCampaignTemplatesItem() : CStatsItem( ETIT_CAMPAIGN_TEMPLATES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CCampaignTemplatePropsItem : public CStatsItem
{
public:
	CCampaignTemplatePropsItem() : CStatsItem( ETIT_CAMPAIGN_TEMPLATE_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

}
