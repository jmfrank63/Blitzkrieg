#include "campaign.h"

#include "../../combos.h"
#include "../../editor_env.h"

namespace NResourceModel
{

void CCampaignTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_CAMPAIGN_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic info";
	child.szDisplayName = "Basic info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CAMPAIGN_CHAPTERS_ITEM;
	child.szDefaultName = "Chapters";
	child.szDisplayName = "Chapters";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CAMPAIGN_TEMPLATES_ITEM;
	child.szDefaultName = "Templates";
	child.szDisplayName = "Templates";
	defaultChilds.push_back( child );
}

void CCampaignCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	if ( !HasEditorFrame() )
	{
		values = defaultValues;
		return;
	}

	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Header text";
	prop.szDisplayName = "Header text";
	prop.value = "header";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szTextFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "SubHeader text";
	prop.szDisplayName = "SubHeader text";
	prop.value = "subheader";
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Map image";
	prop.szDisplayName = "Map image";
	prop.value = "map";
	prop.szStrings.push_back( GetFrameProjectFileName() );
	prop.szStrings.push_back( szTGAFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 4;
	prop.nDomenType = DT_MOVIE_REF;
	prop.szDefaultName = "Intro movie";
	prop.szDisplayName = "Intro movie";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_MOVIE_REF;
	prop.szDefaultName = "Outro movie";
	prop.szDisplayName = "Outro movie";
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_MUSIC_REF;
	prop.szDefaultName = "Interface music";
	prop.szDisplayName = "Interface music";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Player side";
	prop.szDisplayName = "Player side";
	prop.value = "German";
	FillVectorOfSides( prop.szStrings );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	values = defaultValues;
}

void CCampaignChaptersItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CCampaignChapterPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_CHAPTER_REF;
	prop.szDefaultName = "Chapter";
	prop.szDisplayName = "Chapter";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Chapter position X";
	prop.szDisplayName = "Chapter position X";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Chapter position Y";
	prop.szDisplayName = "Chapter position Y";
	prop.value = 0;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Is this chapter visible?";
	prop.szDisplayName = "Is this chapter visible?";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Chapter secret flag";
	prop.szDisplayName = "Chapter secret flag";
	prop.value = false;
	defaultValues.push_back( prop );
}

void CCampaignTemplatesItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Templates directory";
	prop.szDisplayName = "Templates directory";
	prop.value = "";
	std::string szDir = GetEditorDataDir();
	szDir += "Scenarios\\TemplateMissions\\";
	prop.szStrings.push_back( szDir.c_str() );
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
}

void CCampaignTemplatePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_TEMPLATE_MISSION_REF;
	prop.szDefaultName = "Template";
	prop.szDisplayName = "Template";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
}

}
