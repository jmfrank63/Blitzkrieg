#include "StdAfx.h"

#include "StatSystem.h"
int SStatEntry::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szName );
	saver.Add( 2, &szValue );
	saver.Add( 3, &fCurr );
	saver.Add( 4, &fMin );
	saver.Add( 5, &fAve );
	saver.Add( 6, &fMax );
	return 0;
}
int CStatSystem::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &entriesList );
	if ( saver.IsReading() )
	{
		entriesMap.clear();
		for ( CEntriesList::iterator it = entriesList.begin(); it != entriesList.end(); ++it )
			entriesMap[it->szName] = &( *it );
	}
	saver.Add( 2, &nPosX );
	saver.Add( 3, &nPosY );
	return 0;
}
bool CStatSystem::Draw( IGFX *pGFX )
{
	pGFX->SetupDirectTransform();
	int nX = nPosX, nY = nPosY;
	for ( CEntriesList::const_iterator it = entriesList.begin(); it != entriesList.end(); ++it )
	{
		if ( it->szValue.empty() || (it->szValue == it->szName) )
			pGFX->DrawStringA( it->szName.c_str(), nX, nY );
		else
			pGFX->DrawStringA( NStr::Format("%s = %s", it->szName.c_str(), it->szValue.c_str()), nX, nY );
		nY += 20;
	}
	pGFX->RestoreTransform();
	return true;
}
void CStatSystem::Visit( ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSceneObject( this );
}
