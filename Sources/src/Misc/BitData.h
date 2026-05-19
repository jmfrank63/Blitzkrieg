#ifndef __BIT_DATA_H__
#define __BIT_DATA_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "2DArrayRLEWrapper.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CArray1Bit
{
	int xSize;
	std::vector<BYTE> array;
public:
	int operator&( interface IStructureSaver &ss ) { CSaverAccessor saver = &ss; saver.Add( 1, &array ); saver.Add( 2, &xSize ); return 0; }
	int operator&( interface IDataTree &ss ) { CTreeAccessor saver = &ss; saver.Add( "Size", &xSize ); saver.Add( "BitArray", &array ); return 0; }

	CArray1Bit() : xSize( 0 ) { }
	CArray1Bit( const int size ) { SetSize( size ); }
	CArray1Bit( const CArray1Bit &a ) : xSize( a.xSize ), array( a.array ) { }
	~CArray1Bit() {}
	//
	const CArray1Bit& operator=( const CArray1Bit &a ) { xSize = a.xSize; array = a.array; return *this; }
	//
	void SetSize( const int size ) { xSize = size; array.resize( (size >> 3) + 1 ); }
	void Clear() { xSize = 0; array.clear(); }
	void SetZero() { if ( !IsEmpty() ) memset( &(array[0]), 0, array.size() ); }

	//
	int GetSize() const { return xSize; }
	bool IsEmpty() const { return xSize == 0; }

	void SetData( const int n )
	{
		array[n >> 3] |= 1 << (n & 7);
	}

	const BYTE GetData( const int n ) const
	{
		return ( array[n >> 3] >> (n & 7) ) & 1;
	}

	void RemoveData( const int n )
	{
		array[n >> 3] &= ~( 1 << (n & 7) );
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CArray2D4Bit
{
	int xSize;
	CArray2D<BYTE> array;
public:
	int operator&( interface IStructureSaver &ss ) { CSaverAccessor saver = &ss; saver.Add( 1, &xSize ); saver.Add( 2, &array ); return 0; }

	CArray2D4Bit() : xSize( 0 ) {}
	CArray2D4Bit( const int xsize, const int ysize ) { SetSizes( xsize, ysize ); }
	CArray2D4Bit( const CArray2D4Bit &a ) : xSize( a.xSize ), array( a.array ) { }
	~CArray2D4Bit() {}
	//
	const CArray2D4Bit& operator=( const CArray2D4Bit &a ) { xSize = a.xSize; array = a.array; return *this; }
	//
	void SetSizes( const int xsize, const int ysize ) { xSize = xsize; array.SetSizes( (xsize >> 1) + 1, ysize ); }
	void Clear() { xSize = 0; array.Clear(); }
	void SetZero() { array.SetZero(); }

	//
	int GetSizeX() const { return xSize; }
	int GetSizeY() const { return array.GetSizeY(); }
	//
	bool IsEmpty() const { return array.IsEmpty(); }

	void SetData( const int x, const int y, const BYTE cData )
	{
		RemoveData( x, y );
		array( x >> 1, y ) |= cData << ( (x & 1) << 2 );
	}

	const BYTE GetData( const int x, const int y ) const
	{
		return ( array( x >> 1, y ) >> ( (x & 1) << 2 ) ) & 0xf;
	}

	void RemoveData( const int x, const int y )
	{
		array( x >> 1, y ) &= 0xf0 >> ( (x & 1) << 2 );
	}

	friend class CBitArray2DRLEWrapper<CArray2D4Bit>;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CArray2D1Bit
{
	int xSize;
	CArray2D<BYTE> array;
public:
	int operator&( interface IStructureSaver &ss ) { CSaverAccessor saver = &ss; saver.Add( 1, &xSize ); saver.Add( 2, &array ); return 0; }

	CArray2D1Bit() : xSize( 0 ) {  }
	CArray2D1Bit( const int xsize, const int ysize )  { SetSizes( xsize, ysize ); }
	CArray2D1Bit( const CArray2D1Bit &a ) : xSize( a.xSize ), array( a.array ) { }
	~CArray2D1Bit() {}
	//
	const CArray2D1Bit& operator=( const CArray2D1Bit &a ) { xSize = a.xSize; array = a.array; return *this; }
	//
	void SetSizes( const int xsize, const int ysize ) { xSize = xsize; array.SetSizes( (xsize >> 3) + 1, ysize ); }
	void Clear() { xSize = 0; array.Clear(); }
	void SetZero() { array.SetZero(); }
	void SetZeroFast() { memset( array.GetBuffer(), 0, array.GetSizeX() * array.GetSizeY() ); }
	//
	void CopyEqualSizes( CArray2D1Bit & dest ) 
	{ 
		NI_ASSERT_T( xSize == dest.xSize && array.GetSizeX() == dest.array.GetSizeX() && array.GetSizeY() == dest.array.GetSizeY(), "cannot copy, unequal sizes" );
		memcpy( array.GetBuffer(), dest.array.GetBuffer(), array.GetSizeX() * array.GetSizeY() );
	}
	void Set1() 
	{ 
		memset( array.GetBuffer(), unsigned int(-1), array.GetSizeX() * array.GetSizeY());
	};

	int GetSizeX() const { return xSize; }
	int GetSizeY() const { return array.GetSizeY(); }
	//
	bool IsEmpty() const { return array.IsEmpty(); }

	void SetData( const int x, const int y )
	{
		array( x >> 3, y ) |= 1 << (x & 7);
	}

	const BYTE GetData( const int x, const int y ) const
	{
		return ( array( x >> 3, y ) >> (x & 7) ) & 1;
	}

	void RemoveData( const int x, const int y )
	{
		array( x >> 3, y ) &= ~( 1 << (x & 7) );
	}

	friend class CBitArray2DRLEWrapper<CArray2D1Bit>;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __BIT_DATA_H__
