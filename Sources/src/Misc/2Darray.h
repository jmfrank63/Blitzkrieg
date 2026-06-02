#ifndef __2DARRAY_H_
#define __2DARRAY_H_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
template <class T>
class CArray2D
{
	typedef T *PT;
#ifdef _DEBUG
	struct CBoundCheck
	{
		T *data;
		int nSize;
		CBoundCheck( T *d, int nS ) { data = d; nSize = nS; }
		T& operator[]( int i ) const { NI_ASSERT_SLOW_T( i>=0 && i<nSize, NStr::Format("X size (%d) miss in 2D array (%d)", nSize, i) ); return data[i]; }
	};
#endif // _DEBUG
	T *data;															// main data pointer
	T **pData;														// row pointers
	int nSizeX, nSizeY;										// array size
	void Copy( const CArray2D &a ) { nSizeX = a.nSizeX; nSizeY = a.nSizeY; Create(); for ( int i=0; i<nSizeX*nSizeY; ++i ) data[i] = a.data[i]; }
	void Destroy() { if ( data ) delete []data; if ( pData ) delete []pData; data = 0; pData = 0; nSizeX = nSizeY = 0; }
	void Create() { data = new T[nSizeX*nSizeY]; pData = new PT[nSizeY]; for ( int i=0; i<nSizeY; ++i ) pData[i] = data + i*nSizeX; }
public:
	CArray2D() : data( 0 ), pData( 0 ), nSizeX( 0 ), nSizeY( 0 ) {  }
	CArray2D( int xsize, int ysize ) : nSizeX( xsize ), nSizeY( ysize ) { Create(); }
	CArray2D( const CArray2D &a ) { Copy( a ); }
	~CArray2D() { Destroy(); }
	const CArray2D& operator=( const CArray2D &a ) { Destroy(); Copy( a ); return *this; }
	void SetSizes( int xsize, int ysize ) { if ( nSizeX == xsize && nSizeY == ysize ) return; Destroy(); nSizeX = xsize; nSizeY = ysize; Create(); }
	void Clear() { Destroy(); }
	void SetZero() { memset( data, 0, sizeof(T) * nSizeX * nSizeY ); }
	void Set( const T &a ) { for ( int i=0; i<nSizeX*nSizeY; ++i ) data[i] = a; }
#ifdef _DEBUG
	CBoundCheck operator[]( int i ) const { NI_ASSERT_SLOW_T( i>=0 && i<nSizeY, NStr::Format("Y size (%d) miss in 2D array (%d)", nSizeY, i) ); return CBoundCheck( pData[i], nSizeX ); }
#else
	T* operator[]( int i ) const { NI_ASSERT_SLOW_T( i>=0 && i<nSizeY, NStr::Format("Y size (%d) miss in 2D array (%d)", nSizeY, i) ); return pData[i]; }
#endif // _DEBUG
	T& operator()( const int nX, const int nY ) { return (*this)[nY][nX]; }
	const T& operator()( const int nX, const int nY ) const { return (*this)[nY][nX]; }
	int GetSizeX() const { return nSizeX; }
	int GetSizeY() const { return nSizeY; }
	int GetBoundX() const { return GetSizeX() - 1; }
	int GetBoundY() const { return GetSizeY() - 1; }
	T* GetBuffer() { return data; }
	bool IsEmpty() const { return data == 0; }
};
#endif // __2DARRAY_H_
