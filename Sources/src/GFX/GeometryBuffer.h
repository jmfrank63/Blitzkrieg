#ifndef __GEOMETRYBUFFER_H__
#define __GEOMETRYBUFFER_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "GFX.h"
#include "RangeAllocs.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// helper functions
D3DPRIMITIVETYPE GFXPrimitiveToD3D( EGFXPrimitiveType type );
int GetNumPrimitives( D3DPRIMITIVETYPE type, int nNumElements );
int GetVertexSize( DWORD dwFormat );
int GetIndexSize( DWORD dwFormat );
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// аналог IDirect3DVertexBuffer/IDirect3DIndexBuffer с несколько расширенной функциональностью
template <class TBuffer>
class CDataBuffer : public CRefCount
{
	NWin32Helper::com_ptr<TBuffer> pBuffer;
	int nLockCounter;                     // number of locks
	//
	int nElementSize;                     // sizeof() of one element, stored in the buffer
	DWORD dwFormat;                       // format. FVF for vertices and D3DFORMAT for indices
	//
	DWORD dwLockFlags;                    // current lock flags - dwAppendFlags or dwDiscardFlags
	DWORD dwAppendFlags;                  // NOOVERWRITE for dynamic, 0 for other
	DWORD dwDiscardFlags;                 // DISCARD for dynamic, 0 for other
public:
	CDataBuffer( TBuffer *_pBuffer, int _nElementSize, DWORD _dwFormat, bool bDynamic )
		: pBuffer( _pBuffer ), nLockCounter( 0 ), nElementSize( _nElementSize ), dwFormat( _dwFormat )
	{
		if ( bDynamic )
			dwAppendFlags = D3DLOCK_NOSYSLOCK | D3DLOCK_NOOVERWRITE, dwDiscardFlags = D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD;
		else
			dwAppendFlags = dwDiscardFlags = 0;
		dwLockFlags = dwAppendFlags;
	}
	//
	TBuffer* GetInternalContainer() { return pBuffer; }
	DWORD GetFormat() const { return dwFormat; }
  int GetElementSize() const { return nElementSize; }
	// прямой доступ к данным (lock/unlock)
	void* Lock( int nStart, int nNumElements )
	{
		void *pMemory = 0;
		HRESULT dxrval = pBuffer->Lock( nStart*nElementSize, nNumElements*nElementSize, &pMemory, dwLockFlags );
		NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't lock vertex/index buffer from %d on %d elements", nStart, nNumElements), return 0 );
		++nLockCounter;
		dwLockFlags = dwAppendFlags;
		return pMemory;
	}
	void Unlock()
	{
		if ( nLockCounter > 0 )
		{
			HRESULT dxrval = pBuffer->Unlock();
			NI_ASSERTHR_SLOW_T( dxrval, "Can't unlock vertex/index buffer" );
			--nLockCounter;
		}
	}
	// range allocation
	virtual bool AllocateRange( int nAmount, SRangeLimits *pRange ) = 0;
  virtual void FreeRange( const SRangeLimits &range ) = 0;
};
typedef CDataBuffer<IDirect3DVertexBuffer8> CVertexBuffer;
typedef CDataBuffer<IDirect3DIndexBuffer8> CIndexBuffer;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// буффер, который может выделять range'и
template <class TBuffer, class TAllocator>
class CRangeDataBuffer : public CDataBuffer<TBuffer>
{
	TAllocator tAllocator;
public:
	CRangeDataBuffer( TBuffer *pBuffer, int nElementSize, DWORD dwFormat, int nNumElements, bool bDynamic )
		: CDataBuffer<TBuffer>( pBuffer, nElementSize, dwFormat, bDynamic ), tAllocator( nNumElements ) {  }
	// проверка на наличие непрерывного блока на 'nAmount' элементов
	bool HasSolidBlock( int nAmount ) const { return tAllocator.HasBlock(nAmount) == EAV_SUCCESS; };
	// range allocation
  virtual bool AllocateRange( int nAmount, SRangeLimits *pRange ) { return tAllocator.Allocate(nAmount, pRange) == EAV_SUCCESS; }
  virtual void FreeRange( const SRangeLimits &range ) { tAllocator.Free( range ); }
	static int GetOptimalSize( int nDesiredSize, int nElementSize ) { return TAllocator::GetOptimalSize( nDesiredSize, nElementSize ); }
};
typedef CRangeDataBuffer<IDirect3DVertexBuffer8, CStaticAllocator> CStaticVB;
typedef CRangeDataBuffer<IDirect3DIndexBuffer8, CStaticAllocator> CStaticIB;
typedef CRangeDataBuffer<IDirect3DVertexBuffer8, CPow2Allocator> CDynamicVB;
typedef CRangeDataBuffer<IDirect3DIndexBuffer8, CPow2Allocator> CDynamicIB;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CVertices : public IGFXVertices
{
	OBJECT_NORMAL_METHODS( CVertices );
	SHARED_RESOURCE_METHODS( nRefData.a, "Vertices" );
	//
	CPtr2<CVertexBuffer> pData;
	D3DPRIMITIVETYPE ptPrimitiveType;
	SRangeLimits range;
public:
	CVertices() : ptPrimitiveType( D3DPT_TRIANGLELIST ), range( 0, 0 ) {  }
	CVertices( CVertexBuffer *_pData, D3DPRIMITIVETYPE type, const SRangeLimits &_range )
		: pData( _pData ), ptPrimitiveType( type ), range( _range ) {  }
	virtual ~CVertices() { if ( range.second != 0 ) pData->FreeRange( range ); }
	//
	void Init( CVertexBuffer *_pData, D3DPRIMITIVETYPE type, const SRangeLimits &_range )
	{
		pData = _pData;
		ptPrimitiveType = type;
		range = _range;
	}
	//
	IDirect3DVertexBuffer8* GetInternalContainer() { return pData->GetInternalContainer(); }
	DWORD GetFormat() const { return pData->GetFormat(); }
	int GetElementSize() const { return pData->GetElementSize(); }
	// primitives... set/get type and number of primitives
	void SetPrimitiveType( EGFXPrimitiveType type ) { ptPrimitiveType = GFXPrimitiveToD3D(type); }
  D3DPRIMITIVETYPE GetPrimitiveType() const { return ptPrimitiveType; }
  DWORD GetNumPrimitives() const { return ::GetNumPrimitives(ptPrimitiveType, range.second); }
  int GetRangeStart() const { return range.first; }
  int GetNumElements() const { return range.second; }
	//
	virtual void STDCALL SwapData( ISharedResource *pResource )
	{
		CVertices *pRes = dynamic_cast<CVertices*>( pResource );
		NI_ASSERT_TF( pRes != 0, "shared resource is not a CVertices", return; );
		std::swap( pData, pRes->pData );
		std::swap( ptPrimitiveType, pRes->ptPrimitiveType );
		std::swap( range, pRes->range );
	}
	// internal container clearing
	virtual void STDCALL ClearInternalContainer() { pData = 0; }
	virtual bool STDCALL Load( const bool bPreLoad = false ) { return false; }
	// direct data access
	virtual void* STDCALL Lock() { return pData->Lock( range.first, range.second ); }
	virtual void STDCALL Unlock() { pData->Unlock(); }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CIndices : public IGFXIndices
{
	OBJECT_NORMAL_METHODS( CIndices );
	SHARED_RESOURCE_METHODS( nRefData.a, "Indices" );
	//
	CPtr2<CIndexBuffer> pData;
	D3DPRIMITIVETYPE ptPrimitiveType;
	SRangeLimits range;
	int nNumUsedVertices;
public:
	CIndices() : ptPrimitiveType( D3DPT_TRIANGLELIST ), range( 0, 0 ), nNumUsedVertices( 0 ) {  }
	CIndices( CIndexBuffer *_pData, D3DPRIMITIVETYPE type, const SRangeLimits &_range )
		: pData( _pData ), ptPrimitiveType( type ), range( _range ), nNumUsedVertices( 0 ) {  }
	virtual ~CIndices() { if ( range.second != 0 ) pData->FreeRange( range ); }
	//
	void Init( CIndexBuffer *_pData, D3DPRIMITIVETYPE type, const SRangeLimits &_range )
	{
		pData = _pData;
		ptPrimitiveType = type;
		range = _range;
	}
	//
	IDirect3DIndexBuffer8* GetInternalContainer() { return pData->GetInternalContainer(); }
	DWORD GetFormat() const { return pData->GetFormat(); }
	int GetElementSize() const { return pData->GetElementSize(); }
	// primitives... set/get type and number of primitives
	void SetPrimitiveType( EGFXPrimitiveType type ) { ptPrimitiveType = GFXPrimitiveToD3D(type); }
  D3DPRIMITIVETYPE GetPrimitiveType() const { return ptPrimitiveType; }
  DWORD GetNumPrimitives() const { return ::GetNumPrimitives(ptPrimitiveType, range.second); }
  int GetRangeStart() const { return range.first; }
  int GetNumElements() const { return range.second; }
	//
	virtual void STDCALL SwapData( ISharedResource *pResource )
	{
		CIndices *pRes = dynamic_cast<CIndices*>( pResource );
		NI_ASSERT_TF( pRes != 0, "shared resource is not a CIndices", return; );
		std::swap( pData, pRes->pData );
		std::swap( ptPrimitiveType, pRes->ptPrimitiveType );
		std::swap( range, pRes->range );
		std::swap( nNumUsedVertices, pRes->nNumUsedVertices );
	}
	// internal container clearing
	virtual void STDCALL ClearInternalContainer() { pData = 0; }
	virtual bool STDCALL Load( const bool bPreLoad = false ) { return false; }
	// direct data access
	virtual void* STDCALL Lock() { return pData->Lock( range.first, range.second ); }
	virtual void STDCALL Unlock() { pData->Unlock(); }
	// set number of vertices, which is addressed by this index set
	virtual void STDCALL SetNumUsedVertices( int _nNumUsedVertices ) { nNumUsedVertices = _nNumUsedVertices; }
	int GetNumUsedVertices() const { return nNumUsedVertices; }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TBuffer>
class CTempBuffer : public CRefCount
{
	enum ELockFlags
	{
		LOCKFLAGS_FLUSH       = D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD,
		LOCKFLAGS_APPEND      = D3DLOCK_NOSYSLOCK | D3DLOCK_NOOVERWRITE,
		LOCKFLAGS_FORCE_DWORD = 0x7fffffff
	};
	//
	NWin32Helper::com_ptr<TBuffer> pBuffer;
	DWORD dwFormat;												// element format
	int nElementSize;											// sizeof( element )
	int nNumElements;											// buffer size (in elements)
	int nCurrElement;											// current element (was locked with)
	int nNextElement;											// next element (for the next lock)
	int nLockCount;												// lock counter
	EGFXPrimitiveType type;								// temporal type
	bool bForceFlush;											// force flush mode on the next lock
	bool bUseOptimized;										// use APPEND/DISCARD scheme or simply discard
public:
	CTempBuffer()
		: dwFormat( 0 ), nElementSize( 0 ), nNumElements( 0 ), nCurrElement( 0 ), 
		  nNextElement( 0 ), nLockCount( 0 ), bForceFlush( false ), bUseOptimized( false ) {  }	
	CTempBuffer( TBuffer *_pBuffer, int _nElementSize, DWORD _dwFormat, int _nNumElements, bool _bDynamic )
		: pBuffer( _pBuffer ), dwFormat( _dwFormat ), nElementSize( _nElementSize ), nNumElements( _nNumElements ), 
		  nCurrElement( 0 ), nNextElement( 0 ), nLockCount( 0 ), bForceFlush( false ), bUseOptimized( false ) {  }
	//
	void* Lock( int nAmount )
	{
		NI_ASSERT_T( nLockCount == 0, NStr::Format("temp buffer already locked %d times", nLockCount) );
		NI_ASSERT_SLOW_TF( nAmount <= nNumElements, NStr::Format("Buffer too small (%d bytes) to allocate %d bytes", nNumElements, nAmount), return 0 );
		// select flags
		ELockFlags flags = LOCKFLAGS_APPEND;
		if ( bUseOptimized ) 
		{
			if ( (nNextElement + nAmount > nNumElements) || bForceFlush )
			{
				nNextElement = 0;
				flags = LOCKFLAGS_FLUSH;
				bForceFlush = false;
			}
		}
		else
		{
			nNextElement = 0;
			flags = LOCKFLAGS_FLUSH;
			bForceFlush = false;
		}
		nCurrElement = nNextElement;
		// lock
		void *pData = 0;
		HRESULT dxrval = pBuffer->Lock( nCurrElement*nElementSize, nAmount*nElementSize, &pData, flags );
		NI_ASSERTHR_SLOW_TF( dxrval, NStr::Format("Can't lock temporary buffer for %d elements", nAmount), return 0 );
		nNextElement = nCurrElement + nAmount + 1;
		++nLockCount;
		return pData;
	}
	void Unlock()
	{
		if ( nLockCount > 0 )
		{
			--nLockCount;
			HRESULT dxrval = pBuffer->Unlock();
			NI_ASSERTHR_SLOW_TF( dxrval, "Can't unlock temporary buffer", return );
		}
	}
	void UnlockAll()
	{
		while ( nLockCount )
			Unlock();
	}
	//
	void ForceFlush() { bForceFlush = true; }
	void UseOptimized( bool bUse ) { bUseOptimized = bUse; }
	//
	TBuffer* GetInternalContainer() const { return pBuffer; }
	int GetRangeStart() const { return nCurrElement; }
	int GetNumElements() const { return nNextElement - nCurrElement - 1; }
	int GetElementSize() const { return nElementSize; }
	DWORD GetFormat() const { return dwFormat; }
	bool HaveSolidBlock( int nAmount ) const { return nAmount <= nNumElements; }
	void SetType( EGFXPrimitiveType _type ) { type = _type; }
	EGFXPrimitiveType GetType() const { return type; }
	static int GetOptimalSize( int nDesiredSize, int nElementSize ) 
	{ 
		return Max( nDesiredSize, 65536/nElementSize ); 
	}
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef CTempBuffer<IDirect3DVertexBuffer8> CTempVB;
typedef CTempBuffer<IDirect3DIndexBuffer8> CTempIB;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GEOMETRYBUFFER_H__
