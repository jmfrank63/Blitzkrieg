#include "StdAfx.h"

#include "IUIInternal.h"

int SUICommandSequence::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Reversable", &bReversable );
	saver.Add( "Commands", &cmds );
	return 0;
}
int SUICommandSequence::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( false, "" );
	return 0;
};
