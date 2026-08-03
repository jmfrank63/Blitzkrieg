#include "StdAfx.h"
#include "KeyBasedData.h"
int SParticleSetup::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &trackLife );
	saver.Add( 2, &trackDensity );
	saver.Add( 3, &trackWeight );
	saver.Add( 4, &trackSpeed );
	saver.Add( 5, &trackGenerateArea );
	saver.Add( 6, &trackBeginSpeed );
	saver.Add( 7, &trackSize );
	saver.Add( 8, &trackGenerateAngle );
	saver.Add( 9, &trackSpin );
	saver.Add( 10, &trackGenerateSpin );
	saver.Add( 11, &vWind );
	saver.Add( 12, &vDirection );
	saver.Add( 13, &vPosition );
	saver.Add( 14, &fGravity );
	saver.Add( 15, &nTextureDY );
	saver.Add( 16, &nTextureDX );
	saver.Add( 17, &nGenerateAngel );
	saver.Add( 18, &szTextureName );

	saver.Add( 19, &trackGenerateOpacity );
	saver.Add( 20, &trackOpacity );
	saver.Add( 21, &nLifeTime );
	saver.Add( 22, &trackGenerateSpinRand );
	
	saver.Add( 23, &trackTextureFrame );
	return 0;
}
int SParticleSetup::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "lifeTime", &trackLife );
	saver.Add( "Density", &trackDensity );
	saver.Add( "Wight", &trackWeight );
	saver.Add( "Speed", &trackSpeed );
	saver.Add( "GenerateArea", &trackGenerateArea );
	saver.Add( "BeginSpeed", &trackBeginSpeed );
	saver.Add( "Size", &trackSize );
	saver.Add( "GenerateAngel", &trackGenerateAngle );
	saver.Add( "Spin", &trackSpin );
	saver.Add( "GenerateSpin", &trackGenerateSpin );
	saver.Add( "Wind", &vWind );
	saver.Add( "Direction", &vDirection );
	saver.Add( "Position", &vPosition );
	saver.Add( "Gravity", &fGravity );
	saver.Add( "TextureDY", &nTextureDY );
	saver.Add( "TextureDX", &nTextureDX );
	saver.Add( "GenerateAngel", &nGenerateAngel );
	saver.Add( "TextureName", &szTextureName );
	saver.Add( "GenerateOpacity", &trackGenerateOpacity );
	saver.Add( "Opacity", &trackOpacity );
	saver.Add( "LifeTime", &nLifeTime );
	saver.Add( "GenerateSpinRand", &trackGenerateSpinRand );
	saver.Add( "TextureFrame", &trackTextureFrame );
	return 0;
}
int CKeyBasedData::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 
	saver.Add( "KeyData",&keyData );	
	return 0;
}

