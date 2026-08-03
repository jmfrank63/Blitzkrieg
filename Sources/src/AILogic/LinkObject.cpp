#include "StdAfx.h"

#include "LinkObject.h"

#include "MPLog.h"
int CLinkObject::nCurUniqueID = 0;

// Like GetLinkedObjects() below, these registries are leaked on purpose:
// CLinkObject instances held by globals in other translation units
// (theSuspendedUpdates and friends) are released during DLL teardown, after
// namespace-scope statics of this file would already have been destroyed.
std::vector< CPtr<CLinkObject> >& CLinkObject::Link2Object()
{
	static std::vector< CPtr<CLinkObject> >* p = new std::vector< CPtr<CLinkObject> >;
	return *p;
}
std::list<int>& CLinkObject::DeletedObjects()
{
	static std::list<int>* p = new std::list<int>;
	return *p;
}
std::unordered_map< int, CPtr<CLinkObject> >& CLinkObject::UnitsID2Object()
{
	static std::unordered_map< int, CPtr<CLinkObject> >* p = new std::unordered_map< int, CPtr<CLinkObject> >;
	return *p;
}
std::list<int>& CLinkObject::DeletedUniqueObjects()
{
	static std::list<int>* p = new std::list<int>;
	return *p;
}

namespace
{
std::set<CLinkObject*>& GetLinkedObjects()
{
	// CLinkObject instances can be constructed and destroyed by globals in
	// other translation units while this DLL is being initialized or unloaded.
	// Keep the registry alive for the DLL lifetime to avoid static
	// initialization/destruction order dependencies.
	static std::set<CLinkObject*>* linkedObjects = new std::set<CLinkObject*>;
	return *linkedObjects;
}
}

extern NTimer::STime curTime;
CLinkObject::CLinkObject()
: nLink( -1 ), nUniqueID( 0 )
{
}
CLinkObject::CLinkObject( const int _nLink )
: nLink( -1 ), nUniqueID( 0 )
{
	SetLink( _nLink );
}
CLinkObject::~CLinkObject()
{
	GetLinkedObjects().erase( this );

	if ( GetLink() > 0 )
	{
		if ( Link2Object().size() <= GetLink() )
		{
			NStr::DebugTrace(
				"CLinkObject::~CLinkObject invariant fail: link=%d size=%d unique=%d this=0x%08x\n",
				GetLink(),
				(int)Link2Object().size(),
				nUniqueID,
				(unsigned int)(size_t)this
			);
		}
		NI_ASSERT_T(
			Link2Object().size() > GetLink(),
			NStr::Format(
				"Wrong size: link=%d size=%d unique=%d this=0x%08x",
				GetLink(),
				(int)Link2Object().size(),
				nUniqueID,
				(unsigned int)(size_t)this
			)
		);
		DeletedObjects().push_back( GetLink() );

		NI_ASSERT_T( nUniqueID != 0, NStr::Format( "wrong unique id for link=%d this=0x%08x", GetLink(), (unsigned int)(size_t)this ) );
		DeletedUniqueObjects().push_back( nUniqueID );
	}
}
void CLinkObject::SetUniqueId()
{
	nUniqueID = ++nCurUniqueID;
}
void CLinkObject::SetLink( const int _nLink )
{
	const int oldLink = nLink;

	if ( oldLink > 0 && _nLink <= 0 )
		GetLinkedObjects().erase( this );

	nLink = _nLink;
	if ( _nLink > 0 )
	{
		GetLinkedObjects().insert( this );

		NI_ASSERT_T( Link2Object().size() <= _nLink || Link2Object()[_nLink] == 0, NStr::Format( "Repeated link %d", _nLink ) );

		if ( Link2Object().size() <= _nLink )
			Link2Object().resize( ( nLink + 1 ) * 1.5 );

		Link2Object()[nLink] = this;
	}
}
void CLinkObject::Mem2UniqueIdObjs()
{
	NI_ASSERT_T( nUniqueID > 0, "Unique id isn't set" );
	UnitsID2Object().insert( std::pair< int, CPtr<CLinkObject> >( nUniqueID, this ) );
}
CLinkObject* CLinkObject::GetObjectByLink( const int nLink )
{
	if ( nLink >= Link2Object().size() || nLink <= 0 )
		return 0;
	else
		return Link2Object()[nLink];
}
void CLinkObject::Segment()
{
	for ( std::list<int>::iterator iter = DeletedObjects().begin(); iter != DeletedObjects().end(); ++iter )
		Link2Object()[*iter] = 0;
	for ( std::list<int>::iterator iter = DeletedUniqueObjects().begin(); iter != DeletedUniqueObjects().end(); ++iter )
	{
		int nID = *iter;
		if ( UnitsID2Object().find( nID ) != UnitsID2Object().end() )
			UnitsID2Object().erase( nID );
	}
	DeletedObjects().clear();
}
void CLinkObject::Clear()
{
	std::vector<CLinkObject*> linked;
	linked.reserve( GetLinkedObjects().size() );
	for ( std::set<CLinkObject*>::iterator it = GetLinkedObjects().begin(); it != GetLinkedObjects().end(); ++it )
		linked.push_back( *it );

	for ( int i = 0; i < linked.size(); ++i )
	{
		if ( linked[i] != 0 )
			linked[i]->SetLink( -1 );
	}

	for ( int i = 0; i < Link2Object().size(); ++i )
	{
		if ( Link2Object()[i] != 0 )
			static_cast<CLinkObject*>( Link2Object()[i].GetPtr() )->SetLink( -1 );
	}

	for ( std::unordered_map< int, CPtr<CLinkObject> >::iterator it = UnitsID2Object().begin(); it != UnitsID2Object().end(); ++it )
	{
		if ( it->second != 0 )
			static_cast<CLinkObject*>( it->second.GetPtr() )->SetLink( -1 );
	}

	Link2Object().clear();
	DeletedObjects().clear();
	DeletedUniqueObjects().clear();
	UnitsID2Object().clear();
	GetLinkedObjects().clear();

	nCurUniqueID = 0;
}
void CLinkObject::ClearLinks()
{
	std::vector<CLinkObject*> linked;
	linked.reserve( GetLinkedObjects().size() );
	for ( std::set<CLinkObject*>::iterator it = GetLinkedObjects().begin(); it != GetLinkedObjects().end(); ++it )
		linked.push_back( *it );

	for ( int i = 0; i < linked.size(); ++i )
	{
		if ( linked[i] != 0 )
			linked[i]->SetLink( -1 );
	}

	for ( int i = 0; i < Link2Object().size(); ++i )
	{
		if ( Link2Object()[i] != 0 )
			static_cast<CLinkObject*>( Link2Object()[i].GetPtr() )->SetLink( -1 );
	}

	for ( std::unordered_map< int, CPtr<CLinkObject> >::iterator it = UnitsID2Object().begin(); it != UnitsID2Object().end(); ++it )
	{
		if ( it->second != 0 )
			static_cast<CLinkObject*>( it->second.GetPtr() )->SetLink( -1 );
	}

	Link2Object().clear();
	GetLinkedObjects().clear();
}
CLinkObject* CLinkObject::GetObjectByUniqueId( const int nUniqueID )
{
	NI_ASSERT_T( nUniqueID > 0, "Wrong object" );
	NI_ASSERT_T( UnitsID2Object().find( nUniqueID ) != UnitsID2Object().end(), NStr::Format( "Wrong unique id (%d)", nUniqueID ) );
	return UnitsID2Object()[nUniqueID];
}
void CLinkObject::GetFreeLinks( std::list<int> *pLinks, const int nSize )
{
	pLinks->clear();
	for ( int i = 1; i < Link2Object().size() && pLinks->size() < nSize; ++i )
	{
		if ( GetObjectByLink( i ) == 0 )
			pLinks->push_back( i );
	}

	if ( pLinks->size() < nSize )
	{
		int nLink = Max( (int)Link2Object().size(), 1 );
		while ( pLinks->size() < nSize )
		{
			pLinks->push_back( nLink );
			++nLink;
		}
	}
}
