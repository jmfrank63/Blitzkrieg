#ifndef __DEPTHOPTIMIZER_H__
#define __DEPTHOPTIMIZER_H__
#pragma ONCE
inline const float ClampF( const float fVal, const float fMin, const float fMax )
{
	union { float f; int hex; };
	f = fVal - fMin;
	hex &= ~hex>>31;
	f += fMin - fMax;
	hex &= hex>>31;
	f += fMax;
	return f;
}
class CDepthOptimizer
{
	CArray2D<float> depthbuffer;					//
	const float fAllowedDepth;						// allowed max depth to allow add object
	int nScreenSizeX;											//
	int nScreenSizeY;											//
	float fCoeffX;												//
	float fCoeffY;												//
	bool CanAddLocal( const int nX1, const int nY1, const int nX2, const int nY2 ) const
	{
		float fTotalDepth = 0;
		for ( int i = nY1; i < nY2; ++i )
		{
			const float *pCurr = &(depthbuffer[0][0]) + i*depthbuffer.GetSizeX() + nX1;
			const float *pEnd = &(depthbuffer[0][0]) + i*depthbuffer.GetSizeX() + nX2;
			while ( pCurr < pEnd ) 
			{
				fTotalDepth += *pCurr;
				++pCurr;
			}
		}
		return fTotalDepth / float( (nX2 - nX1) * (nY2 - nY1) ) <= fAllowedDepth;
	}
	void AddLocal( const int nX1, const int nY1, const int nX2, const int nY2 )
	{
		for ( int i = nY1; i < nY2; ++i )
		{
			float *pCurr = &(depthbuffer[0][0]) + i*depthbuffer.GetSizeX() + nX1;
			const float *pEnd = &(depthbuffer[0][0]) + i*depthbuffer.GetSizeX() + nX2;
			while ( pCurr < pEnd ) 
			{
				++(*pCurr);
				++pCurr;
			}
		}
	}
public:	
	CDepthOptimizer( const int nSizeX, const int nSizeY, const float _fAllowedDepth )
		: depthbuffer( nSizeX, nSizeY ), fAllowedDepth( _fAllowedDepth ) 
	{  
		nScreenSizeX = nScreenSizeY = 0;
		fCoeffX = fCoeffY = 0;
	}
	void SetScreenSize( const int nSizeX, const int nSizeY )
	{
		nScreenSizeX = nSizeX;
		nScreenSizeY = nSizeY;
		fCoeffX = float( depthbuffer.GetSizeX() ) / float( nScreenSizeX );
		fCoeffY = float( depthbuffer.GetSizeY() ) / float( nScreenSizeY );
	}
	bool CheckAndAdd( const float fX1, const float fY1, const float fX2, const float fY2 )
	{
		const int nX1 = int( ClampF(fX1*fCoeffX       , 0.0f, float(depthbuffer.GetSizeX())) );
		const int nY1 = int( ClampF(fY1*fCoeffY       , 0.0f, float(depthbuffer.GetSizeY())) );
		const int nX2 = int( ClampF(fX2*fCoeffX + 1.0f, 0.0f, float(depthbuffer.GetSizeX())) );
		const int nY2 = int( ClampF(fY2*fCoeffY + 1.0f, 0.0f, float(depthbuffer.GetSizeY())) );
		if ( CanAddLocal(nX1, nY1, nX2, nY2) ) 
		{
			AddLocal( nX1, nY1, nX2, nY2 );
			return true;
		}
		return false;
	}
	void Clear()
	{
		depthbuffer.SetZero();
	}
};
#endif // __DEPTHOPTIMIZER_H__
