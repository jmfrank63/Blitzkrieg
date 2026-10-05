#include "localization.h"

#include "editor_env.h"

// CLocalizationItem::InitDefaultValues, line for line from
// Sources/src/editor/localization.cpp. A translation unit of its own so the
// file reads in localization.cpp link without the tree item classes.

namespace NResourceModel
{

void CLocalizationItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "name.txt";
	std::string szProjectFileName;
	if ( GetActiveProjectFileName( szProjectFileName ) )
		prop.szStrings.push_back( GetDirectory( szProjectFileName ) );
	else
		prop.szStrings.push_back( "" );
	prop.szStrings.push_back( szTextFilter );
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Description";
	prop.szDisplayName = "Description";
	prop.value = "desc.txt";
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Statistics";
	prop.szDisplayName = "Statistics";
	prop.value = "stats.txt";
	defaultValues.push_back( prop );

	values = defaultValues;
}

}
