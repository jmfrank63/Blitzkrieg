#include "stdafx.h"

#include "LinkObject.h"
int CLinkObject::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &nLink );
	saver.Add( 2, &nUniqueID );

	return 0;
}
