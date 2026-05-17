#ifndef __BASICOBJECTFACTORY_H__
#define __BASICOBJECTFACTORY_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBasicObjectFactory : public IObjectFactory
{
	typedef std::unordered_map<int, ObjectFactoryNewFunc> CNewFuncsMap;
	typedef std::unordered_map<const type_info*, int, SDefaultPtrHash> CRTTIMap;
	// таблица функций дл€ создани€ новых объектов
	CNewFuncsMap newfuncs;
	CRTTIMap rttis;
private:
	// внутренн€€ регистраци
	void RegisterType( int nObjectTypeID, const type_info *pObjectTypeInfo, ObjectFactoryNewFunc newFunc );
public:
	// создать объект по его typeID
	virtual IRefCount* STDCALL CreateObject( int nTypeID );
	// зарегистрировать тип
	virtual void STDCALL RegisterType( int nObjectTypeID, ObjectFactoryNewFunc newFunc );
	// аггрегировать другую factory внутрь этой (перерегистрировать еЄ объекты на эту фабрику)
	virtual void STDCALL Aggregate( IObjectFactory *pFactory );
	// получить количество типов объектов, которые эта фабрика (+ все аггрегированные в неЄ) может создать
	virtual int STDCALL GetNumKnownTypes() { return newfuncs.size(); }
	// получить type info объектов, которые эта фабрика (+ все аггрегированные в неЄ) может создать
	virtual void STDCALL GetKnownTypes( SObjectFactoryTypeInfo *pInfoBuffer, int nBufferSize );
	// получить typeID объекта по указателю на него
	virtual int STDCALL GetObjectTypeID( IRefCount *pObj ) const
	{
		NI_ASSERT_T( pObj != 0, "can't get object type ID from NULL pointer" );
		const type_info &rtti = typeid( *pObj );
		CRTTIMap::const_iterator pos = rttis.find( &rtti );
		return pos != rttis.end() ? pos->second : -1;
	}
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define REGISTER_CLASS( pFactory, nTypeID, className ) pFactory->RegisterType( nTypeID, reinterpret_cast<ObjectFactoryNewFunc>( className##::CreateNewClassInstanceInternal ) );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BASICOBJECTFACTORY_H__
