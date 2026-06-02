#include "StdAfx.h"

#include "BasicObjectFactory.h"
IRefCount* CBasicObjectFactory::CreateObject( int nTypeID )
{
	CNewFuncsMap::iterator pos = newfuncs.find( nTypeID );
	NI_ASSERT_SLOW_T( pos != newfuncs.end(), NStr::Format("unregistered object 0x%x - no new-function", nTypeID) );
#ifdef _DO_ASSERT_SLOW
	if ( pos == newfuncs.end() )
		return 0;
#endif // _DO_ASSERT_SLOW
	ObjectFactoryNewFunc newFunc = pos->second;
	return (*newFunc)();
}
void CBasicObjectFactory::RegisterType( int nObjectTypeID, const type_info *pObjectTypeInfo, ObjectFactoryNewFunc newFunc )
{
	NI_ASSERT_SLOW_T( (pObjectTypeInfo != 0) && (newFunc != 0), "Can't register type with empty type info or new-function" );
	NI_ASSERT_SLOW_T( newfuncs.find(nObjectTypeID) == newfuncs.end(), NStr::Format("Object 0x%x (%s) already registered in 'NewFuncs'", nObjectTypeID, pObjectTypeInfo->name()) );
	NI_ASSERT_SLOW_T( rttis.find(pObjectTypeInfo) == rttis.end(), NStr::Format("Object 0x%x (%s) already registered 'RTTIs'", nObjectTypeID, pObjectTypeInfo->name()) );
	newfuncs[nObjectTypeID] = newFunc;
	rttis[pObjectTypeInfo] = nObjectTypeID;
}
void CBasicObjectFactory::RegisterType( int nObjectTypeID, ObjectFactoryNewFunc newFunc )
{
	NI_ASSERT_T( newFunc != 0, "can't register type with empty new-function" );
	CPtr<IRefCount> pObj = (*newFunc)();
	NI_ASSERT_T( pObj != 0, "new-function can't create object for RTTI extraction" );
	RegisterType( nObjectTypeID, &( typeid(*pObj) ), newFunc );
}
void CBasicObjectFactory::Aggregate( IObjectFactory *pFactory )
{
	std::vector<SObjectFactoryTypeInfo> types( pFactory->GetNumKnownTypes() );
	pFactory->GetKnownTypes( &(types[0]), types.size() );
	for ( std::vector<SObjectFactoryTypeInfo>::const_iterator pos = types.begin(); pos != types.end(); ++pos )
		RegisterType( pos->nTypeID, reinterpret_cast<const type_info*>(pos->pTypeInfo), pos->newFunc );
}
class CTypeInfoFindFunctional
{
	int nTypeID;
public:
	CTypeInfoFindFunctional( int _nTypeID ) : nTypeID( _nTypeID ) {  }
	bool operator()( const SObjectFactoryTypeInfo &info ) { return info.nTypeID == nTypeID; }
};
void CBasicObjectFactory::GetKnownTypes( SObjectFactoryTypeInfo *pInfoBuffer, int nBufferSize )
{
	NI_ASSERT_T( GetNumKnownTypes() <= nBufferSize, "buffer for types are too small" );
	nBufferSize = GetNumKnownTypes();
	int i = 0;
	for ( CNewFuncsMap::const_iterator pos = newfuncs.begin(); pos != newfuncs.end(); ++pos, ++i )
	{
		pInfoBuffer[i].nTypeID = pos->first;
		pInfoBuffer[i].newFunc = pos->second;
		pInfoBuffer[i].pTypeInfo = 0;
	}
	for ( CRTTIMap::const_iterator pos = rttis.begin(); pos != rttis.end(); ++pos )
	{
		SObjectFactoryTypeInfo *pInfo = std::find_if( pInfoBuffer, pInfoBuffer + nBufferSize, CTypeInfoFindFunctional(pos->second) );
		if ( pInfo != pInfoBuffer + nBufferSize )
			pInfo->pTypeInfo = pos->first;
	}
}
