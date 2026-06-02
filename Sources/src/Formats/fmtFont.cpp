#include "StdAfx.h"

#include "fmtFont.h"
int SFontFormat::SFontMetrics::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add(  1, &nHeight );
	saver.Add(  2, &nAscent );
	saver.Add(  3, &nDescent );
	saver.Add(  4, &nInternalLeading );
	saver.Add(  5, &nExternalLeading );
	saver.Add(  6, &nAveCharWidth );
	saver.Add(  7, &nMaxCharWidth );
	saver.Add(  8, &fSpaceWidth );
	saver.Add(  9, &cCharSet );
	saver.Add( 10, &wDefaultChar );
	return 0;
}
int SFontFormat::SCharDesc::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &x1 );
	saver.Add( 2, &y1 );
	saver.Add( 3, &x2 );
	saver.Add( 4, &y2 );
	saver.Add( 5, &fA );
	saver.Add( 6, &fB );
	saver.Add( 7, &fC );
	saver.Add( 8, &fWidth );
	return 0;
}
int SFontFormat::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szFaceName );
	saver.Add( 2, &metrics );
	saver.Add( 3, &chars );
	saver.Add( 4, &kerns );
	return 0;
}
