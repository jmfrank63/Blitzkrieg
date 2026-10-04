#pragma once
// Campaign sub-editor - project extension .cgc, project XML root tag
// "Campaign_Composer_Project". MFC source: Sources/src/editor/CampaignTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:258-263.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CCampaignTreeRootItem : public CStatsItem
{
public:
	CCampaignTreeRootItem() : CStatsItem( ETIT_CAMPAIGN_ROOT_ITEM, "Campaign_Composer_Project" ) {}
};
class CCampaignCommonPropsItem    : public CStatsItem { public: CCampaignCommonPropsItem()    : CStatsItem( ETIT_CAMPAIGN_COMMON_PROPS_ITEM ) {} };
class CCampaignChaptersItem       : public CStatsItem { public: CCampaignChaptersItem()       : CStatsItem( ETIT_CAMPAIGN_CHAPTERS_ITEM ) {} };
class CCampaignChapterPropsItem   : public CStatsItem { public: CCampaignChapterPropsItem()   : CStatsItem( ETIT_CAMPAIGN_CHAPTER_PROPS_ITEM ) {} };
class CCampaignTemplatesItem      : public CStatsItem { public: CCampaignTemplatesItem()      : CStatsItem( ETIT_CAMPAIGN_TEMPLATES_ITEM ) {} };
class CCampaignTemplatePropsItem  : public CStatsItem { public: CCampaignTemplatePropsItem()  : CStatsItem( ETIT_CAMPAIGN_TEMPLATE_PROPS_ITEM ) {} };

}
