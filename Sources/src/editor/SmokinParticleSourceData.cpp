#include "StdAfx.h"

#include "../Scene/SmokinParticleSourceData.h"
#include "../Scene/PFX.h"
bool SSmokinParticleSourceData::Load( const bool bPreLoad )
{
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_SLOW_TF( pStream != 0, NStr::Format("Can't open stream \"%s\" to load smokin particle effect", szStreamName.c_str()), return false );
	if ( pStream == 0 )
		return false;
	CTreeAccessor saver = CreateDataTreeSaver( pStream , IDataTree::READ );	
	saver.Add( "KeyData", this );
	return true;
}
SSmokinParticleSourceData::SSmokinParticleSourceData() 
: bComplexParticleSource( true )
{
}
int SSmokinParticleSourceData::operator&( IStructureSaver &ss )
{
	return 0;
}
int SSmokinParticleSourceData::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "LifeTime", &nLifeTime );
	saver.Add( "Gravity", &fGravity );
	saver.Add( "GenerateArea", &trackGenerateArea );
	saver.Add( "Density", &trackDensity );
	saver.Add( "BeginSpeed", &trackBeginSpeed );
	saver.Add( "BeginSpeedRandomizer", &trackBeginSpeedRandomizer );
	saver.Add( "BeginAngleRandomizer", &trackBeginAngleRandomizer );
	saver.Add( "Weight", &trackWeight );
	saver.Add( "Wind", &vWind );
	saver.Add( "Speed", &trackSpeed );
	saver.Add( "SpeedRnd", &trackSpeedRnd );
	saver.Add( "Direction", &vDirection );
	saver.Add( "AreaType", &nAreaType );
	saver.Add( "RadialWind", &fRadialWind );
	saver.Add( "ParticleEffect", &szParticleEffectName );
	saver.Add( "ComplexParticleSource", &bComplexParticleSource );
	Normalize( &vDirection );
	if ( saver.IsReading() )
	{
		InitIntegrals();
		fDensityCoeff = GetGlobalVar( "Options.GFX.DensityCoeff", 100.0f ) / 100.0f;
	}
	return 0;
}
void SSmokinParticleSourceData::SwapData( ISharedResource *pResource )
{
	SSmokinParticleSourceData *pRes = dynamic_cast<SSmokinParticleSourceData*>( pResource );
	NI_ASSERT_TF( pRes != 0, "shared resource is not a SSmokinParticleSourceData", return );
	std::swap( trackDensity, pRes->trackDensity );
	std::swap( trackWeight, pRes->trackWeight );
	std::swap( trackGenerateArea, pRes->trackGenerateArea );
	std::swap( trackBeginSpeed, pRes->trackBeginSpeed );
	std::swap( fGravity, pRes->fGravity );
	std::swap( nLifeTime, pRes->nLifeTime );
	std::swap( trackBeginSpeedRandomizer, pRes->trackBeginSpeedRandomizer );
	std::swap( vDirection, pRes->vDirection );
	std::swap( trackBeginAngleRandomizer, pRes->trackBeginAngleRandomizer );
	std::swap( trackSpeed, pRes->trackSpeed );
	std::swap( trackSpeedRnd, pRes->trackSpeedRnd );
	std::swap( vWind, pRes->vWind );
	std::swap( nAreaType, pRes->nAreaType );
	std::swap( fRadialWind, pRes->fRadialWind );
	std::swap( szParticleEffectName, pRes->szParticleEffectName );
	std::swap( fDensityCoeff, pRes->fDensityCoeff );
	std::swap( bComplexParticleSource, pRes->bComplexParticleSource );
	std::swap( trackIntegralMass, pRes->trackIntegralMass );
	std::swap( nUpdateStep, pRes->nUpdateStep );
}
void SSmokinParticleSourceData::InitIntegrals()
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
	lastTime = 0;
	trackIntegralMass.Clear();
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
	CPtr<IParticleSource> pSource = GetSingleton<IParticleManager>()->GetKeyBasedSource( (szParticleEffectName + ".xml").c_str() );
	nUpdateStep = pSource->GetOptimalUpdateTime();
}






