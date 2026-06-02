#include "StdAfx.h"

#include "..\Scene\ParticleSourceData.h"
#include "..\Misc\Win32Helper.h"
bool SParticleSourceData::Load( const bool bPreLoad )
{
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_SLOW_TF( pStream != 0, NStr::Format("Can't open stream \"%s\" to load particle effect", szStreamName.c_str()), return false );
	if ( pStream == 0 )
		return false;
	CTreeAccessor saver = CreateDataTreeSaver( pStream , IDataTree::READ );	
	saver.Add( "KeyData", this );
	return true;
}
SParticleSourceData::SParticleSourceData() 
: bComplexParticleSource( false )
{
}
int SParticleSourceData::operator&( IStructureSaver &ss )
{
	return 0;
}
int SParticleSourceData::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	if ( saver.IsReading() )
	{
		fRadialWind = 0;
		nAreaType = 0;
	}
	saver.Add( "lifeTime", &trackLife );
	saver.Add( "LifeTime", &nLifeTime );
	saver.Add( "Gravity", &fGravity );
	saver.Add( "TextureDX", &nTextureDX );
	saver.Add( "TextureDY", &nTextureDY );
	saver.Add( "GenerateArea", &trackGenerateArea );
	saver.Add( "Density", &trackDensity );
	saver.Add( "BeginSpeed", &trackBeginSpeed );
	saver.Add( "BeginSpeedRandomizer", &trackBeginSpeedRandomizer );
	saver.Add( "GenerateAngel", &trackBeginAngleRandomizer );
	saver.Add( "ParticleLifeTimeRandomizer", &trackLifeRandomizer );
	saver.Add( "GenerateSpin", &trackGenerateSpin );
	saver.Add( "GenerateSpinRnd", &trackGenerateSpinRandomizer );
	saver.Add( "GenerateOpacity", &trackGenerateOpacity );
	saver.Add( "Spin", &trackSpin );
	saver.Add( "Wight", &trackWeight );
	saver.Add( "TextureFrame", &trackTextureFrame );
	saver.Add( "Size", &trackSize );
	saver.Add( "Opacity", &trackOpacity );
	saver.Add( "TextureName", &szTextureName );
	saver.Add( "Wind", &vWind );
	saver.Add( "Speed", &trackSpeed );
	saver.Add( "SpeedRnd", &trackSpeedRnd );
	saver.Add( "Direction", &vDirection );
	saver.Add( "AreaType", &nAreaType );
	saver.Add( "RadialWind", &fRadialWind );
	saver.Add( "ComplexParticleSource", &bComplexParticleSource );
	if ( saver.IsReading() )
	{
		Init();
		fDensityCoeff = GetGlobalVar( "Options.GFX.DensityCoeff", 100.0f ) / 100.0f;
	}
	return 0;
}
void SParticleSourceData::SwapData( ISharedResource *pResource )
{
	SParticleSourceData *pRes = dynamic_cast<SParticleSourceData*>( pResource );
	NI_ASSERT_TF( pRes != 0, "shared resource is not a SParticleSourceData", return );
	std::swap( trackGenerateSpin, pRes->trackGenerateSpin );
	std::swap( trackLife, pRes->trackLife );
	std::swap( trackDensity, pRes->trackDensity );
	std::swap( trackWeight, pRes->trackWeight );
	std::swap( trackGenerateArea, pRes->trackGenerateArea );
	std::swap( trackBeginSpeed, pRes->trackBeginSpeed );
	std::swap( trackSize, pRes->trackSize );
	std::swap( nTextureDX, pRes->nTextureDX );
	std::swap( nTextureDY, pRes->nTextureDY );
	std::swap( fGravity, pRes->fGravity );
	std::swap( trackGenerateOpacity, pRes->trackGenerateOpacity );
	std::swap( trackOpacity, pRes->trackOpacity );
	std::swap( nLifeTime, pRes->nLifeTime );
	std::swap( trackTextureFrame, pRes->trackTextureFrame );
	std::swap( trackBeginSpeedRandomizer, pRes->trackBeginSpeedRandomizer );
	std::swap( vDirection, pRes->vDirection );
	std::swap( trackBeginAngleRandomizer, pRes->trackBeginAngleRandomizer );
	std::swap( trackLifeRandomizer, pRes->trackLifeRandomizer );
	std::swap( trackGenerateSpinRandomizer, pRes->trackGenerateSpinRandomizer );
	std::swap( trackSpin, pRes->trackSpin );
	std::swap( trackSpeed, pRes->trackSpeed );
	std::swap( trackSpeedRnd, pRes->trackSpeedRnd );
	std::swap( vWind, pRes->vWind );
	std::swap( szTextureName, pRes->szTextureName );
	std::swap( nAreaType, pRes->nAreaType );
	std::swap( fRadialWind, pRes->fRadialWind );
	std::swap( trackIntegralMass, pRes->trackIntegralMass );
	std::swap( fDensityCoeff, pRes->fDensityCoeff );
	std::swap( bComplexParticleSource, pRes->bComplexParticleSource );
}
void SParticleSourceData::Init()
{
	if ( trackBeginSpeedRandomizer.IsEmpty() )
	{
		trackBeginSpeedRandomizer.AddKey( 0, 0 );
		trackBeginSpeedRandomizer.AddKey( 1, 0 );
	}
	if ( trackSpeedRnd.IsEmpty() )
	{
		trackSpeedRnd.AddKey( 0, 0 );
		trackSpeedRnd.AddKey( 1, 0 );
	}
	if ( trackLifeRandomizer.IsEmpty() )
	{
		trackLifeRandomizer.AddKey( 0, 0 );
		trackLifeRandomizer.AddKey( 1, 0 );
	}
	if ( trackGenerateSpinRandomizer.IsEmpty() )
	{
		trackGenerateSpinRandomizer.AddKey( 0, 0 );
		trackGenerateSpinRandomizer.AddKey( 1, 0 );
	}
	if ( trackTextureFrame.IsEmpty() )
	{
		trackTextureFrame.AddKey( 0, 0 );
		trackTextureFrame.AddKey( 1, 0 );
	}
	trackSpeed.AddKey( trackSpeed.GetTimeByIndex( 1 ) * 0.5, trackSpeed.GetValue( trackSpeed.GetTimeByIndex( 1 ) * 0.5 ) );
	trackSpeed.RemoveKey( 0 );
	trackSpeed.AddKey( 0, 1 );
	Normalize( &vDirection );
	InitIntegrals();
}
void SParticleSourceData::InitIntegrals()
{	
	CTrack trackIndIntegral;
	trackIndIntegral.AddKey( 0, 0 );
	STrackContext context;
	trackWeight.CreateStartContext( &context );
	float lastValue = 0;
	float lastTime = 0;
	for ( int i = 1; i < trackWeight.GetNumKeys(); i++ )
	{
		lastValue += trackWeight.Integrate( &context, (lastTime + trackWeight.GetTimeByIndex(i)) * 0.5 );
		trackIndIntegral.AddKey( (lastTime + trackWeight.GetTimeByIndex(i)) * 500, fGravity * lastValue );
		lastTime = trackWeight.GetTimeByIndex(i);
		lastValue += trackWeight.Integrate( &context, lastTime );
		trackIndIntegral.AddKey( lastTime * 1000, fGravity * lastValue );
	}
	trackIndIntegral.Normalize( 1 );
	trackIntegralMass.Clear();
	lastTime = 0;
	for ( int i = 0, j = 0; i < trackIndIntegral.GetNumKeys() && j < trackSpeed.GetNumKeys(); )
	{
		const float t1 = trackIndIntegral.GetTimeByIndex(i);
		const float t2 = trackSpeed.GetTimeByIndex(j);
		if ( t1 <= t2 )
		{
			const float fMiddleTime = (t1 + lastTime) * 0.5;
			trackIntegralMass.AddKey( fMiddleTime * 1000, trackIndIntegral.GetValue( fMiddleTime ) * trackSpeed.GetValue( fMiddleTime ) );
			trackIntegralMass.AddKey( t1 * 1000, trackIndIntegral.GetValue( t1 ) * trackSpeed.GetValue( t1 ) );
			i++;
			lastTime = t1;
			if ( t1 == t2 )
				j++;
		}
		else
		{
			const float fMiddleTime = (t2 + lastTime) * 0.5;
			trackIntegralMass.AddKey( fMiddleTime * 1000, trackIndIntegral.GetValue( fMiddleTime ) * trackSpeed.GetValue( fMiddleTime ) );
			trackIntegralMass.AddKey( t2 * 1000, trackIndIntegral.GetValue( t2 ) * trackSpeed.GetValue( t2 ) );
			j++;
			lastTime = t2;
		}
	}
	trackIntegralMass.Normalize( 1 );
}
