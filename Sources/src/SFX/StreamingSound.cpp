#include "StdAfx.h"

#include "StreamingSound.h"
#include "../Platform/LegacyAlgorithm.h"
void CPlayList::CreateRandomList()
{
	szRandomized = szMelodies;
	NPlatform::RandomShuffle( szRandomized.begin(), szRandomized.end() );
}
const char* CPlayList::GetNextMelody()
{
	if ( szMelodies.empty() )
		return 0;
	switch ( nSequenceOrder )
	{
		case IPlayList::ORDER_SEQUENTIAL:
			++nCurrentStream;
			return nCurrentStream < szMelodies.size() ? szMelodies[nCurrentStream].c_str() : 0;
		case IPlayList::ORDER_CYCLE:
			nCurrentStream = ( nCurrentStream + 1 ) % szMelodies.size();
			return szMelodies[nCurrentStream].c_str();
		case IPlayList::ORDER_RANDOM:
			nCurrentStream = 0;
			if ( szRandomized.empty() )
			{
				CreateRandomList();
				return GetNextMelody();
			}
			szRandomized.pop_back();
			if ( szRandomized.empty() )
			{
				CreateRandomList();
				return GetNextMelody();
			}
			return szRandomized.back().c_str();
		default:
			return 0;
	}
}
int CPlayList::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szMelodies );
	saver.Add( 2, &szRandomized );
	saver.Add( 3, &nCurrentStream );
	saver.Add( 4, &nSequenceOrder );
	return 0;
}
