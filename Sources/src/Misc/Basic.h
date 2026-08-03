#ifndef __BASIC_H__
#define __BASIC_H__
#pragma ONCE
#include "../Platform/Compiler.h"
#include "../Platform/LegacyTypes.h"
#include "../Platform/LegacyVariant.h"
#include "../zlib/zlib.h"
#ifndef interface
#define interface struct
#endif // interface
#ifndef STDCALL
#define STDCALL BK_STDCALL
#endif // STDCALL
template <int N> struct SGenericNumber { int operator()() const { return N; } };
interface IRefCount
{
	virtual void STDCALL AddRef( int nRef = 1, int nMask = 0x7fffffff ) = 0;
	virtual void STDCALL Release( int nRef = 1, int nMask = 0x7fffffff ) = 0;
	virtual bool STDCALL IsValid() const = 0;
	virtual IRefCount* STDCALL QI( int nInterfaceTypeID ) { return 0; }
	virtual int STDCALL operator&( interface IStructureSaver &ss ) { return 0; }
};
namespace NRefCount
{
	// Set (once per module) when process-exit teardown begins. Static destructors
	// run in undefined order across translation units, so a Release cascade can
	// re-enter a destructor already on the stack or free an object another static
	// still references. From that point on Release only decrements: objects are
	// deliberately leaked, the OS reclaims everything at process exit.
	inline bool& LeakObjectsOnExit() { static bool bLeakObjectsOnExit = false; return bLeakObjectsOnExit; }
}
#ifdef _DO_ASSERT_SLOW
#define ADD_REF_PREGUARD( ref ) const int __nOldRef = ref
#define ADD_REF_POSTGUARD( ref ) if ( (__nOldRef & (~nMask)) != (ref & (~(nMask))) ) { _asm { int 3 } }
#else
#define ADD_REF_PREGUARD( ref )
#define ADD_REF_POSTGUARD( ref )
#endif // _DO_ASSERT_SLOW
template <class TBase>
class CTRefCount : public TBase
{
protected:
	int nRefData;													// refcounting data. DO NOT USE IT DIRECTLY!!!
	void DecRef( int nRef ) { nRefData -= nRef; }
	virtual void STDCALL DestroyContents() {  }
	void InternalClear() { AddRef( 1 ); DestroyContents(); DecRef( 1 ); }
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
		if ( NRefCount::LeakObjectsOnExit() )
			return;
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
	bool IsSelfValid() const { return this != 0 && !IsValid(); }
};
#define OBJECT_SERVICE_METHODS( classname )	\
public:																			\
	virtual void STDCALL DestroyContents() { classname::~classname(); int nHoldRefs = nRefData; ::new(this) classname; nRefData += nHoldRefs; } \
	static IRefCount* STDCALL CreateNewClassInstanceInternal() { return new classname(); }	\
private:
typedef IRefCount* (STDCALL *ObjectFactoryNewFunc)(); // ������� ��� �������� ������ �������
struct SObjectFactoryTypeInfo
{
	int nTypeID;													// user-defined typeID
	const void *pTypeInfo;								// system-dependent type info (can be different each program run)
	ObjectFactoryNewFunc newFunc;					// ������� ��� �������� ������ �������
};
interface IObjectFactory
{
	virtual IRefCount* STDCALL CreateObject( int nTypeID ) = 0;
	virtual void STDCALL RegisterType( int nObjectTypeID, ObjectFactoryNewFunc newFunc ) = 0;
	virtual void STDCALL Aggregate( IObjectFactory *pFactory ) = 0;
	virtual int STDCALL GetNumKnownTypes() = 0;
	virtual void STDCALL GetKnownTypes( SObjectFactoryTypeInfo *pInfoBuffer, int nBufferSize ) = 0;
	virtual int STDCALL GetObjectTypeID( IRefCount *pObj ) const = 0;
};
interface IObjectLoader
{
	virtual bool STDCALL Init( interface ISingleton *pSingleton ) = 0;
	virtual void STDCALL Done() = 0;
	virtual bool STDCALL LoadObject( const char *pszKey, IRefCount *pObject ) = 0;
};
interface IModuleChecker
{
	virtual int STDCALL CheckFunctionality() const = 0;
	virtual void STDCALL SetModuleFunctionalityLimits() const = 0;
};
struct SModuleDescriptor
{
	const char *pszName;									// module name
	int nType;														// type (gfx, input, sound, etc. - see constants)
	int nVersion;													// version (0xXXYY, where XX - primary version, YY - subversion)
	IObjectFactory *pFactory;							// object factory (for all module's objects creating (can't be NULL))
	IModuleChecker *pChecker;							// module checker...
	SModuleDescriptor( const char *_pszName, int _nType, int _nVersion, IObjectFactory *_pFactory, IModuleChecker *_pChecker )
		: pszName( _pszName ), nType( _nType ), nVersion( _nVersion ), pFactory( _pFactory ), pChecker( _pChecker ) {  }
};
typedef const SModuleDescriptor* (STDCALL *GETMODULEDESCRIPTOR)();
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
interface ISharedResource : public IRefCount
{
	virtual void STDCALL SwapData( ISharedResource *pResource ) = 0;
	virtual int STDCALL GetRefCounter() const = 0;
	virtual const char* STDCALL GetSharedResourceName() const = 0;
	virtual void STDCALL SetSharedResourceName( const std::string &szName ) = 0;
	virtual bool STDCALL Load( const bool bPreLoad = false ) = 0;
	virtual void STDCALL ClearInternalContainer() = 0;
	virtual int STDCALL GetResourceConsumption() const { return 0; }
};
interface ISharedManager : public IRefCount
{
	enum EClearMode
	{
		CLEAR_ALL						= 0,
		CLEAL_UNREFERENCED	= 1,
		CLEAR_LRU						= 2
	};
	virtual bool STDCALL Init() = 0;
	virtual void STDCALL SetSerialMode( ESharedDataSerialMode eSerialMode ) = 0;
	virtual void STDCALL SetShareMode( ESharedDataSharingMode eShareMode ) = 0;
	virtual void STDCALL Clear( const EClearMode eClearMode = CLEAR_ALL, const int nUsage = 0, const int nAmount = 0 ) = 0;
};
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
interface IGDBObject
{
	virtual const char* STDCALL GetName() const = 0;
	virtual const char* STDCALL GetParentName() const = 0;
	virtual const uLong STDCALL GetCheckSum() const = 0;
};
interface IGDB : public IRefCount
{
	virtual const IGDBObject* STDCALL Get( const char *pszObjectName, const char *pszParentName ) = 0;
};
interface IBaseCommand : public IRefCount
{
	virtual void STDCALL Do() = 0;
	virtual void STDCALL UnDo() = 0;
	virtual bool STDCALL CanUnDo() = 0;
};
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
interface IManipulatorIterator : public IRefCount
{
	virtual bool STDCALL Next() = 0;
	virtual bool STDCALL IsEnd() const = 0;
	virtual const SPropertyDesc* STDCALL GetPropertyDesc() const = 0;
};
interface IManipulator : public IRefCount
{
	virtual void STDCALL Configure( const char *pszName, const variant_t &value ) {  }
	virtual IManipulatorIterator* STDCALL Iterate() = 0;
	virtual const SPropertyDesc* STDCALL GetPropertyDesc( const char *pszName ) = 0;
	virtual bool STDCALL GetValue( const char *pszValueName, variant_t *pValue ) = 0;
	virtual bool STDCALL SetValue( const char *pszValueName, const variant_t &value ) = 0;
};
interface IMultiManipulator : public IManipulator
{
	virtual void STDCALL Clear() = 0;
	virtual void STDCALL AddManipulator( IManipulator *pManipulator ) = 0;
};
struct SInt
{
private:
	SInt( const SInt &val ) : a( val.a ) {  }
public:
	int a;
	SInt() : a( 0 ) {  }
	explicit SInt( int val ) : a( val ) {  }
};
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
		if ( NRefCount::LeakObjectsOnExit() )																						\
			return;																																				\
		if ( (nRefData.a & 0x7fffffff) == 0 )																						\
			delete this;																																	\
		else if ( (nRefData.a & nMask) == 0 )																						\
			nRefData.a |= 0x80000000;																											\
	}																																									\
	virtual bool STDCALL IsValid() const { return (nRefData.a & 0x80000000) == 0; }   \
private:                                                                            \
  SInt nRefData;

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
		if ( NRefCount::LeakObjectsOnExit() )																						\
			return;																																				\
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

#define OBJECT_COMPLETE_METHODS(classname)  \
	OBJECT_NORMAL_METHODS(classname)					\
protected:                                  \
	virtual ~classname() {  }                 \
private:
#define DECLARE_SERIALIZE                                         \
public:                                                           \
	virtual int STDCALL operator&( interface IStructureSaver &ss );	\
private:
#define DECLARE_SUPER( classname ) typedef classname CSuper;
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
template <class TYPE>
class CBasicAccessor
{
	TYPE obj;
public:
	CBasicAccessor() {  }
	CBasicAccessor( TYPE _obj ) { obj = _obj; }
	CBasicAccessor( const CBasicAccessor<TYPE> &accessor ) { obj = accessor.obj; }
	CBasicAccessor<TYPE>& operator=( TYPE _obj ) { obj = _obj; return *this; }
	CBasicAccessor<TYPE>& operator=( const CBasicAccessor &accessor ) { obj = accessor.obj; return *this; }
	bool operator==( const CBasicAccessor<TYPE> &_obj ) const { return ( obj == _obj.obj ); }
	bool operator==( const TYPE &_obj ) const { return ( obj == _obj ); }
	bool operator!=( const CBasicAccessor<TYPE> &_obj ) const { return ( obj != _obj.obj ); }
	bool operator!=( const TYPE &_obj ) const { return ( obj != _obj ); }
	operator TYPE() const { return obj; }
};
#define BASIC_REGISTER_CLASS( classname )																									\
template<> IRefCount* CastToRefCountImpl<classname >( classname *p, void* ) { return p; }	\
template<> classname* CastToUserObjectImpl<classname >( IRefCount *p, classname*, void* ) { return static_cast<classname*>( p ); }

template <class TUserObj> IRefCount* CastToRefCountImpl( TUserObj *p, void* );
template <class TUserObj> IRefCount* CastToRefCountImpl( TUserObj *p, IRefCount* ) { return p; }
template <class TUserObj> TUserObj* CastToUserObjectImpl( IRefCount *p, TUserObj*, void * );
template <class TUserObj> TUserObj* CastToUserObjectImpl( IRefCount *p, TUserObj*, IRefCount* ) { return static_cast<TUserObj*>( p ); }

template <class TUserObj> inline IRefCount* CastToRefCount( TUserObj *p ) { return CastToRefCountImpl( p, p ); }
template <class TUserObj> inline TUserObj* CastToUserObject( IRefCount *p, TUserObj *pu ) { return CastToUserObjectImpl( p, pu, pu ); }
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
template <class TUserObj, class TRefFunc>
class CPtrBase
{
	typedef CPtrBase<TUserObj, TRefFunc> TPtrBase;
	TUserObj *pObj;
protected:
	void AddRef( TUserObj *_pObj ) { if ( _pObj ) TRefFunc::AddRef( CastToRefCount(_pObj) ); }
	void Release( TUserObj *_pObj ) { if ( _pObj ) TRefFunc::Release( CastToRefCount(_pObj) ); }
	void Set( TUserObj *_pObj ) { TUserObj *pOld = pObj; pObj = _pObj; AddRef( pObj ); Release( pOld ); }
public:
	CPtrBase() : pObj( 0 ) {  }
	CPtrBase( TUserObj *_pObj ) : pObj( _pObj ) { AddRef( pObj ); }
	CPtrBase( const TPtrBase &ptr ) : pObj( ptr.pObj ) { AddRef( pObj ); }
	~CPtrBase() { Release( pObj ); }
	TPtrBase& operator=( TUserObj *_pObj ) { Set( _pObj ); return *this; }
	TPtrBase& operator=( const TPtrBase &ptr ) { Set( ptr.pObj ); return *this; }
	operator TUserObj*() const { return pObj; }
	TUserObj* operator->() const { return pObj; }
	bool IsEmpty() const { return pObj == 0; }
	bool IsValid() const { return !IsEmpty() && GetBarePtr()->IsValid(); }
	TUserObj* GetPtr() const { return pObj; }
	IRefCount* GetBarePtr() const { return CastToRefCount( pObj ); }
};
#define BASIC_PTR_DECLARE( TPtrName, TRefFunc )																				\
template <class TUserObj>																															\
class TPtrName: public CPtrBase<TUserObj, TRefFunc>																		\
{																																											\
	typedef CPtrBase<TUserObj, TRefFunc> TBase;																					\
public:																																								\
	TPtrName() {}																																				\
	TPtrName( TUserObj *_ptr ): TBase( _ptr ) {  }																			\
	TPtrName( const TPtrName &a ): TBase( a ) {  }																			\
	TPtrName( int _ptr ) { (void)_ptr; Set( 0 ); }	\
	TPtrName& operator=( TUserObj *_ptr ) { Set( _ptr ); return *this; }								\
	TPtrName& operator=( const TPtrName &a ) { Set( a.GetPtr() ); return *this; }				\
	bool operator==( const TPtrName &a ) const { return GetPtr() == a.GetPtr(); }				\
	bool operator==( TUserObj *a ) const { return GetPtr() == a; }	\
	bool operator==( int a ) const { (void)a; return GetPtr() == 0; }	\
	bool operator!=( const TPtrName &a ) const { return GetPtr() != a.GetPtr(); }				\
	bool operator!=( TUserObj *a ) const { return GetPtr() != a; }	\
	bool operator!=( int a ) const { (void)a; return GetPtr() != 0; }	\
	bool operator< ( const TPtrName &a ) const { return GetPtr() < a.GetPtr(); }				\
	bool operator> ( const TPtrName &a ) const { return GetPtr() > a.GetPtr(); }				\
	bool operator<=( const TPtrName &a ) const { return GetPtr() <= a.GetPtr(); }			\
	bool operator>=( const TPtrName &a ) const { return GetPtr() >= a.GetPtr(); }			\
};
BASIC_PTR_DECLARE( CPtr, NRefCount::SRefPtrFunc );
BASIC_PTR_DECLARE( CObj, NRefCount::SRefObjFunc );
BASIC_PTR_DECLARE( CMObj, NRefCount::SRefMFunc );
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
template <class TUserObj>
class CGDBPtr
{
	const TUserObj *pObj;
public:
	CGDBPtr() : pObj( 0 ) {  }
	CGDBPtr( const TUserObj *_pObj ) : pObj( _pObj ) {  }
	CGDBPtr( const CGDBPtr<TUserObj> &ptr ) : pObj( ptr.pObj ) {  }
	const CGDBPtr<TUserObj>& operator=( const TUserObj *_pObj ) { pObj = _pObj; return *this; }
	const CGDBPtr<TUserObj>& operator=( const CGDBPtr<TUserObj> &ptr ) { pObj = ptr.pObj; return *this; }
	operator const TUserObj*() const { return pObj; }
	const TUserObj* operator->() const { return pObj; }
	bool operator==( const CGDBPtr<TUserObj> &ptr ) const { return ( pObj == ptr.pObj ); }
	bool operator==( const TUserObj *_pObj ) const { return ( pObj == _pObj ); }
	bool operator!=( const CGDBPtr<TUserObj> &ptr ) const { return ( pObj != ptr.pObj ); }
	bool operator!=( const TUserObj *_pObj ) const { return ( pObj != _pObj ); }
	bool IsEmpty() const { return ( pObj == 0 ); }
	const TUserObj* GetPtr() const { return pObj; }
	int operator&( struct IStructureSaver &ss );
};
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
struct SGameMessage
{
	int nEventID;													// message event ID
	int nParam;														// optional parameter
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
#endif // __BASIC_H__
