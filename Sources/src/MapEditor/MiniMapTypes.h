#if !defined(__EditorMiniMap_Types__)
#define __EditorMiniMap_Types__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class IMiniMapDrawTool
{
public:
	virtual void SetPen( int penStyle, int penWidth, COLORREF penColor ) = 0;
	virtual void DestroyPen() = 0;
	virtual void MoveTo( const CTPoint<int> &pointTo ) = 0;
	virtual void LineTo( const CTPoint<int> &pointTo ) = 0;
	virtual void Line( const CTPoint<int> &pointFrom, const CTPoint<int> &pointTo ) = 0;
	virtual void Circle( const CTPoint<int> &center, int radius ) = 0;
	virtual void Frame( const CTRect<int> &rectFrame ) = 0;
	virtual void FillSolidRect( const CTRect<int> &rectFrame, COLORREF color ) = 0;
	virtual void DrawBitmap( CDC* pBitmapDC, int nXSize, int nYSize ) = 0;
	virtual void DrawDoubleLine( int nWidth, COLORREF darkColor, COLORREF lightColor, const CTPoint<int> &rShift ) {}

	virtual ~IMiniMapDrawTool() { }
};
class IMiniMapElement : public IRefCount
{
public:
	virtual void GetName( std::string *pszName ) = 0;
	virtual void Update( CDC *pDC ) = 0;
	virtual void Draw( IMiniMapDrawTool* pTool ) = 0;
};

class CScreenFrame : public IMiniMapElement
{
	OBJECT_NORMAL_METHODS( CScreenFrame );

private:
	CTPoint<int> point0;
	CTPoint<int> point1;
	CTPoint<int> point2;
	CTPoint<int> point3;

	int penStyle;
	int penWidth;
	COLORREF penColor;

public:	
	CScreenFrame( int _penStyle = PS_SOLID, int _penWidth = 1, COLORREF _penColor = RGB( 0xFF, 0xFF, 0xFF ) )
		: point0( 0, 0 ), point1( 0, 0 ), point2( 0, 0 ), point3( 0, 0 ),
		  penStyle( _penStyle ), penWidth ( _penWidth ), penColor( _penColor ) { }
	virtual void GetName( std::string *pszName )
	{
		NI_ASSERT_SLOW_T( pszName != 0,
											NStr::Format( "Wrong parameter <pszName>: %x", pszName ) );
		*pszName = "Screen Frame";
	}
	virtual void Update( CDC *pDC );
	virtual void Draw( IMiniMapDrawTool* pTool )
	{
		NI_ASSERT_SLOW_T( pTool != 0,
											NStr::Format( "Wrong parameter <pTool>: %x", pTool ) );

		pTool->MoveTo( point0 );
		pTool->LineTo( point1 );
		pTool->LineTo( point2 );
		pTool->LineTo( point3 );
		pTool->LineTo( point0 );
		pTool->DrawDoubleLine( 2, RGB( 0x20, 0x20, 0x20 ), RGB( 0xB0, 0xB0, 0xB0 ), CTPoint<int>( -2, 0 ) );
	}
};

class CUnitsSelection : public IMiniMapElement
{
	OBJECT_NORMAL_METHODS( CUnitsSelection );

	struct SUnitInfo
	{
		COLORREF color;
		CTRect<int> position;
	};

	std::list<SUnitInfo> units;

public:
	virtual void GetName( std::string *pszName )
	{
		NI_ASSERT_SLOW_T( pszName != 0,
											NStr::Format( "Wrong parameter <pszName>: %x", pszName ) );
		*pszName = "Units";
	}
	virtual void Update( CDC *pDC );
	virtual void Draw( IMiniMapDrawTool* pTool )
	{
		NI_ASSERT_SLOW_T( pTool != 0,
											NStr::Format( "Wrong parameter <pTool>: %x", pTool ) );

		for ( std::list<SUnitInfo>::const_iterator unitIterator = units.begin(); unitIterator != units.end(); ++unitIterator )
		{
			pTool->FillSolidRect( unitIterator->position, unitIterator->color );		
		}
	}
};


class CFireRangeAreas : public IMiniMapElement
{
	OBJECT_NORMAL_METHODS( CFireRangeAreas );

private:
	std::vector<SShootAreas> areas;

	int circlePenStyle;
	int circlePenWidth;
	
	int sectorPenStyle;
	int sectorPenWidth;

	void DrawShootArea( IMiniMapDrawTool* pTool, const SShootArea &area );
public:	
	CFireRangeAreas( const int _circlePenStyle = PS_SOLID, const int _circlePenWidth = 1,
									 const int _sectorPenStyle = PS_SOLID, const int _sectorPenWidth = 1 )
		: circlePenStyle( _circlePenStyle ), circlePenWidth( _circlePenWidth ),
		  sectorPenStyle( _sectorPenStyle ), sectorPenWidth( _sectorPenWidth ) { }

	virtual void GetName( std::string *pszName )
	{
		NI_ASSERT_SLOW_T( pszName != 0, NStr::Format( "Wrong parameter <pszName>: %x", pszName ) );
		*pszName = "Fire Range Areas";
	}
	virtual void Update( CDC *pDC );
	virtual void Draw( IMiniMapDrawTool* pTool );
};
/**
class CReservePositions : public IMiniMapElement
{
	OBJECT_NORMAL_METHODS( CReservePositions );

private:
	std::vector< std::vector<CTPoint<int> > > positions;
	std::vector<CTPoint<int> > currentPosition;

	int circlePenStyle;
	int circlePenWidth;
	COLORREF circlePenColor;
	
	int sectorPenStyle;
	int sectorPenWidth;
	COLORREF sectorPenColor;

public:	
	CFireRangeAreas( int _circlePenStyle = PS_SOLID, 
									 int _circlePenWidth = 1,
									 COLORREF _circlePenColor = RGB( 0xFF, 0xFF, 0xFF ),
									 int _sectorPenStyle = PS_SOLID, 
									 int _sectorPenWidth = 1,
									 COLORREF _sectorPenColor = RGB( 0xFF, 0xFF, 0xFF ) )
		: circlePenStyle( _circlePenStyle ),
		  circlePenWidth (_circlePenWidth ),
			circlePenColor( _circlePenColor ),
		  sectorPenStyle( _sectorPenStyle ),
		  sectorPenWidth (_sectorPenWidth ),
			sectorPenColor( _sectorPenColor ) { }

	virtual void GetName( std::string *pszName )
	{
		NI_ASSERT_SLOW_T( pszName != 0,
											NStr::Format( "Wrong parameter <pszName>: %x", pszName ) );
		*pszName = "Fire Range Areas";
	}
	virtual void Update( CDC *pDC );
	virtual void Draw( IMiniMapDrawTool* pTool )
	{
		NI_ASSERT_SLOW_T( pTool != 0,
											NStr::Format( "Wrong parameter <pTool>: %x", pTool ) );

		for ( int index = 0; index < areas.size(); ++index )
		{
			pTool->SetPen( circlePenStyle, circlePenWidth, circlePenColor );
			pTool->Circle( CTPoint<int>( areas[index].shootCircle.center.x, areas[index].shootCircle.center.y ), areas[index].shootCircle.r );
			pTool->DestroyPen();
			
			if ( areas[index].wStartAngle != areas[index].wFinishAngle )
			{
				pTool->SetPen( sectorPenStyle, sectorPenWidth, sectorPenColor );
				pTool->MoveTo( CTPoint<int>( areas[index].shootCircle.center.x, areas[index].shootCircle.center.y ) );
				{
					float fAngle = fmod( float( areas[index].wStartAngle ) / 65535.0f * FP_2PI + FP_PI2, FP_2PI );
					float fCos = cos( fAngle );
					float fSin = sin( fAngle );

					int x = areas[index].shootCircle.center.x + areas[index].shootCircle.r * fCos;
					int y = areas[index].shootCircle.center.y + areas[index].shootCircle.r * fSin;
		
					pTool->LineTo( CTPoint<int>( x, y ) );
				}
				pTool->MoveTo( CTPoint<int>( areas[index].shootCircle.center.x, areas[index].shootCircle.center.y ) );
				{
					float fAngle = fmod( float( areas[index].wFinishAngle ) / 65535.0f * FP_2PI + FP_PI2, FP_2PI );
					float fCos = cos( fAngle );
					float fSin = sin( fAngle );

					int x = areas[index].shootCircle.center.x + areas[index].shootCircle.r * fCos;
					int y = areas[index].shootCircle.center.y + areas[index].shootCircle.r * fSin;
		
					pTool->LineTo( CTPoint<int>( x, y ) );
				}
				pTool->DestroyPen();
			}
		}
	}
};
/**/
class CMiniMapTerrain : public IMiniMapElement
{
	OBJECT_NORMAL_METHODS( CMiniMapTerrain );

private:
	CDC dc;
	CBitmap *pBitmap;
	CBitmap *pOldBitmap;
	CTPoint<int> size; 
	bool bGame;

	COLORREF colors[255];
	void Create( CDC *pDC )
	{
		dc.CreateCompatibleDC( pDC );
		pBitmap = new CBitmap();
		if ( pBitmap )
		{
			pBitmap->CreateCompatibleBitmap( pDC, size.x, size.y );
			pOldBitmap = dc.SelectObject( pBitmap );
		}
	}
	void Clear()
	{
		if ( pBitmap )
		{
			dc.SelectObject( pOldBitmap );
			delete pBitmap;
			pBitmap = 0;
		}
		dc.DeleteDC();
		size.x = 0;
		size.y = 0;
	}

public:	
	CMiniMapTerrain() 
		: pBitmap( 0 ), pOldBitmap( 0 ), size( 0, 0 ), bGame( false )
	{
		for ( int index = 0; index < 255; ++index )
		{
			colors[index] = RGB( index, index, index );
		}
	}
	~CMiniMapTerrain()
	{
		Clear();		
	}

	void SetInGame( bool _bGame ) { bGame = _bGame; } 
	bool GetInGame() { return bGame; } 

	void UpdateColor();

	virtual void GetName( std::string *pszName )
	{
		NI_ASSERT_SLOW_T( pszName != 0,
											NStr::Format( "Wrong parameter <pszName>: %x", pszName ) );
		*pszName = "Terrain";
	}
	virtual void Update( CDC *pDC );
	virtual void Draw( IMiniMapDrawTool* pTool )
	{
		NI_ASSERT_SLOW_T( pTool != 0,
											NStr::Format( "Wrong parameter <pTool>: %x", pTool ) );

		pTool->DrawBitmap( &dc, size.x, size.y );
	}
};
class CMiniMapTerrainGrid : public IMiniMapElement
{
	OBJECT_NORMAL_METHODS( CMiniMapTerrainGrid );

private:
	CTPoint<int> size;
	CTPoint<int> glidLines;

	int penStyle;
	int penWidth;
	COLORREF penColor;

public:	
	CMiniMapTerrainGrid( int _penStyle = PS_SOLID, int _penWidth = 1, COLORREF _penColor = RGB( 0xFF, 0xFF, 0xFF ) )
		: size( 0, 0 ), glidLines( 0, 0 ),
		  penStyle( _penStyle ), penWidth ( _penWidth ), penColor( _penColor ) { }
	virtual void GetName( std::string *pszName )
	{
		NI_ASSERT_SLOW_T( pszName != 0,
											NStr::Format( "Wrong parameter <pszName>: %x", pszName ) );
		*pszName = "Grid";
	}
	virtual void Update( CDC *pDC );
	virtual void Draw( IMiniMapDrawTool* pTool )
	{
		NI_ASSERT_SLOW_T( pTool != 0,
											NStr::Format( "Wrong parameter <pTool>: %x", pTool ) );

		if ( ( glidLines.y > 0 ) && ( glidLines.y > 0 ) )
		{
			pTool->SetPen( penStyle, penWidth, penColor );

			for ( int index = 1; index < glidLines.x; ++index )
			{
				CTPoint<int> startPoint( index * size.x / glidLines.x, 0 );
				CTPoint<int> finishPoint( index * size.x / glidLines.x, size.y );

				pTool->MoveTo( startPoint );
				pTool->LineTo( finishPoint );
			}
			for ( int index = 1; index < glidLines.y; ++index )
			{
				CTPoint<int> startPoint( 0, index * size.y / glidLines.y );
				CTPoint<int> finishPoint( size.x, index * size.y / glidLines.y );

				pTool->MoveTo( startPoint );
				pTool->LineTo( finishPoint );
			}

			pTool->DestroyPen();
		}
	}
};
class CSizedMiniMapDrawTool : public IMiniMapDrawTool
{
protected:
	CDC* pDC;
	CRect drawingRect;
	CPoint maxPoint;

	int ActualX( int x )
	{
		return drawingRect.left + ( x * drawingRect.Width() / maxPoint.x );
	}
	int ActualY( int y )
	{
		return drawingRect.bottom - 1 - (y * drawingRect.Height()  / maxPoint.y );
	}

public:
	CSizedMiniMapDrawTool( const CRect &_drawingRect, const CPoint &_maxPoint, CDC* _pDC )
		: drawingRect( _drawingRect ), maxPoint( _maxPoint ), pDC( _pDC )
	{
		NI_ASSERT_SLOW_T( pDC != 0,
											NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );
		CRgn clipRgn;
		clipRgn.CreateRectRgnIndirect( &drawingRect );
		pDC->SelectClipRgn( &clipRgn, RGN_COPY );
	}
	~CSizedMiniMapDrawTool()
	{
		pDC->SelectClipRgn( 0, RGN_COPY );
	}
	
	virtual void MoveTo( const CTPoint<int> &pointTo )
	{
		pDC->MoveTo( ActualX( pointTo.x ), ActualY( pointTo.y ) );
	}
	virtual void LineTo( const CTPoint<int> &pointTo )
	{
		pDC->LineTo( ActualX( pointTo.x ), ActualY( pointTo.y ) );
	}
	virtual void Line( const CTPoint<int> &pointFrom, const CTPoint<int> &pointTo )
	{
		pDC->MoveTo( ActualX( pointFrom.x ), ActualY( pointFrom.y ) );
		pDC->LineTo( ActualX( pointTo.x ), ActualY( pointTo.y ) );
	}
	virtual void Circle( const CTPoint<int> &center, int radius )
	{
		pDC->MoveTo( ActualX( center.x + radius ), ActualY( center.y ) );
		pDC->Arc( ActualX( center.x - radius ),
							ActualY( center.y - radius ),
							ActualX( center.x + radius ),
							ActualY( center.y + radius ),
							ActualX( center.x + radius ),
							ActualY( center.y ),
							ActualX( center.x + radius ),
							ActualY( center.y ) );
	}
	virtual void Frame( const CTRect<int> &rectFrame )
	{
		pDC->MoveTo( ActualX( rectFrame.left ), ActualY( rectFrame.top ) );
		pDC->LineTo( ActualX( rectFrame.right ), ActualY( rectFrame.top ) );
		pDC->LineTo( ActualX( rectFrame.right ), ActualY( rectFrame.bottom ) );
		pDC->LineTo( ActualX( rectFrame.left ), ActualY( rectFrame.bottom ) );
		pDC->LineTo( ActualX( rectFrame.left ), ActualY( rectFrame.top ) );
	}
	virtual void FillSolidRect( const CTRect<int> &rectFrame, COLORREF color )
	{
		pDC->FillSolidRect( ActualX( rectFrame.minx ),
												ActualY( rectFrame.miny ),
												ActualX( rectFrame.maxx ) - ActualX( rectFrame.minx ),
												ActualY( rectFrame.maxy ) - ActualY( rectFrame.miny ),
												color );
	}
};
class CXORLinesMiniMapDrawTool : public CSizedMiniMapDrawTool
{
protected:
	CPen* pOldPen;
	CPen* pNewPen;
	int nPreviousDrawMode;

	void CreateNewPen( int penStyle, int penWidth, COLORREF penColor )
	{
		pNewPen = new CPen( penStyle, penWidth, penColor );
	}
	void DestroyNewPen()
	{
		if ( pNewPen )
		{
			delete pNewPen;
		}
		pNewPen = 0;
	}

public:
	CXORLinesMiniMapDrawTool( const CRect &_drawingRect, const CPoint &_maxPoint, CDC* _pDC )
		: CSizedMiniMapDrawTool( _drawingRect, _maxPoint, _pDC ), pOldPen( 0 ), pNewPen( 0 )
	{
	}
	~CXORLinesMiniMapDrawTool()
	{
		DestroyPen();
	}
	virtual void SetPen( int penStyle, int penWidth, COLORREF penColor )
	{
		DestroyPen();
		CreateNewPen( penStyle, penWidth, penColor );
		if ( pNewPen )
		{
			pOldPen = pDC->SelectObject( pNewPen );
		}
	}
	virtual void DestroyPen()
	{
		if ( pOldPen )
		{
			pDC->SelectObject( pOldPen );
		}
		pOldPen = 0;
		DestroyNewPen();
	}
	virtual void DrawBitmap( CDC* pBitmapDC, int nXSize, int nYSize ) {}
};

class CXORDoubleLinesMiniMapDrawTool : public CSizedMiniMapDrawTool
{
	std::list<CTPoint<int> > points;
public:
	CXORDoubleLinesMiniMapDrawTool( const CRect &_drawingRect, const CPoint &_maxPoint, CDC* _pDC ) :
			CSizedMiniMapDrawTool( _drawingRect, _maxPoint, _pDC ) {}
	~CXORDoubleLinesMiniMapDrawTool()
	{
	}

	virtual void SetPen( int penStyle, int penWidth, COLORREF penColor ) {}
	virtual void DestroyPen() {}
	virtual void DrawBitmap( CDC* pBitmapDC, int nXSize, int nYSize ) {}
	
	virtual void MoveTo( const CTPoint<int> &pointTo )
	{
		points.clear();
		points.push_back( CTPoint<int>( ActualX( pointTo.x ), ActualY( pointTo.y ) ) );
	}
	
	virtual void LineTo( const CTPoint<int> &pointTo )
	{
		points.push_back( CTPoint<int>( ActualX( pointTo.x ), ActualY( pointTo.y ) ) );
	}
	
	virtual void DrawDoubleLine( int nWidth, COLORREF darkColor, COLORREF lightColor, const CTPoint<int> &rShift )
	{
		if ( !points.empty() )
		{
			CPen darkPen( PS_SOLID, nWidth, darkColor );
			CPen lightPen( PS_SOLID, nWidth, lightColor );

			CPen *pOldPen = pDC->SelectObject( &darkPen );
			pDC->MoveTo( points.front().x, points.front().y );
			for ( std::list<CTPoint<int> >::const_iterator pointIterator = points.begin(); pointIterator != points.end(); ++pointIterator )
			{
				pDC->LineTo( pointIterator->x, pointIterator->y );
			}
			pDC->SelectObject( pOldPen );

			pOldPen = pDC->SelectObject( &lightPen );
			pDC->MoveTo( points.front().x + rShift.x, points.front().y + rShift.y );
			for ( std::list<CTPoint<int> >::const_iterator pointIterator = points.begin(); pointIterator != points.end(); ++pointIterator )
			{
				pDC->LineTo( pointIterator->x + rShift.x, pointIterator->y + rShift.y );
			}
			pDC->SelectObject( pOldPen );
		}
	}
};

class CBitmapMiniMapDrawTool : public CSizedMiniMapDrawTool
{
public:
	CBitmapMiniMapDrawTool( const CRect &_drawingRect, const CPoint &_maxPoint, CDC* _pDC )
		: CSizedMiniMapDrawTool( _drawingRect, _maxPoint, _pDC ) {}
	virtual void SetPen( int penStyle, int penWidth, COLORREF penColor ) {}
	virtual void DestroyPen() {}
	virtual void DrawBitmap( CDC* pBitmapDC, int nXSize, int nYSize )
	{
		CPoint orgPoint;
		::GetBrushOrgEx( pDC->m_hDC, &orgPoint );
		pDC->SetStretchBltMode( HALFTONE );
		pDC->StretchBlt( drawingRect.left,
										 drawingRect.top,
										 drawingRect.Width(),
										 drawingRect.Height(),
										 pBitmapDC,
										 0,
										 0,
										 nXSize,
										 nYSize,
										 SRCCOPY );
		CPoint point;
		::SetBrushOrgEx( pDC->m_hDC, orgPoint.x, orgPoint.y, &point );
	}
};
	/**
	CScreenFrame( const CScreenFrame &screenFrame )
		: point0( screenFrame.point0 ), point1( screenFrame.point1 ), point2( screenFrame.point2 ), point3( screenFrame.point3 ),
		  penStyle( screenFrame.penStyle ), penWidth ( screenFrame.penWidth ), penColor( screenFrame.penColor ) { }
	CScreenFrame& operator=( const CScreenFrame &screenFrame )
	{
		if ( this != &screenFrame )
		{
			point0 = screenFrame.point0;
			point1 = screenFrame.point1;
			point2 = screenFrame.point2;
			point3 = screenFrame.point3;
		  penStyle = screenFrame.penStyle;
			penWidth = screenFrame.penWidth;
			penColor = screenFrame.penColor;
		}
		return ( *this );
	}
	/**/
/**
class CMiniMapVectorElement : public IMiniMapElement
{
	OBJECT_NORMAL_METHODS( CMiniMapVectorElement );

private:
	std::vector<CPtr<IMiniMapElement> > vElements;
	
public:	
	std::vector<CPtr<IMiniMapElement> >& Elements() { return vElements; }
	
	virtual void Draw( IMiniMapDrawTool* pTool )
	{
		for( int index = 0; index < vElements.size(); ++index )
		vElements[index]->Draw( pTool );
	}
};
/**/
/**
							pColors[i].r = builder.GetTerrainType( pTiles[i].tile ) * 16;
							pColors[i].b = pTiles[i].shade;
							pColors[i].g = 255;
/**/
/**
enum MiniMapLayers
{
	LAYER_TERRAIN = 0,
	LAYER_ENVIRONMENT = 1,
	LAYER_ROAD = 2,
	LAYER_UNIT = 3,
	LAYER_GRID = 4,
	LAYER_FRAME = 5,

	LAYER_DIMENSION = 6,
};
/**/
#endif // !defined(__EditorMiniMap_Types__)

