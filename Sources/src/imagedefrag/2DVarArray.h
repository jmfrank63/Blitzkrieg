#if !defined(__VarArray2D__)
#define __VarArray2D__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

template <class T, class TCounter = BYTE>
class CVarArray2D
{
	typedef T *PT;

  int nSize;    										    // total elements count
	int nLineCount;                       // data line array size
  T *pData;														  // main data pointer
  T **pLineData;											  // data lines pointers
  TCounter *pLineDataSize;              // data line sizes pointer
  
  void IndexLineData()
  {
    int index = 0;
    int count = 0;
    for ( index = 0; index < nLineCount; ++index )
    {
      pLineData[index] = pData + count;
      count += pLineDataSize[index];
    }
  }

  void Copy( const CVarArray2D &varArray2D )
  {
    nSize = varArray2D.nSize;
    nLineCount = varArray2D.nLineCount;
    Create();

    int index = 0;
    for ( index = 0; index < nSize; ++index )
    {
      pData[index] = varArray2D.pData[index];
    }
    for ( index = 0; index < nLineCount; ++index )
    {
      pLineDataSize[ index ] = varArray2D.pLineDataSize[ index ];
    }
    IndexLineData();
  }

	void Destroy()
  {
    if ( pData ) 
    {
      delete[] pData;
    }
    if ( pLineData )
    {
      delete[] pLineData;
    }
    if ( pLineDataSize ) 
    {
      delete[] pLineDataSize;
    }
    nSize = 0;
    nLineCount = 0;
    pData = 0;
    pLineData = 0;
    pLineDataSize = 0;
  }

  void Create()
  {
    pData = new T[nSize];
    pLineData = new PT[nLineCount];
    pLineDataSize = new TCounter[nLineCount];
    for ( int index = 0; index < nLineCount; ++index )
    {
      pLineData[index] = 0;
      pLineDataSize[index] = 0;              
    }
  }

public:
	CVarArray2D() : nSize( 0 ), nLineCount( 0 ), pData( 0 ), pLineData( 0 ), pLineDataSize( 0 ) {  }
  CVarArray2D( int _nSize, int _nLineCount ) : nSize( _nSize ), nLineCount( _nLineCount ) { Create(); }
  CVarArray2D( const CVarArray2D &varArray2D ) { Copy( varArray2D ); }
  ~CVarArray2D() { Destroy(); }

  const CVarArray2D& operator=( const CVarArray2D &varArray2D )
  {
    if ( this == &varArray2D )
    {
      return *this;
    }
    Destroy();
    Copy( varArray2D );
    return *this;
  }

	void SetSizes( int _nSize, int _nLineCount )
  {
    if ( ( nSize == _nSize ) && ( nLineCount == _nLineCount ) ) 
    {
      return;
    }
    Destroy();
    NI_ASSERT_SLOW_T( ( _nSize > 0 ) && ( _nLineCount > 0 ),
                      NStr::Format( "Can't create 2D var array with zero length (%d) or with zero line count (%d)", _nSize, _nLineCount ) );
    nSize = _nSize;
    nLineCount = _nLineCount;
    Create();
  }

  void Clear() { Destroy(); }

  void Set( const T &data )
  {
    for ( int index = 0; index < nSize; ++index )
    {
      pData[ index ] = data;
    }
  }

	T* operator[]( const int lineIndex ) const
  {
    NI_ASSERT_SLOW_T( ( lineIndex >= 0 ) && ( lineIndex < nLineCount ),
                      NStr::Format( "line index (%d) miss in 2D var array line count (%d)", lineIndex, nLineCount ) );
    NI_ASSERT_SLOW_T( pLineData[lineIndex] != 0,
                      NStr::Format( "line index (%d) not inisialized in 2D var array line count (%d)", lineIndex, nLineCount ) );
    return pLineData[lineIndex]; 
  }

	T& operator()( const int lineIndex, const TCounter inLineIndex )
  {
    NI_ASSERT_SLOW_T( ( lineIndex >= 0 ) && ( lineIndex < nLineCount ),
                      NStr::Format( "line index (%d) miss in 2D var array line count: (%d)", lineIndex, nLineCount ) );
    NI_ASSERT_SLOW_T( ( inLineIndex >= 0 ) && ( inLineIndex < pLineDataSize[lineIndex] ),
                      NStr::Format( "in line index (%d) miss in 2D var array line  at index (%d): (%d)", inLineIndex, lineIndex, pLineDataSize[lineIndex] ) );
    return (*this)[lineIndex][inLineIndex];
  }
	
  const T& operator()( const int lineIndex, const TCounter inLineIndex ) const
  {
    NI_ASSERT_SLOW_T( ( lineIndex >= 0 ) && ( lineIndex < nLineCount ),
                      NStr::Format( "line index (%d) miss in 2D var array line count: (%d)", lineIndex, nLineCount ) );
    NI_ASSERT_SLOW_T( ( inLineIndex >= 0 ) && ( inLineIndex < pLineDataSize[lineIndex] ),
                      NStr::Format( "in line index (%d) miss in 2D var array line  at index (%d): (%d)", inLineIndex, lineIndex, pLineDataSize[lineIndex] ) );
    return (*this)[lineIndex][inLineIndex];
  }

  int GetSize() const { return nSize; }

  int GetLineCount() const { return nLineCount; }

	TCounter GetLineSize( int lineIndex ) const
  {
    NI_ASSERT_SLOW_T( ( lineIndex >= 0 ) && ( lineIndex < nLineCount ),
                      NStr::Format( "line index (%d) miss in 2D var array line count(%d)", lineIndex, nLineCount ) );
    NI_ASSERT_SLOW_T( pLineData[lineIndex] != 0,
                      NStr::Format( "line index (%d) not inisialized in 2D var array line count (%d)", lineIndex, nLineCount ) );
    return pLineDataSize[lineIndex];
  }
  
  T* GetBuffer() { return pData; }
  
  bool IsEmpty() const { return pData == 0; }

  void SetLineLength( int lineIndex, TCounter nLineLength )
  {
#ifdef _DEBUG
    NI_ASSERT_SLOW_T( ( lineIndex >= 0 ) && ( lineIndex < nLineCount ),
                      NStr::Format( "line index (%d) miss in 2D var array line count(%d)", lineIndex, nLineCount ) );
    int debugIndex = 0;
    int debugCount = 0;
    for ( debugIndex = 0; debugIndex < lineIndex; ++debugIndex)
    {
      NI_ASSERT_SLOW_T( pLineData[debugIndex] != 0,
                        NStr::Format( "line data at (%d) is sero, please set line length at (%d) before (%d)", debugIndex, debugIndex, lineIndex ) );
      debugCount += ( int )( pLineDataSize[debugIndex] );
    }
    NI_ASSERT_SLOW_T( ( int )( nLineLength ) <= ( nSize - debugCount ),
                      NStr::Format( "line length at (%d) too big: (%d), may be under or equal (%d)", nLineLength, nSize - debugCount ) );
#endif //#ifdef _DEBUG

    int index = 0;
    int count = 0;
    for( index = 0; index < lineIndex; index++ )
    {
      count += ( int )( pLineDataSize[index] );
    }
    pLineData[lineIndex] = pData + count;
    pLineDataSize[lineIndex] = nLineLength;
  }

  void SetZero() { memset( pData, 0, sizeof( T ) * nSize ); }

  int operator&( IStructureSaver &ss )
  {
    CSaverAccessor saver = &ss;
    if( saver.IsReading() )
    {
      Destroy();
    }
    saver.Add( 1,  &nSize );
    saver.Add( 2,  &nLineCount );
    if( saver.IsReading() )
    {
      Create();      
    }
    saver.AddRawData( 3, ( void* )( pData ), sizeof( T ) * nSize );
    saver.AddRawData( 4, ( void* )( pLineDataSize ), sizeof( TCounter ) * nLineCount );
/** long data file but safe!   
    int index = 0;
    for( index = 0; index < nSize;  ++index )
    {
      saver.Add( index + 3,  &( pData[index] ) );
    }
    for( index = 0; index < nLineCount; ++index )
    {
      saver.Add( index + nSize + 3 , &( pLineDataSize[index] ) );
    }
/**/
    if( saver.IsReading() )
    {
      IndexLineData();
    }
	  return 0;
  }

#ifdef _DEBUG
  void Trace( char *pBuffer )
  {
    int elements = 0;
    TCounter maxLineSize = 0;
    int signedLines = 0;
    int lineIndex = 0;
    int index = 0;
    for ( lineIndex = 0; lineIndex < GetLineCount(); ++lineIndex )
    {
      for ( index = 0; index < GetLineSize( lineIndex ); ++index )
      {
        ++elements;
      }
      if ( GetLineSize( lineIndex ) >  maxLineSize )
      {
        maxLineSize = GetLineSize( lineIndex );
      }
      if (  GetLineSize( lineIndex ) > 0)
      {
        ++signedLines;
      }
    }
    sprintf( pBuffer, "VarArray2D statistic:\nsize %d\nlines count %d\nelements %d\nmax line length %d\nnon-empty lines count %d\naverage elements per line %f\naverage elements per non-emply line %f\n",
             GetSize(),
             GetLineCount(),
             elements,
             maxLineSize,
             signedLines,
             elements * 1.0 / GetLineCount(),
             elements * 1.0 / signedLines );
  }
#endif //#ifdef _DEBUG
};
#endif //#if !defined(__VarArray2D__)
