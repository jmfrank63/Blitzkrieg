#ifndef __LINK_OBJECT_H__
#define __LINK_OBJECT_H__

#pragma ONCE
#include "UpdatableObject.h"
class CLinkObject : public IUpdatableObj
{
	DECLARE_SERIALIZE;

	// Deliberately leaked singletons: other globals (theSuspendedUpdates etc.)
	// release CLinkObject instances during DLL teardown, after namespace-scope
	// statics of this class would already have been destroyed.
	static std::vector< CPtr<CLinkObject> >& Link2Object();
	static std::list<int>& DeletedObjects();
	static std::list<int>& DeletedUniqueObjects();

	static std::unordered_map< int, CPtr<CLinkObject> >& UnitsID2Object();
	static int nCurUniqueID;

	int nLink;
	int nUniqueID;
public:
	CLinkObject();
	CLinkObject( const int _nLink );
	virtual ~CLinkObject();
	
	void SetUniqueId();
	void SetLink( const int _nLink );
	const int GetLink() const { return nLink; }
	void Mem2UniqueIdObjs();
	const int GetUniqueId() const { /*NI_ASSERT_T( nUniqueID > 0, "Unique id isn't set" ); */return nUniqueID; }

	static void Clear();
	static void ClearLinks();
	static CLinkObject* GetObjectByLink( const int nLink );
	static void Segment();
	static CLinkObject* GetObjectByUniqueId( const int nUniqueID );
	
	static CLinkObject* GetObjectByUniqueIdSafe( const int nUniqueID )
	{
		NI_ASSERT_T( nUniqueID > 0, "Wrong object" );
		if ( UnitsID2Object().find( nUniqueID ) == UnitsID2Object().end() )
			return 0;
		else
			return UnitsID2Object()[nUniqueID];
	}

	static void GetFreeLinks( std::list<int> *pLinks, const int nSize );
	
	friend class CStaticMembers;
};
template<class T>
inline T* GetObjectByUniqueIdSafe( const int nUniqueID )
{
	CLinkObject *pLinkObject = CLinkObject::GetObjectByUniqueIdSafe( nUniqueID );
	return 
		pLinkObject ? checked_cast<T*>( pLinkObject ) : 0;
}
#endif // __LINK_OBJECT_H__
