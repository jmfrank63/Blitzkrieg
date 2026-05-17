#ifndef __STATSYSTEM_H__
#define __STATSYSTEM_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SStatEntry
{
	std::string szName;										// entry name
	std::string szValue;									// value...
	double fCurr;													// current value
	double fMin, fAve, fMax;							// min/max/ave values
	//
	SStatEntry() : fCurr( 0 ), fMin( 0 ), fAve( 0 ), fMax( 0 ) {  }
	SStatEntry( const char *pszName ) : szName( pszName ), fCurr( 0 ), fMin( 0 ), fAve( 0 ), fMax( 0 ) {  }
	//
	int operator&( IStructureSaver &ss );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CStatSystem : public IStatSystem
{
	OBJECT_NORMAL_METHODS( CStatSystem );
	DECLARE_SERIALIZE;
	//
	typedef std::list<SStatEntry> CEntriesList;
	typedef std::unordered_map<std::string, SStatEntry*> CEntriesPtrMap;
	CEntriesList entriesList;							// entries list for fast sequential access and sequence ordering
	CEntriesPtrMap entriesMap;						// entries map for fast access
	//
	int nPosX, nPosY;											// screen coords
public:
	// add/remove statistics entry
	virtual void STDCALL AddEntry( const char *pszName )
	{
		if ( entriesMap.find( pszName ) == entriesMap.end() )
		{
			// add to list
			entriesList.push_back( SStatEntry(pszName) );
			// add to map
			entriesMap[pszName] = &( entriesList.back() );
		}
	}
	virtual void STDCALL RemoveEntry( const char *pszName )
	{
		if ( entriesMap.find( pszName ) != entriesMap.end() )
		{
			// remove from map
			entriesMap.erase( pszName );
			// remove from list
			for ( CEntriesList::iterator it = entriesList.begin(); it != entriesList.end(); ++it )
			{
				if ( it->szName == pszName )
				{
					entriesList.erase( it );
					break;
				}
			}
		}
	}
	// update entry
	virtual void STDCALL UpdateEntry( const char *pszName, double val )
	{
		CEntriesPtrMap::iterator it = entriesMap.find( pszName );
		// add entry, if still not exist
		if ( it == entriesMap.end() )
		{
			AddEntry( pszName );
			it = entriesMap.find( pszName );
		}
		// update data
		it->second->fCurr = val;
		it->second->szValue = NStr::Format( "%g", it->second->fCurr );
	}
	virtual void STDCALL UpdateEntry( const char *pszName, const char *pszVal )
	{
		CEntriesPtrMap::iterator it = entriesMap.find( pszName );
		// add entry, if still not exist
		if ( it == entriesMap.end() )
		{
			AddEntry( pszName );
			it = entriesMap.find( pszName );
		}
		// update data
		it->second->szValue = pszVal;
	}
	virtual void STDCALL ResetEntry( const char *pszName )
	{
		CEntriesPtrMap::iterator it = entriesMap.find( pszName );
		if ( it != entriesMap.end() )
		{
			it->second->fMin = it->second->fMax = it->second->fAve = it->second->fCurr = 0;
			it->second->szValue.clear();
		}
	}
	//
	virtual void STDCALL SetPosition( int _nX, int _nY ) { nPosX = _nX, nPosY = _nY; }
	// update object
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false ) { return true; }
	// drawing
	virtual bool STDCALL Draw( IGFX *pGFX );
	// visiting
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __STATSYSTEM_H__
