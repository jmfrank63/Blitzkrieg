#include "StdAfx.h"

#include "fmtAIGeneral.h"

int SAIGeneralParcelInfo::SReinforcePointInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	
	saver.Add( "Center", &vCenter );
	saver.Add( "Direction", &wDir );
	
	return 0;
}

int SAIGeneralParcelInfo::SReinforcePointInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &vCenter );
	saver.Add( 2, &wDir );
	
	return 0;
}

int SAIGeneralParcelInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	
	saver.Add( "Center", &vCenter );
	saver.Add( "Direction", &wDefenceDirection );
	saver.Add( "Type", &eType );
	saver.Add( "Radius", &fRadius );
	saver.Add( "ReinforcePoints", &reinforcePoints );
	
	return 0;
}

int SAIGeneralParcelInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &vCenter );
	saver.Add( 2, &wDefenceDirection );
	saver.Add( 3, &eType );
	saver.Add( 4, &fRadius );
	saver.Add( 5, &reinforcePoints );
	
	return 0;
}


int SAIGeneralSideInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	
	saver.Add( "MobileReinforcement", &mobileScriptIDs );
	saver.Add( "Parcels", &parcels );
	
	return 0;
}

int SAIGeneralSideInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &mobileScriptIDs );
	saver.Add( 2, &parcels );
	
	return 0;
}

int SAIGeneralMapInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Sides", &sidesInfo );
	
	return 0;
}

int SAIGeneralMapInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &sidesInfo );
	
	return 0;
}
