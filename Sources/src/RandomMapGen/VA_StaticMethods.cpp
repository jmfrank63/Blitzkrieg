#include "stdafx.h"

#include "VA_Types.h"
#include "..\Formats\FmtTerrain.h"
#include "IB_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const float CVertexAltitudeInfo::CAMERA_ALPHA = ( FP_SQRT_2 / FP_SQRT_3 );
const float CVertexAltitudeInfo::WORLD_CAMERA_ALPHA = CAMERA_ALPHA * fWorldCellSize;
const CVec3 CVertexAltitudeInfo::V3_CAMERA_NEGATIVE( -1, -1, -( CVertexAltitudeInfo::CAMERA_ALPHA ) );
const CVec3 CVertexAltitudeInfo::V3_CAMERA_POSITIVE( 1, 1, -( CVertexAltitudeInfo::CAMERA_ALPHA ) );

const CVec3 CVertexAltitudeInfo::GetNormale( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, int nXPos, int nYPos )
{
	if ( !IsValidIndices( rAltitude, nXPos, nYPos ) )
	{
		return VNULL3;
	}

	const float fZ = rAltitude[nYPos][nXPos].fHeight;
	const CVec3 v1( - fWorldCellSize, + fWorldCellSize, ( ( ( nXPos == 0 ) || ( nYPos == 0 ) ) ? fZ : rAltitude[nYPos - 1][nXPos - 1].fHeight ) - fZ );
	const CVec3 v2( 0               , + fWorldCellSize, ( ( nYPos == 0 ) ? fZ : rAltitude[nYPos - 1][nXPos].fHeight ) - fZ );
	const CVec3 v3( + fWorldCellSize, 0               , ( ( nXPos == ( rAltitude.GetSizeX() - 1 ) ) ? fZ : rAltitude[nYPos][nXPos + 1].fHeight ) - fZ );
	const CVec3 v4( + fWorldCellSize, - fWorldCellSize, ( ( ( nXPos == ( rAltitude.GetSizeX() - 1 ) ) || ( nYPos == ( rAltitude.GetSizeY() - 1 ) ) ) ? fZ : rAltitude[nYPos + 1][nXPos + 1].fHeight ) - fZ );
	const CVec3 v5( 0               , - fWorldCellSize, ( ( nYPos == ( rAltitude.GetSizeY() - 1 ) ) ? fZ : rAltitude[nYPos + 1][nXPos].fHeight ) - fZ );
	const CVec3 v6( - fWorldCellSize, 0               , ( ( nXPos == 0 ) ? fZ : rAltitude[nYPos][nXPos - 1].fHeight ) - fZ );

	CVec3 vNorm = ( v2 ^ v1 ) + (v3 ^ v2) + (v4 ^ v3) + (v5 ^ v4) + (v6 ^ v5) + (v1 ^ v6);
	if ( Normalize( &vNorm ) )
	{
		return vNorm;
	}
	else
	{
		return VNULL3;
	}
}

const CVec3 CVertexAltitudeInfo::GetNormale( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const CTPoint<int> &rPoint )
{
	return GetNormale( rAltitude, rPoint.x, rPoint.y );
}

bool CVertexAltitudeInfo::UpdateShades( STerrainInfo::TVertexAltitudeArray2D *pAltitude, const CTRect<int> &rUpdateRect, const SGFXLightDirectional &rSunlight )
{
	NI_ASSERT_TF( pAltitude != 0,
							  NStr::Format( "Wrong parameter: %x\n", pAltitude ),
							  return false );

	CTRect<int> updateRect( rUpdateRect );
	CTRect<int> altitudeRect( 0, 0, pAltitude->GetSizeX(), pAltitude->GetSizeY() );
	
	if ( ValidateIndices( altitudeRect, &updateRect ) >= 0 )
	{
		for ( int nXIndex = updateRect.minx; nXIndex < updateRect.maxx; ++nXIndex )
		{
			for ( int nYIndex = updateRect.miny; nYIndex < updateRect.maxy; ++nYIndex )
			{
				const CVec3 vNorm = GetNormale( *pAltitude, nXIndex, nYIndex );
				if ( vNorm != VNULL3 )
				{
					const float fShade = Clamp( -( rSunlight.vDir * vNorm ), 0.6f, 1.0f );
					( *pAltitude )[nYIndex][nXIndex].shade = BYTE( fShade * 255.0f );
				}
			}
		}
	}

	return true;
}

bool CVertexAltitudeInfo::GetHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const CVec2 &rvPos, float *pfHeight )
{
	return GetHeight( rAltitude, rvPos.x, rvPos.y, pfHeight );
}

bool CVertexAltitudeInfo::GetHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float fXPos, float fYPos, float *pfHeight )
{
	NI_ASSERT_TF( pfHeight != 0,
							  NStr::Format( "Wrong parameter: %x\n", pfHeight ),
							  return false );
	
	*pfHeight = 0.0f;

	fYPos = ( ( rAltitude.GetSizeY() - 1 ) * fWorldCellSize ) - fYPos;

	if ( fXPos < 0.0f ) 
	{
		fXPos = 0.0f;
	}
	if ( fYPos < 0.0f )
	{
		fYPos = 0.0f;
	}
	if ( fXPos >= ( ( rAltitude.GetSizeX() - 1 ) * fWorldCellSize ) )
	{
		fXPos = ( ( rAltitude.GetSizeX() - 1 ) * fWorldCellSize ) - 0.1f;
	}
	if ( fYPos >= ( ( rAltitude.GetSizeY() - 1 ) * fWorldCellSize ) )
	{
		fYPos = ( ( rAltitude.GetSizeY() - 1 ) * fWorldCellSize ) - 0.1f;
	}

	const int nXTile = fXPos / fWorldCellSize;
	const int nYTile = fYPos / fWorldCellSize;
	const float fXRest = fXPos - ( nXTile * fWorldCellSize );
	const float fYRest = fYPos - ( nYTile * fWorldCellSize );

	SPlane plane;
	if ( ( fXRest + fYRest ) <= fWorldCellSize ) 
	{
		const CVec3 v0( ( nXTile + 0 ) * fWorldCellSize, ( nYTile + 0 ) * fWorldCellSize, rAltitude[nYTile + 0][nXTile + 0].fHeight );
		const CVec3 v1( ( nXTile + 0 ) * fWorldCellSize, ( nYTile + 1 ) * fWorldCellSize, rAltitude[nYTile + 1][nXTile + 0].fHeight );
		const CVec3 v2( ( nXTile + 1 ) * fWorldCellSize, ( nYTile + 0 ) * fWorldCellSize, rAltitude[nYTile + 0][nXTile + 1].fHeight );

		plane.Set( v0, v1, v2 );
	}
	else
	{
		const CVec3 v0( ( nXTile + 1 ) * fWorldCellSize, ( nYTile + 1 ) * fWorldCellSize, rAltitude[nYTile + 1][nXTile + 1].fHeight );
		const CVec3 v1( ( nXTile + 0 ) * fWorldCellSize, ( nYTile + 1 ) * fWorldCellSize, rAltitude[nYTile + 1][nXTile + 0].fHeight );
		const CVec3 v2( ( nXTile + 1 ) * fWorldCellSize, ( nYTile + 0 ) * fWorldCellSize, rAltitude[nYTile + 0][nXTile + 1].fHeight );
		
		plane.Set( v0, v1, v2 );
	}

	*pfHeight = -( ( plane.a * fXPos ) + ( plane.b * fYPos ) + plane.d ) / plane.c;
	return true;
}

/**
const float CStaticMap::GetVisZ( float x, float y ) const
{
	float u, v;
	float ptCtrls[16];
	
	x = Clamp( x, 0.0f, float(SConsts::TILE_SIZE * theStaticMap.GetSizeX()) );
	y = Clamp( y, 0.0f, float(SConsts::TILE_SIZE * theStaticMap.GetSizeY()) );

	if ( (theStaticMap.GetSizeX() + theStaticMap.GetSizeY()) == 0 ) 
		return 0;
	GetPoint4Spline( CVec2( x, y ), &u, &v, ptCtrls );
	return betaSpline3D.Value( u, v, ptCtrls ) * 2.0f * SConsts::TILE_SIZE * fAITileZCoeff1;
}
/**/

bool CVertexAltitudeInfo::IsValidHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, int nXPos, int nYPos )
{
	if ( ( nXPos >= 0 ) &&
			 ( nYPos >= 0 ) &&
			 ( nXPos < rAltitude.GetSizeX() ) &&
			 ( nYPos < rAltitude.GetSizeY() ) )
	{
		CVec3 v0( ( nXPos + 0 ) * fWorldCellSize, ( nYPos + 0 ) * fWorldCellSize, rAltitude[nYPos + 0][nXPos + 0].fHeight );
		CVec3 v1( ( nXPos - 1 ) * fWorldCellSize, ( nYPos + 0 ) * fWorldCellSize, ( nXPos > 0 ) ? rAltitude[nYPos + 0][nXPos - 1].fHeight : v0.z );
		CVec3 v2( ( nXPos + 0 ) * fWorldCellSize, ( nYPos - 1 ) * fWorldCellSize, ( nYPos > 0 ) ? rAltitude[nYPos - 1][nXPos + 0].fHeight : v0.z );
		CVec3 v3( ( nXPos + 0 ) * fWorldCellSize, ( nYPos + 1 ) * fWorldCellSize, ( nYPos < ( rAltitude.GetSizeY() - 1 ) ) ? rAltitude[nYPos + 1][nXPos + 0].fHeight : v0.z );
		CVec3 v4( ( nXPos + 1 ) * fWorldCellSize, ( nYPos + 0 ) * fWorldCellSize, ( nXPos < ( rAltitude.GetSizeX() - 1 ) ) ? rAltitude[nYPos + 0][nXPos + 1].fHeight : v0.z );

		CVec3 n0 = ( v2 - v0 ) ^ ( v1 - v0 );
		CVec3 n1 = ( v3 - v0 ) ^ ( v4 - v0 );

		return ( ( V3_CAMERA_NEGATIVE * n0 ) > 0 ) && 
					 ( ( V3_CAMERA_NEGATIVE * n1 ) > 0 ) && 
					 ( ( V3_CAMERA_POSITIVE * n0 ) > 0 ) &&
					 ( ( V3_CAMERA_POSITIVE * n1 ) > 0 );
	}
	return false;
}

bool CVertexAltitudeInfo::IsValidHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const CTPoint<int> &rPoint )
{
	return IsValidHeight( rAltitude, rPoint.x, rPoint.y );
}

bool CVertexAltitudeInfo::IsValidHeight( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, const CTRect<int> &rRect )
{
	CTRect<int> rect( rRect );
	ValidateIndices( CTRect<int>( 0, 0, rAltitude.GetSizeX(), rAltitude.GetSizeY() ), &rect );
	for ( int nXIndex = rect.minx; nXIndex < rect.maxx; ++nXIndex )
	{
		for ( int nYIndex = rect.miny; nYIndex < rect.maxy; ++nYIndex )
		{
			if( !IsValidHeight( rAltitude, nXIndex, nYIndex ) )
			{
				return false;
			}
		}
	}
	return true;
}

bool CVertexAltitudeInfo::ValidateHeights( STerrainInfo::TVertexAltitudeArray2D *pAltitude, int nPosX, int nPosY, int nSize, CTRect<int> *pAffectedRect )
{
	NI_ASSERT_T( pAffectedRect != 0,
							 NStr::Format( "Wrong parameter: %x\n", pAffectedRect ) );
	NI_ASSERT_T( pAltitude != 0,
							 NStr::Format( "Wrong parameter: %x\n", pAltitude ) );
	NI_ASSERT_T( ( nPosX >= 0 ) &&
							 ( nPosY >= 0 ) &&
							 ( nPosX < ( pAltitude->GetSizeX() - 1 ) ) &&
							 ( nPosY < ( pAltitude->GetSizeY() - 1 ) ),
							 NStr::Format( "Invalid indices: (%d, %d), ( 0, 0, %d, %d)\n",
														 nPosX,
														 nPosY,
														 pAltitude->GetSizeX() - 1,
														 pAltitude->GetSizeY() - 1 ) );

	( *pAffectedRect ) = CTRect<int>( 0, 0, 0, 0 );
	return true;
}

bool CVertexAltitudeInfo::ValidateHeights( STerrainInfo::TVertexAltitudeArray2D *pAltitude, const CTPoint<int> &rPoint, int nSize, CTRect<int> *pAffectedRect )
{
	return ValidateHeights( pAltitude, rPoint.x, rPoint.y, nSize, pAffectedRect );
}

bool CVertexAltitudeInfo::GetHeightsRange( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float *pfMinHeight, float *pfMaxHeight )
{
	NI_ASSERT_T( ( pfMinHeight != 0 ) && ( pfMaxHeight != 0 ),
							 NStr::Format( "Wrong parameter: %x %x\n", pfMinHeight, pfMaxHeight ) );

	const float fZeroHeight = rAltitude[0][0].fHeight;
	( *pfMinHeight ) = fZeroHeight;
	( *pfMaxHeight ) = fZeroHeight;

	for ( int nYIndex = 0; nYIndex < rAltitude.GetSizeY(); ++nYIndex )
	{
		for ( int nXIndex = 0; nXIndex < rAltitude.GetSizeX(); ++nXIndex )
		{
			const float fHeight = rAltitude[nYIndex][nXIndex].fHeight;
			if ( fHeight > ( *pfMaxHeight ) )
			{
				( *pfMaxHeight ) = fHeight;
			}
			else if ( fHeight < ( *pfMinHeight ) )
			{
				( *pfMinHeight ) = fHeight;
			}
		}
	}
	return true;
}

bool CVertexAltitudeInfo::GetShadesRange( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float *pfMinShade, float *pfMaxShade )
{
	NI_ASSERT_T( ( pfMinShade != 0 ) && ( pfMaxShade != 0 ),
							 NStr::Format( "Wrong parameter: %x %x\n", pfMinShade, pfMaxShade ) );

	const float fZeroShade = rAltitude[0][0].shade;
	( *pfMinShade ) = fZeroShade;
	( *pfMaxShade ) = fZeroShade;

	for ( int nYIndex = 0; nYIndex < rAltitude.GetSizeY(); ++nYIndex )
	{
		for ( int nXIndex = 0; nXIndex < rAltitude.GetSizeX(); ++nXIndex )
		{
			const float fShade = rAltitude[nYIndex][nXIndex].shade;
			if ( fShade > ( *pfMaxShade ) )
			{
				( *pfMaxShade ) = fShade;
			}
			else if ( fShade < ( *pfMinShade ) )
			{
				( *pfMinShade ) = fShade;
			}
		}
	}
	return true;
}

IImage* CVertexAltitudeInfo::GetHeightsImage( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float fRatio, bool bTerrainSize )
{
	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return 0;
	}
	
	CTPoint<int> imageSize( rAltitude.GetSizeX() - ( bTerrainSize ? 1 : 0 ),
													rAltitude.GetSizeY() - ( bTerrainSize ? 1 : 0 ) );
	IImage *pHeightsImage = pImageProcessor->CreateImage( imageSize.x, imageSize.y );
	if ( pHeightsImage )
	{
		CUnsafeImageAccessor heightsImageAccessor = pHeightsImage;
		
		float fMinHeight = 0.0f;
		float fMaxHeight = 0.0f;
		GetHeightsRange( rAltitude, &fMinHeight, &fMaxHeight );
		if ( ( fMaxHeight - fMinHeight ) <= FP_EPSILON )
		{
			pHeightsImage->Set( CRMImageBuilder::GRAY_LIGHTER_COLOR );
		}
		else
		{
			for ( int nYIndex = 0; nYIndex < imageSize.y; ++nYIndex )
			{
				for ( int nXIndex = 0; nXIndex < imageSize.x; ++nXIndex )
				{
					const float fColor = fRatio * 2.0f * ( ( ( rAltitude[nYIndex][nXIndex].fHeight - fMinHeight ) / ( fMaxHeight - fMinHeight ) ) - 0.5f );
					heightsImageAccessor[nYIndex][nXIndex].a = 0xFF;
					heightsImageAccessor[nYIndex][nXIndex].r = static_cast<BYTE>( CRMImageBuilder::GRAY_DARKER_COLOR.r * ( 1.0f + fColor ) );
					heightsImageAccessor[nYIndex][nXIndex].g = static_cast<BYTE>( CRMImageBuilder::GRAY_DARKER_COLOR.g * ( 1.0f + fColor ) );
					heightsImageAccessor[nYIndex][nXIndex].b = static_cast<BYTE>( CRMImageBuilder::GRAY_DARKER_COLOR.b * ( 1.0f + fColor ) );
				}
			}
		}
	}
	return pHeightsImage;
}

IImage* CVertexAltitudeInfo::GetShadesImage( const STerrainInfo::TVertexAltitudeArray2D &rAltitude, float fRatio, bool bTerrainSize )
{
	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	if ( !pImageProcessor )
	{
		return 0;
	}
	
	CTPoint<int> imageSize( rAltitude.GetSizeX() - ( bTerrainSize ? 1 : 0 ),
													rAltitude.GetSizeY() - ( bTerrainSize ? 1 : 0 ) );
	IImage *pShadesImage = pImageProcessor->CreateImage( imageSize.x, imageSize.y );
	if ( pShadesImage )
	{
		CUnsafeImageAccessor shadesImageAccessor = pShadesImage;
		
		float fMinShade = 0.0f;
		float fMaxShade = 0.0f;
		GetShadesRange( rAltitude, &fMinShade, &fMaxShade );
		if ( ( fMaxShade - fMinShade ) <= FP_EPSILON )
		{
			pShadesImage->Set( CRMImageBuilder::GRAY_LIGHTER_COLOR );
		}
		else
		{
			for ( int nYIndex = 0; nYIndex < imageSize.y; ++nYIndex )
			{
				for ( int nXIndex = 0; nXIndex < imageSize.x; ++nXIndex )
				{
					float fColor = fRatio * 2.0f * ( ( ( static_cast<float>( rAltitude[nYIndex][nXIndex].shade ) - fMinShade ) / ( fMaxShade - fMinShade ) ) - 0.5f );
					shadesImageAccessor[nYIndex][nXIndex].a = 0xFF;
					shadesImageAccessor[nYIndex][nXIndex].r = static_cast<BYTE>( CRMImageBuilder::GRAY_DARKER_COLOR.r * ( 1.0f + fColor ) );
					shadesImageAccessor[nYIndex][nXIndex].g = static_cast<BYTE>( CRMImageBuilder::GRAY_DARKER_COLOR.g * ( 1.0f + fColor ) );
					shadesImageAccessor[nYIndex][nXIndex].b = static_cast<BYTE>( CRMImageBuilder::GRAY_DARKER_COLOR.b * ( 1.0f + fColor ) );
				}
			}
		}
	}
	return pShadesImage;
}

/**
							CTRect<int> rect( updateRect );
							SVAPattern checkPattern;
							while ( !CVertexAltitudeInfo::IsValidHeight( rTerrainInfo.altitudes, rect ) )
							{
								checkPattern.CreateValue( 1.0f, rect.Width() );
								checkPattern.pos.x = terrainTile.x - ( checkPattern.heights.GetSizeX() / 2 - 1 );
								checkPattern.pos.y = terrainTile.y - ( checkPattern.heights.GetSizeY() / 2 - 1 );
								
								const float fLevelToHeight = checkPattern.GetAverageHeight( rTerrainInfo.altitudes );
								const float fLevelRatio = 0.1f;
								
								SVALevelFunctional functional( &( rTerrainInfo.altitudes ), fLevelToHeight, fLevelRatio );
								ApplyVAPattern( rect, checkPattern, functional, true );

								rect.minx -= 1;	
								rect.miny -= 1;	
								rect.maxx += 1;	
								rect.maxy += 1;	
							}
/**/
/**
	const int nSideCount = 6;
	int sideIndexCounts[nSideCount] = { 1, 1, 2, 2, 1, 3, };
	const int addSideIndexCounts[nSideCount] = { 1, 1, 1, 1, 1, 1 };
	const CTPoint<int> addStartPoint( -1, 1 );
	const CTPoint<int> addIndexPoints[nSideCount] = { CTPoint<int>( 0, -1 ),
																										CTPoint<int>( 1, -1 ),
																										CTPoint<int>( 1, 0 ),
																										CTPoint<int>( 0, 1 ),
																										CTPoint<int>( -1, 1 ),
																										CTPoint<int>( -1, 0 ), };
	
	const CTPoint<int> z1ShiftPoints[nSideCount] = { CTPoint<int>( 1, 0 ),
																									 CTPoint<int>( 0, 1 ),
																									 CTPoint<int>( -1, 1 ),
																									 CTPoint<int>( -1, 0 ),
																									 CTPoint<int>( 0, -1 ),
																									 CTPoint<int>( 1, -1 ), };
	
	const CTPoint<int> z2ShiftPoints[nSideCount] = { CTPoint<int>( 1, -1 ),
																									 CTPoint<int>( 1, 0 ),
																									 CTPoint<int>( 0, 1 ),
																									 CTPoint<int>( -1, 1 ),
																									 CTPoint<int>( -1, 0 ),
																									 CTPoint<int>( 0, -1 ), };
	
	const CTPoint<int> z3ShiftPoints[nSideCount] = { CTPoint<int>( 0, 1 ),
																									 CTPoint<int>( -1, 1 ),
																									 CTPoint<int>( -1, 0 ),
																									 CTPoint<int>( 0, -1 ),
																									 CTPoint<int>( 1, -1 ),
																									 CTPoint<int>( 1, 0 ), };

	CTPoint<int> nCurrentPoint( nPosX + addStartPoint.x, nPosY + addStartPoint.y );
	bool bChanged = true;
	int nSizeIndex = 0;
	while (  bChanged || ( nSizeIndex < nSize ) )
	{
		++nSizeIndex;
		bChanged = false;
		for ( int nSideIndex = 0; nSideIndex < nSideCount; ++nSideIndex )	
		{
			for ( int nPointIndex = 0; nPointIndex < sideIndexCounts[nSideIndex]; ++nPointIndex )
			{
				if ( ( nCurrentPoint.x >= 0 ) &&
						 ( nCurrentPoint.y >= 0 ) &&
						 ( nCurrentPoint.x < pAltitude->GetSizeX() ) &&
						 ( nCurrentPoint.y < pAltitude->GetSizeY() ) )
				{
					const float z0 = (*pAltitude)[nCurrentPoint.y][nCurrentPoint.x].fHeight;
					
					const float z1 = (*pAltitude)[nCurrentPoint.y + z1ShiftPoints[nSideIndex].y][nCurrentPoint.x + z1ShiftPoints[nSideIndex].x].fHeight;
					const float z2 = (*pAltitude)[nCurrentPoint.y + z2ShiftPoints[nSideIndex].y][nCurrentPoint.x + z2ShiftPoints[nSideIndex].x].fHeight;

					float fMin0 = 0.0f;
					float fMax0 = 0.0f;

					if ( ( nSideIndex == 0 ) || ( nSideIndex == 3 ) )
					{
						fMin0 = ( 2 * z1 ) - z2 - WORLD_CAMERA_ALPHA;
						fMax0 = ( 2 * z1 ) - z2 + WORLD_CAMERA_ALPHA;
					}
					else if ( ( nSideIndex == 2 ) || ( nSideIndex == 5 ) )
					{
						fMin0 = ( 2 * z2 ) - z1 - WORLD_CAMERA_ALPHA;
						fMax0 = ( 2 * z2 ) - z1 + WORLD_CAMERA_ALPHA;
					}
					else
					{
						fMin0 = ( z1 + z2 - WORLD_CAMERA_ALPHA ) / 2.0f;
						fMax0 = ( z1 + z2 + WORLD_CAMERA_ALPHA ) / 2.0f;
					}
					
					if ( z0 < fMin0 )
					{
						(*pAltitude)[nCurrentPoint.y][nCurrentPoint.x].fHeight = fMin0;
						bChanged = true;
					}
					else if ( z0 > fMax0 )
					{
						(*pAltitude)[nCurrentPoint.y][nCurrentPoint.x].fHeight = fMax0;
						bChanged = true;
					}
				}
				nCurrentPoint += addIndexPoints[nSideIndex];
			}
			sideIndexCounts[nSideIndex] += addSideIndexCounts[nSideIndex];
		}
	}	
/**/
