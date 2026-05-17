#if !defined(__UIMiniMap__)
#define __UIMiniMap__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\GFX\GFX.h"
#include "..\GFX\GFXHelper.h"
#include "UIBasic.h"
#include "..\AILogic\AIConsts.h"
#include "..\AILogic\AITypes.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STextureMarker
{
	CTRect<float> textureRect; //0.0f ... 1.0f
	CTRect<int> screenRect;	//� ������ HotSpot
	CTPoint<int> size;

	//constructors
	STextureMarker()
		: textureRect( 0.0f, 0.0f, 0.0f, 0.0f ), screenRect( 0, 0, 0, 0 ), size( 0, 0 ) {}
	STextureMarker( const CTRect<float> &rTextureRect, const CTRect<int> &rScreenRect, const CTPoint<int> &rSize )
		: textureRect( rTextureRect ), screenRect( rScreenRect ), size( rSize ) {}
	STextureMarker( const STextureMarker &rTextureMarker )
		: textureRect( rTextureMarker.textureRect ), screenRect( rTextureMarker.screenRect ), size( rTextureMarker.size ) {}
	STextureMarker& operator=( const STextureMarker &rTextureMarker )
	{
		if( &rTextureMarker != this )
		{
			textureRect = rTextureMarker.textureRect;
			screenRect = rTextureMarker.screenRect;
			size = rTextureMarker.size;
		}
		return *this;
	}

	// serializing...
	virtual int STDCALL operator&( IDataTree &ss );
	virtual int STDCALL operator&( IStructureSaver &ss );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMiniMapMarker : public STextureMarker
{
	std::string szName;	//��� �������� ����� � ����� �������
	CVec2 vPos;					////0.0f ... 1.0f
	bool bActive;				//������� �� ������ ����� ������
	int nID;						//ID �������
	NTimer::STime timeStart;		//����� �����������
	NTimer::STime timeDuration; //����������������� �����������
	//constructors
	SMiniMapMarker()
		: vPos( VNULL2 ), bActive( false ), nID( -1 ), timeStart( 0 ), timeDuration( 0 ) {}
	SMiniMapMarker( const STextureMarker &rTextureMarker, const std::string &rszName, const CVec2 &vPos, bool _bActive, int _nID, const NTimer::STime &rTimeStart, const NTimer::STime &rTimeDuration )
		: STextureMarker( rTextureMarker ), szName( rszName ), vPos( vPos ), bActive( _bActive ), nID( _nID ), timeStart( rTimeStart ), timeDuration( rTimeDuration ) {}
	SMiniMapMarker( const SMiniMapMarker &rMiniMapMarker )
		: STextureMarker( rMiniMapMarker.textureRect, rMiniMapMarker.screenRect, rMiniMapMarker.size ), szName( rMiniMapMarker.szName ), vPos( rMiniMapMarker.vPos ), bActive( rMiniMapMarker.bActive ), nID( rMiniMapMarker.nID ), timeStart( rMiniMapMarker.timeStart ), timeDuration( rMiniMapMarker.timeDuration ) {}
	SMiniMapMarker& operator=( const SMiniMapMarker &rMiniMapMarker )
	{
		if( &rMiniMapMarker != this )
		{
			textureRect = rMiniMapMarker.textureRect;
			screenRect = rMiniMapMarker.screenRect;
			szName = rMiniMapMarker.szName;
			vPos = rMiniMapMarker.vPos;
			bActive = rMiniMapMarker.bActive;
			nID = rMiniMapMarker.nID;
			timeStart = rMiniMapMarker.timeStart;
			timeDuration = rMiniMapMarker.timeDuration;
		}
		return *this;
	}
	
	virtual int STDCALL operator&( IStructureSaver &ss );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SMiniMapCircle
{
	CVec2 vCenter;
	float fRadius;
	NTimer::STime timeStart;
	NTimer::STime timeDuration;
	int nStyle;
	WORD wColor;
	LPARAM lParam;

	SMiniMapCircle()
		: vCenter( VNULL2 ), fRadius( 0.0f ), timeStart( 0 ), timeDuration( 0 ), nStyle( 0 ), wColor( 0xFFFF ), lParam( 0 ) {}
	SMiniMapCircle( const CVec2 &rvCenter, float _fRadius, const NTimer::STime &rTimeStart, const NTimer::STime &rTimeDuration, int _nStyle, WORD _wColor, LPARAM _lParam )
		: vCenter( rvCenter ), fRadius ( _fRadius ), timeStart( rTimeStart ), timeDuration( rTimeDuration ), nStyle( _nStyle ), wColor( _wColor ), lParam( _lParam ) {}
	SMiniMapCircle(	const SMiniMapCircle &rCircle )
		: vCenter( rCircle.vCenter ), fRadius ( rCircle.fRadius ), timeStart( rCircle.timeStart ), timeDuration( rCircle.timeDuration ), nStyle( rCircle.nStyle ), wColor( rCircle.wColor ), lParam( rCircle.lParam ) {}
	SMiniMapCircle& operator=( const SMiniMapCircle &rCircle )
	{
		if( &rCircle != this )
		{
			vCenter = rCircle.vCenter;
			fRadius = rCircle.fRadius;
			timeStart = rCircle.timeStart;
			timeDuration = rCircle.timeDuration;
			nStyle = rCircle.nStyle;
			wColor = rCircle.wColor;
			lParam = rCircle.lParam;
		}
		return *this;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMarkPixelFunctional
{
	CTextureLock<SGFXColor4444> *pTextureLock;
	SGFXColor4444 color;
	CTPoint<int> size;
public:
	CMarkPixelFunctional( CTextureLock<SGFXColor4444> *_pTextureLock, SGFXColor4444 _color , const CTPoint<int> &_size )
		: pTextureLock( _pTextureLock ), color( _color ), size( _size ) {}
	void operator()( int nXPos, int nYPos )
	{
		if ( ( nXPos >= 0 ) &&
			   ( nXPos < size.x )&&
			   ( nYPos >= 0 ) &&
			   ( nYPos < size.y ) )
		{
			( *pTextureLock )[size.y - 1 - nYPos][nXPos] = color;
		}
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//�������� ����������� ��������� �������:
// � ��������: ( � AI ������ ��� Y �������������)
//0 ---------------- 3
//  |  --------->   |
//  |  |      ��� X |
//  |  |            |
//  |  |            |
//  |  V ��� Y      |
//1 ---------------- 2
// �� ������ ( � �������� )
//         0
//        / \
//      /     \
//    /         \
//1 /             \ 3
//  \             /
//    \         /
//      \     /
//        \ /
//         2
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIMiniMap : public CSimpleWindow
{
	DECLARE_SERIALIZE;

  const static char MARKERS_TYPES_FILE_NAME[];
  const static char MARKERS_TEXTURE_NAME[];
	const static char MARKERS_TYPES_NAME[];

	//�������� �������� pWarFog � pWarFogTexture (0 - ���� ������, 1 - ���� ������)
	SGFXColor4444 pWarFogValues[SAIConsts::VIS_POWER + 1];
	//����� ������ � ����������� �� ������
	SGFXColor4444 pPartyColors[SAIConsts::MAX_NUM_OF_PLAYERS + 1];
	//���� ����� ������
	DWORD dwScreenFrameColor;
	DWORD dwScreenFrameColorShadow;
  //������ MiniMap �� ����������� ( ����������� �� XML ����� )
  //������ MiniMap �� ��������� ����� nSize / 2
  int nSize;
	int nPlayersCount;
	//������ �������� ����������� ���� (0 - �����, 1 - ������� 3x3, 2 - ������� 5x5 ... )
  int nUnitCrossSize;

	//������ ������� ���� � VIS ������ ( ������� �� AITerrain ��� ������������� )
  CTPoint<int> terrainSize;
  //���������� ����������� VIS ������ �� ������ ������
  int nFiledVISTiles;

  //������ AI �������� ( ����������� �� ���������� �� AILogic )
  std::vector<SMiniMapUnitInfo> units;
  //������ �������� �������� ( ����������� �� ���������� �� AILogic )
  std::vector<SShootAreas> shootAreas;
	//������ ������ ����������( ����������� �� ���������� �� AILogic )
	std::list<SMiniMapCircle> circles;
	
	//������ �������, ������������ �������
	std::unordered_map<std::string, STextureMarker> markersTypes;
	std::list<SMiniMapMarker> markers;

	bool isWarFogNeedUpdate;
	bool isInstantObjectsNeedUpdate;

	//�������� Fog Of War ( ����������� �� ���������� �� AILogic )
  CPtr<IGFXTexture> pWarFog;
	//�������� Fog Of War ( ��� ��������������� ����������� � ����������� � �������������� �� ����� )
  CPtr<IGFXTexture> pWarFogTexture;
	//�������� � �������, ��������� ���������  �.�. ( ����������� �� ���������� �� AILogic )
  CPtr<IGFXTexture> pInstantObjects;
	//�������� � �������, ��������� ���������  �.�. ( ��� ��������������� ����������� � ����������� � �������������� �� ����� )
  CPtr<IGFXTexture> pInstantObjectsTexture;
	//�������� Mini Map ( �������� �� �������� )
  CPtr<IGFXTexture> pBackgroundTexture;
	//�������� Objective ( �������� �� �������� )
  CPtr<IGFXTexture> pMarkerTexture;

  // ����� ����������� ������� ( � �������� �������� ������� )
  //DWORD dwPreviousUpdateTime;
  // ������� ���������� ���������� �� �������� �� MiniMap
  //DWORD dwRefreshTimeout;

	void CreateMiniMapTextures();
	//���������� ���������� ������� ����� � ���������� ����������� ( isTopLeft == true - ������������� ��� Y )
  //���������� ���������� ������� ����� � ����������� fog of war ( isTopLeft == false )
  void GetZeroPoint( float *pfXZeroPoint, float *pfYZeroPoint, bool isTopLeft = true );
  //���������� ���������� ����� � ���������� ����������� ( isTopLeft == true - ������������� ��� Y )
  //���������� ���������� ����� � ����������� fog of war ( isTopLeft == false )
  void PointToTextureMiniMap( float fXPos, float fYPos, float *pfXMiniMapPos, float *pfYMiniMapPos , bool isLeftTop = true );
  //���������� ���������� ����� � ������� ����������� ( isTopLeft == true - ������������� ��� Y )
  //���������� ���������� ����� � ������� ����������� ( isTopLeft == false )
	void TextureMiniMapToPoint( float fXMiniMapPos, float fYMiniMapPos, float *pfXPos, float *pfYPos , bool isLeftTop = true );
	//���������� Y �� ����� �, ���������� ����� ��������� ������ ���������� ����� ��� �����
	float GetYByX( float fX, float fX0, float fY0, float fX1, float fY1 )
	{
		NI_ASSERT_SLOW_T( ( fX1 - fX0 ) != 0,
											NStr::Format( "Devision by zero: (%f)", fX1 - fX0 ) );
		return ( fY1 * ( fX - fX0 ) - fY0 * ( fX - fX1 ) ) / ( fX1 - fX0 );

	}
	//����� X �� ���������� Y, ���������� ����� ��������� ������ ���������� ����� ��� �����
	float GetXByY( float fY, float fX0, float fY0, float fX1, float fY1 )
	{
		NI_ASSERT_SLOW_T( ( fY1 - fY0 ) != 0,
											NStr::Format( "Devision by zero: (%f)", fY1 - fY0 ) );
		return ( fX1 * ( fY - fY0 ) - fX0 * ( fY - fY1 ) ) / ( fY1 - fY0 );
	}
	//���������� ����� ����� ������ �� miniMap � ����������� ������, � ������� �����������
	//������������������ �����, ��� � ����� 0-1
	void GetVerticalClippedScreenEdge( const CTPoint<float> &v0, const CTPoint<float> &v1, std::vector<CTPoint<float> > *pvPoints );
	//���������� ����� ����� ������ �� miniMap � ����������� ������, � ������� �����������
	//������������������ �����, ��� � ����� 1-2
	void GetHorizontalClippedScreenEdge( const CTPoint<float> &v1, const CTPoint<float> &v2, std::vector<CTPoint<float> > *pvPoints );
	//���������� ����� ������ �� miniMap � ����������� ������, � ������� �����������
	//��������� ������� � ��������� �������� ���������� �������� � ������� ������ PointToTextureMiniMap()
	void GetClippedScreenFrame( std::vector<CTPoint<float> > *pvPoints, IGFX *pGFX );

	//������� ������ ������ SetScreenSize() ���� ��� �� �������, minimap �� ����� ���������
	bool IsInitialized()
  {
    return ( ( terrainSize.x > 0 ) &&
             ( terrainSize.y > 0 ) );
  }

	//�������� �������������� ����� �������������� minimap
	bool InMiniMap( float fXPos, float fYPos )
	{
		return ( ( fXPos >= 0 ) &&
			       ( fXPos <= terrainSize.x ) &&
				     ( fYPos >=0 ) &&
				     ( fYPos <= terrainSize.y ) );
	}

	//
	void DrawFireRanges( CTextureLock<SGFXColor4444> *pTextureLock );
public:
	CUIMiniMap( )
		: nSize( 0 ), nUnitCrossSize( 1 ), terrainSize( 0, 0 ), nFiledVISTiles( 0 ),  dwScreenFrameColor( 0xFFAFAFAF ), dwScreenFrameColorShadow( 0xFF4F4F4F ), isWarFogNeedUpdate( false ), isInstantObjectsNeedUpdate( false ), nPlayersCount( 2 )
	{
		for ( int nFogValue = 0; nFogValue <= SAIConsts::VIS_POWER; ++nFogValue )
		{
			WORD a = 8 - 8 * nFogValue / SAIConsts::VIS_POWER;
			pWarFogValues[nFogValue] = a << 12;
		}

		//CRAP{ ���������� ������� ���������������� ����� �������
		pPartyColors[0] = 0xF0F0;
		pPartyColors[1] = 0xFF00;
		pPartyColors[2] = 0xF00F;
		pPartyColors[3] = 0xFFF0;
		pPartyColors[4] = 0xF0FF;
		pPartyColors[5] = 0xFF0F;
		pPartyColors[6] = 0xFFFF;
		pPartyColors[7] = 0xFF80;
		pPartyColors[8] = 0xFF08;
		pPartyColors[9] = 0xF8F0;
		pPartyColors[10] = 0xF0F8;
		pPartyColors[11] = 0xF80F;
		pPartyColors[12] = 0xF08F;
		pPartyColors[13] = 0xFF88;
		pPartyColors[14] = 0xF8F8;
		pPartyColors[15] = 0xF88F;
		pPartyColors[16] = 0x0000;
		/**
		for ( int index = 2; index <= SAIConsts::MAX_NUM_OF_PLAYERS; ++index )
		{
			pPartyColors[index] = 0x0000;
		}
		/**/
		//}CRAP
	}
	~CUIMiniMap()
	{
	}

	//���������� ������ ���� ��� ��������� ���������� �� fog of war, �������� ������ ����� ������� ������ �� ���� ��������
  //������ � VIS ������
  virtual void STDCALL SetTerrainSize( int nXTerrainSize, int nYTerrainSize, int _nPlayersCount );
	//���������� �������� �����
	virtual void STDCALL SetBackgroundTexture( IGFXTexture *_pBackgroundTexture )
	{
		pBackgroundTexture = _pBackgroundTexture;
	}

	//�������� ���������� � fog of war, ������� �� AILogic ������� ������:
	//���� �������� ��������� ������ ���������� - ������������ true
	//����� - ������������ false
	virtual bool STDCALL AddWarFogData( const BYTE *pVizBuffer, int nLength );
	//�������� ���������� � ������, ������� �� AILogic ������� ������:
	virtual void STDCALL AddUnitsData( const struct SMiniMapUnitInfo *pUnitsBuffer, int nUnitsCount );
	//�������� ���������� �� ������������ �������� � ����� ��������
	virtual void STDCALL AddFireRangeAreas( const struct SShootAreas *pShootAreasBuffer, int nShootAreasCount );
	//
	virtual void STDCALL AddCircle( const CVec2 &vCenter, const float fRadius, int nStyle, WORD wColor, const NTimer::STime &rStart, const NTimer::STime &rDuration, bool bRelative, LPARAM lParam );
	virtual int STDCALL AddMarker( const std::string &rszName, const CVec2 &vPos, bool _bActive, int _nID, const NTimer::STime &rStart, const NTimer::STime &rDuration, bool bRelative );
	virtual void STDCALL ActivateMarker( int _nID, bool _bActive );
	virtual void STDCALL ActivateMarker( const std::string &rszName, bool _bActive );
	virtual void STDCALL RemoveMarker( int _nID );
	virtual void STDCALL RemoveMarker( const std::string &rszName );
	
	//from UIControl interface
	// serializing...
	virtual int STDCALL operator&( IDataTree &ss );

//Mouse moving
	virtual bool STDCALL IsInside( const CVec2 &vPos );
	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnLButtonUp( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnMouseMove( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnRButtonUp( const CVec2 &vPos, EMouseState mouseState );
// update
  virtual bool STDCALL Update( const NTimer::STime &currTime );
// drawing
	virtual void STDCALL Draw( interface IGFX *_pGFX ) = 0;
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIMiniMapBridge : public IUIMiniMap, public CUIMiniMap
{
	OBJECT_NORMAL_METHODS( CUIMiniMapBridge );
	DECLARE_SUPER( CUIMiniMap );
	DEFINE_UIELEMENT_BRIDGE;

  virtual void STDCALL SetTerrainSize( int nXTerrainSize, int nYTerrainSize, int _nPlayersCount ) { CSuper::SetTerrainSize( nXTerrainSize, nYTerrainSize, _nPlayersCount ); }
	virtual void STDCALL SetBackgroundTexture( IGFXTexture *_pBackgroundTexture )  { CSuper::SetBackgroundTexture( _pBackgroundTexture ); }
	virtual bool STDCALL AddWarFogData( const BYTE *pVizBuffer, int nLength ) { return CSuper::AddWarFogData( pVizBuffer, nLength ); }
	virtual void STDCALL AddUnitsData( const struct SMiniMapUnitInfo *pUnitsBuffer, int nUnitsCount )  { CSuper::AddUnitsData( pUnitsBuffer, nUnitsCount ); }
	virtual void STDCALL AddFireRangeAreas( const struct SShootAreas *pShootAreasBuffer, int nShootAreasCount )  { CSuper::AddFireRangeAreas( pShootAreasBuffer, nShootAreasCount ); }
	virtual void STDCALL AddCircle( const CVec2 &vCenter, const float fRadius, int nStyle, WORD wColor, const NTimer::STime &rStart, const NTimer::STime &rDuration, bool bRelative, LPARAM lParam ) { CSuper::AddCircle( vCenter, fRadius, nStyle, wColor, rStart, rDuration, bRelative, lParam ); }
	//
	virtual int STDCALL AddMarker( const std::string &rszName, const CVec2 &vPos, bool _bActive, int _nID, const NTimer::STime &rStart, const NTimer::STime &rDuration, bool bRelative ) { return CSuper::AddMarker( rszName, vPos, _bActive, _nID, rStart, rDuration, bRelative ); }
	virtual void STDCALL ActivateMarker( int _nID, bool _bActive ) { CSuper::ActivateMarker( _nID, _bActive ); }
	virtual void STDCALL ActivateMarker( const std::string &rszName, bool _bActive ) { CSuper::ActivateMarker( rszName, _bActive ); }
	virtual void STDCALL RemoveMarker( int _nID ) { CSuper::RemoveMarker( _nID ); }
	virtual void STDCALL RemoveMarker( const std::string &rszName )  { CSuper::RemoveMarker( rszName ); }
};
#endif //#if !defined(__UIMiniMap__)

