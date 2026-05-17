#ifndef __LINK_OBJECT_H__
#define __LINK_OBJECT_H__

#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "UpdatableObject.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CLinkObject : public IUpdatableObj
{
	DECLARE_SERIALIZE;

	static std::vector< CPtr<CLinkObject> > link2object;
	static std::list<int> deletedObjects;
	static std::list<int> deletedUniqueObjects;

	static std::unordered_map< int, CPtr<CLinkObject> > unitsID2object;
	static int nCurUniqueID;

	int nLink;
	int nUniqueID;
public:
	CLinkObject();
	CLinkObject( const int _nLink ) { SetLink( _nLink ); }
	virtual ~CLinkObject();
	
	void SetUniqueId();
	void SetLink( const int _nLink );
	const int GetLink() const { return nLink; }
	// �������� �� ������ � unitsID2Object
	void Mem2UniqueIdObjs();
	const int GetUniqueId() const { /*NI_ASSERT_T( nUniqueID > 0, "Unique id isn't set" ); */return nUniqueID; }

	static void Clear();
	static void ClearLinks();
	static CLinkObject* GetObjectByLink( const int nLink );
	static void Segment();
	// ������, ���� ������� ������������ nUniqueID
	static CLinkObject* GetObjectByUniqueId( const int nUniqueID );
	
	// ���������� 0, ���� ������� ������������ nUniqueID	
	static CLinkObject* GetObjectByUniqueIdSafe( const int nUniqueID )
	{
		NI_ASSERT_T( nUniqueID > 0, "Wrong object" );
		if ( unitsID2object.find( nUniqueID ) == unitsID2object.end() )
			return 0;
		else
			return unitsID2object[nUniqueID];
	}

	// ��� nSize ��������� ������
	static void GetFreeLinks( std::list<int> *pLinks, const int nSize );
	
	// for Saving/Loading of static members
	friend class CStaticMembers;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ���������� 0, ���� ������� ������������ nUniqueID	
template<class T>
inline T* GetObjectByUniqueIdSafe( const int nUniqueID )
{
	CLinkObject *pLinkObject = CLinkObject::GetObjectByUniqueIdSafe( nUniqueID );
	return 
		pLinkObject ? checked_cast<T*>( pLinkObject ) : 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __LINK_OBJECT_H__
