#include "StdAfx.h"

#include "Trigonometry.h"
namespace NTrg
{
	const static int NPOWER = 18;
	const static int ACCURACY = 262144; // 2^NPOWER
	static std::vector<float> values;

	void Init()
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "sin.arr", STREAM_ACCESS_READ );
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
		CSaverAccessor saver = pSaver;
		saver.Add( 1, &values );
		values.push_back( 1.0f );
	}

	float Sin( float fAlpha )
	{
/*		
		return sin( fAlpha );
*/
		int nSign = 1 - 2 * int(bit_cast<DWORD>(fAlpha) >> 31);
		fAlpha = fabsf(fAlpha);

		int nIndex = int(fAlpha / FP_PI2 * ACCURACY) & ( 4 * ACCURACY - 1 );

		nSign = nSign * ( 1 - 2 * ( nIndex >> (NPOWER+1) ) );
		nIndex = nIndex & (ACCURACY * 2 - 1);

		const int nDecr = nIndex >> NPOWER;
		nIndex = nIndex * ( 1 - nDecr ) + ( 2*ACCURACY - nIndex ) * nDecr;

		return nSign * values[nIndex];

		/*
		float nSign = 1;
		if ( fAlpha < 0 )
		{
			fAlpha = -fAlpha;
			nSign = -1;
		}

		int nIndex = int(fAlpha / FP_PI2 * ACCURACY) & ( 4 * ACCURACY - 1 );
		if ( nIndex >= 2 * ACCURACY )
		{
			nIndex -= 2 * ACCURACY;
			nSign *= -1;
		}
		if ( nIndex >= ACCURACY )
			nIndex = 2*ACCURACY - nIndex;

		return nSign * values[nIndex];		
			
		fAlpha = fmod( fAlpha, 2.0 * FP_PI );

		if ( fAlpha >= FP_PI )
		{
			fAlpha -= FP_PI;
			fSign *= -1.0f;
		}
		if ( fAlpha >= FP_PI2 )
			fAlpha = PI - fAlpha;

		const int nIndex = fAlpha / FP_PI2 * ACCURACY;

		return fSign * values[nIndex];
		*/
	}
	
	float ASin( float fSin )
	{
		const float fSign = 1 - 2 * int(bit_cast<DWORD>(fSin) >> 31);
		fSin = fabsf( fSin );

		std::vector<float>::iterator iter = std::lower_bound( values.begin(), values.end(), fSin );
		const int nDistance = std::distance( values.begin(), iter );

		return fSign * FP_PI2 * ( (float)nDistance /(float)ACCURACY );
	}
};
