#include "StdAfx.h"

#include "FlashVisObj.h"
int CFlashVisObj::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pTexture );
	saver.Add( 2, &timeStart );
	saver.Add( 3, &timeDuration );
	saver.Add( 4, &spriteInfo.pos );
	saver.Add( 5, &spriteInfo.color );
	saver.Add( 6, &spriteInfo.rect );
	saver.Add( 7, &spriteInfo.maps );
	saver.Add( 8, &dwAlpha );
	if ( saver.IsReading() ) 
		spriteInfo.pTexture = pTexture;
	return 0;
}
void CFlashVisObj::Visit( interface ISceneVisitor *pVisitor, int nType )
{
	pVisitor->VisitSprite( &spriteInfo, SGVOGT_FLASH, 0 );
}
bool CFlashVisObj::Update( const NTimer::STime &time, bool bForced )
{
	if ( time < timeStart ) 
		return true;
	const NTimer::STime timeDiff = time - timeStart;
	if ( timeDiff > timeDuration ) 
		return false;
	const NTimer::STime timeRaise = timeDuration / 10;
	if ( timeDiff <= timeRaise ) 
		SetAlpha( dwAlpha * timeDiff / timeRaise );
	else
		SetAlpha( dwAlpha - dwAlpha*( timeDiff - timeRaise )/( timeDuration - timeRaise ) );
	return true;
}
