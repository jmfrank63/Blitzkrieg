#ifndef __ANTI_ARTILLERY_MANAGER_H__
#define __ANTI_ARTILLERY_MANAGER_H__

#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Heap.h"
#include "AIHashFuncs.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAntiArtillery;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAntiArtilleryManager
{
	DECLARE_SERIALIZE;

	// круги от выстрелов от собственной артиллерии для каждой из сторон
	typedef std::unordered_set<int> CAntiArtilleries;
	std::vector<CAntiArtilleries> antiArtilleries;
	
	static bool IsHeardForParty( CAntiArtillery *pAntiArt, const int nParty );
public:
	void Init();
	void Clear();

	void AddAA( CAntiArtillery *pAA );
	void RemoveAA( CAntiArtillery *pAA );
	void Segment();

	// не сэйвится!
	class CIterator
	{
		int nIterParty;
		int nCurParty;
		CAntiArtilleries::iterator curIter;

		public:
			CIterator( const int nParty );

			const CCircle operator*() const;
			CAntiArtillery* GetAntiArtillery() const;

			void Iterate();
			bool IsFinished() const;
	};

	friend class CIterator;
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __ANTI_ARTILLERY_MANAGER_H__
