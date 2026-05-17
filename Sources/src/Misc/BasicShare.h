#ifndef __BASICSHARE_H__
#define __BASICSHARE_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template < class TKey, class TValue, int NClassTypeID, class THash = std::hash<TKey> >
class CBasicShare
{
public:	
	typedef std::unordered_map< TKey, CObj<TValue>, THash > CDataHash;
	typedef typename CDataHash::iterator iterator;
	typedef typename CDataHash::const_iterator const_iterator;
private:
	int nID;															// data identifier for this share
	CDataHash data;												// data
	ESharedDataSerialMode eSerialMode;		// serialization mode
	ESharedDataSharingMode eShareMode;		// sharing mode
	std::string szExt;										// extension
protected:
	virtual TValue* Create( const TKey &key )
	{
		TValue *pObj = CreateObject<TValue>( NClassTypeID );
		pObj->SetSharedResourceName( key );
		if ( pObj->Load(true) ) 
			return pObj;
		// delete - can't load data :P
		pObj->AddRef();
		pObj->Release();
		return 0;
	}
	int GetID() const { return nID; }
public:
	CBasicShare( int _nID, const char *pszExt = "" )
		: nID( _nID ), eSerialMode( SDSM_MERGE ), eShareMode( SDSM_SHARE ), szExt( pszExt ) {  }
	//
	bool Init() 
	{ 
		SetGlobalVar( TValue::GetSharedResourceExtVarName(), szExt.c_str() );
		return true; 
	}
	void Clear() { data.clear(); }
	//
	void SetExt( const char *pszExt ) 
	{ 
		szExt = pszExt; 
		SetGlobalVar( TValue::GetSharedResourceExtVarName(), szExt.c_str() );
		if ( !data.empty() ) 
		{
			ClearContainers();
			ReloadAllData();
		}
	}
	//
	void SetSerialMode( ESharedDataSerialMode _eSerialMode ) { eSerialMode = _eSerialMode; }
	void SetShareMode( ESharedDataSharingMode _eShareMode ) { eShareMode = _eShareMode; }
	//
	void AddPair( const TKey &key, TValue *pValue ) 
	{ 
		NI_ASSERT_SLOW_T( data.find(key) == data.end(), NStr::Format("Adding data to share with key \"%s\", which is already exists", key.c_str()) );
		data[key] = pValue; 
		pValue->SetSharedResourceName( key );
	}
	//
	TValue* Get( const TKey &key )
	{
		CDataHash::iterator pos = data.find( key );
		if ( (pos == data.end()) || (pos->second == 0) )
		{
			TValue *pRes = Create( key );
			if ( pRes ) 
				data[key] = pRes;
			return pRes;
		}
		else if ( eShareMode == SDSM_RELOAD )
		{
			pos->second->ClearInternalContainer();
			pos->second->Load( true );
		}
		return pos->second;
	}
	
	bool Remove( const TKey &key )
	{
		CDataHash::iterator pos = data.find( key );
		if ( pos != data.end() ) 
		{
			data.erase( pos );
			return true;
		}
		else 
			return false;
	}

	void Serialize( interface IStructureSaver *pSS )
	{
		CSaverAccessor saver = pSS;
		if ( saver.IsReading() ) 
		{
			if ( eSerialMode == SDSM_REPLACE )
			{
				saver.Add( GetID(), &data );
				for ( CDataHash::const_iterator i = data.begin(); i != data.end(); ++i )
				{
					if ( i->second == 0 )
						continue;
					i->second->SetSharedResourceName( i->first );
					i->second->Load( true );
				}
			}
			else if ( eSerialMode == SDSM_MERGE )
			{
				CDataHash holder = data;
				//
				saver.Add( GetID(), &data );
				for ( CDataHash::const_iterator i = data.begin(); i != data.end(); ++i )
				{
					if ( i->second == 0 )
						continue;
					CDataHash::iterator pos = holder.find( i->first );
					if ( pos == holder.end() )
					{
						i->second->SetSharedResourceName( i->first );
						i->second->Load( true );
					}
					else
					{
						i->second->SwapData( pos->second );
						i->second->SetSharedResourceName( i->first );
					}
				}
			}
			else if ( eSerialMode == SDSM_ADD )
			{
				CDataHash holder;
				//
				saver.Add( GetID(), &holder );
				for ( CDataHash::const_iterator i = holder.begin(); i != holder.end(); ++i )
				{
					if ( i->second == 0 )
						continue;
					CDataHash::iterator pos = data.find( i->first );
					if ( pos == data.end() )			// load new data
					{
						i->second->SetSharedResourceName( i->first );
						i->second->Load( true );
					}
					data[i->first] = i->second;
				}
			}
			else
				NI_ASSERT_TF( 0, "unknown serialization mode", return );
		}
		else
			saver.Add( GetID(), &data );
	}
	//
	const TKey* GetKey( TValue *pValue )
	{
		for ( typename CDataHash::const_iterator it = data.begin(); it != data.end(); ++it )
		{
			if ( it->second.GetPtr() == pValue )
				return &( it->first );
		}
		return 0;
	}
	//
	void ClearUnreferencedResources()
	{
		std::list<TKey> keys;
		// form 'candidates-to-delete' list
		for ( CDataHash::const_iterator it = data.begin(); it != data.end(); ++it )
		{
			if ( (it->second->GetRefCounter() & 0x00ffffff) == NRefCount::REF_ADD_OBJ )
				keys.push_back( it->first );
		}
		// delete all candidates from the list
		for ( std::list<TKey>::const_iterator it = keys.begin(); it != keys.end(); ++it )
			data.erase( *it );
	}
	// return freed amount of resources
	int ClearLRUResources( const int nUsage, const int nAmount )
	{
		// form 'candidates-to-delete' list. use std::map to sort elements
		std::multimap<int, TValue*> keys;
		for ( CDataHash::const_iterator it = data.begin(); it != data.end(); ++it )
		{
			const int nLastUsage = it->second->GetSharedResourceLastUsage();
			if ( (nLastUsage < nUsage) && (it->second->GetResourceConsumption() > 0) ) 
				keys.insert( std::multimap<int, TValue*>::value_type(nLastUsage, it->second) );
		}
		// clear selected elements containers
		int nFreedResources = 0;
		for ( std::multimap<int, TValue*>::const_iterator it = keys.begin(); it != keys.end() && nFreedResources < nAmount; ++it )
		{
			nFreedResources += it->second->GetResourceConsumption();
			it->second->ClearInternalContainer();
		}
		return nFreedResources;
	}
	// 
	void ClearContainers()
	{
		for ( CDataHash::const_iterator i = data.begin(); i != data.end(); ++i )
		{
			if ( i->second != 0 )
				i->second->ClearInternalContainer();
		}
	}
	//
	void ReloadAllData()
	{
		for ( CDataHash::const_iterator i = data.begin(); i != data.end(); ++i )
		{
			if ( i->second == 0 )
				continue;
			i->second->SetSharedResourceName( i->first );
			i->second->Load( true );
		}
	}
	//
	const bool HasData( const TKey &key ) const { return data.find( key ) != data.end(); }
	//
	iterator begin() { return data.begin(); }
	const_iterator begin() const { return data.begin(); }
	iterator end() { return data.end(); }
	const_iterator end() const { return data.end(); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define BASIC_SHARE_DECLARE( TShareName, TKey, TValue, NClassTypeID, nShareID, pszExt )	\
class TShareName : public CBasicShare<TKey, TValue, NClassTypeID>												\
{																																												\
public:																																									\
	typedef CBasicShare<TKey, TValue, NClassTypeID> CBase;																\
	TShareName() : CBasicShare<TKey, TValue, NClassTypeID>( nShareID, pszExt ) {  }				\
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BASICSHARE_H__
