#include "StdAfx.h"

#include "SampleSounds.h"
int CSound2D::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.AddTypedSuper( 1, static_cast<CBaseSound*>(this) );
	saver.Add( 2, &fVolume );
	saver.Add( 3, &fPan );

	return 0;
}
int CSound3D::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CBaseSound*>(this) );

	saver.Add( 2, &vPos );
	saver.Add( 3, &bDopplerFlag );
	saver.Add( 4, &lastUpdateTime );
	saver.Add( 5, &vLastPos );
	return 0;

}
int CBaseSound::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &pSample );
	saver.Add( 2, &nChannel );

	return 0;

}
