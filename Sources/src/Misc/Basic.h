#ifndef __BASIC_H__
#define __BASIC_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\zlib\zlib.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// define 'interface' keyword if not already defined
#ifndef interface
#define interface struct
#endif // interface
// define STDCALL if not already defined
#ifndef STDCALL
#define STDCALL __stdcall
#endif // STDCALL
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <int N> struct SGenericNumber { int operator()() const { return N; } };
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** base interface, which supports refcounting system
// ** all interfaces, which will use refcounting system, must be derived from this one
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IRefCount
{
	// refcounting
	virtual void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff ) = 0;
	virtual void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff ) = 0;
	virtual bool STDCALL IsValid() const = 0;
	// service
	virtual IRefCount* STDCALL QI( int nInterfaceTypeID ) { return 0; }
	virtual int STDCALL operator&( interface IStructureSaver &ss ) { return 0; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef _DO_ASSERT_SLOW
#define ADD_REF_PREGUARD( ref ) const int __nOldRef = ref
#define ADD_REF_POSTGUARD( ref ) if ( (__nOldRef & (~nMask)) != (ref & (~(nMask))) ) { _asm { int 3 } }
#else
#define ADD_REF_PREGUARD( ref )
#define ADD_REF_POSTGUARD( ref )
#endif // _DO_ASSERT_SLOW
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TBase>
class CTRefCount : public TBase
{
protected:
	int nRefData;													// refcounting data. DO NOT USE IT DIRECTLY!!!
	// just decrease reference counter
	void DecRef( int nRef ) { nRefData -= nRef; }
	// function should clear contents of object, easy to implement via consequent calls to
	// destructor and constructor, this function should not be called directly, use Clear()
	virtual void STDCALL DestroyContents() {  }
	// reset data in class to default values, saves RefCount from destruction
	void InternalClear() { AddRef( 1 ); DestroyContents(); DecRef( 1 ); }
	//
	CTRefCount() : nRefData( 0 ) {  }
	virtual ~CTRefCount() {  }
public:
	virtual void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff ) 
	{ 
		ADD_REF_PREGUARD( nRefData );
		nRefData += nRef; 
		ADD_REF_POSTGUARD( nRefData );
	}
	virtual void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff )
	{
		DecRef( nRef );
		if ( (nRefData & 0x7fffffff) == 0 )
			delete this;
		else if ( (nRefData & nMask) == 0 )
		{
			AddRef();
			DestroyContents();
			nRefData |= 0x80000000;
			Release();
		}
	}
	virtual bool STDCALL IsValid() const { return (nRefData & 0x80000000) == 0; }
	// non-virtual service functions for internal use
	// check for self-validity
	bool IsSelfValid() const { return this != 0 && !IsValid(); }
};
// this MACRO defines some usefull functions for the class implementation
#define OBJECT_SERVICE_METHODS( classname )	\
public:																			\
	virtual void STDCALL DestroyContents() { classname::~classname(); int nHoldRefs = nRefData; ::new(this) classname; nRefData += nHoldRefs; } \
	static IRefCount* STDCALL CreateNewClassInstanceInternal() { return new classname(); }	\
private:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** object factory interface - metaobject, which produces other objects
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef IRefCount* (STDCALL *ObjectFactoryNewFunc)(); // функция для создания нового объекта
struct SObjectFactoryTypeInfo
{
	int nTypeID;													// user-defined typeID
	const void *pTypeInfo;								// system-dependent type info (can be different each program run)
	ObjectFactoryNewFunc newFunc;					// функция для создания нового объекта
};
interface IObjectFactory
{
	// создать объект по его typeID
	virtual IRefCount* STDCALL CreateObject( int nTypeID ) = 0;
	// зарегистрировать тип
	virtual void STDCALL RegisterType( int nObjectTypeID, ObjectFactoryNewFunc newFunc ) = 0;
	// аггрегировать другую factory внутрь этой (перерегистрировать её объекты на эту фабрику)
	virtual void STDCALL Aggregate( IObjectFactory *pFactory ) = 0;
	// получить количество типов объектов, которые эта фабрика (+ все аггрегированные в неё) может создать
	virtual int STDCALL GetNumKnownTypes() = 0;
	// получить type info объектов, которые эта фабрика (+ все аггрегированные в неё) может создать
	virtual void STDCALL GetKnownTypes( SObjectFactoryTypeInfo *pInfoBuffer, int nBufferSize ) = 0;
	// получить typeID объекта по указателю на него
	virtual int STDCALL GetObjectTypeID( IRefCount *pObj ) const = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** object loader interface - for complex data loading
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IObjectLoader
{
	virtual bool STDCALL Init( interface ISingleton *pSingleton ) = 0;
	// завершить всю работу с Loader'ом
	virtual void STDCALL Done() = 0;
	// загрузить данные в заранее созданный объект
	virtual bool STDCALL LoadObject( const char *pszKey, IRefCount *pObject ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** module descriptor
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IModuleChecker
{
	// check module functionality - return some kind of 'grade' for this module
	virtual int STDCALL CheckFunctionality() const = 0;
	// set module functionality limits
	virtual void STDCALL SetModuleFunctionalityLimits() const = 0;
};
struct SModuleDescriptor
{
	// main module information
	const char *pszName;									// module name
	int nType;														// type (gfx, input, sound, etc. - see constants)
	int nVersion;													// version (0xXXYY, where XX - primary version, YY - subversion)
	// factory and loader
	IObjectFactory *pFactory;							// object factory (for all module's objects creating (can't be NULL))
	// checker
	IModuleChecker *pChecker;							// module checker...
	//
	SModuleDescriptor( const char *_pszName, int _nType, int _nVersion, IObjectFactory *_pFactory, IModuleChecker *_pChecker )
		: pszName( _pszName ), nType( _nType ), nVersion( _nVersion ), pFactory( _pFactory ), pChecker( _pChecker ) {  }
};
typedef const SModuleDescriptor* (STDCALL *GETMODULEDESCRIPTOR)();
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** shared data interfaces - shared resource and shared manager
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum ESharedDataSerialMode
{
	SDSM_REPLACE = 1,											// replace all resources
	SDSM_MERGE   = 2,											// merge with current resource
	SDSM_ADD     = 3,											// add to current resource
	SDSM_FORCE_DWORD = 0x7fffffff
};
enum ESharedDataSharingMode
{
	SDSM_SHARE	= 1,
	SDSM_RELOAD	= 2
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface ISharedResource : public IRefCount
{
	// swap own data with resource's one
	virtual void STDCALL SwapData( ISharedResource *pResource ) = 0;
	// check, is this resource referenced by anybody, except shared manager?
	virtual int STDCALL GetRefCounter() const = 0;
	// shared resource name
	virtual const char* STDCALL GetSharedResourceName() const = 0;
	virtual void STDCALL SetSharedResourceName( const std::string &szName ) = 0;
	// internal container load/clear
	virtual bool STDCALL Load( const bool bPreLoad = false ) = 0;
	virtual void STDCALL ClearInternalContainer() = 0;
	// shared resource usage (for LRU/MRU data management)
	virtual int STDCALL GetResourceConsumption() const { return 0; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface ISharedManager : public IRefCount
{
	enum EClearMode
	{
		CLEAR_ALL						= 0,
		CLEAL_UNREFERENCED	= 1,
		CLEAR_LRU						= 2
	};
	// initialize
	virtual bool STDCALL Init() = 0;
	// setup serializing mode
	virtual void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) = 0;
	// setup sharing mode
	virtual void STDCALL SetShareMode( ESharedDataSharingMode eShareMode ) = 0;
	// remove all shared resource from this manager
	virtual void STDCALL Clear( const EClearMode eClearMode = CLEAR_ALL, const int nUsage = 0, const int nAmount = 0 ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SHARED_RESOURCE_METHODS( refcounter, extvarname )																							\
public:																																																\
	virtual int STDCALL GetRefCounter() const { return (refcounter); }																	\
	virtual const char* STDCALL GetSharedResourceName() const { return szSharedResourceName.c_str(); }	\
	virtual void STDCALL SetSharedResourceName( const std::string &szName ) { szSharedResourceName = szName; } \
	static const char* GetSharedResourceExtVarName() { return "SharedResource."extvarname".Ext"; }			\
	void SetSharedResourceLastUsage( const int nUsage ) { nSharedResourceLastUsage.a = nUsage; }				\
	int GetSharedResourceLastUsage() const { return nSharedResourceLastUsage.a; }												\
private:																																															\
	std::string szSharedResourceName;																																		\
	SInt nSharedResourceLastUsage;																																			\
	const std::string GetSharedResourceFullName() const { return szSharedResourceName + GetGlobalVar("SharedResource."extvarname".Ext", ""); }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** game database
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IGDBObject
{
	virtual const char* STDCALL GetName() const = 0;
	virtual const char* STDCALL GetParentName() const = 0;
	virtual const uLong STDCALL GetCheckSum() const = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IGDB : public IRefCount
{
	virtual const IGDBObject* STDCALL Get( const char *pszObjectName, const char *pszParentName ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** base command interface
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IBaseCommand : public IRefCount
{
	// execute command
	virtual void STDCALL Do() = 0;
	// un-execute command
	virtual void STDCALL UnDo() = 0;
	// can this command be un-executed
	virtual bool STDCALL CanUnDo() = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** manipulator basic interface
// **
// **
// **
// ************************************************************************************************************************ //
struct SPropertyDesc
{
	enum EPropertyType
	{
		VAL_INT					= 0,
		VAL_FLOAT				= 1,
		VAL_BROWSEFILE	= 2,
		VAL_BROWSEDIR		= 3,
		VAL_COMBO				= 4,
		VAL_BOOL				= 5,
		VAL_UNITS				= 6,

	};
	const char *pszName;
	int nType;

	std::vector<variant_t> values;
	EPropertyType ePropType;
};
// manipulator property iterator
interface IManipulatorIterator : public IRefCount
{
	// go to the next property
	virtual bool STDCALL Next() = 0;
	// is all properties already iterated?
	virtual bool STDCALL IsEnd() const = 0;
	// get current property descriptor
	virtual const SPropertyDesc* STDCALL GetPropertyDesc() const = 0;
};
// manipulator
interface IManipulator : public IRefCount
{
	// configure manipulator
	virtual void STDCALL Configure( const char *pszName, const variant_t &value ) {  }
	// begin to iterate through all properties
	virtual IManipulatorIterator* STDCALL Iterate() = 0;
	// get property descriptor by name
	virtual const SPropertyDesc* STDCALL GetPropertyDesc( const char *pszName ) = 0;
	// retrieve value. returns false if no such property
	virtual bool STDCALL GetValue( const char *pszValueName, variant_t *pValue ) = 0;
	// set value. returns false if no such property
	virtual bool STDCALL SetValue( const char *pszValueName, const variant_t &value ) = 0;
};
// multimanipulator
interface IMultiManipulator : public IManipulator
{
	// remove all manipulators
	virtual void STDCALL Clear() = 0;
	// add new manipulator
	virtual void STDCALL AddManipulator( IManipulator *pManipulator ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** refcounting system base and necessary realization
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// это специальный класс, который ведёт себя как int, но ещё и инициализируется в нужное нам значение
// применяется для автоматической инициализации refcount'ов
struct SInt
{
private:
	// disable copying
	SInt( const SInt &val ) : a( val.a ) {  }
public:
	int a;
	//
	SInt() : a( 0 ) {  }
	explicit SInt( int val ) : a( val ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// macro that helps to create neccessary members for proper operation of refcount system
#define OBJECT_MINIMAL_METHODS(classname)                                           \
public:                                                                             \
	virtual void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff )								\
	{																																									\
		ADD_REF_PREGUARD( nRefData.a );																									\
		nRefData.a += nRef;																															\
		ADD_REF_POSTGUARD( nRefData.a );																								\
	}																																									\
	virtual void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff )							\
	{																																									\
		nRefData.a -= nRef;																															\
		if ( (nRefData.a & 0x7fffffff) == 0 )																						\
			delete this;																																	\
		else if ( (nRefData.a & nMask) == 0 )																						\
			nRefData.a |= 0x80000000;																											\
	}																																									\
	virtual bool STDCALL IsValid() const { return (nRefData.a & 0x80000000) == 0; }   \
private:                                                                            \
  SInt nRefData;

// TRICK{ при вызове DestroyContents() после деструктора значение ref count получаетс
//        нужным нам с учётом отсвобождения перекрёстных ссылок, поэтому после конструктора мы его переприсваеваем
#define OBJECT_NORMAL_METHODS(classname)                                            \
public:                                                                             \
	static IRefCount* STDCALL CreateNewClassInstanceInternal() { return new classname(); }    \
	virtual void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff )								\
	{																																									\
		ADD_REF_PREGUARD( nRefData.a );																									\
		nRefData.a += nRef;																															\
		ADD_REF_POSTGUARD( nRefData.a );																								\
	}																																									\
	virtual void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff )							\
	{																																									\
		nRefData.a -= nRef;																															\
		if ( (nRefData.a & 0x7fffffff) == 0 )																						\
			delete this;																																	\
		else if ( (nRefData.a & nMask) == 0 )																						\
		{																																								\
			AddRef();																																			\
			DestroyContents();																														\
			nRefData.a |= 0x80000000;																											\
			Release();																																		\
		}																																								\
	}																																									\
	virtual bool STDCALL IsValid() const { return (nRefData.a & 0x80000000) == 0; }   \
protected:                                                                          \
	void DestroyContents() { classname::~classname(); int nOldRef = nRefData.a; ::new(this) classname; nRefData.a += nOldRef; } \
private:                                                                            \
  SInt nRefData;
// TRICK}

#define OBJECT_COMPLETE_METHODS(classname)  \
	OBJECT_NORMAL_METHODS(classname)					\
protected:                                  \
	virtual ~classname() {  }                 \
private:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define DECLARE_SERIALIZE                                         \
public:                                                           \
	virtual int STDCALL operator&( interface IStructureSaver &ss );	\
private:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define DECLARE_SUPER( classname ) typedef classname CSuper;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// walks container of pointers and erases references on invalid entries
template <class TContainer>
inline void EraseInvalidRefs( TContainer *pData )
{
	for ( TContainer::iterator it = pData->begin(); it != pData->end(); )
	{
		if ( (*it)->IsValid() )
			++it;
		else
			it = pData->erase( it );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// clear container, which consist of raw pointers
template <class TContainer>
void ClearContainer( TContainer &container )
{
	for ( TContainer::iterator it = container.begin(); it != container.end(); ++it )
	{
		if ( *it )
			delete *it;
	}
	container.clear();
}
// clear complex container (*map, *set), which consist of raw pointers
template <class TContainer>
void ClearComplexContainer( TContainer &container )
{
	for ( TContainer::iterator it = container.begin(); it != container.end(); ++it )
	{
		if ( it->second )
			delete it->second;
	}
	container.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** basic system for smart pointers
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TYPE>
class CBasicAccessor
{
	TYPE obj;
public:
	CBasicAccessor() {  }
	CBasicAccessor( TYPE _obj ) { obj = _obj; }
	CBasicAccessor( const CBasicAccessor<TYPE> &accessor ) { obj = accessor.obj; }
	// assignment
	CBasicAccessor<TYPE>& operator=( TYPE _obj ) { obj = _obj; return *this; }
	CBasicAccessor<TYPE>& operator=( const CBasicAccessor &accessor ) { obj = accessor.obj; return *this; }
	// comparison
	bool operator==( const CBasicAccessor<TYPE> &_obj ) const { return ( obj == _obj.obj ); }
	bool operator==( const TYPE &_obj ) const { return ( obj == _obj ); }
	bool operator!=( const CBasicAccessor<TYPE> &_obj ) const { return ( obj != _obj.obj ); }
	bool operator!=( const TYPE &_obj ) const { return ( obj != _obj ); }
	// object access operators (dereference and pointer access)
	operator TYPE() const { return obj; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** ref counting pointers
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define BASIC_REGISTER_CLASS( classname )																									\
template<> IRefCount* CastToRefCountImpl<classname >( classname *p, void* ) { return p; }	\
template<> classname* CastToUserObjectImpl<classname >( IRefCount *p, classname*, void* ) { return static_cast<classname*>( p ); }

template <class TUserObj> IRefCount* CastToRefCountImpl( TUserObj *p, void* );
template <class TUserObj> IRefCount* CastToRefCountImpl( TUserObj *p, IRefCount* ) { return p; }
template <class TUserObj> TUserObj* CastToUserObjectImpl( IRefCount *p, TUserObj*, void * );
template <class TUserObj> TUserObj* CastToUserObjectImpl( IRefCount *p, TUserObj*, IRefCount* ) { return static_cast<TUserObj*>( p ); }

template <class TUserObj> inline IRefCount* CastToRefCount( TUserObj *p ) { return CastToRefCountImpl( p, p ); }
template <class TUserObj> inline TUserObj* CastToUserObject( IRefCount *p, TUserObj *pu ) { return CastToUserObjectImpl( p, pu, pu ); }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRefCount
{
	enum
	{
		REF_ADD_REF		= 1,
		REF_MASK_REF	= 0x7fffffff,

		REF_ADD_OBJ		= 0x1000,
		REF_MASK_OBJ	= 0x00fff000,

		REF_ADD_MOBJ	= 0x1000000,
		REF_MASK_MOBJ	= 0x7f000000,
	};
	//
	struct SRefPtrFunc
	{
		static void AddRef( IRefCount *pObj ) { pObj->AddRef(); }
		static void Release( IRefCount *pObj ) { pObj->Release(); }
	};
	struct SRefObjFunc
	{
		static void AddRef( IRefCount *pObj ) { pObj->AddRef( REF_ADD_OBJ ); }
		static void Release( IRefCount *pObj ) { pObj->Release( REF_ADD_OBJ, REF_MASK_OBJ ); }
	};
	struct SRefMFunc
	{
		void AddRef( IRefCount *pObj ) { pObj->AddRef( REF_ADD_MOBJ ); }
		void Release( IRefCount *pObj ) { pObj->Release( REF_ADD_MOBJ, REF_MASK_MOBJ ); }
	};
};
// base ref pointer
template <class TUserObj, class TRefFunc>
class CPtrBase
{
	typedef CPtrBase<TUserObj, TRefFunc> TPtrBase;
	TUserObj *pObj;
protected:
	void AddRef( TUserObj *_pObj ) { if ( _pObj ) TRefFunc::AddRef( CastToRefCount(_pObj) ); }
	void Release( TUserObj *_pObj ) { if ( _pObj ) TRefFunc::Release( CastToRefCount(_pObj) ); }
	// set new object and remove old
	void Set( TUserObj *_pObj ) { TUserObj *pOld = pObj; pObj = _pObj; AddRef( pObj ); Release( pOld ); }
public:
	CPtrBase() : pObj( 0 ) {  }
	CPtrBase( TUserObj *_pObj ) : pObj( _pObj ) { AddRef( pObj ); }
	CPtrBase( const TPtrBase &ptr ) : pObj( ptr.pObj ) { AddRef( pObj ); }
	~CPtrBase() { Release( pObj ); }
	// assignment operators
	TPtrBase& operator=( TUserObj *_pObj ) { Set( _pObj ); return *this; }
	TPtrBase& operator=( const TPtrBase &ptr ) { Set( ptr.pObj ); return *this; }
	// object access operators (dereference and pointer access)
	operator TUserObj*() const { return pObj; }
	TUserObj* operator->() const { return pObj; }
	// check for empty and valid object
	bool IsEmpty() const { return pObj == 0; }
	bool IsValid() const { return !IsEmpty() && GetBarePtr()->IsValid(); }
	// direct acces to the object... ugly functions, but it is neccessary,
	// because C++ can't cast from smartptr to polymorphics of the stored data
	TUserObj* GetPtr() const { return pObj; }
	IRefCount* GetBarePtr() const { return CastToRefCount( pObj ); }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define BASIC_PTR_DECLARE( TPtrName, TRefFunc )																				\
template <class TUserObj>																															\
class TPtrName: public CPtrBase<TUserObj, TRefFunc>																		\
{																																											\
	typedef CPtrBase<TUserObj, TRefFunc> TBase;																					\
public:																																								\
	TPtrName() {}																																				\
	TPtrName( TUserObj *_ptr ): TBase( _ptr ) {  }																			\
	TPtrName( const TPtrName &a ): TBase( a ) {  }																			\
	TPtrName& operator=( TUserObj *_ptr ) { Set( _ptr ); return *this; }								\
	TPtrName& operator=( const TPtrName &a ) { Set( a.GetPtr() ); return *this; }				\
	bool operator==( const TPtrName &a ) const { return GetPtr() == a.GetPtr(); }				\
	bool operator==( const TUserObj *a ) const { return GetPtr() == a; }								\
	bool operator!=( const TPtrName &a ) const { return GetPtr() != a.GetPtr(); }				\
	bool operator!=( const TUserObj *a ) const { return GetPtr() != a; }								\
	bool operator< ( const TUserObj *a ) const { return GetPtr() < a; }									\
	bool operator> ( const TUserObj *a ) const { return GetPtr() > a; }									\
	bool operator<=( const TUserObj *a ) const { return GetPtr() <= a; }								\
	bool operator>=( const TUserObj *a ) const { return GetPtr() >= a; }								\
};
// ptr specialization
BASIC_PTR_DECLARE( CPtr, NRefCount::SRefPtrFunc );
// obj specialization
BASIC_PTR_DECLARE( CObj, NRefCount::SRefObjFunc );
// M-obj specialization
BASIC_PTR_DECLARE( CMObj, NRefCount::SRefMFunc );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// polimorphic casts for pointers above
template <class TOut, class TUserObj, class TRefFunc>
inline TOut reinterpret_cast_ptr( const CPtrBase<TUserObj, TRefFunc> &ptr )
{
	return reinterpret_cast<TOut>( ptr.GetPtr() );
}
template <class TOut, class TUserObj, class TRefFunc>
inline TOut static_cast_ptr( const CPtrBase<TUserObj, TRefFunc> &ptr )
{
	return static_cast<TOut>( ptr.GetPtr() );
}
template <class TOut, class TUserObj, class TRefFunc>
inline TOut dynamic_cast_ptr( const CPtrBase<TUserObj, TRefFunc> &ptr )
{
	return dynamic_cast<TOut>( ptr.GetPtr() );
}
template <class TOut, class TUserObj, class TRefFunc>
inline TOut const_cast_ptr( const CPtrBase<TUserObj, TRefFunc> &ptr )
{
	return const_cast<TOut>( ptr.GetPtr() );
}

#ifdef _DO_CHECKED_CAST
template <class TOut, class TUserObj, class TRefFunc>
inline TOut checked_cast_ptr( const CPtrBase<TUserObj, TRefFunc> &ptr )
{
	if ( dynamic_cast_ptr<TOut, TUserObj, TRefFunc>(ptr) == 0 ) { _asm { int 3 } }
	
	return static_cast_ptr<TOut, TUserObj, TRefFunc>(ptr);
}
#else
#define checked_cast_ptr static_cast_ptr
#endif // _DO_CHECKED_CAST
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TUserObj>
class CGDBPtr
{
	const TUserObj *pObj;
public:
	CGDBPtr() : pObj( 0 ) {  }
	CGDBPtr( const TUserObj *_pObj ) : pObj( _pObj ) {  }
	CGDBPtr( const CGDBPtr<TUserObj> &ptr ) : pObj( ptr.pObj ) {  }
	// assignment operators
	const CGDBPtr<TUserObj>& operator=( const TUserObj *_pObj ) { pObj = _pObj; return *this; }
	const CGDBPtr<TUserObj>& operator=( const CGDBPtr<TUserObj> &ptr ) { pObj = ptr.pObj; return *this; }
	// object access operators (dereference and pointer access)
	operator const TUserObj*() const { return pObj; }
	const TUserObj* operator->() const { return pObj; }
	// comparison operators
	bool operator==( const CGDBPtr<TUserObj> &ptr ) const { return ( pObj == ptr.pObj ); }
	bool operator==( const TUserObj *_pObj ) const { return ( pObj == _pObj ); }
	bool operator!=( const CGDBPtr<TUserObj> &ptr ) const { return ( pObj != ptr.pObj ); }
	bool operator!=( const TUserObj *_pObj ) const { return ( pObj != _pObj ); }
	// check for empty object
	bool IsEmpty() const { return ( pObj == 0 ); }
	// direct acces to the object... ugly functions, but it is neccessary,
	// because C++ can't cast from smartptr to polymorphics of the stored data
	const TUserObj* GetPtr() const { return pObj; }
	// forward declaration of the serialization operator. will be realized in the 'SSHelper.h' file
	int operator&( struct IStructureSaver &ss );
};
// polimorphic casts for pointer above
template <class TOut, class TUserObj>
inline TOut reinterpret_cast_gdb( const CGDBPtr<TUserObj> &ptr )
{
	return reinterpret_cast<TOut>( ptr.GetPtr() );
}
template <class TOut, class TUserObj>
inline TOut static_cast_gdb( const CGDBPtr<TUserObj> &ptr )
{
	return static_cast<TOut>( ptr.GetPtr() );
}
template <class TOut, class TUserObj>
inline TOut dynamic_cast_gdb( const CGDBPtr<TUserObj> &ptr )
{
	return dynamic_cast<TOut>( ptr.GetPtr() );
}
template <class TOut, class TUserObj>
inline TOut const_cast_gdb( const CGDBPtr<TUserObj> &ptr )
{
	return const_cast<TOut>( ptr.GetPtr() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** messages
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SGameMessage
{
	int nEventID;													// message event ID
	int nParam;														// optional parameter
	//
	SGameMessage()
		: nEventID( -1 ), nParam( 0 ) {  }
	explicit SGameMessage( int _nEventID, int _nParam = 0 )
		: nEventID( _nEventID ), nParam( _nParam ) {  }
};
struct STextMessage
{
	WORD wChars[2];												// maximum 1 scancode can be represented by 2 (wide) chars
	int nVirtualKey;											// windows virtual key
	int nScanCode;												// hardware scan code
	bool bPressed;												// char key was pressed or released ?
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BASIC_H__
