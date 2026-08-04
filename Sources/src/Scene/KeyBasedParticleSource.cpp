#include "StdAfx.h"

#include "KeyBasedParticleSource.h"
#include "../Misc/Win32Random.h"
#include "FastSinCos.h"
IGFXTexture* CKeyBasedParticleSource::GetTexture() const
{
	return pTexture;
}
const CVec3 CKeyBasedParticleSource::GetPos() const
{
	return vPosition;
}
void CKeyBasedParticleSource::SetPos( const CVec3 &vPos )
{
	vPosition = vPos;
}
const CVec3 CKeyBasedParticleSource::GetDirection() const
{
	return vDirection;
}
void CKeyBasedParticleSource::SetDirection( const SHMatrix &mDir )
{
	mDir.RotateVector( &vDirection, V3_AXIS_Z );
	CVec3 vDir;
	mDir.RotateVector( &vDir, pData->vDirection );
	Normalize( &vDir );
	fDirectionTheta = acos( vDir.z );
	const float direction_x = static_cast<float>( vDir.x / sin( fDirectionTheta ) );
	fDirectionPhi = fDirectionTheta == 0 ? 0 : ( vDir.y > 0 ? acos( Clamp<float>( direction_x, -1.0f, 1.0f ) ) : PI * 2 - acos( Clamp<float>( direction_x, -1.0f, 1.0f ) ) );
}
void CKeyBasedParticleSource::SetStartTime( const NTimer::STime &time )
{
	nStartTime = time;
	nLastUpdateTime = time;
	nLastParticleUpdate = time;
	lastError = 0;
}
const NTimer::STime CKeyBasedParticleSource::GetStartTime() const
{
	return nStartTime;
}
const NTimer::STime CKeyBasedParticleSource::GetEffectLifeTime() const
{
	return MINT( float( pData->nLifeTime ) + pData->trackLife.GetValue(1.0f) );
}
bool CKeyBasedParticleSource::IsFinished() const
{
	return particles.empty() && ( nLastUpdateTime > nStartTime + pData->nLifeTime || bStopped );
}
const int CKeyBasedParticleSource::GetNumParticles() const
{
	return particles.size();
}
void CKeyBasedParticleSource::FillParticleBuffer( SSimpleParticle *buff ) const
{
	for ( std::list<SExtendedParticle>::const_iterator it = particles.begin(); it != particles.end() ; it++  )
	{
		*buff = static_cast<const SSimpleParticle &>(*it);
		++buff;
	}
}
void CKeyBasedParticleSource::GetInfo( SParticleSourceInfo &info )
{
	SetStartTime( 0 );
	float nMaxCount = 0;
	float nMaxSize = 0;
	float	nAllCount = 0;
	float	nAllSize = 0;
	for ( int i = 0; i < GetEffectLifeTime(); i += 64 )
	{
		float area = 0;
		Update( i );
		nMaxCount = Max( nMaxCount, float( GetNumParticles() ) );
		for ( std::list<SExtendedParticle>::const_iterator it = particles.begin(); it != particles.end(); ++it )
		{
			area += it->fSize * it->fSize;
		}
		nMaxSize = Max( nMaxSize, area );
		nAllCount += GetNumParticles();
		nAllSize += area;
	}	
	info.fMaxCount = nMaxCount;
	info.fMaxSize = nMaxSize / (  800.0f * 600.0f );
	info.fAverageSize = ( nAllSize * 64.0f) / ( GetEffectLifeTime() * 800.0f * 600.0f );
	info.fAverageCount = ( nAllCount  * 64.0f )/ GetEffectLifeTime() ;
}
void CKeyBasedParticleSource::Update( const NTimer::STime &time )
{
	NI_ASSERT_SLOW_T( pData != 0, "Updating uninitialized particle source!" );
	const int dt = time - nLastUpdateTime;
	if ( dt > 64 && pData->nLifeTime + nStartTime > time )
	{
		const int nStep = 16; // �.� �������� �  ������������ � 16 ��������.
		for ( int i = 0; i < dt; i += nStep )
		{
			const float fRelTime = (nLastUpdateTime + i - nStartTime) / float(pData->nLifeTime);
			const float fTempVal = ( lastError + pData->trackDensity.Integrate( &contextDensity, fRelTime ) * pData->nLifeTime ) * pData->fDensityCoeff;
			int nNumForGenerating = MINT( fTempVal );
			lastError = fTempVal - nNumForGenerating;
			CParticleGenerator::ResetGenerator( nNumForGenerating );
			while ( nNumForGenerating > 0 && !bStopped && !bSuspended )
			{
				SExtendedParticle part;
				part.birthTime = nLastUpdateTime + i; 
				part.deathTime = part.birthTime + pData->trackLife.GetValue( fRelTime, pData->trackLifeRandomizer );
				part.fOpacity = pData->trackGenerateOpacity.GetValue( fRelTime );
				part.color.color = 0xFFFFFFFF;
				part.color.a = BYTE( part.fOpacity * pData->trackOpacity.GetValue(0) );
				part.fSize = pData->trackSize.GetValue( 0 ) * fScale;
				part.rcMaps = rcRects[ MINT( pData->trackTextureFrame.GetValue(0) * rcRects.size() ) % rcRects.size() ];
				part.fAngle = 0;
				part.fSpin = pData->trackGenerateSpin.GetValue( fRelTime ) + GetRandomFromTrack( fRelTime, pData->trackGenerateSpinRandomizer );
				const float area = pData->trackGenerateArea.GetValue( fRelTime ) * fScale;
				part.vPosition = (*pfnGPPfunc)( area, vPosition );
				const float fSpeed = pData->trackBeginSpeed.GetValue( fRelTime, pData->trackBeginSpeedRandomizer );
				const int nPhi = FSinCosMakeAngleChecked( GetRandomFromTrack( fRelTime, pData->trackBeginAngleRandomizer ) / 2 + fDirectionPhi );
				const int nTheta = FSinCosMakeAngleChecked( GetRandomFromTrack( fRelTime, pData->trackBeginAngleRandomizer ) / 2 + fDirectionTheta );

				part.vSpeed.x = FSinCalibrated( nTheta ) * FCosCalibrated( nPhi );
				part.vSpeed.y = FSinCalibrated( nTheta ) * FSinCalibrated( nPhi );
				part.vSpeed.z = FCosCalibrated( nTheta );
				CVec3 vRadialWind = part.vPosition - vPosition;
				Normalize( &vRadialWind );
				part.vSpeed *= fSpeed;
				part.vSpeed += pData->fRadialWind * vRadialWind;
				part.vWind = pData->vWind;
				pData->trackSpeed.CreateStartContext( &(part.contextSpeed), pData->trackSpeedRnd );
				pData->trackIntegralMass.CreateStartContext( &(part.contextZSpeed), pData->trackSpeedRnd );
				pData->trackSpin.CreateStartContext( &(part.contextSpin) );
				particles.push_back( part );
				--nNumForGenerating;
			}
		}
		nLastUpdateTime += dt & 0xfffffff0;
	}
	if ( !particles.empty() )
	{
		for ( std::list<SExtendedParticle>::iterator it = particles.begin(); it != particles.end(); )
		{
			if ( it->deathTime <= time )
			{
				it = particles.erase( it );
			}
			else
			{
				const float fTime = ( time - it->birthTime ) / float( it->deathTime - it->birthTime );
				if ( fTime > it->contextSpeed.fTime && fTime < 1.0f )
				{
					const float fLifetime = it->deathTime - it->birthTime;
					it->color.a = BYTE( it->fOpacity * pData->trackOpacity.GetValue(fTime) );
					it->fSize = pData->trackSize.GetValue( fTime ) * fScale;
					it->rcMaps = rcRects[ MINT( pData->trackTextureFrame.GetValue(fTime) * rcRects.size() ) % rcRects.size() ];
					it->fAngle += it->fSpin * pData->trackSpin.Integrate( &(it->contextSpin), fTime ) * fLifetime;
					const float fSpeedCoeff = pData->trackSpeed.Integrate( &(it->contextSpeed), fTime, pData->trackSpeedRnd ) * fLifetime;
					it->vPosition += ( it->vSpeed * fSpeedCoeff + it->vWind * ( time - nLastParticleUpdate ) ) * fScale;
					float grav = pData->trackIntegralMass.Integrate( &(it->contextZSpeed), fTime, pData->trackSpeedRnd ) * fLifetime * fLifetime * fScale; 
					it->vPosition.z -= grav;
				}
				++it;
			}
		}
	}
	nLastParticleUpdate = time;
}
void CKeyBasedParticleSource::Init( SParticleSourceData *_pData )
{
	NI_ASSERT_SLOW_T( _pData != 0, "Unable to initialize particle source with empty data!" );
	pData = _pData;
	const float dx = 1.0f / pData->nTextureDX;
	const float dy = 1.0f / pData->nTextureDY;
	rcRects.reserve( pData->nTextureDX * pData->nTextureDY );
	for ( int i = 0; i < pData->nTextureDY; ++i )
	{
		for ( int j = 0; j < pData->nTextureDX; ++j )
			rcRects.push_back( CTRect<float>( dx*j, dy*i, dx*(j + 1), dy*(i + 1) ) );
	}
	pTexture = GetSingleton<ITextureManager>()->GetTexture( pData->szTextureName.c_str() );
	fScale = 1.0f;
	bStopped = false;
	bSuspended = false;
	switch ( pData->nAreaType )
	{
		case PSA_TYPE_DISK:
			pfnGPPfunc = &CParticleGenerator::GetParticlePositionDisk;
			break;
		case PSA_TYPE_CIRCLE:
			pfnGPPfunc = &CParticleGenerator::GetParticlePositionCircle;
			break;
		case PSA_TYPE_SQUARE:
		default:
			pfnGPPfunc = &CParticleGenerator::GetParticlePositionSquare;
			break;
	}
	vDirection.Set( 0, 0, 1 );
	fDirectionPhi = 1;
	fDirectionTheta = 0;
	pData->trackDensity.CreateStartContext( &contextDensity );
}
int CKeyBasedParticleSource::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pData );
	saver.Add( 2, &nStartTime );
	saver.Add( 3, &nLastUpdateTime );
	saver.Add( 4, &vPosition );
	saver.Add( 5, &fDirectionPhi );
	saver.Add( 6, &fDirectionTheta );
	saver.Add( 7, &pTexture );
	saver.Add( 8, &lastError );
	saver.Add( 9, &rcRects );
	saver.Add( 10, &particles );
	saver.Add( 11, &nLastParticleUpdate );
	saver.Add( 12, &vDirection );
	saver.Add( 13, &fScale );
	saver.Add( 14, &bStopped );
	saver.Add( 15, &contextDensity );
	saver.Add( 16, &bSuspended );
	switch ( pData->nAreaType )
	{
		case PSA_TYPE_DISK:
			pfnGPPfunc = &CParticleGenerator::GetParticlePositionDisk;
			break;
		case PSA_TYPE_CIRCLE:
			pfnGPPfunc = &CParticleGenerator::GetParticlePositionCircle;
			break;
		case PSA_TYPE_SQUARE:
		default:
			pfnGPPfunc = &CParticleGenerator::GetParticlePositionSquare;
			break;
	}
	if ( saver.IsReading() ) 
	{
		if ( fScale == 0 ) 
			fScale = 1.0f;
	}
	return 0;
}
void CKeyBasedParticleSource::SetScale( float _fScale )
{
	fScale *= _fScale;
}
float CKeyBasedParticleSource::GetArea() const
{
	float result = 0;
	std::list<SExtendedParticle>::const_iterator it = particles.begin();
	while ( it != particles.end() )
	{
		result += it->fSize * it->fSize;
		++it;
	}
	return result;
}
void CKeyBasedParticleSource::Stop()
{
	bStopped = true;
}
int CKeyBasedParticleSource::GetOptimalUpdateTime() const
{
	STrackContext context;
	pData->trackDensity.CreateStartContext( &context );
	int nTime = int(Clamp( 1.0f / pData->trackDensity.Integrate( &context, 1.0f ) * pData->fDensityCoeff, 4.0f, 32.0f )) >> 2;
	int result = 4;
	while ( nTime && result < 32 )
	{
		nTime = nTime >> 1;
		result = result << 1;
	}
	return result;
}
void CKeyBasedParticleSource::SetSuspendedState( bool bState )
{
	bSuspended = bState;
}
CVec3 CParticleGenerator::GetParticlePositionSquare( const float area, const CVec3 &vPosition )
{
	const float dx = NWin32Random::RandomCheck( -area, area );
	const float dy = NWin32Random::RandomCheck( -area, area );
	return CVec3( vPosition.x + dx, vPosition.y + dy, vPosition.z );
}
CVec3 CParticleGenerator::GetParticlePositionDisk( const float area, const CVec3 &vPosition )
{
	const float fRndRad = NWin32Random::RandomCheck( 0.0f, area );
	const float fRndPhi = NWin32Random::RandomCheck( 0.0f, FP_2PI );
	return CVec3( vPosition.x + fRndRad * FCos( fRndPhi ), vPosition.y + fRndRad * FSin( fRndPhi ), vPosition.z );
}
CVec3 CParticleGenerator::GetParticlePositionCircle( const float area, const CVec3 &vPosition )
{
	const float fRndPhi = fStartAngle + nCurrParticle * fStep;
	nCurrParticle++;
	return CVec3( vPosition.x + area * FCos( fRndPhi ), vPosition.y + area * FSin( fRndPhi ), vPosition.z );
}
void CParticleGenerator::ResetGenerator( int nNextNumParticles )
{
	if ( nNextNumParticles > 0 )
	{
		fStartAngle = NWin32Random::Random( 0.0f, FP_2PI );
		nCurrParticle = 0;
		fStep = FP_2PI / float( nNextNumParticles );
	}
}
float CParticleGenerator::fStartAngle;
float CParticleGenerator::nCurrParticle;
float CParticleGenerator::fStep;
