#include "stdafx.h"

#include "AIEditorInternal.h"
int CAIEditor::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 2, &pGameSegment );
	return 0;
}
