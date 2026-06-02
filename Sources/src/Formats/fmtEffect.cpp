#include "StdAfx.h"

#include "fmtEffect.h"
int SSpriteEffectDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "path", &szPath );
	saver.Add( "start", &nStart );
	saver.Add( "repeat", &nRepeat );
	saver.Add( "pos", &vPos );
	return 0;
};
int SParticleEffectDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	if ( saver.IsReading() )
	{
		fScale = 1.0f;
	}
	saver.Add( "path", &szPath );
	saver.Add( "start", &nStart );
	saver.Add( "duration", &nDuration );
	saver.Add( "pos", &vPos );
	saver.Add( "scale", &fScale );
	return 0;
};
int SSmokinParticleEffectDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	if ( saver.IsReading() )
	{
		fScale = 1.0f;
	}
	saver.Add( "path", &szPath );
	saver.Add( "start", &nStart );
	saver.Add( "duration", &nDuration );
	saver.Add( "pos", &vPos );
	saver.Add( "scale", &fScale );
	return 0;
};
int SEffectDesc::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "sprites", &sprites );
	saver.Add( "particles", &particles );
	saver.Add( "SmokinParticles", &smokinParticles );
	saver.Add( "sound", &szSound );
	return 0;
}
