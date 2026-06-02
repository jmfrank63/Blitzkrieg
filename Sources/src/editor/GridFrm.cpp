#include "StdAfx.h"
#include "..\Scene\Scene.h"
#include "GridFrm.h"
#include "SpriteCompose.h"

static const int ICON_SIZE = 64;
static const float NORMAL_LENGTH = 40;
static const float fOX = -622;			//не менять эти значения, подсчитанны экспериментально для совместимости со старыми проектами.
static const float fOY = 296;				//иначе съедет сетка залоченных AI тайлов

int SAITile::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "x", &nTileX );
	saver.Add( "y", &nTileY );
	saver.Add( "val", &nVal );
	return 0;
}

int SAINormalTile::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "x", &nTileX );
	saver.Add( "y", &nTileY );
	saver.Add( "val", &nVal );
	return 0;
}

IMPLEMENT_DYNCREATE(CGridFrame, CParentFrame)

CGridFrame::CGridFrame()
{
	tbStyle = E_MOVE_OBJECT;
}

void CGridFrame::Init( IGFX *_pGFX )
{
	pGFX = _pGFX;
	
	MyCreateGrid();
}

void CGridFrame::GFXDraw()
{
	if ( pGridVertices )
		pGFX->Draw( pGridVertices, 0 );
}

void CGridFrame::MyCreateGrid()
{
	const int nX = 60;
	const int nY = 60;			//количество полосок по горизонтали и по вертикали
	float fX = fOX;
	float fY = fOY;

	const int MY_SIZE_X = 1000;
	const int MY_SIZE_Y = 500;

	pGridVertices = pGFX->CreateVertices( nX*4 + nY*4, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
	{
		int nStart;
		CVerticesLock<SGFXTLVertex> vertices( pGridVertices );

		fX = fOX;
		fY = fOY + fCellSizeY;
		nStart = 0;

		for ( int i=0; i<nY*4; ++i )
		{
			vertices[nStart + i*2 + 0].Setup( 0, 0, 1, 1, 0xffc0c0c0, 0xff00000, 0, 0 );
			vertices[nStart + i*2 + 1].Setup( 0, 0, 1, 1, 0xffc0c0c0, 0xff00000, 0, 0 );
		}

		
		for ( int i=0; i<nY; ++i )
		{
			vertices[nStart + i*2 + 0].Setup( fX + fCellSizeX*i, fY + fCellSizeY*i,
				1, 1, 0xffc0c0c0, 0xff00000, 0, 0 );
			vertices[nStart + i*2 + 1].Setup( fX + fCellSizeX*i + MY_SIZE_X, fY + fCellSizeY*i - MY_SIZE_Y,
				1, 1, 0xffc0c0c0, 0xff00000, 0, 0 );
		}

		nStart = nY * 2;
		for ( int i=0; i<nX; ++i )
		{
			vertices[nStart + i*2 + 0].Setup( fX + fCellSizeX*i, fY - fCellSizeY*i,
				1, 1, 0xffc0c0c0, 0xff00000, 0, 0 );
			vertices[nStart + i*2 + 1].Setup( fX + fCellSizeX*i + MY_SIZE_X, fY - fCellSizeY*i + MY_SIZE_Y,
				1, 1, 0xffc0c0c0, 0xff00000, 0, 0 );
		}

		fX = fOX;
		fY = fOY;
		nStart = nY * 4;
		for ( int i=0; i<nY; ++i )
		{
			vertices[nStart + i*2 + 0].Setup( fX + fCellSizeX*i, fY + fCellSizeY*i,
				1, 1, 0xffffffff, 0xff00000, 0, 0 );
			vertices[nStart + i*2 + 1].Setup( fX + fCellSizeX*i + MY_SIZE_X,	fY + fCellSizeY*i - MY_SIZE_Y,
				1, 1, 0xffffffff, 0xff00000, 0, 0 );
		}
		nStart = nY * 6;
		for ( int i=0; i<nX; ++i )
		{
			vertices[nStart + i*2 + 0].Setup( fX + fCellSizeX*i, fY - fCellSizeY*i,
				1, 1, 0xffffffff, 0xff00000, 0, 0 );
			vertices[nStart + i*2 + 1].Setup( fX + fCellSizeX*i + MY_SIZE_X, fY - fCellSizeY*i + MY_SIZE_Y,
				1, 1, 0xffffffff, 0xff00000, 0, 0 );
		}
	}
	pMarkerIndices = pGFX->CreateIndices( 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST, GFXD_DYNAMIC );
	{
		CIndicesLock<WORD> indices( pMarkerIndices );
		indices[0] = 1;
		indices[1] = 2;
		indices[2] = 0;
		indices[3] = 0;
		indices[4] = 2;
		indices[5] = 3;
	}
}

void CGridFrame::ComputeGameTileCoordinates( POINT pt, float &ftX, float &ftY )
{
	float OM = sqrt( (fOX-pt.x)*(fOX-pt.x) + (fOY-pt.y)*(fOY-pt.y) );
	float alpha = asin( 1.0f/sqrt(5) );
	float beta = atan2( pt.y - fOY, pt.x - fOX );
	float fTemp = OM / sin(2*alpha);
	float OC = fTemp * sin(alpha+beta);
	float CM = fTemp * sin(alpha-beta);
	float aiTileRealSize = sqrt( (fCellSizeX/2)*(fCellSizeX/2) + (fCellSizeY/2)*(fCellSizeY/2) );
	ftX = OC/aiTileRealSize;
	ftY = CM/aiTileRealSize;
}

void CGridFrame::GetGameTileCoordinates( int nTileX, int nTileY, float &fX1, float &fY1, float &fX2, float &fY2, float &fX3, float &fY3, float &fX4, float &fY4 )
{
	float alpha = asin( 1.0f/sqrt(5) );
	float aiTileRealSize = sqrt( (fCellSizeX/2)*(fCellSizeX/2) + (fCellSizeY/2)*(fCellSizeY/2) );
	fX2 = fOX + aiTileRealSize*nTileX*cos(alpha) + aiTileRealSize*nTileY*cos(alpha);
	fY2 = fOY + aiTileRealSize*nTileX*sin(alpha) - aiTileRealSize*nTileY*sin(alpha);
	
	fX3 = fOX + aiTileRealSize*(nTileX+1)*cos(alpha) + aiTileRealSize*nTileY*cos(alpha);
	fY3 = fOY + aiTileRealSize*(nTileX+1)*sin(alpha) - aiTileRealSize*nTileY*sin(alpha);
	
	fX4 = fOX + aiTileRealSize*(nTileX+1)*cos(alpha) + aiTileRealSize*(nTileY+1)*cos(alpha);
	fY4 = fOY + aiTileRealSize*(nTileX+1)*sin(alpha) - aiTileRealSize*(nTileY+1)*sin(alpha);
	
	fX1 = fOX + aiTileRealSize*nTileX*cos(alpha) + aiTileRealSize*(nTileY+1)*cos(alpha);
	fY1 = fOY + aiTileRealSize*nTileX*sin(alpha) - aiTileRealSize*(nTileY+1)*sin(alpha);
}

void CGridFrame::SetTileInListOfTiles( CListOfTiles &listOfTiles, int nTileX, int nTileY, int nVal, int nTypeOfTile )
{
	CListOfTiles::iterator it = listOfTiles.begin();
	for ( ; it!=listOfTiles.end(); ++it )
	{
		if ( it->nTileX == nTileX && it->nTileY == nTileY )
		{
			if ( nVal == 0 )
			{
				listOfTiles.erase( it );
				return;
			}
			it->nVal = nVal;
			break;
		}
	}
	
	if ( it == listOfTiles.end() )
	{
		if ( nVal == 0 )
			return;

		SAITile tile;
		tile.nTileX = nTileX;
		tile.nTileY = nTileY;
		tile.nVal = nVal;
		listOfTiles.push_back( tile );
	}
	
	DWORD dwColor = 0x00000000;
	if ( nTypeOfTile == E_TRANSEPARENCE_TILE )
	{
		switch ( nVal )
		{
		case 1:
			dwColor = 0xff202000;
			break;
		case 2:
			dwColor = 0xff404000;
			break;
		case 3:
			dwColor = 0xff606000;
			break;
		case 4:
			dwColor = 0xff808000;
			break;
		case 5:
			dwColor = 0xffa0a000;
			break;
		case 6:
			dwColor = 0xffc0c000;
			break;
		case 7:
			dwColor = 0xffe0e000;
			break;
		default:
			NI_ASSERT( 0 );			//WTF? поддерживается всего 8 значений прозрачности
		}
	}
	else if ( nTypeOfTile == E_ENTRANCE_TILE )
		dwColor = 0xff00ff00;
	else if ( nTypeOfTile == E_LOCKED_TILE )
		dwColor = 0xffff0000;
	else if ( nTypeOfTile == E_UNLOCKED_TILE )
		dwColor = 0xff00ff00;
	
	
	float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
	CPtr<IGFXVertices> pMarkerVertices;
	CGridFrame::GetGameTileCoordinates( nTileX, nTileY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
	pMarkerVertices = pGFX->CreateVertices( 4, SGFXTLVertex::format, GFXPT_TRIANGLELIST, GFXD_DYNAMIC );
	{
		CVerticesLock<SGFXTLVertex> vertices( pMarkerVertices );
		vertices[0].Setup( fx1, fy1, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[1].Setup( fx2, fy2, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[2].Setup( fx3, fy3, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[3].Setup( fx4, fy4, 1, 1, dwColor, 0xff000000, 0, 0 );
	}
	if ( it == listOfTiles.end() )
		listOfTiles.back().pVertices = pMarkerVertices;
	else
		it->pVertices = pMarkerVertices;
}

void CGridFrame::SetTileInListOfNormalTiles( CListOfNormalTiles &listOfTiles, int nTileX, int nTileY, int nVal )
{
	float fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4;
	GetGameTileCoordinates( nTileX, nTileY, fx1, fy1, fx2, fy2, fx3, fy3, fx4, fy4 );
	float fcx = (fx2 + fx4)/2, fcy = (fy2 + fy4)/2;
	float alpha = (float) PI * nVal / 8 - PI;
	IScene *pSG = GetSingleton<IScene>();
	CVec2 vShift;
	vShift.x = fcx;
	vShift.y = fcy;
	CVec3 vPos3;
	pSG->GetPos3( &vPos3, vShift );
	vPos3.x += NORMAL_LENGTH*sin(alpha);
	vPos3.y -= NORMAL_LENGTH*cos(alpha);
	pSG->GetPos2( &vShift, vPos3 );
	DWORD dwColor = 0xff008000;
	
	CListOfNormalTiles::iterator it = listOfTiles.begin();
	for ( ; it!=listOfTiles.end(); ++it )
	{
		if ( it->nTileX == nTileX && it->nTileY == nTileY )
		{
			it->nVal = nVal;
			float alpha = (float) PI * nVal / 8;
			CVerticesLock<SGFXTLVertex> vertices( it->pNormalVertices );
			vertices[0].Setup( fcx, fcy, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
			vertices[1].Setup( vShift.x, vShift.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
			return;
		}
	}
	
	SAINormalTile tile;
	tile.nTileX = nTileX;
	tile.nTileY = nTileY;
	tile.nVal = nVal;
	tile.pNormalVertices = pGFX->CreateVertices( 2, SGFXTLVertex::format, GFXPT_LINELIST, GFXD_DYNAMIC );
	{
		CVerticesLock<SGFXTLVertex> vertices( tile.pNormalVertices );
		vertices[0].Setup( fcx, fcy, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
		vertices[1].Setup( vShift.x, vShift.y, 1, 1, 0xffff0000, 0xff000000, 0, 0 );
	}

	tile.pVertices = pGFX->CreateVertices( 4, SGFXTLVertex::format, GFXPT_TRIANGLELIST, GFXD_DYNAMIC );
	{
		CVerticesLock<SGFXTLVertex> vertices( tile.pVertices );
		vertices[0].Setup( fx1, fy1, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[1].Setup( fx2, fy2, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[2].Setup( fx3, fy3, 1, 1, dwColor, 0xff000000, 0, 0 );
		vertices[3].Setup( fx4, fy4, 1, 1, dwColor, 0xff000000, 0, 0 );
	}
	listOfTiles.push_back( tile );
}

void CGridFrame::DeleteTileInListOfNormalTiles( CListOfNormalTiles &listOfTiles, int nTileX, int nTileY )
{
	CListOfNormalTiles::iterator it = listOfTiles.begin();
	for ( ; it!=listOfTiles.end(); ++it )
	{
		if ( it->nTileX == nTileX && it->nTileY == nTileY )
		{
			listOfTiles.erase( it );
			return;
		}
	}
}

bool CGridFrame::SaveIconFile( const char *pszSrc, const char *pszRes )
{
	IImageProcessor *pIP = GetSingleton<IImageProcessor>();
	bool bErr = true;
	do
	{
		CPtr<IDataStream> pSrcStream = OpenFileStream( pszSrc, STREAM_ACCESS_READ );
		if ( pSrcStream == 0 )
			break;
		CPtr<IImage> pSrcImage = pIP->LoadImage( pSrcStream );
		if ( pSrcImage == 0 )
			break;

		int nSizeX = pSrcImage->GetSizeX();
		int nSizeY = pSrcImage->GetSizeY();
		int nMinX = nSizeX, nMinY = -1;
		int nMaxX = 0, nMaxY = 0;
		SColor *pLFB = pSrcImage->GetLFB();
		for ( int y=0; y<nSizeY; y++ )
		{
			int nCurMinX = -1;
			int nCurMaxX = 0;

			for ( int x=0; x<nSizeX; x++ )
			{
				const SColor &cur = pLFB[x+y*nSizeX];
				if ( cur.a )
				{
					nCurMaxX = x;
					if ( nCurMinX == -1 )
						nCurMinX = x;
				}
			}
			if ( nCurMinX >= 0 && nCurMinX < nMinX )
				nMinX = nCurMinX;
			if ( nCurMaxX > nMaxX )
				nMaxX = nCurMaxX;
			if ( nCurMaxX > 0 )
			{
				nMaxY = y;
				if ( nMinY == -1 )
					nMinY = y;
			}
		}

		if ( nMinY == -1 )
		{
			std::string szErr = "Error: image alpha is empty?\n";
			szErr += pszSrc;
			szErr += "\nCan not create icon image";
			AfxMessageBox( szErr.c_str() );
			return false;
		}

		CPtr<IImage> pMinImage;
		if ( nMaxX - nMinX != nSizeX || nMaxY - nMinY != nSizeY )
		{
			pMinImage = pIP->CreateImage( nMaxX - nMinX, nMaxY - nMinY );
			SColor col;
			col.r = col.g = col.b = 146;
			col.a = 0;
			pMinImage->Set( col );
			RECT rc = { nMinX, nMinY, nMaxX, nMaxY };
			pMinImage->CopyFromAB( pSrcImage, &rc, 0, 0 );
		}
		
		nSizeX = pMinImage->GetSizeX();
		nSizeY = pMinImage->GetSizeY();
		double fRateX = (double) ICON_SIZE/nSizeX;
		double fRateY = (double) ICON_SIZE/nSizeY;
		double fRate = Min( fRateX, fRateY );
		CPtr<IImage> pScaleImage = pIP->CreateScale( pMinImage, fRate, ISM_LANCZOS3 );
		NI_ASSERT( pScaleImage != 0 );

		CPtr<IImage> pResImage = pIP->CreateImage( ICON_SIZE, ICON_SIZE );
		NI_ASSERT( pResImage != 0 );
		SColor col;
		col.r = col.g = col.b = 146;
		col.a = 0;
		pResImage->Set( col );

		nSizeX = pScaleImage->GetSizeX();
		nSizeY = pScaleImage->GetSizeY();

		if ( nSizeY < ICON_SIZE )
		{
			int nUp = (ICON_SIZE - nSizeY)/2;
			RECT rc = { 0, 0, nSizeX, nSizeY };
			pResImage->CopyFrom( pScaleImage, &rc, 0, nUp );
		}
		else if ( nSizeX < ICON_SIZE )
		{
			int nLeft = (ICON_SIZE - nSizeX)/2;
			RECT rc = { 0, 0, nSizeX, nSizeY };
			pResImage->CopyFrom( pScaleImage, &rc, nLeft, 0 );
		}
		else
			pResImage = pScaleImage;		//равного размера

		CPtr<IDataStream> pResStream = CreateFileStream( pszRes, STREAM_ACCESS_WRITE );
		if ( pResStream == 0 )
			break;
		pIP->SaveImageAsTGA( pResStream, pResImage );
		bErr = false;
	} while ( 0 );

	if ( bErr )
	{
		std::string szErr = "Error: can not create icon file\n";
		szErr += pszSrc;
		szErr += "\nFrom file\n";
		szErr += pszRes;
		AfxMessageBox( szErr.c_str() );
		return false;
	}

	return true;
}
