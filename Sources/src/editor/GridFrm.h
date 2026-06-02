#ifndef __GRIDFRM_H__
#define __GRIDFRM_H__

#include "..\\Common\\StingrayCompat.h"
#include "..\GFX\GFX.h"
#include "..\GFX\GFXTypes.h"
#include "..\GFX\GFXHelper.h"
#include "ParentFrame.h"

using std::list;

struct SAITile
{
	int nTileX;
	int nTileY;
	int nVal;
	CPtr<IGFXVertices> pVertices;
	
	SAITile() : nTileX( 0 ), nTileY( 0 ), nVal( 0 ) {};

	int operator&( IDataTree &ss );
};
typedef list<SAITile> CListOfTiles;

struct SAINormalTile
{
	int nTileX;
	int nTileY;
	int nVal;
	CPtr<IGFXVertices> pVertices;
	CPtr<IGFXVertices> pNormalVertices;
	
	SAINormalTile() : nTileX( 0 ), nTileY( 0 ), nVal( 0 ) {};
	
	int operator&( IDataTree &ss );
};
typedef list<SAINormalTile> CListOfNormalTiles;

class CGridFrame : public CParentFrame
{
	DECLARE_DYNCREATE(CGridFrame)
public:
	CGridFrame();
	virtual ~CGridFrame() {}

	virtual void Init( IGFX *_pGFX );
	
private:
	CPtr<IGFXVertices> pGridVertices;
	
protected:
	CPtr<IGFXIndices> pMarkerIndices;
	
	enum ETBStyle
	{
		E_MOVE_OBJECT,
		E_DRAW_GRID,
		E_DRAW_PASS,
		E_DRAW_TRANSEPARENCE,
		E_SET_ZERO,
		E_SET_ENTRANCE,
		E_SET_SHOOT_POINT,
		E_SET_HORIZONTAL_SHOOT,
		E_SET_SHOOT_ANGLE,
		E_DRAW_ONE_WAY_TRANSEPARENCE,
		E_SET_FIRE_POINT,
		E_SET_DIRECTION_EXPLOSION,
		E_SET_SMOKE_POINT,
	};
	ETBStyle tbStyle;

	enum ETypeOfTile
	{
		E_LOCKED_TILE,
		E_TRANSEPARENCE_TILE,
		E_ENTRANCE_TILE,
		E_UNLOCKED_TILE,
	};

protected:
	void GFXDraw();
	void MyCreateGrid();
	void ComputeGameTileCoordinates( POINT pt, float &ftX, float &ftY );
	void GetGameTileCoordinates( int nTileX, int nTileY, float &fX1, float &fY1, float &fX2, float &fY2, float &fX3, float &fY3, float &fX4, float &fY4 );

	void SetTileInListOfTiles( CListOfTiles &listOfTiles, int nTileX, int nTileY, int nVal, int nTypeOfTile );
	void SetTileInListOfNormalTiles( CListOfNormalTiles &listOfTiles, int nTileX, int nTileY, int nVal );
	void DeleteTileInListOfNormalTiles( CListOfNormalTiles &listOfTiles, int nTileX, int nTileY );

	bool SaveIconFile( const char *pszSrc, const char *pszRes );
};

#endif		//__GRIDFRM_H__

