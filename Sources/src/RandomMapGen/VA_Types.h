#if !defined(__VA__Types__)
#define __VA__Types__

#include "../Formats/fmtMap.h"
#include "../Image/Image.h"

#include "MapInfo_Types.h"

struct SVAGradient
{
private:
	mutable float a;
	mutable float b;
	mutable float c;
	mutable int nPreviousIndex;

public:
	std::vector<float> heights;
	CTPoint<float> range;
	CTPoint<float> heightRange;

	SVAGradient() : a( 0.0f ), b( 0.0f ), c( 0.0f ), nPreviousIndex( -1 ), range( 0.0f, 0.0f ), heightRange( 0.0f, 0.0f ) {}
	SVAGradient( const std::vector<float> rHeights,	const CTPoint<float> &rRange, const CTPoint<float> &rHeightRange )
		: a( 0.0f ), b( 0.0f ), c( 0.0f ), nPreviousIndex( -1 ), heights( rHeights ), range( rRange ), heightRange( rHeightRange ) {}
	SVAGradient( const SVAGradient &rGradient )
		: a( 0.0f ), b( 0.0f ), c( 0.0f ), nPreviousIndex( -1 ), heights( rGradient.heights ), range( rGradient.range ), heightRange( rGradient.heightRange ) {}
	SVAGradient& operator=( const SVAGradient &rGradient )
	{
		if( &rGradient != this )
		{
			a = 0.0f;
			b = 0.0f;
			c = 0.0f;
			nPreviousIndex = ( -1 );
			heights = rGradient.heights;
			range = rGradient.range;
			heightRange = rGradient.heightRange;
		}
		return *this;
	}
	
	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );

	void UpdateHeightRanges();
	bool CreateFromImage( IImage *pImage, const CTPoint<float> &rRange, const CTPoint<float> &rHeightRange );
	float operator()( float fPosition, bool isSquareInterpolated = false ) const;
};

struct SVAPattern
{
	CTPoint<int> pos;
	float fRatio;
	CArray2D<float> heights;

	SVAPattern() : pos( 0, 0 ), fRatio( 1.0f ) {}
	SVAPattern( const CTPoint<int> &rPos, float _fRatio, const CArray2D<float> &rHeights )
		: pos( rPos ), fRatio( _fRatio ), heights( rHeights ) {}
	SVAPattern( int nPosX, int nPosY, float _fRatio, const CArray2D<float> &rHeights )
		: pos( nPosX, nPosY ), fRatio( _fRatio ), heights( rHeights ) {}
	SVAPattern( const SVAPattern &rPattern )
		: pos( rPattern.pos ), fRatio( rPattern.fRatio ), heights( rPattern.heights ) {}
	SVAPattern& operator=( const SVAPattern &rPattern )
	{
		if( &rPattern != this )
		{
			pos = rPattern.pos;
			fRatio = rPattern.fRatio;
			heights = rPattern.heights;
		}
		return *this;
	}

	bool CreateFromGradient( const SVAGradient &rGradient, int nGridLines, float fRatio = 1.0f );
	bool CreateValue( float fValue, int nGridLines, bool bAll = false );
	float GetAverageHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltidude );

	virtual int STDCALL operator&( IStructureSaver &ss );
	virtual int STDCALL operator&( IDataTree &ss );
};

struct SVACalculateAverageHeightFunctional
{
	const STerrainInfo::TVertexAltitudeArray2D *pAltidude;
	float fTotalHeight;
	int nPointCount;
		
	SVACalculateAverageHeightFunctional( const STerrainInfo::TVertexAltitudeArray2D *_pAltidude ) 
		: pAltidude( _pAltidude ), fTotalHeight( 0.0f ), nPointCount( 0 )
	{
		NI_ASSERT_T( pAltidude != 0,
								 NStr::Format( "Wrong parameter: %x\n", pAltidude ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		if ( fValue != 0.0f )
		{
			fTotalHeight += (*pAltidude)[nYIndex][nXIndex].fHeight;
			++nPointCount;
		}
		return true;
	}

	float GetAverageHeight() { return ( ( nPointCount != 0 ) ? ( fTotalHeight / nPointCount ) : 0.0f ); }
};

struct SVALevelFunctional
{
	STerrainInfo::TVertexAltitudeArray2D *pAltidude;
	float fAverageHeight;
	float fLevelRatio;

	SVALevelFunctional( STerrainInfo::TVertexAltitudeArray2D *_pAltidude, float _fAverageHeight, float _fLevelRatio )
		: pAltidude( _pAltidude ), fAverageHeight( _fAverageHeight ), fLevelRatio( _fLevelRatio )
	{
		NI_ASSERT_T( pAltidude != 0,
								 NStr::Format( "Wrong parameter: %x\n", pAltidude ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		if ( fValue != 0.0f )
		{
			float fDelta = ( fAverageHeight - (*pAltidude)[nYIndex][nXIndex].fHeight ) * fLevelRatio;
			( *pAltidude )[nYIndex][nXIndex].fHeight += fDelta;
		}
		return true;
	}
};

struct SVALevelAndCreateUndoPatternFunctional
{
	STerrainInfo::TVertexAltitudeArray2D *pAltidude;
	SVAPattern *pUndoPattern;
	float fAverageHeight;
	float fLevelRatio;

	SVALevelAndCreateUndoPatternFunctional( STerrainInfo::TVertexAltitudeArray2D *_pAltidude, float _fAverageHeight, float _fLevelRatio, SVAPattern *_pUndoPattern )
		: pAltidude( _pAltidude ), fAverageHeight( _fAverageHeight ), fLevelRatio( _fLevelRatio ), pUndoPattern( _pUndoPattern )
	{
		NI_ASSERT_T( pAltidude != 0,
								 NStr::Format( "Wrong parameter: %x\n", pAltidude ) );
		NI_ASSERT_T( pUndoPattern != 0,
								 NStr::Format( "Wrong parameter: %x\n", pUndoPattern ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		if ( fValue != 0.0f )
		{
			float fDelta = ( fAverageHeight - (*pAltidude)[nYIndex][nXIndex].fHeight ) * fLevelRatio;
			( *pAltidude )[nYIndex][nXIndex].fHeight += fDelta;
			pUndoPattern->heights[nYIndex - pUndoPattern->pos.y][nXIndex - pUndoPattern->pos.x] = fDelta;
		}
		return true;
	}
};

struct SVASetPatternFunctional
{
	STerrainInfo::TVertexAltitudeArray2D *pAltidude;
	bool bLevel;

	SVASetPatternFunctional( STerrainInfo::TVertexAltitudeArray2D *_pAltidude, bool _bLevel = false ) 
		: pAltidude( _pAltidude ), bLevel( _bLevel )
	{
		NI_ASSERT_T( pAltidude != 0,
								 NStr::Format( "Wrong parameter: %x\n", pAltidude ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		if ( !bLevel || ( fValue != 0.0f ) )
		{
			( *pAltidude )[nYIndex][nXIndex].fHeight = fValue;
		}
		return true;
	}
};

struct SVAAddPatternFunctional
{
	STerrainInfo::TVertexAltitudeArray2D *pAltidude;

	SVAAddPatternFunctional( STerrainInfo::TVertexAltitudeArray2D *_pAltidude ) 
		: pAltidude( _pAltidude )
	{
		NI_ASSERT_T( pAltidude != 0,
								 NStr::Format( "Wrong parameter: %x\n", pAltidude ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		( *pAltidude )[nYIndex][nXIndex].fHeight += fValue;
		return true;
	}
};

struct SVASubstractPatternFunctional
{
	STerrainInfo::TVertexAltitudeArray2D *pAltidude;

	SVASubstractPatternFunctional( STerrainInfo::TVertexAltitudeArray2D *_pAltidude ) 
		: pAltidude( _pAltidude )
	{
		NI_ASSERT_T( pAltidude != 0,
								 NStr::Format( "Wrong parameter: %x\n", pAltidude ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		( *pAltidude )[nYIndex][nXIndex].fHeight -= fValue;
		return true;
	}
};

struct SVASetMaxPatternFunctional
{
	STerrainInfo::TVertexAltitudeArray2D *pAltidude;

	SVASetMaxPatternFunctional( STerrainInfo::TVertexAltitudeArray2D *_pAltidude ) 
		: pAltidude( _pAltidude )
	{
		NI_ASSERT_T( pAltidude != 0,
								 NStr::Format( "Wrong parameter: %x\n", pAltidude ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		if ( fabs( ( *pAltidude )[nYIndex][nXIndex].fHeight ) < fabs( fValue ) )
		{
			( *pAltidude )[nYIndex][nXIndex].fHeight = fValue;
		}
		return true;
	}
};

template<class TYPE>
bool ApplyVAPatterns( const CTRect<int> &rRect,
											const std::vector<SVAPattern> &rPatterns,
											TYPE &rApplyFunctional,								//����������
											bool isIgnoreInvalidIndices = false )	//���������� ������� �� ������ �����
{
	for ( int nPatternIndex = 0; nPatternIndex < rPatterns.size(); ++nPatternIndex )
	{
		CTRect<int> indices( rPatterns[nPatternIndex].pos.x,
												 rPatterns[nPatternIndex].pos.y,
												 rPatterns[nPatternIndex].pos.x + rPatterns[nPatternIndex].heights.GetSizeX(),
												 rPatterns[nPatternIndex].pos.y + rPatterns[nPatternIndex].heights.GetSizeY() );
		int result = ValidateIndices( rRect, &indices );

		if ( result < 0 )
		{
			if ( isIgnoreInvalidIndices )
			{
				continue;
			}
			else
			{
				return false;
			}
		}
		
		if ( ( result == 0 ) && !isIgnoreInvalidIndices )
		{
			return false;
		}

		for ( int nXIndex = indices.minx; nXIndex < indices.maxx; ++nXIndex )
		{
			for ( int nYIndex = indices.miny; nYIndex < indices.maxy; ++nYIndex )
			{
				if ( !rApplyFunctional( nXIndex, nYIndex, rPatterns[nPatternIndex].heights[nYIndex - rPatterns[nPatternIndex].pos.y][nXIndex - rPatterns[nPatternIndex].pos.x] * rPatterns[nPatternIndex].fRatio ) )
				{
					return false;
				}
			}
		}
	}
	return true;
}

template<class TYPE>
bool ApplyVAPattern( const CTRect<int> &rRect,
										 const SVAPattern &rPattern,
										 TYPE &rApplyFunctional,
										 bool isIgnoreInvalidIndices = false )
{
	CTRect<int> indices( rPattern.pos.x,
											 rPattern.pos.y,
											 rPattern.pos.x + rPattern.heights.GetSizeX(),
											 rPattern.pos.y + rPattern.heights.GetSizeY() );
	int result = ValidateIndices( rRect, &indices );

	if ( result < 0 )
	{
		if ( isIgnoreInvalidIndices )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	
	if ( ( result == 0 ) && !isIgnoreInvalidIndices )
	{
		return false;
	}

	for ( int nXIndex = indices.minx; nXIndex < indices.maxx; ++nXIndex )
	{
		for ( int nYIndex = indices.miny; nYIndex < indices.maxy; ++nYIndex )
		{
			if ( !rApplyFunctional( nXIndex, nYIndex, rPattern.heights[nYIndex - rPattern.pos.y][nXIndex - rPattern.pos.x] * rPattern.fRatio ) )
			{
				return false;
			}
		}
	}
	
	return true;
}

template<class TYPE>
bool ApplyVAPatternInChain( const CTRect<int> &rRect,
														SVAPattern *pPattern,
														const std::vector<CTPoint<int> > &rPointsChain,
														TYPE &rApplyFunctional,
														bool isIgnoreInvalidIndices = false,
														std::vector<CTRect<int> > *pIgnoreRects = 0 )
{
	NI_ASSERT_T( pPattern != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPattern ) );

	for ( int nPointIndex = 0; nPointIndex < rPointsChain.size(); ++nPointIndex )
	{
		pPattern->pos = rPointsChain[nPointIndex];
		CTRect<int> indices( pPattern->pos.x,
												 pPattern->pos.y,
												 pPattern->pos.x + pPattern->heights.GetSizeX(),
												 pPattern->pos.y + pPattern->heights.GetSizeY() );
		int result = ValidateIndices( rRect, &indices );

		if ( result < 0 )
		{
			if ( isIgnoreInvalidIndices )
			{
				continue;
			}
			else
			{
				return false;
			}
		}
		
		if ( ( result == 0 ) && !isIgnoreInvalidIndices )
		{
			return false;
		}

		for ( int nXIndex = indices.minx; nXIndex < indices.maxx; ++nXIndex )
		{
			for ( int nYIndex = indices.miny; nYIndex < indices.maxy; ++nYIndex )
			{
				bool bOutsideIgnoreRects = true;				
				if ( pIgnoreRects )
				{
					for ( int nRectIndex = 0; nRectIndex < pIgnoreRects->size(); ++nRectIndex )
					{
						const CTRect<int> &rIgrnoreRect = ( *pIgnoreRects )[nRectIndex];
						if ( IsValidPoint( rIgrnoreRect, nXIndex, nYIndex ) )
						{
							bOutsideIgnoreRects = false;
							break;	
						}
					}
				}
				if ( bOutsideIgnoreRects )
				{
					if ( !rApplyFunctional( nXIndex, nYIndex, pPattern->heights[nYIndex - pPattern->pos.y][nXIndex - pPattern->pos.x] * pPattern->fRatio ) )
					{
						return false;
					}
				}
			}
		}
	}
	return true;
}

struct SVACreatePatternByGradientFunctional
{
	SVAPattern *pPattern;
	const SVAGradient *pGradient;
	float fRatio;

	SVACreatePatternByGradientFunctional( SVAPattern *_pPattern, const SVAGradient *_pGradient, float _fRatio = 1.0f )
		: pPattern( _pPattern ), pGradient( _pGradient ), fRatio( _fRatio )
	{
		NI_ASSERT_T( pPattern != 0,
								 NStr::Format( "Wrong parameter: %x\n", pPattern ) );
		NI_ASSERT_T( pGradient != 0,
								 NStr::Format( "Wrong parameter: %x\n", pGradient ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		pPattern->heights[nYIndex][nXIndex] = ( *pGradient )( fValue * ( pGradient->range.max - pGradient->range.min ) ) * fRatio;
		return true;
	}
};

struct SVASetPatternToValueFunctional
{
	SVAPattern *pPattern;
	float fSetValue;

	SVASetPatternToValueFunctional( SVAPattern *_pPattern, float _fSetValue )
		: pPattern( _pPattern ), fSetValue( _fSetValue )
	{
		NI_ASSERT_T( pPattern != 0,
								 NStr::Format( "Wrong parameter: %x\n", pPattern ) );
	}

	bool operator()( int nXIndex, int nYIndex, float fValue )
	{ 
		pPattern->heights[nYIndex][nXIndex] = fSetValue;
		return true;
	}
};
	
template<class TYPE>
static bool ApplyVAInRadius( const CTRect<int> &rRect, TYPE &rApplyFunctional )
{
	NI_ASSERT_T( ( ( rRect.Width() > 0 ) && ( ( rRect.Width() & 0x1 ) == 0 ) ) &&
							 ( ( rRect.Height() > 0 ) && ( ( rRect.Height() & 0x1 ) == 0 ) ), 
							 NStr::Format( "Invalid sizes: (%d, %d)\n", rRect.Width(), rRect.Height() ) );

	float fHalfAxisA = ( rRect.Width() - 1.0f ) / 2.0f;
	float fHalfAxisB = ( rRect.Height() - 1.0f ) / 2.0f;

	for ( int nXIndex = 0; nXIndex < rRect.Width(); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < rRect.Height(); ++nYIndex )
		{
			const float fPosX = ( nXIndex - fHalfAxisA ) / fHalfAxisA;
			const float fPosY = ( nYIndex - fHalfAxisB ) / fHalfAxisB;
			const float fDistance = fabs( fPosX, fPosY );
			if ( fDistance <= 1.0f )
			{
				if ( !rApplyFunctional( nXIndex + rRect.minx, nYIndex + rRect.miny, fDistance ) )
				{
					return false;
				}
			}
		}
	}
	return true;
}

class CVertexAltitudeInfo : public STerrainInfo::TVertexAltitudeArray2D
{
private:
	CVertexAltitudeInfo() {}

public:
	static const float CAMERA_ALPHA;
	static const float WORLD_CAMERA_ALPHA;
	static const CVec3 V3_CAMERA_NEGATIVE;
	static const CVec3 V3_CAMERA_POSITIVE;
		
	inline static SGFXLightDirectional GetSunLight( CMapInfo::SEASON nSeason )
	{
		NI_ASSERT_T( ( nSeason >= 0 ) && ( nSeason < CMapInfo::SEASON_COUNT ),
								 NStr::Format( "Invalid season: %d\n", nSeason ) );

		std::string szSeason = CMapInfo::SEASON_NAMES[nSeason];

		SGFXLightDirectional sunlight;
		Zero( sunlight );
		sunlight.vDiffuse.Set( GetGlobalVar( ("Scene.SunLight." + szSeason + ".Diffuse.A").c_str(), 1.0f ),
													 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Diffuse.R").c_str(), 1.0f ),
													 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Diffuse.G").c_str(), 1.0f ),
													 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Diffuse.B").c_str(), 1.0f ) );
		sunlight.vAmbient.Set( GetGlobalVar( ("Scene.SunLight." + szSeason + ".Ambient.A").c_str(), 1.0f ),
													 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Ambient.R").c_str(), 1.0f ),
													 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Ambient.G").c_str(), 1.0f ),
													 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Ambient.B").c_str(), 1.0f ) );
		sunlight.vDir.Set( GetGlobalVar( ("Scene.SunLight." + szSeason + ".Direction.X").c_str(),  1.0f ),
											 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Direction.Y").c_str(),  1.0f ),
											 GetGlobalVar( ("Scene.SunLight." + szSeason + ".Direction.Z").c_str(), -2.0f ) );
		Normalize( &sunlight.vDir );
		sunlight.vSpecular.Set( 1, 1, 1, 1 );

		return sunlight;
	}

	static bool UpdateShades( STerrainInfo::TVertexAltitudeArray2D *pAltitude, const CTRect<int> &rUpdateRect, const SGFXLightDirectional &rSunlight );

	static const CVec3 GetNormale( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, int nXPos, int nYPos );
	static const CVec3 GetNormale( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const CTPoint<int> &rPoint );
	
	static bool GetHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float fXPos, float fYPos, float *pfHeight );
	static bool GetHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const CVec2 &rvPos, float *pfHeight );
	
	static bool IsValidHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, int nXPos, int nYPos );
	static bool IsValidHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const CTPoint<int> &rPoint );
	static bool IsValidHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const CTRect<int> &rRect );

	static bool ValidateHeights( STerrainInfo::TVertexAltitudeArray2D *pAltitude, int nPosX, int nPosY, int nSize, CTRect<int> *pAffectedRect );
	static bool ValidateHeights( STerrainInfo::TVertexAltitudeArray2D *pAltitude, const CTPoint<int> &rPoint, int nSize, CTRect<int> *pAffectedRect );

	static bool GetHeightsRange( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float *pfMinHeight, float *pfMaxHeight );
	static bool GetShadesRange( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float *pfMinShade, float *pfMaxShade );

	static IImage* GetHeightsImage( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float fRatio = 1.0f, bool bTerrainSize = true );
	static IImage* GetShadesImage( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float fRatio = 1.0f, bool bTerrainSize = true );
};
#endif //#if !defined(__VA__Types__)

