#include "stdafx.h"

#include "LinkObject.h"

#include "MPLog.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector< CPtr<CLinkObject> > CLinkObject::link2object;
std::list<int> CLinkObject::deletedObjects;
std::unordered_map< int, CPtr<CLinkObject> > CLinkObject::unitsID2object;
std::list<int> CLinkObject::deletedUniqueObjects;
int CLinkObject::nCurUniqueID = 0;

extern NTimer::STime curTime;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CLinkObject::CLinkObject()
: nLink( -1 )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CLinkObject::~CLinkObject()
{
	if ( GetLink() > 0 )
	{
		NI_ASSERT_T( link2object.size() > GetLink(), "Wrong size" );
		deletedObjects.push_back( GetLink() );

		NI_ASSERT_T( nUniqueID != 0, "wrong unique id" );
		deletedUniqueObjects.push_back( nUniqueID );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CLinkObject::SetUniqueId()
{
//	NI_ASSERT_T( nUniqueID == -1, "Double set of unique id" );
	nUniqueID = ++nCurUniqueID;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CLinkObject::SetLink( const int _nLink )
{
	nLink = _nLink;
	// CRAP{ ����� ��������� ������ �����
	if ( _nLink > 0 )
	// }CRAP
	{
		NI_ASSERT_T( link2object.size() <= _nLink || link2object[_nLink] == 0, NStr::Format( "Repeated link %d", _nLink ) );

		if ( link2object.size() <= _nLink )
			link2object.resize( ( nLink + 1 ) * 1.5 );

		link2object[nLink] = this;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CLinkObject::Mem2UniqueIdObjs()
{ 
	NI_ASSERT_T( nUniqueID > 0, "Unique id isn't set" );
	unitsID2object.insert( std::pair< int, CPtr<CLinkObject> >( nUniqueID, this ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CLinkObject* CLinkObject::GetObjectByLink( const int nLink )
{
	if ( nLink >= link2object.size() || nLink <= 0 )
		return 0;
	else
		return link2object[nLink];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CLinkObject::Segment()
{
	for ( std::list<int>::iterator iter = deletedObjects.begin(); iter != deletedObjects.end(); ++iter )
		link2object[*iter] = 0;
	for ( std::list<int>::iterator iter = deletedUniqueObjects.begin(); iter != deletedUniqueObjects.end(); ++iter )
	{
		int nID = *iter;
		if ( unitsID2object.find( nID ) != unitsID2object.end() )
			unitsID2object.erase( nID );
	}
	deletedObjects.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CLinkObject::Clear()
{
	for ( int i = 0; i < link2object.size(); ++i )
	{
		if ( link2object[i] != 0 && link2object[i]->IsValid() )
			static_cast<CLinkObject*>( link2object[i].GetPtr() )->SetLink( -1 );
	}
	
	link2object.clear();
	deletedObjects.clear();
	deletedUniqueObjects.clear();
	unitsID2object.clear();

	nCurUniqueID = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CLinkObject::ClearLinks()
{
	for ( int i = 0; i < link2object.size(); ++i )
	{
		if ( link2object[i] != 0 && link2object[i]->IsValid() )
			static_cast<CLinkObject*>( link2object[i].GetPtr() )->SetLink( -1 );
	}
	
	link2object.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CLinkObject* CLinkObject::GetObjectByUniqueId( const int nUniqueID )
{ 
	NI_ASSERT_T( nUniqueID > 0, "Wrong object" );
	NI_ASSERT_T( unitsID2object.find( nUniqueID ) != unitsID2object.end(), NStr::Format( "Wrong unique id (%d)", nUniqueID ) );
	return unitsID2object[nUniqueID];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CLinkObject::GetFreeLinks( std::list<int> *pLinks, const int nSize )
{
	pLinks->clear();
	for ( int i = 1; i < link2object.size() && pLinks->size() < nSize; ++i )
	{
		if ( GetObjectByLink( i ) == 0 )
			pLinks->push_back( i );
	}

	if ( pLinks->size() < nSize )
	{
		int nLink = Max( (int)link2object.size(), 1 );
		while ( pLinks->size() < nSize )
		{
			pLinks->push_back( nLink );
			++nLink;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
