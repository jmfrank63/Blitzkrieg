#include "StdAfx.h"
#include "frames.h"
#include "localization.h"

void CLocalizationItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;
	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "name.txt";
	CParentFrame *p = g_frameManager.GetActiveFrame();
	if ( p )
		prop.szStrings.push_back( GetDirectory( p->GetProjectFileName().c_str() ).c_str() );
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

void CLocalizationItem::UpdateItemValue( int nItemId, const CVariant &value )
{
	CTreeItem::UpdateItemValue( nItemId, value );

	if ( !IsRelatedPath( value ) )
	{
		string szValue = value;
		string szRelatedPath;
		bool bRes =	MakeSubRelativePath( GetDirectory(g_frameManager.GetActiveFrame()->GetProjectFileName().c_str() ).c_str(), szValue.c_str(), szRelatedPath );
		if ( bRes )
		{
/*
			szRelatedPath = szRelatedPath.substr( 0, szRelatedPath.rfind( '.' ) );
*/

			CVariant newVal = szRelatedPath;
			CTreeItem::UpdateItemValue( nItemId, newVal );
			g_frameManager.GetActiveFrame()->UpdatePropView( this );
		}
		else
		{
			AfxMessageBox( "Error: .txt localization file must be inside project directory" );
			CTreeItem::UpdateItemValue( nItemId, "" );
			g_frameManager.GetActiveFrame()->UpdatePropView( this );
		}
	}
}
