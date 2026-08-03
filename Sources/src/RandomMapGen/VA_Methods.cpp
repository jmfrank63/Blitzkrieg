#include "StdAfx.h"

#include "VA_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

int SVAGradient::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &heights );	
	saver.Add( 2, &range );
	saver.Add( 3, &heightRange );

	if ( saver.IsReading() )
	{
		a = 0.0f;
		b = 0.0f;
		c = 0.0f;
		nPreviousIndex = ( -1 );
	}

	return 0;
}

int SVAGradient::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Heights", &heights );	
	saver.Add( "Range", &range );
	saver.Add( "HRange", &heightRange );

	if ( saver.IsReading() )
	{
		a = 0.0f;
		b = 0.0f;
		c = 0.0f;
		nPreviousIndex = ( -1 );
	}

	return 0;
}

void SVAGradient::UpdateHeightRanges()
{
	std::vector<float>::iterator it = heights.begin();

	float fMax = heightRange.max;
	float fMin = heightRange.min;

	heightRange.min = *it;
	heightRange.max = *it;
	
	for ( ++it; it != heights.end(); ++it )
	{
		if ( *it < heightRange.min ) 
		{
			heightRange.min = *it;
		}
		else if ( *it > heightRange.max )
		{
			heightRange.max = *it;
		}
	}

	float fActualSize = heightRange.max - heightRange.min;
	float fRequiredSize = fMax - fMin;
	
	for ( it = heights.begin(); it != heights.end(); ++it )
	{
		*it -= heightRange.min;
		*it *= fRequiredSize / fActualSize;
	}
	
	heightRange.max -=heightRange.min;
	heightRange.max *= fRequiredSize / fActualSize;
	heightRange.min = 0;
}

bool SVAGradient::CreateFromImage( IImage *pImage, const CTPoint<float> &rRange, const CTPoint<float> &rHeightRange )
{
	heights.clear();

	NI_ASSERT_TF( pImage != 0,
								NStr::Format( "Wrong parameter: %x\n", pImage ),
								return false );

	NI_ASSERT_TF( ( rRange.max - rRange.min ) > FP_EPSILON,
								NStr::Format( "Invalid range: %g %g\n", rRange.min, rRange.max ),
								return false );
	
	CImageAccessor imageAccessor = pImage;

	range = rRange;
	heightRange = rHeightRange;

	for ( int nXIndex = 0; nXIndex < pImage->GetSizeX(); ++nXIndex )
	{
		bool isCompleted = false;
		for ( int nYIndex = 0; nYIndex < pImage->GetSizeY(); ++nYIndex )
		{
			const SColor &rColor = imageAccessor[nYIndex][nXIndex];
			if ( ( rColor.r < 0x80 ) && 
					 ( rColor.g < 0x80 ) &&
					 ( rColor.b < 0x80 ) )
			{
				heights.push_back( heightRange.min + ( ( pImage->GetSizeY() - nYIndex - 1 ) * ( heightRange.max - heightRange.min ) / ( pImage->GetSizeY() - 1 ) ) );
				isCompleted = true;
				break;
			}
		}
		if ( !isCompleted )
		{
			heights.push_back( heightRange.min );
		}
  }

	UpdateHeightRanges();
	return true;
}

float SVAGradient::operator()( float fPosition, bool isSquareInterpolated ) const
{
	NI_ASSERT_T( ( ( heights.size() > 2 ) && isSquareInterpolated ) || 
							 ( ( heights.size() > 1 ) && !isSquareInterpolated ),
							 NStr::Format( "Heights size not enough: %d\n", heights.size() ) );
	NI_ASSERT_T( range.min != range.max,
							 NStr::Format( "Invalid range: (%g, %g)\n", range.min, range.max ) );
	NI_ASSERT_T( ( range.min <= fPosition ) && ( range.max >= fPosition ),
							 NStr::Format( "Invalid parameter: %g, must be in range: (%g, %g)\n", fPosition, range.min, range.max ) );
	
	float granularity = ( range.max - range.min ) / ( heights.size() - 1 );
	int nIndex = int( ( fPosition - range.min ) / granularity );

	if ( fabs( fPosition - ( granularity * nIndex ) ) <= FP_EPSILON )
	{
		return heights[nIndex];
	}

	if ( isSquareInterpolated )
	{
		if ( nIndex != nPreviousIndex )
		{
			CTPoint<float> p0;
			CTPoint<float> p1;
			CTPoint<float> p2;
			if ( nIndex == 0 )
			{
				p0.x = range.min + 0.0f * granularity;;
				p0.y = heights[0];
				p1.x = range.min + 1.0f * granularity;
				p1.y = heights[1];
				p2.x = range.min + 2.0f * granularity;
				p2.y = heights[2];
			}
			else 
			{
				p0.x = range.min + ( nIndex - 1 ) * granularity;
				p0.y = heights[nIndex - 1];
				p1.x = range.min + ( nIndex + 0 ) * granularity;
				p1.y = heights[nIndex];
				p2.x = range.min + ( nIndex + 1 ) * granularity;
				p2.y = heights[nIndex + 1];
			}
			a = ( ( p1.y - p0.y ) / ( p1.x - p0.x ) - ( p2.y - p0.y ) / ( p2.x - p0.x ) ) / ( p1.x - p2.x );
			b = ( p2.y - p1.y ) / ( p2.x - p1.x ) - ( p2.y + p1.y ) * a;
			c = p1.y - a * p1.x * p1.x - b * p1.x;
			nPreviousIndex = nIndex;
		}
		return a * fPosition * fPosition + b * fPosition + c;
	}
	else
	{
		return heights[nIndex] + ( heights[nIndex + 1] - heights[nIndex] ) * ( fPosition - range.min - ( granularity * nIndex ) ) / granularity; 
	}
}

int SVAPattern::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pos );	
	saver.Add( 2, &fRatio );
	saver.Add( 3, &heights );

	return 0;
}

int SVAPattern::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Pos", &pos );	
	saver.Add( "Ratio", &fRatio );
	saver.Add( "Hights", &heights );

	return 0;
}

bool SVAPattern::CreateFromGradient( const SVAGradient &rGradient, int nGridLines, float fRatio )
{
	NI_ASSERT_TF( ( ( nGridLines > 0 ) && ( ( nGridLines & 0x1 ) == 0 ) ), 
							  NStr::Format( "Invalid GridLines Number: %d\n", nGridLines ),
								return false );

	heights.SetSizes( nGridLines, nGridLines );
	heights.SetZero();

	SVACreatePatternByGradientFunctional functional( this, &rGradient, fRatio );
	return ApplyVAInRadius( CTRect<int>( 0, 0, nGridLines, nGridLines ), functional );
}

bool SVAPattern::CreateValue( float fValue, int nGridLines, bool bAll )
{
	NI_ASSERT_TF( ( ( nGridLines > 0 ) && ( ( nGridLines & 0x1 ) == 0 ) ), 
							  NStr::Format( "Invalid GridLines Number: %d\n", nGridLines ),
								return false );

	heights.SetSizes( nGridLines, nGridLines );
	if ( bAll )
	{
		heights.Set( fValue );
		return true;
	}
	else
	{
		heights.SetZero();
		SVASetPatternToValueFunctional functional( this, fValue );
		return ApplyVAInRadius( CTRect<int>( 0, 0, nGridLines, nGridLines ), functional );
	}
}

float SVAPattern::GetAverageHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltidude )
{
	SVACalculateAverageHeightFunctional functional( &rAltidude );
	ApplyVAPattern( CTRect<int>( 0, 0, rAltidude.GetSizeX(), rAltidude.GetSizeY() ), (*this), functional, true );
	return functional.GetAverageHeight();
}
