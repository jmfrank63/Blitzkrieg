#include <d3d9.h>

typedef IDirect3D9 IDirect3D8;
typedef IDirect3DDevice9 IDirect3DDevice8;
typedef IDirect3DTexture9 IDirect3DTexture8;
typedef IDirect3DSurface9 IDirect3DSurface8;
typedef IDirect3DVertexBuffer9 IDirect3DVertexBuffer8;
typedef IDirect3DIndexBuffer9 IDirect3DIndexBuffer8;
typedef D3DVIEWPORT9 D3DVIEWPORT8;
typedef D3DLIGHT9 D3DLIGHT8;
typedef D3DCAPS9 D3DCAPS8;
typedef D3DADAPTER_IDENTIFIER9 D3DADAPTER_IDENTIFIER8;

inline IDirect3D9* Direct3DCreate8( UINT sdkVersion )
{
	return Direct3DCreate9( sdkVersion );
}

#include "..\Misc\Win32Helper.h"

#include "GFX.h"
#include "GFXHelper.h"
#include "CommonStructs.h"
class CRefCount
{
	int nRefData;
protected:
	virtual ~CRefCount() {  }							// this object cannot be deleted directly - only through 'Release'
public:
	CRefCount() : nRefData( 0 ) {  }			// by default, object created with ref count = '0'
	int AddRef() { ++nRefData; return nRefData; }
	int Release() { int nRef = --nRefData; if ( (nRefData & 0x7fffffff) == 0 ) delete this; return (nRef & 0x7fffffff); }
	void Invalidate() { nRefData |= 0x80000000; }
	bool IsValid() const { return (nRefData & 0x80000000) == 0; }
	DWORD GetRTID() const { return reinterpret_cast<DWORD>( this ); }
};
template <class TUserObj>
class CPtr2
{
	typedef CPtr2<TUserObj> TPtr;
	TUserObj *pObj;
protected:
	void AddRef( TUserObj *_pObj ) { if ( _pObj ) _pObj->AddRef(); }
	void Release( TUserObj *_pObj ) { if ( _pObj ) _pObj->Release(); }
	void Set( TUserObj *_pObj ) { TUserObj *pOld = pObj; pObj = _pObj; AddRef( pObj ); Release( pOld ); }
public:
	CPtr2() : pObj( 0 ) {  }
	CPtr2( TUserObj *_pObj ) : pObj( _pObj ) { AddRef( pObj ); }
	CPtr2( const TPtr &ptr ) : pObj( ptr.pObj ) { AddRef( pObj ); }
	~CPtr2() { Release( pObj ); }
	TPtr& operator=( TUserObj *_pObj ) { Set( _pObj ); return *this; }
	TPtr& operator=( const TPtr &ptr ) { Set( ptr.pObj ); return *this; }
	bool operator==( const TPtr &a ) const { return GetPtr() == a.GetPtr(); }
	bool operator==( const TUserObj *a ) const { return GetPtr() == a; }
	bool operator!=( const TPtr &a ) const { return GetPtr() != a.GetPtr(); }
	bool operator!=( const TUserObj *a ) const { return GetPtr() != a; }
	bool operator< ( const TUserObj *a ) const { return GetPtr() < a; }
	bool operator> ( const TUserObj *a ) const { return GetPtr() > a; }
	bool operator<=( const TUserObj *a ) const { return GetPtr() <= a; }
	bool operator>=( const TUserObj *a ) const { return GetPtr() >= a; }
	operator TUserObj*() const { return pObj; }
	TUserObj* operator->() const { return pObj; }
	bool IsEmpty() const { return pObj == 0; }
	bool IsValid() const { return !IsEmpty() && GetBarePtr()->IsValid(); }
	TUserObj* GetPtr() const { return pObj; }
	CRefCount* GetBarePtr() const { return pObj; }
};
