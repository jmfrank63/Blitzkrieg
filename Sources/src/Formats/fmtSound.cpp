#include "StdAfx.h"
#include "fmtSound.h"

int CMapSoundInfo::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Name", &szName );
	saver.Add( "Position", &vPos );
	return 0;
}

int CMapSoundInfo::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &szName );
	saver.Add( 2, &vPos );
	return 0;
}
