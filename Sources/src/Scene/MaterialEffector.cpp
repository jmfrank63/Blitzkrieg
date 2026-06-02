#include "StdAfx.h"

#include "MaterialEffector.h"
void CMaterialEffector::SetupData( BYTE bMaxAlpha, DWORD dwMaxSpecular )
{
	bAlpha = bMaxAlpha;
	dwSpecular = dwMaxSpecular;
}
BYTE CMaterialEffector::GetAlpha() const
{
	return ( 0xFF - BYTE(( 0xFF - bAlpha ) * fCoeff) ) ;
}
DWORD CMaterialEffector::GetSpecular() const
{
	DWORD result = 0xFF000000;
	result |= DWORD( (dwSpecular & 0x00FF0000) * fCoeff ) & 0x00FF0000;
	result |= DWORD( (dwSpecular & 0x0000FF00) * fCoeff ) & 0x0000FF00;
	result |= DWORD( ( dwSpecular & 0x000000FF ) * fCoeff );
	return result;
}
void CMaterialEffector::SetupTimes( const NTimer::STime &timeStart, const NTimer::STime &timeLife )
{
	nStartTime = timeStart;
	nDuration = timeLife;
	fCoeff = 0.0f;
}
bool CMaterialEffector::Update( const NTimer::STime &time )
{
	if ( time >= nStartTime + nDuration )
			return false;
	if ( time > nStartTime + nDuration * 0.5f )
		fCoeff = 2.0f - (time - nStartTime) / (nDuration * 0.5f);
	else
		fCoeff = (time - nStartTime) / (nDuration * 0.5f) ;
	return true;
}
int CMaterialEffector::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nStartTime );
	saver.Add( 2, &nDuration );
	saver.Add( 3, &fCoeff );
	saver.Add( 4, &dwSpecular );
	saver.Add( 5, &bAlpha );
	return 0;
}


