#include "StdAfx.h"
#include "../AILogic//AILogic.h"
#include "../AILogic//UnitCreation.h"
#include "UnitSide.h"

void FillVectorOfSides( std::vector<std::string> &sides )
{
	std::vector<CUnitCreation::SPartyDependentInfo> partyDependentInfo;
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "partys.xml" , STREAM_ACCESS_READ );
	CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
	tree.Add( "PartyInfo", &partyDependentInfo );
	for ( std::vector<CUnitCreation::SPartyDependentInfo>::const_iterator partyIterator = partyDependentInfo.begin(); partyIterator != partyDependentInfo.end(); ++partyIterator )
	{
		sides.push_back( partyIterator->szPartyName );
	}
}
