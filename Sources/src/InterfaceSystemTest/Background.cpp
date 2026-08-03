
#include "StdAfx.h"
#include "Background.h"

#include "..\Scene\Scene.h"
BASIC_REGISTER_CLASS(IBackground)
IMPLEMENT_CLONABLE(CBackgroundPlainTexture)
int CBackground::operator&( interface IDataTree &ss ) 
{ 
	CTreeAccessor saver = &ss;
	std::string szTextureName;
	saver.Add( "Texture", &szTextureName );
	saver.Add( "Color", &color );
	saver.Add( "Specular", &specular );
	
	if ( szTextureName.empty() )
		pTexture = 0;
	else
		pTexture = GetSingleton<ITextureManager>()->GetTexture( szTextureName.c_str() );
	return 0;
}
void CBackground::SetPos( const CVec2 &vPos, const CVec2 &vSize )
{
	pos.left = vPos.x;
	pos.top = vPos.y;
	pos.bottom = vPos.y + vSize.y;
	pos.right = vPos.x + vSize.x;
}
int CBackground::operator&( interface IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;

}
int CBackgroundPlainTexture::operator&( interface IDataTree &ss ) 
{ 
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CBackground*>(this) );
	
	return 0;
}
int CBackgroundPlainTexture::operator&( interface IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;

}
CBackgroundPlainTexture::CBackgroundPlainTexture()
{

}
void CBackgroundPlainTexture::Visit( interface ISceneVisitor * pVisitor )
{
	SGFXRect2 *pRects = GetTempBuffer<SGFXRect2>( 1 );
	SGFXRect2 &rc = pRects[0];
	rc.rect = pos;
	rc.maps.x1 = 0;
	rc.maps.y1 = 0;
	rc.maps.x2 = 1;
	rc.maps.y2 = 1;
	
	rc.color = color;
	rc.specular = specular;
	rc.fZ = 0;

	pVisitor->VisitUIRects( pTexture, 3, pRects, 1 );
}
IMPLEMENT_CLONABLE(CBackgroundTiledTexture)
int CBackgroundTiledTexture::SSubRect::operator&( interface IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "size", &vSize );
	saver.Add( "map", &maps );
	saver.Add( "rotation", &nRotate );
	return 0;
}
int CBackgroundTiledTexture::operator&( interface IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CBackground*>(this) );
	
	saver.Add( "LT", &rLT );
	saver.Add( "RT", &rRT );
	saver.Add( "LB", &rLB );
	saver.Add( "RB", &rRB );

	saver.Add( "T", &rT );
	saver.Add( "B", &rB );
	saver.Add( "L", &rL );
	saver.Add( "R", &rR );
	
	saver.Add( "F", &rF );
	
	if ( saver.IsReading() )
	{
		InitTiles( &rLT );
		InitTiles( &rRT );
		InitTiles( &rLB );
		InitTiles( &rRB );

		InitTiles( &rT );
		InitTiles( &rB );
		InitTiles( &rL );
		InitTiles( &rR );
		
		InitTiles( &rF );
		InitBorderAndFill();
	}	
	return 0;
}
void CBackgroundTiledTexture::DivideSubrects( const SSubRect &in, std::vector<SGFXRect2> *pArr )
{
	SGFXRect2 sub;
	sub.color = color;
	sub.fZ = 0;
	sub.specular = specular;

	sub.maps.y1 = in.maps.y1;
	for ( int y = in.rect.y1; y < in.rect.y2; y += in.vSize.y )
	{
		sub.rect.y1 = y;
		if ( y + in.vSize.y > in.rect.y2 )
		{
			sub.rect.y2 = in.rect.y2;
			const float k = sub.rect.Height() / in.vSize.y;
			sub.maps.y2 = ( in.maps.y1 + k * in.maps.Height() ) ;
		}
		else
		{
			sub.rect.y2 = y + in.vSize.y;
			sub.maps.y2 = in.maps.y2;
		}

		sub.maps.x1 = ( in.maps.x1 ) ;
		for ( int x = in.rect.x1; x < in.rect.x2; x += in.vSize.x )
		{
			sub.rect.x1 = x;

			if ( x + in.vSize.x > in.rect.x2 )
			{
				sub.rect.x2 = in.rect.x2;
				const float k = sub.rect.Width() / in.vSize.x;
				sub.maps.x2 = ( in.maps.x1 + k * in.maps.Width()  );// / in.vSize.x;
			}
			else
			{
				sub.rect.x2 = x + in.vSize.x;
				sub.maps.x2 = (  in.maps.x2 );
			}

			pArr->push_back( sub );
		}
	}
}
#define _X(v) CTPoint<float>(v.vSize.x,0)
#define _Y(v) CTPoint<float>(0,v.vSize.y)
void CBackgroundTiledTexture::InitBorderAndFill()
{
	SGFXRect2 *pRects = GetTempBuffer<SGFXRect2>( 9 );
	
	rLT.rect.Set( pos.GetLeftTop(), pos.GetLeftTop() + rLT.vSize );
	rRT.rect.Set( pos.GetRightTop() - _X(rRT), pos.GetRightTop() + _Y(rRT) );
	rLB.rect.Set( pos.GetLeftBottom() - _Y(rLB), pos.GetLeftBottom() + _X(rLB) );
	rRB.rect.Set( pos.GetRightBottom() - rRB.vSize, pos.GetRightBottom() );

	rT.rect.Set( pos.GetLeftTop() + _X(rLT), pos.GetRightTop() - _X(rRT) + _Y(rRT) );
	rB.rect.Set( pos.GetLeftBottom() + _X(rLB) - _Y(rLB), pos.GetRightBottom() - _X(rRB) );
	rL.rect.Set( pos.GetLeftTop() + _Y(rRT), pos.GetLeftBottom() - _Y(rLB) + _X(rLB) );
	rR.rect.Set( pos.GetRightTop() + _Y(rRT) - _X(rRT), pos.GetRightBottom() - _Y(rRB) );

	rF.rect.Set( pos.GetLeftTop() + rLT.vSize, pos.GetRightBottom() - rRT.vSize );
}
void CBackgroundTiledTexture::InitTiles( SSubRect *pSub )
{
	float fSizeX = pTexture->GetSizeX( 0 );
	float fSizeY = pTexture->GetSizeY( 0 );
		
	CTRect<float> maps;
	maps.x1 = ( pSub->maps.x1 + 0.5f ) / fSizeX;
	maps.x2 = ( pSub->maps.x1 + pSub->maps.x2 + 0.5f ) / fSizeX;
	maps.y1 = ( pSub->maps.y1 + 0.5f ) / fSizeY;
	maps.y2 = ( pSub->maps.y1 + pSub->maps.y2 + 0.5f ) / fSizeY;

	pSub->maps = maps;
}
void CBackgroundTiledTexture::SetPos( const CVec2 &vPos, const CVec2 &vSize )
{
	CBackground::SetPos( vPos, vSize );
	InitBorderAndFill();
	rects.clear();

	DivideSubrects( rLT, &rects );
	DivideSubrects( rT, &rects );
	
	DivideSubrects( rRT, &rects );
	DivideSubrects( rL, &rects );
	DivideSubrects( rF, &rects );
	DivideSubrects( rR, &rects );
	DivideSubrects( rLB, &rects );
	DivideSubrects( rB, &rects );
	DivideSubrects( rRB, &rects );
}
void CBackgroundTiledTexture::Visit( interface ISceneVisitor * pVisitor )
{
	if ( !rects.empty() )
	{
		pVisitor->VisitUIRects( pTexture, 3, &rects[0], rects.size() );
	}
}
int CBackgroundTiledTexture::operator&( interface IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
CBackgroundTiledTexture::CBackgroundTiledTexture()
{
}
