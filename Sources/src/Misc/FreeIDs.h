#ifndef __FREEIDS_H__
#define __FREEIDS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <unordered_set>
#include <vector>
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*											Free Identifiers														*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFreeIds
{
	enum { NUM_OF_ELEMENTS = 200 };

	//CRAP{ for testing
	std::unordered_set<int> givenIDs;
	//CRAP}
	
	std::vector<int> nexts;
	std::vector<int> preds;
	int freePtr, front;

	//
	void AddToFree( int pos ) 
	{ 
		nexts[pos] = freePtr; 
		freePtr = pos; 
		NI_ASSERT_T( freePtr >= nexts.size() || freePtr != nexts[freePtr], "Wrong freeptr" ); 
	}
	
	int GetFreePos();
public:
	explicit CFreeIds( const int nElements = NUM_OF_ELEMENTS ) { Init( nElements ); }

	void Init( const int nElements = NUM_OF_ELEMENTS );
	void Clear();
	
	int begin( )	const	{ return front; }
	int end( ) const { return 0; }
	int GetNext( int id ) const	{ return nexts[id]; }
	int GetPred( int id ) const	{ return preds[id]; }

	int GetFreeId();
	void AddToFreeId( int id );

	inline int CFreeIds::operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		
		int nCnt = -1;
		saver.Add( 6, &nCnt );

		// CRAP{ for compatibility with legacy saves
		if ( saver.IsReading() && nCnt == 0 )
		{
			std::vector<WORD> nextsWord;
			std::vector<WORD> predsWord;
			std::unordered_set<WORD> givenIDsWord;

			saver.Add( 1, &nextsWord );
			saver.Add( 2, &predsWord );
			saver.Add( 3, &freePtr );
			saver.Add( 4, &front );
			saver.Add( 5, &givenIDsWord );

			nexts.reserve( nextsWord.size() ); preds.reserve( predsWord.size() );

			nexts.assign( nextsWord.begin(), nextsWord.end() );
			preds.assign( predsWord.begin(), predsWord.end() );

			givenIDs.clear(); givenIDs.insert( givenIDsWord.begin(), givenIDsWord.end() );
		}
		// CRAP}
		else
		{
			saver.Add( 1, &nexts );
			saver.Add( 2, &preds );
			saver.Add( 3, &freePtr );
			saver.Add( 4, &front );
			saver.Add( 5, &givenIDs );
		}

		// the nexd it is 7

		return 0;
	}

};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __FREEIDS_H__
