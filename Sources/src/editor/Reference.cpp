#include "StdAfx.h"
#include "../main/rpgstats.h"
#include "TreeItem.h"


void LoadAIClassCombo( SProp *pProp )
{
	pProp->szStrings.push_back( "wheel" );
	pProp->szStrings.push_back( "halftrack" );
	pProp->szStrings.push_back( "track" );
	pProp->szStrings.push_back( "human" );
}

int GetAIClassInfo( const char *pszVal )
{
	std::string szVal = pszVal;
	
	if ( szVal == "wheel" )
		return AI_CLASS_WHEEL;
	if ( szVal == "halftrack" )
		return AI_CLASS_HALFTRACK;
	if ( szVal == "track" )
		return AI_CLASS_TRACK;
	if ( szVal == "human" )
		return AI_CLASS_HUMAN;
	NI_ASSERT( 0 );
	return 0;
}

std::string GetAIClassInfo( int nVal )
{
	std::string szVal;
	switch ( nVal )
	{
		case AI_CLASS_WHEEL:
			szVal = "wheel";
			break;
		case AI_CLASS_HALFTRACK:
			szVal = "halftrack";
			break;
		case AI_CLASS_TRACK:
			szVal = "track";
			break;
		case AI_CLASS_HUMAN:
			szVal = "human";
			break;
		default:
			NI_ASSERT( 0 );
	}
	return szVal;
}
