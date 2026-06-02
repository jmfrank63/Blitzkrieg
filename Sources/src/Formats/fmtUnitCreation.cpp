#include "StdAfx.h"

#include "FmtUnitCreation.h"

const char* SUnitCreationInfo::DEFAULT_AIRCRAFT_NAME[SUCAviation::AT_COUNT] =
{
	"Po_2",
	"Yak-7",
	"Tb-3",
	"Tb-3",
	"IL_2",
};
const char* SUnitCreationInfo::DEFAULT_PARADROP_SOLDIER_NAME = "USSR_rpd_43";
const char* SUnitCreationInfo::DEFAULT_PARTY_NAME = "USSR";
const int   SUnitCreationInfo::DEFAULT_RELAX_TIME = 20;

void SUnitCreationInfo::Validate()
{
	while ( units.size() < ( UT_COUNT - 1 ) )
	{
		units.push_back( SUnitCreation() );
	}
	for ( int nUnitTypeIndex = 0; nUnitTypeIndex <  units.size(); ++nUnitTypeIndex )
	{
		for ( int nAircraftIndex = SUCAviation::AT_SCOUT; nAircraftIndex < SUCAviation::AT_COUNT; ++nAircraftIndex )
		{
			if ( units[nUnitTypeIndex].aviation.aircrafts[nAircraftIndex].szName.empty() )
			{
				units[nUnitTypeIndex].aviation.aircrafts[nAircraftIndex].szName = DEFAULT_AIRCRAFT_NAME[nAircraftIndex];
			}
		}
		if ( units[nUnitTypeIndex].aviation.szParadropSquadName.empty() )
		{ 
			units[nUnitTypeIndex].aviation.szParadropSquadName = DEFAULT_PARADROP_SOLDIER_NAME;
		}
		/**
		if ( units[nUnitTypeIndex].aviation.vAppearPoints.empty() )
		{
			units[nUnitTypeIndex].aviation.vAppearPoints.push_back( VNULL3 );
		}
		/**/
		if ( units[nUnitTypeIndex].aviation.nRelaxTime <= 0 )
		{
			units[nUnitTypeIndex].aviation.nRelaxTime = DEFAULT_RELAX_TIME;
		}
		if ( units[nUnitTypeIndex].szPartyName.empty() )
		{
			units[nUnitTypeIndex].szPartyName = DEFAULT_PARTY_NAME;
		}
	}
}
