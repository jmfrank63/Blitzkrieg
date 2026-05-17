#include "StdAfx.h"
//ReportObjectiveStateChanged
#include "..\Input\Input.h"
#include "..\Main\iMainCommands.h"
#include "..\Scene\Scene.h"
#include "..\Formats\fmtTerrain.h"

#include "..\GFX\GFX.h"
#include "..\GFX\GFXHelper.h"

#include "UIMiniMap.h"
#include "..\RandomMapGen\Bresenham_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\Common\PauseGame.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char CUIMiniMap::MARKERS_TYPES_FILE_NAME[] = "UI\\MiniMapMarkers";
const char CUIMiniMap::MARKERS_TEXTURE_NAME[] = "MiniMapMarkersTexture";
const char CUIMiniMap::MARKERS_TYPES_NAME[] = "MiniMapMarkers";

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STextureMarker::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Size", &size );	
	if ( saver.IsReading() )
	{
		CTPoint<int> hotSpot;
		saver.Add( "Rect", &screenRect );
		saver.Add( "HotSpot", &hotSpot );
		//
		textureRect.minx = ( screenRect.minx + 0.5f ) / size.x;
		textureRect.miny = ( screenRect.miny + 0.5f ) / size.y;
		textureRect.maxx = ( screenRect.maxx + 0.5f ) / size.x;
		textureRect.maxy = ( screenRect.maxy + 0.5f ) / size.y;
		//
		screenRect.minx -= hotSpot.x;
		screenRect.miny -= hotSpot.y;
		screenRect.maxx -= hotSpot.x;
		screenRect.maxy -= hotSpot.y;
	}
	else
	{
		CTPoint<int> hotSpot( ( textureRect.minx * size.x ) - screenRect.minx, ( textureRect.miny * size.y ) - screenRect.miny ); 
		CTRect<int> originalScreenRect;
		originalScreenRect.minx = screenRect.minx + hotSpot.x;
		originalScreenRect.miny = screenRect.miny + hotSpot.y;
		originalScreenRect.maxx = screenRect.maxx + hotSpot.x;
		originalScreenRect.maxy = screenRect.maxy + hotSpot.y;

		saver.Add( "Rect", &originalScreenRect );
		saver.Add( "HotSpot", &hotSpot );
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STextureMarker::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &textureRect );	
	saver.Add( 2, &screenRect );	
	saver.Add( 3, &size );	

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SMiniMapMarker::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<STextureMarker*>( this ) );
	saver.Add( 2, &szName );	
	saver.Add( 3, &vPos );	
	saver.Add( 4, &bActive );	
	saver.Add( 5, &nID );	
	saver.Add( 6, &timeStart );	
	saver.Add( 7, &timeDuration );
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::CreateMiniMapTextures()
{
	IGFX* _pGFX = GetSingleton<IGFX>();

	if ( IsInitialized() )
	{
		//������� �������� ������� �������
		pWarFog = _pGFX->CreateTexture( GetNextPow2( terrainSize.x ), GetNextPow2( terrainSize.y ), 1, GFXPF_ARGB4444, GFXD_SYSMEM );
		pWarFogTexture = _pGFX->CreateTexture( GetNextPow2( terrainSize.x ), GetNextPow2( terrainSize.y ), 1, GFXPF_ARGB4444, GFXD_STATIC );

		pInstantObjects = _pGFX->CreateTexture( GetNextPow2( static_cast<int>( wndRect.right - wndRect.left ) ), GetNextPow2( static_cast<int>( wndRect.bottom - wndRect.top ) ), 1, GFXPF_ARGB4444, GFXD_SYSMEM );
		pInstantObjectsTexture = _pGFX->CreateTexture( GetNextPow2( static_cast<int>(wndRect.right - wndRect.left ) ), GetNextPow2( static_cast<int>( wndRect.bottom - wndRect.top ) ), 1, GFXPF_ARGB4444, GFXD_STATIC );

		//�������������� �������� ��� �����������
		if ( pWarFog )
		{
			//�������������� �������� pWarFog
			{
				CTextureLock<SGFXColor4444> textureLock( pWarFog , 0 );
				const int nNumElements = pWarFog->GetSizeX( 0 );
				const DWORD dwValue = PackDWORD( pWarFogValues[0], pWarFogValues[0] );
				for ( int nYIndex = 0; nYIndex < pWarFog->GetSizeY( 0 ); ++nYIndex )
				{
					MemSetDWord( reinterpret_cast<DWORD*>( textureLock[nYIndex] ), dwValue, nNumElements / 2 );
				}
				isWarFogNeedUpdate = true;
			}
		}
		if ( pInstantObjects )
		{
			//�������������� �������� pInstantObjects
			{
				CTextureLock<SGFXColor4444> textureLock( pInstantObjects , 0 );
				int nBytesCount = pInstantObjects->GetSizeX( 0 ) * sizeof( WORD );
				for ( int nYIndex = 0; nYIndex < pInstantObjects->GetSizeY( 0 ); ++nYIndex )
				{
					memset( static_cast<void*>( textureLock[nYIndex] ), 0x00, nBytesCount );
				}
				isInstantObjectsNeedUpdate = true;
			}
		}

		//���������  �������� pWarFog � �����������
		if ( pWarFog && pWarFogTexture && isWarFogNeedUpdate )
		{
			pWarFog->AddDirtyRect( &( static_cast<RECT>( CTRect<int>( 0, 0, pWarFog->GetSizeX( 0 ), pWarFog->GetSizeY( 0 ) ) ) ) );
			_pGFX->UpdateTexture( pWarFog, pWarFogTexture, false );
			isWarFogNeedUpdate = false;
		}

		//���������  �������� pInstantObjects � �����������
		if ( pInstantObjects && pInstantObjectsTexture && isInstantObjectsNeedUpdate )
		{
			pInstantObjects->AddDirtyRect( &( static_cast<RECT>( CTRect<int>( 0, 0, pInstantObjects->GetSizeX( 0 ), pInstantObjects->GetSizeY( 0 ) ) ) ) );
			_pGFX->UpdateTexture( pInstantObjects, pInstantObjectsTexture, false );
			isInstantObjectsNeedUpdate = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIMiniMap::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CSimpleWindow*>( this ) );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIMiniMap::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CSimpleWindow*>( this ) );

	saver.Add( 3, &pPartyColors );
	saver.Add( 4, &dwScreenFrameColor );

  saver.Add( 5, &nSize );
  saver.Add( 6, &nUnitCrossSize );
  saver.Add( 7, &terrainSize );

  saver.Add( 8, &nFiledVISTiles );

  saver.Add( 9, &pBackgroundTexture );
  
  saver.Add( 11, &shootAreas );
  saver.Add( 12, &circles );
	//�� �����, �������� � CreateMiniMapTextures()
	//saver.Add( 13, &isWarFogNeedUpdate );
	//saver.Add( 14, &isInstantObjectsNeedUpdate );
	saver.Add( 13, &dwScreenFrameColorShadow );
	
	saver.Add( 14, &markersTypes );
	saver.Add( 15, &markers );
	saver.Add( 16, &pMarkerTexture );
	saver.Add( 17, &pWarFogValues );
	saver.Add( 18, &nPlayersCount );
	saver.Add( 19, &units );

	if ( saver.IsReading() )
	{
		CreateMiniMapTextures();
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::GetZeroPoint( float *pfXZeroPoint, float *pfYZeroPoint, bool isTopLeft )
{
	NI_ASSERT_SLOW_T( ( pfXZeroPoint ) && ( pfYZeroPoint ),
										NStr::Format( "Wrong parameters: (%x, %x)", pfXZeroPoint, pfYZeroPoint ) );
	NI_ASSERT_SLOW_T( ( terrainSize.x > 0 ) && ( terrainSize.y > 0 ),
										NStr::Format( "Wrong terrainSize: (%d, %d)", terrainSize.x, terrainSize.y ) );
	if ( terrainSize.x > terrainSize.y )
	{
		*pfXZeroPoint = ( nSize - ( ( nSize * terrainSize.y ) / ( terrainSize.x * 1.0f ) ) ) / 4.0f;
		if ( isTopLeft )
		{
			*pfYZeroPoint = nSize / 4.0f - ( nSize - ( ( nSize * terrainSize.y ) / ( terrainSize.x * 1.0f ) ) ) / 8.0f; 
		}
		else
		{
			*pfYZeroPoint = ( nSize - ( ( nSize * terrainSize.y ) / ( terrainSize.x * 1.0f ) ) ) / 8.0f + nSize / 4.0f; 
		}
	}
	else if ( terrainSize.x < terrainSize.y )
	{
		*pfXZeroPoint = ( nSize - ( ( nSize * terrainSize.x ) / ( terrainSize.y * 1.0f ) ) ) / 4.0f;
		if ( isTopLeft )
		{
			*pfYZeroPoint = ( nSize - ( ( nSize * terrainSize.x ) / ( terrainSize.y * 1.0f ) ) ) / 8.0f + nSize / 4.0f; 
		}
		else
		{
			*pfYZeroPoint = nSize / 4.0f - ( nSize - ( ( nSize * terrainSize.x ) / ( terrainSize.y * 1.0 ) ) ) / 8.0f; 
		}
	}
	else
	{
		*pfXZeroPoint = 0.0f;
		*pfYZeroPoint = nSize / 4.0f; 
	}

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::PointToTextureMiniMap( float fXPos, float fYPos, float *pfXMiniMapPos, float *pfYMiniMapPos, bool isLeftTop )
{
	NI_ASSERT_SLOW_T( ( pfXMiniMapPos ) && ( pfYMiniMapPos ),
										NStr::Format( "Wrong parameters: (%x, %x)", pfXMiniMapPos, pfYMiniMapPos ) );
	NI_ASSERT_SLOW_T( ( terrainSize.x > 0 ) && ( terrainSize.y > 0 ),
										NStr::Format( "Wrong terrainSize: (%d, %d)", terrainSize.x, terrainSize.y ) );

	CTPoint<float> zeroPoint;
	GetZeroPoint( &( zeroPoint.x ), &( zeroPoint.y ), isLeftTop );
	float fXLocalPos = 0;
	float fYLocalPos = 0;
	if ( terrainSize.x >= terrainSize.y )
	{
		fXLocalPos = ( nSize * fXPos ) / ( terrainSize.x * 2.0f );
		fYLocalPos = ( nSize * fYPos ) / ( terrainSize.x * 2.0f );
	}
	else
	{
		fXLocalPos = ( nSize * fXPos ) / ( terrainSize.y * 2.0f );
		fYLocalPos = ( nSize * fYPos ) / ( terrainSize.y * 2.0f );
	}
	*pfXMiniMapPos =  fXLocalPos + fYLocalPos + zeroPoint.x;
	if ( isLeftTop )
	{
		*pfYMiniMapPos =  ( fXLocalPos - fYLocalPos ) / 2.0f  + zeroPoint.y;
	}
	else
	{
		*pfYMiniMapPos =  ( fYLocalPos - fXLocalPos ) / 2.0f  + zeroPoint.y;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::TextureMiniMapToPoint( float fXMiniMapPos, float fYMiniMapPos, float *pfXPos, float *pfYPos , bool isLeftTop )
{
	NI_ASSERT_SLOW_T( ( pfXPos ) && ( pfYPos ),
										NStr::Format( "Wrong parameters: (%x, %x)", pfXPos, pfYPos ) );
	NI_ASSERT_SLOW_T( ( terrainSize.x > 0 ) && ( terrainSize.y > 0 ),
										NStr::Format( "Wrong terrainSize: (%d, %d)", terrainSize.x, terrainSize.y ) );
	CTPoint<float> zeroPoint;
	GetZeroPoint( &( zeroPoint.x ), &( zeroPoint.y ), isLeftTop );
	float fXLocalPos = fXMiniMapPos - zeroPoint.x;
	float fYLocalPos = fYMiniMapPos - zeroPoint.y;
	float fAx = 1.0f;
	float fAy = 1.0f;
	
	if ( terrainSize.x >= terrainSize.y )
	{
		fAx = ( terrainSize.x * 2.0f ) / ( nSize * 1.0f );
		fAy = ( terrainSize.x * 2.0f ) / ( nSize * 1.0f );
	}
	else
	{
		fAx = ( terrainSize.y * 2.0f ) / ( nSize * 1.0f );
		fAy = ( terrainSize.y * 2.0f ) / ( nSize * 1.0f );
	}
	if ( isLeftTop )
	{
		*pfXPos =  ( fXLocalPos / 2.0f + fYLocalPos ) * fAx;
		*pfYPos =  ( fXLocalPos / 2.0f - fYLocalPos ) * fAy;
	}
	else
	{
		*pfXPos =  ( fXLocalPos / 2.0f - fYLocalPos ) * fAx;
		*pfYPos =  ( fXLocalPos / 2.0f + fYLocalPos ) * fAy;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::GetVerticalClippedScreenEdge( const CTPoint<float> &v0, const CTPoint<float> &v1, std::vector<CTPoint<float> > *pvPoints )
{
	NI_ASSERT_SLOW_T( pvPoints != 0,
										NStr::Format( "Wrong parameter: (%x)", pvPoints ) );

	float fXMax = terrainSize.x;
	float fYMax = terrainSize.y;
	
	//������������ ������� 0 � ����� 0 - 1
	if ( InMiniMap( v0.x, v0.y ) )
	{
		if ( InMiniMap ( v1.x, v1.y ) )
		{
			//��������� ����� 0 - 1 ��� ���������
			//��� ����� ������
			pvPoints->push_back( CTPoint<float>( v0.x, v0.y ) );
			pvPoints->push_back( CTPoint<float>( v1.x, v1.y ) );
		}
		else
		{
			float fXbyY0_0_1 = GetXByY( 0.0f, v0.x, v0.y, v1.x, v1.y );
			if ( fXbyY0_0_1 <= fXMax )
			{
				//��������� ����� 0 - 1 c ���������� �� ����� (0,0)-(fXMax, 0)
				//����� 0 ������, ����� 1 ������� �� ������� ����� (0,0)
				pvPoints->push_back( CTPoint<float>( v0.x, v0.y ) );
				pvPoints->push_back( CTPoint<float>( fXbyY0_0_1, 0 ) );
			}
			else
			{
				float fYbyXmax_0_1 = GetYByX( fXMax, v0.x, v0.y, v1.x, v1.y );
				//��������� ����� 0 - 1 c ���������� �� ����� (fXMax,0)-(fXMax,fYMax)
				//����� 0 ������, ����� 1 ������� �� ������� ����� (fXMax,fYMax)
				pvPoints->push_back( CTPoint<float>( v0.x, v0.y ) );
				pvPoints->push_back( CTPoint<float>( fXMax, fYbyXmax_0_1 ) );
			}
		}
	}
	else
	{
		float fYbyX0_0_1 = GetYByX( 0.0f, v0.x, v0.y, v1.x, v1.y );
		if ( InMiniMap ( v1.x, v1.y ) )
		{
			if ( fYbyX0_0_1 <= fYMax )
			{
				//��������� ����� 0 - 1 c ���������� �� ����� (0,0)-(0, fYMax)
				//����� 0 �������, ����� 1 ������ �� ������� ����� (0,0)
				pvPoints->push_back( CTPoint<float>( 0, fYbyX0_0_1 ) );
				pvPoints->push_back( CTPoint<float>( v1.x, v1.y ) );
			}
			else
			{
				float fXbyYmax_0_1 = GetXByY( fYMax, v0.x, v0.y, v1.x, v1.y );
				//��������� ����� 0 - 1 c ���������� �� ����� (0,fYMax)-(fXMax, fYMax)
				//����� 0 �������, ����� 1 ������ �� ������� ����� (fXMax, fYMax)
				pvPoints->push_back( CTPoint<float>( fXbyYmax_0_1, fYMax ) );
				pvPoints->push_back( CTPoint<float>( v1.x, v1.y ) );
			}
		}
		else
		{
			if ( ( fYbyX0_0_1 >= 0 ) && ( fYbyX0_0_1 <= fYMax ) )
			{
				if ( ( fYbyX0_0_1 >= v1.y )  && ( fYbyX0_0_1 <= v0.y ) )
				{
					float fXbyY0_0_1 = GetXByY( 0.0f, v0.x, v0.y, v1.x, v1.y );
					//��������� ����� 0 - 1 c ���������� �� ������ (0,0)-(0, fYMax) � (0,0)-(fXMax, 0)
					//����� 0 �������, ����� 1 ������� �� ������� ����� (0, 0)
					pvPoints->push_back( CTPoint<float>( 0, fYbyX0_0_1 ) );
					pvPoints->push_back( CTPoint<float>( fXbyY0_0_1, 0 ) );
				}
				else
				{
					//��� �����
				}
			}
			else
			{
				float fYbyXmax_0_1 = GetYByX( fXMax, v0.x, v0.y, v1.x, v1.y );
				if ( ( fYbyXmax_0_1 >= 0 ) && ( fYbyXmax_0_1 <= fYMax ) )
				{
					if ( ( fYbyXmax_0_1 >= v1.y ) && ( fYbyXmax_0_1 <= v0.y ) )
					{
						float fXbyYmax_0_1 = GetXByY( fYMax, v0.x, v0.y, v1.x, v1.y );
						//��������� ����� 0 - 1 c ���������� �� ������ (0,fYmax)-(fXMax, fYMax) � (fXmax,0)-(fXMax, fYMax)
						//����� 0 �������, ����� 1 ������� �� ������� ����� (fXMax, fYMax)
						pvPoints->push_back( CTPoint<float>( fXbyYmax_0_1, fYMax ) );
						pvPoints->push_back( CTPoint<float>( fXMax, fYbyXmax_0_1 ) );
					}
					else
					{
						//��� �����
					}
				}
				else
				{
					//��� �����
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::GetHorizontalClippedScreenEdge( const CTPoint<float> &v1, const CTPoint<float> &v2, std::vector<CTPoint<float> > *pvPoints )
{
	NI_ASSERT_SLOW_T( pvPoints != 0,
										NStr::Format( "Wrong parameter: (%x)", pvPoints ) );

	float fXMax = terrainSize.x;
	float fYMax = terrainSize.y;

	if ( InMiniMap( v1.x, v1.y ) )
	{
		if ( InMiniMap ( v2.x, v2.y ) )
		{
			//��������� ����� 1 - 2 ��� ���������
			//��� ����� ������
			pvPoints->push_back( CTPoint<float>( v1.x, v1.y ) );
			pvPoints->push_back( CTPoint<float>( v2.x, v2.y ) );
		}
		else
		{
			float fYbyXmax_1_2 = GetYByX( fXMax, v1.x, v1.y, v2.x, v2.y );
			if ( fYbyXmax_1_2 <= fYMax )
			{
				//��������� ����� 1 - 2 c ���������� �� ����� (fXMax,0)-(fXMax, fYmax)
				//����� 1 ������, ����� 2 ������� �� ������� ����� (fXMax,0)
				pvPoints->push_back( CTPoint<float>( v1.x, v1.y ) );
				pvPoints->push_back( CTPoint<float>( fXMax, fYbyXmax_1_2 ) );
			}
			else
			{
				float fXbyYmax_1_2 = GetXByY( fYMax, v1.x, v1.y, v2.x, v2.y );
				//��������� ����� 1 - 2 c ���������� �� ����� (0,fYMax)-(fXMax,fYMax)
				//����� 1 ������, ����� 2 ������� �� ������� ����� (0,fYMax)
				pvPoints->push_back( CTPoint<float>( v1.x, v1.y ) );
				pvPoints->push_back( CTPoint<float>( fXbyYmax_1_2, fYMax ) );
			}
		}
	}
	else
	{
		float fXbyY0_1_2 = GetXByY( 0.0f, v1.x, v1.y, v2.x, v2.y );
		if ( InMiniMap ( v2.x, v2.y ) )
		{
			if ( fXbyY0_1_2 >= 0 )
			{
				//��������� ����� 1 - 2 c ���������� �� ����� (0,0)-(0, fYMax)
				//����� 1 �������, ����� 2 ������ �� ������� ����� (fXMax,0)
				pvPoints->push_back( CTPoint<float>( fXbyY0_1_2 , 0.0f ) );
				pvPoints->push_back( CTPoint<float>( v2.x, v2.y ) );
			}
			else
			{
				float fYbyX0_1_2 = GetYByX( 0.0f, v1.x, v1.y, v2.x, v2.y );
				//��������� ����� 1 - 2 c ���������� �� ����� (0,0-(0, fYMax)
				//����� 1 �������, ����� 2 ������ �� ������� ����� (0, fYMax)
				pvPoints->push_back( CTPoint<float>( 0.0f, fYbyX0_1_2 ) );
				pvPoints->push_back( CTPoint<float>( v2.x, v2.y ) );
			}
		}
		else
		{
			if ( ( fXbyY0_1_2 >= 0 ) && ( fXbyY0_1_2 <= fXMax ) )
			{
				if ( ( fXbyY0_1_2 >= v1.x )  && ( fXbyY0_1_2 <= v2.x ) )
				{
					float fYbyXmax_1_2 = GetYByX( fXMax, v1.x, v1.y, v2.x, v2.y );
					//��������� ����� 1 - 2 c ���������� �� ������ (0,0)-(fXMax, 0) � (fXMax,0)-(fXMax, fYMax)
					//����� 1 �������, ����� 2 ������� �� ������� ����� (fxMax, 0)
					pvPoints->push_back( CTPoint<float>( fXbyY0_1_2, 0 ) );
					pvPoints->push_back( CTPoint<float>( fXMax, fYbyXmax_1_2 ) );
				}
				else
				{
					//��� �����
				}
			}
			else
			{
				float fXbyYmax_1_2 = GetXByY( fYMax, v1.x, v1.y, v2.x, v2.y );
				if ( ( fXbyYmax_1_2 >= 0 ) && ( fXbyYmax_1_2 <= fXMax ) )
				{
					if ( ( fXbyYmax_1_2 >= v1.x ) && ( fXbyYmax_1_2 <= v2.x ) )
					{
						float fYbyX0_1_2 = GetYByX( 0.0f, v1.x, v1.y, v2.x, v2.y );
						//��������� ����� 0 - 1 c ���������� �� ������ (0,fYmax)-(fXMax, fYMax) � (fXmax,0)-(fXMax, fYMax)
						//����� 0 �������, ����� 1 ������� �� ������� ����� (fXMax, fYMax)
						pvPoints->push_back( CTPoint<float>( 0, fYbyX0_1_2 ) );
						pvPoints->push_back( CTPoint<float>( fXbyYmax_1_2, fYMax ) );
					}
					else
					{
						//��� �����
					}
				}
				else
				{
					//��� �����
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::GetClippedScreenFrame( std::vector<CTPoint<float> > *pvPoints, IGFX *_pGFX )
{
	NI_ASSERT_SLOW_T( pvPoints != 0,
										NStr::Format( "Wrong parameter: (%x)", pvPoints ) );

	//�������� ������� ���������� ����� ������
	CTRect<float> screenRect = _pGFX->GetScreenRect();
	
	CVec3 v0;	//left top
	CVec3 v1;	//left bottom
	CVec3 v2;	//right bottom
	CVec3 v3;	//right top

	IScene *_pScene = GetSingleton<IScene>();
	
	_pScene->GetPos3( &v0, CVec2( screenRect.left, screenRect.top ), true );
	_pScene->GetPos3( &v1, CVec2( screenRect.left, screenRect.bottom ), true );
	_pScene->GetPos3( &v2, CVec2( screenRect.right, screenRect.bottom ), true );
	_pScene->GetPos3( &v3, CVec2( screenRect.right, screenRect.top ), true );

	v0 /= fWorldCellSize;
	v1 /= fWorldCellSize;
	v2 /= fWorldCellSize;
	v3 /= fWorldCellSize;

	GetVerticalClippedScreenEdge( CTPoint<float>( v0.x, v0.y ), CTPoint<float>( v1.x, v1.y ), pvPoints );
	GetHorizontalClippedScreenEdge( CTPoint<float>( v1.x, v1.y ), CTPoint<float>( v2.x, v2.y ), pvPoints );
	GetVerticalClippedScreenEdge( CTPoint<float>( v3.x, v3.y ), CTPoint<float>( v2.x, v2.y ), pvPoints );
	GetHorizontalClippedScreenEdge( CTPoint<float>( v0.x, v0.y ), CTPoint<float>( v3.x, v3.y ), pvPoints );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::SetTerrainSize( int nXTerrainSize, int nYTerrainSize, int _nPlayersCount )
{
	terrainSize.x = nXTerrainSize;
	terrainSize.y = nYTerrainSize;
	nPlayersCount = _nPlayersCount;

	CreateMiniMapTextures();
	markersTypes.clear();
	LoadDataResource( MARKERS_TYPES_FILE_NAME, "", false, 0, MARKERS_TYPES_NAME, markersTypes );
	std::string szTextureFileName;
	LoadDataResource( MARKERS_TYPES_FILE_NAME, "", false, 0, MARKERS_TEXTURE_NAME, szTextureFileName );
	pMarkerTexture = GetSingleton<ITextureManager>()->GetTexture( szTextureFileName.c_str() );

	for ( int nPartyIndex = 0; nPartyIndex < nPlayersCount; ++nPartyIndex )
	{
		const DWORD dwColor = static_cast<DWORD>( GetGlobalVar(NStr::Format( "Scene.PlayerColors.Player%d", nPartyIndex ), static_cast<int>( 0x00000000 ) ) );
		const DWORD a = 0xF000 & ( ( 0xF & ( ( dwColor & 0xFF000000 ) >> 28 ) ) << 12 );
		const DWORD b = 0xF00 & ( ( 0xF & ( ( dwColor & 0xFF0000 ) >> 20 ) ) << 8 );
		const DWORD g = 0xF0 & ( ( 0xF & ( ( dwColor & 0xFF00 ) >> 12 ) ) << 4 );
		const DWORD r = 0xF & ( ( dwColor & 0xFF ) >> 4 );
		const WORD color = a + b + g + r;
		pPartyColors[nPartyIndex] = color;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMiniMap::AddWarFogData( const BYTE *pVizBuffer, int nLength )
{
	if ( IsInitialized() && nLength != 0 )
	{
		NI_ASSERT_SLOW_T( pVizBuffer != 0,
											NStr::Format( "Wrong parameter: (%x)", pVizBuffer ) );

		//��������� � �������� pWarFog � ����������� ������ ������ �� AI
		if ( nFiledVISTiles < ( terrainSize.x * terrainSize.y ) )
		{
			NI_ASSERT_SLOW_T( ( nFiledVISTiles + nLength ) <= ( terrainSize.x * terrainSize.y ),
												NStr::Format( "Wrong parameter: (%d), must be under or equal: %d", nLength, ( terrainSize.x * terrainSize.y ) - nFiledVISTiles ) );

			CTextureLock<SGFXColor4444> textureLock( pWarFog , 0 );

			//��������� �������� pWarFog � ����������� ������
			for ( int nYIndex = 0; nYIndex < ( nLength / terrainSize.x ); ++nYIndex )
			{
				int nYPosition = terrainSize.y - ( nYIndex + ( nFiledVISTiles / terrainSize.x ) ) - 1;
				//int nYPosition = ( terrainSize.y - ( nYIndex + ( nFiledVISTiles / terrainSize.x ) ) ) % terrainSize.y;
				for ( int nXIndex = 0; nXIndex < terrainSize.x; ++nXIndex )
				{
					textureLock[nYPosition][nXIndex] = pWarFogValues[ pVizBuffer[( nYIndex * terrainSize.x ) + nXIndex] ]; // / SAIConsts::VIS_POWER
				}
			}
			nFiledVISTiles += nLength;
		}
		if ( nFiledVISTiles >= ( terrainSize.x * terrainSize.y ) )
		{
			//������������� ������� vis ������ � ����
			isWarFogNeedUpdate = true;
			nFiledVISTiles = 0;
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::AddUnitsData( const SMiniMapUnitInfo *pUnitsBuffer, int nUnitsCount )
{
	if( IsInitialized() )
	{
		if ( nUnitsCount )
		{
			NI_ASSERT_SLOW_T( pUnitsBuffer != 0, NStr::Format( "Wrong parameter: (%x)", pUnitsBuffer ) );
		}

		//��������� ������
		units.clear();
		CTPoint<float> miniMapPoint( 0.0f, 0.0f );
		const float fRatio = FP_SQRT_3 / ( FP_SQRT_2 * fWorldCellSize );
		for ( int nUnitIndex = 0; nUnitIndex < nUnitsCount; ++nUnitIndex )
		{
			units.push_back( pUnitsBuffer[nUnitIndex] );
			PointToTextureMiniMap( pUnitsBuffer[nUnitIndex].x - ( units[nUnitIndex].z * fRatio ), pUnitsBuffer[nUnitIndex].y + ( units[nUnitIndex].z * fRatio ), &( miniMapPoint.x ), &( miniMapPoint.y ) );
			units[nUnitIndex].x = miniMapPoint.x;
			units[nUnitIndex].y = miniMapPoint.y;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::AddFireRangeAreas( const SShootAreas *pShootAreasBuffer, int nShootAreasCount )
{
	if( IsInitialized() )
	{
		if ( nShootAreasCount > 0 )
		{
			NI_ASSERT_SLOW_T( pShootAreasBuffer != 0, NStr::Format( "Wrong parameter: (%x)", pShootAreasBuffer ) );
		}
		//��������� ����� ��������
		shootAreas.clear();
		for ( int index = 0; index < nShootAreasCount; ++index )
			shootAreas.push_back( pShootAreasBuffer[index] );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::AddCircle( const CVec2 &vCenter, const float fRadius, int nStyle, WORD wColor, const NTimer::STime &rStart, const NTimer::STime &rDuration, bool bRelative, LPARAM lParam )
{
	if( IsInitialized() )
	{
		if ( bRelative )
		{
			circles.push_back( SMiniMapCircle( CVec2( vCenter.x * terrainSize.x * fWorldCellSize, vCenter.y * terrainSize.y * fWorldCellSize ), fRadius, rStart, rDuration, nStyle, wColor, lParam ) );
		}
		else
		{
			circles.push_back( SMiniMapCircle( vCenter, fRadius, rStart, rDuration, nStyle, wColor, lParam ) );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STDCALL CUIMiniMap::AddMarker( const std::string &rszName, const CVec2 &vPos, bool _bActive, int _nID, const NTimer::STime &rStart, const NTimer::STime &rDuration, bool bRelative )
{
	if ( markersTypes.find( rszName ) == markersTypes.end() )
	{
		return ( -1 );
	}
	if ( bRelative )
	{
		markers.push_back( SMiniMapMarker( markersTypes[rszName], rszName, CVec2( vPos.x * terrainSize.x * fWorldCellSize, vPos.y * terrainSize.y * fWorldCellSize ), _bActive, _nID, rStart, rDuration ) );
	}
	else
	{
		markers.push_back( SMiniMapMarker( markersTypes[rszName], rszName, vPos, _bActive, _nID, rStart, rDuration ) );
	}
	return _nID;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STDCALL CUIMiniMap::ActivateMarker( int _nID, bool _bActive )
{
	for ( std::list<SMiniMapMarker>::iterator markerIterator = markers.begin(); markerIterator != markers.end(); ++markerIterator )
	{
		if ( markerIterator->nID == _nID )
		{
			markerIterator->bActive = _bActive;
			markerIterator->timeStart = GetSingleton<IGameTimer>()->GetAbsTime();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STDCALL CUIMiniMap::ActivateMarker( const std::string &rszName, bool _bActive )
{
	for ( std::list<SMiniMapMarker>::iterator markerIterator = markers.begin(); markerIterator != markers.end(); ++markerIterator )
	{
		if ( markerIterator->szName == rszName )
		{
			markerIterator->bActive = _bActive;
			markerIterator->timeStart = GetSingleton<IGameTimer>()->GetAbsTime();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STDCALL CUIMiniMap::RemoveMarker( int _nID )
{
	for ( std::list<SMiniMapMarker>::iterator markerIterator = markers.begin(); markerIterator != markers.end(); )
	{
		if ( markerIterator->nID == _nID )
		{
			markerIterator = markers.erase( markerIterator );
		}
		else
		{
			++markerIterator;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void STDCALL CUIMiniMap::RemoveMarker( const std::string &rszName )
{
	for ( std::list<SMiniMapMarker>::iterator markerIterator = markers.begin(); markerIterator != markers.end(); )
	{
		if ( markerIterator->szName == rszName )
		{
			markerIterator = markers.erase( markerIterator );
		}
		else
		{
			++markerIterator;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::DrawFireRanges( CTextureLock<SGFXColor4444> *pTextureLock )
{
	const int nInstantObjSizeX = pInstantObjects->GetSizeX( 0 );
	const int nInstantObjSizeY = pInstantObjects->GetSizeY( 0 );
	for ( int index = 0; index < shootAreas.size(); ++index )
	{
		for ( std::list<SShootArea>::iterator iter = shootAreas[index].areas.begin(); iter != shootAreas[index].areas.end(); ++iter )
		{
			const SShootArea &area = *iter;
			if ( area.eType != SShootArea::ESAT_LINE )
			{
				CMarkPixelFunctional markCircles( pTextureLock, area.GetMiniMapCircleColor(), CTPoint<int>( nInstantObjSizeX, nInstantObjSizeY ) );
				CMarkPixelFunctional markSectors( pTextureLock, area.GetMiniMapSectorColor(), CTPoint<int>( nInstantObjSizeX, nInstantObjSizeY ) );

				CTPoint<float> miniMapPoint( 0.0f, 0.0f );
				CTPoint<float> additionalPoint( 0.0f, 0.0f );

				PointToTextureMiniMap( area.vCenter3D.x / fWorldCellSize, area.vCenter3D.y / fWorldCellSize, &(miniMapPoint.x), &(miniMapPoint.y), false );
				PointToTextureMiniMap( ( area.vCenter3D.x + ( area.fMaxR / SQRT_2 ) ) / fWorldCellSize,
															 ( area.vCenter3D.y + ( area.fMaxR / SQRT_2 ) ) / fWorldCellSize,
															 &(additionalPoint.x), &(additionalPoint.y), false );

				BresenhamEllipse( static_cast<int>( miniMapPoint.x ), static_cast<int>( miniMapPoint.y ),
													static_cast<int>( additionalPoint.x - miniMapPoint.x + 1 ),
													markCircles );

				if ( area.wStartAngle != area.wFinishAngle )
				{
					// ������ ����
					{
						const float fAngle = fmod( float( area.wStartAngle ) / 65535.0f * FP_2PI + FP_PI2, FP_2PI );
						const float fCos = cos( fAngle );
						const float fSin = sin( fAngle );

						const float x = area.vCenter3D.x + area.fMaxR * fCos;
						const float y = area.vCenter3D.y + area.fMaxR * fSin;
						
						PointToTextureMiniMap( x / fWorldCellSize, y / fWorldCellSize, &(additionalPoint.x), &(additionalPoint.y), false );
						
						MakeLine2( static_cast<int>( miniMapPoint.x ), static_cast<int>( miniMapPoint.y ),
											 static_cast<int>( additionalPoint.x ), static_cast<int>( additionalPoint.y ),
											 markSectors );
					}
					
					// ������ ����
					{
						const float fAngle = fmod( float( area.wFinishAngle ) / 65535.0f * FP_2PI + FP_PI2, FP_2PI );
						const float fCos = cos( fAngle );
						const float fSin = sin( fAngle );

						const float x = area.vCenter3D.x + area.fMaxR * fCos;
						const float y = area.vCenter3D.y + area.fMaxR * fSin;
						
						PointToTextureMiniMap( x / fWorldCellSize, y / fWorldCellSize, &(additionalPoint.x), &(additionalPoint.y), false );
						
						MakeLine2( static_cast<int>( miniMapPoint.x ), static_cast<int>( miniMapPoint.y ),
											 static_cast<int>( additionalPoint.x ), static_cast<int>( additionalPoint.y ),
											 markSectors );
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMiniMap::Update( const NTimer::STime &currTime )
{
	if( !IsInitialized() )
	{
		return false;
	}
	IGFX* _pGFX = GetSingleton<IGFX>();
	if ( !_pGFX )
	{
		return false;
	}
	IGameTimer* _pGameTimer = GetSingleton<IGameTimer>();
	if ( !_pGameTimer )
	{
		return false;
	}
	NTimer::STime currentAbsTime = _pGameTimer->GetAbsTime();
	
	CTPoint<float> miniMapPoint( 0.0f, 0.0f );
	CTPoint<float> additionalPoint( 0.0f, 0.0f );

	//�������������� �������� pInstantObjects
	if ( pInstantObjects )
	{
		CTextureLock<SGFXColor4444> textureLock( pInstantObjects , 0 );
		//������� ��������
		{
			int nBytesCount = pInstantObjects->GetSizeX( 0 ) * sizeof( WORD );
			for ( int nYIndex = 0; nYIndex < pInstantObjects->GetSizeY( 0 ); ++nYIndex )
			{
				memset( static_cast<void*>( textureLock[nYIndex] ), 0x00, nBytesCount );
			}
		}
		//������ ������
		{

			for( int nUnitIndex = 0; nUnitIndex < units.size() ; ++nUnitIndex )
			{
				if ( units[nUnitIndex].player < nPlayersCount )
				{
					const int nXPos = units[nUnitIndex].x;
					const int nYPos = units[nUnitIndex].y;

					for ( int index = ( nXPos - nUnitCrossSize ); index <= ( nXPos + nUnitCrossSize ); ++index )
					{
						if ( ( index >= 0 ) &&
								 ( index < pInstantObjects->GetSizeX( 0 ) ) &&
								 ( nYPos >= 0 ) &&
								 ( nYPos < pInstantObjects->GetSizeY( 0 ) ) )
						{
							textureLock[nYPos][index] = pPartyColors[units[nUnitIndex].player];
						}
					}
					for ( int index = ( nYPos - nUnitCrossSize ); index <= ( nYPos + nUnitCrossSize ); ++index )
					{
						if ( ( index >= 0 ) &&
								 ( index < pInstantObjects->GetSizeY( 0 ) ) &&
								 ( nXPos >= 0 ) &&
								 ( nXPos < pInstantObjects->GetSizeX( 0 ) ) )
						{
							textureLock[index][nXPos] = pPartyColors[units[nUnitIndex].player];
						}
					}
				}
			}
		}
		
		//��������� ����� �������� ���� ��� ����
		DrawFireRanges( &textureLock );
		isInstantObjectsNeedUpdate = true;
		
		//�����
		{
			for ( std::list<SMiniMapCircle>::iterator it = circles.begin(); it != circles.end(); )
			{
				//SMiniMapCircle &rCircle = *it;
				if ( it->timeStart > currentAbsTime )
				{
					++it;
					continue;
				}
				else if ( ( it->timeStart + it->timeDuration ) < currentAbsTime )
				{
					it = circles.erase( it );
				}
				else
				{
					float fRadius = 0.0f;
					if ( it->nStyle < MMC_STYLE_LPOLYGON_DIVERGENT )
					{
						switch( it->nStyle )
						{
							case MMC_STYLE_DIVERGENT:
							{
								fRadius = it->fRadius * fabs( ( currentAbsTime - it->timeStart ) / ( it->timeDuration * 1.0f ) );
								break;
							}
							case MMC_STYLE_CONVERGENT:
							{
								fRadius = it->fRadius * fabs( 1.0f - ( ( currentAbsTime - it->timeStart ) / ( it->timeDuration * 1.0f ) ) );
								break;
							}
							case MMC_STYLE_MIXED:
							{
								fRadius = it->fRadius * fabs( 1.0f - 2.0f * ( ( currentAbsTime - it->timeStart ) / ( it->timeDuration * 1.0f ) ) );
								break;
							}
						}
						if ( fRadius > 0.0f )
						{
							CMarkPixelFunctional markCircles( &textureLock, it->wColor, CTPoint<int>( pInstantObjects->GetSizeX( 0 ), pInstantObjects->GetSizeY( 0 ) ) );
							PointToTextureMiniMap( it->vCenter.x / fWorldCellSize,
																		 it->vCenter.y / fWorldCellSize,
																		 &( miniMapPoint.x ),
																		 &( miniMapPoint.y ),
																		 false );
							PointToTextureMiniMap( ( it->vCenter.x + fRadius ) / fWorldCellSize,
																		 ( it->vCenter.y + fRadius ) / fWorldCellSize,
																		 &( additionalPoint.x ),
																		 &( additionalPoint.y ),
																		 false );
							BresenhamEllipse( static_cast<int>( miniMapPoint.x ),
																static_cast<int>( miniMapPoint.y ),
																static_cast<int>( additionalPoint.x - miniMapPoint.x + 1 ),
																markCircles );
						}
					}
					else
					{
						switch( it->nStyle )
						{
							case MMC_STYLE_LPOLYGON_DIVERGENT:
							case MMC_STYLE_RPOLYGON_DIVERGENT:
							{
								fRadius = it->fRadius * fabs( ( currentAbsTime - it->timeStart ) / ( it->timeDuration * 1.0f ) );
								break;
							}
							case MMC_STYLE_LPOLYGON_CONVERGENT:
							case MMC_STYLE_RPOLYGON_CONVERGENT:
							{
								fRadius = it->fRadius * fabs( 1.0f - ( ( currentAbsTime - it->timeStart ) / ( it->timeDuration * 1.0f ) ) );
								break;
							}
							case MMC_STYLE_LPOLYGON_MIXED:
							case MMC_STYLE_RPOLYGON_MIXED:
							{
								fRadius = it->fRadius * fabs( 1.0f - 2.0f * ( ( currentAbsTime - it->timeStart ) / ( it->timeDuration * 1.0f ) ) );
								break;
							}
						}
						if ( fRadius > 0.0f )
						{
							CMarkPixelFunctional markCircles( &textureLock, it->wColor, CTPoint<int>( pInstantObjects->GetSizeX( 0 ), pInstantObjects->GetSizeY( 0 ) ) );
							CTPoint<float> center;
							PointToTextureMiniMap( it->vCenter.x / fWorldCellSize,
																		 it->vCenter.y / fWorldCellSize,
																		 &( center.x ),
																		 &( center.y ),
																		 false );

							WORD wParts = LOWORD( it->lParam );
							WORD wPeriod = HIWORD( it->lParam );

							if ( wParts > 1 && wParts < 17 )
							{
								std::vector<CTPoint<int> > points;
								points.reserve( wParts );

								float fAngle = 0.0f;
								if ( wPeriod > 0 )
								{
									const float fDelimiter = wPeriod;
									if ( it->nStyle < MMC_STYLE_RPOLYGON_DIVERGENT )
									{
										fAngle = fDelimiter * FP_2PI * fabs( ( currentAbsTime - it->timeStart ) / ( it->timeDuration * 1.0f ) );
									}
									else
									{
										fAngle = ( -1.0f ) * fDelimiter * FP_2PI * fabs( ( currentAbsTime - it->timeStart ) / ( it->timeDuration * 1.0f ) );
									}
								}
								if ( wParts == 4 )
								{
									const float fRCos = fRadius * cos( fAngle );
									const float fRSin = fRadius * sin( fAngle );
									points.push_back( CTPoint<int>( center.x + fRCos, center.y + fRSin ) );
									points.push_back( CTPoint<int>( center.x - fRSin, center.y + fRCos ) );
									points.push_back( CTPoint<int>( center.x - fRCos, center.y - fRSin ) );
									points.push_back( CTPoint<int>( center.x + fRSin, center.y - fRCos ) );
								}
								else
								{
									for ( int nPart = 0; nPart < wParts; ++nPart )
									{
										float fLocalAngle = fAngle + FP_2PI * nPart / wParts;
										const float fRCos = fRadius * cos( fLocalAngle );
										const float fRSin = fRadius * sin( fLocalAngle );
										points.push_back( CTPoint<int>( center.x + fRCos, center.y + fRSin ) );
									}
								}
								
								for ( int nPointIndex = 0; nPointIndex < points.size(); ++nPointIndex )
								{
									MakeLine2( points[nPointIndex].x, points[nPointIndex].y,
														 points[( nPointIndex + 1 ) % points.size()].x, points[( nPointIndex + 1 ) % points.size()].y,
														 markCircles );
								}
							}
						}
					}
					++it;
				}
			}
		}
	}
		
	//��������� �������� pWarFogTexture � ����� ������
	if ( pWarFog && pWarFogTexture && isWarFogNeedUpdate )
	{
		pWarFog->AddDirtyRect( &( static_cast<RECT>( CTRect<int>( 0, 0, pWarFog->GetSizeX( 0 ), pWarFog->GetSizeY( 0 ) ) ) ) );
		_pGFX->UpdateTexture( pWarFog, pWarFogTexture, false );
		isWarFogNeedUpdate = false;
	}

	//���������  �������� pInstantObjects � �����������
	if ( pInstantObjects && pInstantObjectsTexture && isInstantObjectsNeedUpdate )
	{
		pInstantObjects->AddDirtyRect( &( static_cast<RECT>( CTRect<int>( 0, 0, pInstantObjects->GetSizeX( 0 ), pInstantObjects->GetSizeY( 0 ) ) ) ) );
		_pGFX->UpdateTexture( pInstantObjects, pInstantObjectsTexture, false );
		isInstantObjectsNeedUpdate = false;
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::Visit( interface ISceneVisitor *pVisitor )
{
	if ( IsInitialized() )
		pVisitor->VisitUICustom( dynamic_cast<IUIElement*>(this) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMiniMap::Draw( IGFX *_pGFX )
{
	if ( IsInitialized() )
	{
		IGameTimer* _pGameTimer = GetSingleton<IGameTimer>();
		if ( !_pGameTimer )
		{
			return;
		}
		const NTimer::STime currentAbsTime = _pGameTimer->GetAbsTime();
		float zCoord = 0.0f;
		// const CTRect<float> &wndRect = GetScreenRect();
		nSize = wndRect.right - wndRect.left;

		// ������������� ���������� ���������, ������������������ ������ ��������� � �������������
		std::vector<CTPoint<float> > vPoints;
		vPoints.resize(4);
		vPoints[0] = CTPoint<float>( 0.0f, terrainSize.y );
		vPoints[1] = CTPoint<float>( 0.0f, 0.0f );
		vPoints[2] = CTPoint<float>( terrainSize.x, 0.0f );
		vPoints[3] = CTPoint<float>( terrainSize.x, terrainSize.y );

		CTPoint<float> miniMapPoint;

		_pGFX->SetShadingEffect( 21 );
		
		// ������ �������� pBackgroundTexture � pWarFogTexture
		IGFXTexture *pTextures[3] = { pBackgroundTexture, pWarFogTexture, pInstantObjectsTexture };
		for ( int textureIndex = 0; textureIndex < 3; ++textureIndex )
		{
			// �������� ������� ��������� � ������������������� ������ ��������� � �������������
			CTempBufferLock<SGFXLVertex> vertices = _pGFX->GetTempVertices( vPoints.size(), SGFXLVertex::format, GFXPT_TRIANGLELIST );
			CTempBufferLock<WORD> indices = _pGFX->GetTempIndices( 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );

			// ��������� ������ ���������
			for ( int index = 0; index < vPoints.size(); ++index )
			{
				PointToTextureMiniMap( vPoints[index].x, vPoints[index].y, &( miniMapPoint.x ), &( miniMapPoint.y ) );

				float tU = 0.0f;
				float tV = 0.0f;
				if ( pTextures[textureIndex] )
				{
					switch ( textureIndex )
					{
						case 0:
						{
							tU = ( ( vPoints[index].x > 1.0f ) ? 1.0f : 0.0f );
							tV = ( ( ( terrainSize.y - vPoints[index].y ) > 1.0f ) ? 1.0f : 0.0f );
							break;
						}
						case 1:
						{
							tU = ( vPoints[index].x / pTextures[textureIndex]->GetSizeX( 0 ) );
							tV = ( ( terrainSize.y - vPoints[index].y ) / pTextures[textureIndex]->GetSizeY( 0 ) );
							break;
						}
						default:
						{
							tU = ( miniMapPoint.x / pTextures[textureIndex]->GetSizeX( 0 ) ) + ( 0.5f / pTextures[textureIndex]->GetSizeX( 0 ) );
							tV = ( miniMapPoint.y / pTextures[textureIndex]->GetSizeY( 0 ) ) + ( 0.5f / pTextures[textureIndex]->GetSizeY( 0 ) );
							break;
						}
					}
				}
				vertices[index].Setup( wndRect.left + miniMapPoint.x,
															 wndRect.top + miniMapPoint.y,
															 zCoord, 1.0f, 0xFFffFFff, 0xFF000000, tU, tV );
			}
			//��������� ������ ������������������� ������ ��������� � �������������
			indices[0] = 0;
			indices[1] = 1;
			indices[2] = 2;
			indices[3] = 0;
			indices[4] = 2;
			indices[5] = 3;

			//������ ��� ����������
			_pGFX->SetTexture( 0, pTextures[textureIndex] );
			_pGFX->DrawTemp();
		}		
		
		_pGFX->SetShadingEffect( 18 );

		int nActiveMarkersCount = 0;
		for ( std::list<SMiniMapMarker>::iterator markerIterator = markers.begin(); markerIterator != markers.end(); )
		{
			if ( markerIterator->timeStart <= currentAbsTime )
			{
				if ( ( markerIterator->timeDuration > 0 ) && ( ( markerIterator->timeStart + markerIterator->timeDuration ) < currentAbsTime ) )
				{
					 markerIterator = markers.erase( markerIterator );
				}
				else
				{
					if ( markerIterator->bActive )
					{
						++nActiveMarkersCount;
					}
					++markerIterator;
				}
			}
			else
			{
				++markerIterator;
			}
		}
		
		if ( nActiveMarkersCount > 0 )
		{
			//�������� ������� ��������� � ������������������� ������ ��������� � �������������
			CTempBufferLock<SGFXLVertex> vertices = _pGFX->GetTempVertices( 4 * nActiveMarkersCount, SGFXLVertex::format, GFXPT_TRIANGLELIST );
			CTempBufferLock<WORD> indices = _pGFX->GetTempIndices( 6 * nActiveMarkersCount, GFXIF_INDEX16, GFXPT_TRIANGLELIST );

			int nActiveMarkerIndex = 0;
			for ( std::list<SMiniMapMarker>::iterator markerIterator = markers.begin(); markerIterator != markers.end(); ++markerIterator )
			{
				if ( ( markerIterator->timeStart <= currentAbsTime ) && ( markerIterator->bActive ) )
				{
					PointToTextureMiniMap( markerIterator->vPos.x / fWorldCellSize, markerIterator->vPos.y / fWorldCellSize, &( miniMapPoint.x ), &( miniMapPoint.y ) );
					//	1			2
					//  -------
					//  |    /|
					//  |  /  |
					//  |/    |
					//  -------
					//	0			3
					
					CTPoint<int> minXYPoint( wndRect.left + miniMapPoint.x + markerIterator->screenRect.minx, wndRect.top + miniMapPoint.y + markerIterator->screenRect.miny );
					CTPoint<int> maxXYPoint( wndRect.left + miniMapPoint.x + markerIterator->screenRect.maxx, wndRect.top + miniMapPoint.y + markerIterator->screenRect.maxy );

					SMiniMapMarker r = *markerIterator;
					//0
					vertices[nActiveMarkerIndex * 4 + 0].Setup( minXYPoint.x,
																											maxXYPoint.y,
																											zCoord, 1.0f, 0xFFffFFff, 0xFF000000,
																											markerIterator->textureRect.minx,
																											markerIterator->textureRect.maxy );
					//1
					vertices[nActiveMarkerIndex * 4 + 1].Setup( minXYPoint.x,
																											minXYPoint.y,
																											zCoord, 1.0f, 0xFFffFFff, 0xFF000000,
																											markerIterator->textureRect.minx,
																											markerIterator->textureRect.miny );
					//2
					vertices[nActiveMarkerIndex * 4 + 2].Setup( maxXYPoint.x,
																											minXYPoint.y,
																											zCoord, 1.0f, 0xFFffFFff, 0xFF000000,
																											markerIterator->textureRect.maxx,
																											markerIterator->textureRect.miny );
					//3
					vertices[nActiveMarkerIndex * 4 + 3].Setup( maxXYPoint.x,
																											maxXYPoint.y,
																											zCoord, 1.0f, 0xFFffFFff, 0xFF000000,
																											markerIterator->textureRect.maxx,
																											markerIterator->textureRect.maxy );

					//��������� ������ ������������������� ������ ��������� � �������������
					indices[nActiveMarkerIndex * 6 + 0] = nActiveMarkerIndex * 4 + 0;
					indices[nActiveMarkerIndex * 6 + 1] = nActiveMarkerIndex * 4 + 2;
					indices[nActiveMarkerIndex * 6 + 2] = nActiveMarkerIndex * 4 + 1;
					indices[nActiveMarkerIndex * 6 + 3] = nActiveMarkerIndex * 4 + 0;
					indices[nActiveMarkerIndex * 6 + 4] = nActiveMarkerIndex * 4 + 3;
					indices[nActiveMarkerIndex * 6 + 5] = nActiveMarkerIndex * 4 + 2;
					++nActiveMarkerIndex;
				}
			}
			_pGFX->SetTexture( 0, pMarkerTexture );
			_pGFX->DrawTemp();
		}
		/**/
		_pGFX->SetTexture( 0, 0 );

		// ������ �����
		// ��������� ���������� �� ������ ����� ������
		vPoints.clear();
		GetClippedScreenFrame( &vPoints, _pGFX );

		if ( vPoints.size() > 0 )
		{
			// �������� ������� ���������
			CTempBufferLock<SGFXLineVertex> vertices = _pGFX->GetTempVertices( vPoints.size() * 2, SGFXLineVertex::format, GFXPT_LINELIST );

			// ��������� �������� ������� ������ ����� ������
			for ( int index = 0; index < vPoints.size(); ++index )
			{
				// �������� �������� ���������� ������� ����� ������
				PointToTextureMiniMap( vPoints[index].x, vPoints[index].y, &( miniMapPoint.x ), &( miniMapPoint.y ) );
				vertices[static_cast<int>( index + vPoints.size() )].Setup( wndRect.left + miniMapPoint.x , wndRect.top + miniMapPoint.y, zCoord, dwScreenFrameColor );
				vertices[index].Setup( wndRect.left + miniMapPoint.x + 1, wndRect.top + miniMapPoint.y + 1, zCoord, dwScreenFrameColorShadow );
			}			
			// ������ ��� ����������
			_pGFX->DrawTemp();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMiniMap::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	if ( IsInitialized() )
	{
		//wndRect - �� �������� ������
		nSize = wndRect.right - wndRect.left;
		
		ICursor *_pCursor = GetSingleton<ICursor>();
		_pCursor->SetBounds( wndRect.left, wndRect.top, wndRect.right, wndRect.bottom );

		//�������� �������� ���������� ����
		CTPoint<float> miniMapMousePos;
		miniMapMousePos.x = vPos.x - wndRect.left;
		miniMapMousePos.y = vPos.y - wndRect.top;

		CTPoint<float> mapMousePos;
		TextureMiniMapToPoint( miniMapMousePos.x, miniMapMousePos.y, &( mapMousePos.x ), &( mapMousePos.y ) );
		// move camera anchor (if in 'controllable' mode)
		if ( GetSingleton<IGameTimer>()->GetPauseReason() < PAUSE_TYPE_NO_CONTROL ) 
		{
			ICamera *_pCamera = GetSingleton<ICamera>();
			CVec3 cameraVec3 = _pCamera->GetAnchor();
			cameraVec3.x = mapMousePos.x * fWorldCellSize;
			cameraVec3.y = mapMousePos.y * fWorldCellSize;
			_pCamera->SetAnchor( cameraVec3 );
		}
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMiniMap::OnLButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	if ( IsInitialized() )
	{
		IGFX* _pGFX = GetSingleton<IGFX>();
		ICursor *_pCursor = GetSingleton<ICursor>();

		CTRect<float> screenRect = _pGFX->GetScreenRect();
		_pCursor->SetBounds( 0, 0, screenRect.Width(), screenRect.Height() );
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMiniMap::OnMouseMove( const CVec2 &vPos, EMouseState mouseState )
{
	if ( IsInitialized() )
	{
		if ( mouseState & E_LBUTTONDOWN )
		{
			return OnLButtonDown( vPos, mouseState );
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMiniMap::OnRButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	if ( IsInitialized() )
	{
		//�������� �������� ���������� ����
		CTPoint<float> miniMapMousePos;
		miniMapMousePos.x = vPos.x - wndRect.left;
		miniMapMousePos.y = vPos.y - wndRect.top;

		CTPoint<float> mapMousePos;
		TextureMiniMapToPoint( miniMapMousePos.x, miniMapMousePos.y, &( mapMousePos.x ), &( mapMousePos.y ) );
		if ( InMiniMap( mapMousePos.x, mapMousePos.y ) )
		{
			//��� ���������� ���������� �� ��������� �������
			//(��� ������� ���� ��������� ��� � �������� �������)
			//CVec2( DWORD(msg.nParam) & 0x00007fff, (DWORD(msg.nParam) & 0x7fff0000) >> 15 );
			//��� ����������� ����, ��� � ��������� ���� ����������, ������� ��� ������ ���� �������
			//(msg.nParam |= 0x80000000) msg.nEventID = CMD_END_ACTION2;
			//���������� ��� GetSingleton<IInput>()->AddMessage( SGameMessage(ID, PARAM) );
			//���������� ������ ���� � �������� ����������� �� ��������� (�.�. (0, 0) => � ����� ������� ����,
			//Y => ����, X => ������)
			DWORD dwParam = 0x80000000 + 
											( ( DWORD )( mapMousePos.x * fWorldCellSize ) & 0x00007fff ) + 
											( ( ( ( DWORD )( mapMousePos.y * fWorldCellSize ) & 0x00007fff ) << 15 ) & 0x7fff0000 );
			GetSingleton<IInput>()->AddMessage( SGameMessage( CMD_END_ACTION2, dwParam ) );
		}
	}
	return CSimpleWindow::OnRButtonUp( vPos, mouseState );;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMiniMap::IsInside( const CVec2 &vPos )
{
	if ( IsInitialized() )
	{
		//�������� �������� ���������� ����
		CTPoint<float> miniMapMousePos;
		miniMapMousePos.x = vPos.x - wndRect.left;
		miniMapMousePos.y = vPos.y - wndRect.top;

		CTPoint<float> mapMousePos;
		TextureMiniMapToPoint( miniMapMousePos.x, miniMapMousePos.y, &( mapMousePos.x ), &( mapMousePos.y ) );
		return ( InMiniMap( mapMousePos.x, mapMousePos.y ) );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
