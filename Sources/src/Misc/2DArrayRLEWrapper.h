#ifndef __2DArrayRLEWrapper_H__
#define __2DArrayRLEWrapper_H__
#pragma ONCE
template<class T>
class CArray2DRLEWrapper
{
	DECLARE_SERIALIZE;

	enum { NZEROS = 4 };

	CArray2D<T> &a;
	T packingData;

	CArray2DRLEWrapper();
	void SaveCode( T *pRes, int *pNResCnt, const int nCode );
	const int ReadCode( T *pRes, int *pNResCnt );
public:
	CArray2DRLEWrapper( CArray2D<T> &_a, const T _packingData ) : a( _a ), packingData( _packingData ) { }
};
template<class T>
class CBitArray2DRLEWrapper
{
	T &a;
	BYTE packingData;

	CBitArray2DRLEWrapper();
public:
	CBitArray2DRLEWrapper( T &_a, const BYTE _packingData ) : a( _a ), packingData( _packingData ) { }

	int operator&( interface IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &a.xSize );

		CArray2DRLEWrapper<BYTE> rleWrapper( a.array, packingData );
		saver.Add( 2, &rleWrapper );

		return 0;
	}
};
template<class T>
inline void CArray2DRLEWrapper<T>::SaveCode( T *pRes, int *pNResCnt, const int nCode )
{
	switch ( sizeof(T) )
	{
		case 1:
			pRes[(*pNResCnt)++] = nCode >> 24;
			pRes[(*pNResCnt)++] = ( nCode & 0x00ff0000 ) >> 16;
			pRes[(*pNResCnt)++] = ( nCode & 0x0000ff00 ) >> 8;
			pRes[(*pNResCnt)++] = nCode & 0x000000ff;

			break;
		case 2:
			pRes[(*pNResCnt)++] = nCode >> 16;
			pRes[(*pNResCnt)++] = nCode & 0x0000ffff;

			break;
		default:
			pRes[(*pNResCnt)++] = nCode;
	}
}
template<class T>
const int CArray2DRLEWrapper<T>::ReadCode( T *pRes, int *pNResCnt )
{
	int nCode = 0;
	switch ( sizeof(T) )
	{
		case 1:
			nCode = ( (int)pRes[ *pNResCnt   ] << 24 ) | ( (int)pRes[(*pNResCnt)+1] << 16 )	|
							( (int)pRes[(*pNResCnt)+2] <<  8 ) | ( (int)pRes[(*pNResCnt)+3] );
			*pNResCnt += 4;

			break;
		case 2:
			nCode = ( (int)pRes[*pNResCnt] << 16 )	| ( (int)pRes[(*pNResCnt)+1] );
			*pNResCnt += 2;

			break;
		default:
			nCode = (int)pRes[(*pNResCnt)++];
	}

	return nCode;
}
template<class T>
inline int CArray2DRLEWrapper<T>::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	if ( !saver.IsReading() )
	{
		int nSizeX = a.GetSizeX();
		int nSizeY = a.GetSizeY();
		saver.Add( 1, &nSizeX );
		saver.Add( 2, &nSizeY );

		if ( a.GetSizeX() * a.GetSizeY() != 0 )
		{
			T *pData = a.GetBuffer();
			const int nRLESize = a.GetSizeX() * a.GetSizeY() * 4;
			T *pRLE = new T[nRLESize];
			int cnt = 0;
			int nRLECnt = 0;

			while ( cnt < a.GetSizeX() * a.GetSizeY() )
			{
				int nType = -1;
				int nZeros = 0;
				int i = cnt;
				while ( i < a.GetSizeX() * a.GetSizeY() )
				{
					if ( pData[i] == packingData )
					{
						++nZeros;
						if ( nZeros > NZEROS )
						{
							if ( nType == -1 )
								nType = 0;
							else if ( nType == 1 )
							{
								i -= NZEROS;
								break;
							}
						}
					}
					else
					{
						nZeros = 0;
						if ( nType == -1 )
							nType = 1;
						else if ( nType == 0 )
							break;
					}

					++i;
				}

				const int nLength = i - cnt;
				NI_ASSERT_TF( nLength < 0x7fffffff, "Arra is too big, can't save", return 0 );
				if ( nType == 0 )
					SaveCode( pRLE, &nRLECnt, nLength );
				else
				{
					SaveCode( pRLE, &nRLECnt, 0x80000000 | nLength );
					memcpy( pRLE + nRLECnt, pData + cnt, nLength * sizeof(T) );
					nRLECnt += nLength;
				}

				cnt = i;
			}

			saver.Add( 3, &nRLECnt );
			saver.AddRawData( 4, pRLE, sizeof(T) * nRLECnt );

			delete []pRLE;
		}
	}
	else
	{
		a.Clear();
		
		int nSizeX, nSizeY;
		saver.Add( 1, &nSizeX );
		saver.Add( 2, &nSizeY );
		
		a.SetSizes( nSizeX, nSizeY );
		if ( packingData == 0 )
			a.SetZero();
		else
			a.Set( packingData );

		if ( nSizeX * nSizeY != 0 )
		{
			int nRLELength;
			saver.Add( 3, &nRLELength );
			T *pRLE = new T[nRLELength];
			saver.AddRawData( 4, pRLE, sizeof(T) * nRLELength );
			
			T *pData = a.GetBuffer();

			int cnt = 0;
			int i = 0;
			while ( i < nRLELength )
			{
				const int nCode = ReadCode( pRLE, &i );
				if ( nCode & 0x80000000 )
				{
					const int nLength = nCode & ~0x80000000;
					memcpy( pData + cnt, pRLE + i, nLength * sizeof(T) );

					cnt += nLength;
					i += nLength;
				}
				else
					cnt += nCode;
			}

			delete []pRLE;

			NI_ASSERT_T( cnt == nSizeX * nSizeY, NStr::Format( "Cnt(%d) != nSizeY * nSizeY(%d)", cnt, nSizeX * nSizeY ) );
		}
	}

	return 0;
}
#endif //__2DArrayRLEWrapper_H__
