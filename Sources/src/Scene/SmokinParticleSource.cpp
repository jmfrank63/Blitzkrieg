#include "StdAfx.h"

#include "SmokinParticleSource.h"
#include "FastSinCos.h"
int SExtendedParticleSource::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pSource );
	saver.Add( 2, &vSpeed );
	saver.Add( 3, &vWind );
	saver.Add( 4, &contextSpeed );
	saver.Add( 5, &contextZSpeed );
	return 0;
}
IGFXTexture* CSmokinParticleSource::GetTexture() const
{
	if ( particles.empty() )
	{
		return 0;
	}
	return particles.begin()->pSource->GetTexture();
}
const CVec3 CSmokinParticleSource::GetPos() const
{
	return vPosition;
}
void CSmokinParticleSource::SetPos( const CVec3 &vPos )
{
	vPosition = vPos;
}
const CVec3 CSmokinParticleSource::GetDirection() const
{
	return vDirection;
}
void CSmokinParticleSource::SetDirection( const SHMatrix &mDir )
{
	mDir.RotateVector( &vDirection, V3_AXIS_Z );
	CVec3 vDir;
	mDir.RotateVector( &vDir, pData->vDirection );
	Normalize( &vDir );
	fDirectionTheta = acos( vDir.z );
	const float direction_x = static_cast<float>( vDir.x / sin( fDirectionTheta ) );
	fDirectionPhi = fDirectionTheta == 0 ? 0 : ( vDir.y > 0 ? acos( Clamp<float>( direction_x, -1.0f, 1.0f ) ) : PI * 2 - acos( Clamp<float>( direction_x, -1.0f, 1.0f ) ) );
}
void CSmokinParticleSource::SetStartTime( const NTimer::STime &time )
{
	nStartTime = time;
	nLastUpdateTime = time;
	lastError = 0;
}
const NTimer::STime CSmokinParticleSource::GetStartTime() const
{
	return nStartTime;
}
const NTimer::STime CSmokinParticleSource::GetEffectLifeTime() const
{
	if ( particles.empty() )
		return pData->nLifeTime;
	else
		return pData->nLifeTime + particles.begin()->pSource->GetEffectLifeTime();
}
bool CSmokinParticleSource::IsFinished() const
{
	return particles.empty() && ( nLastUpdateTime > nStartTime + pData->nLifeTime || bStopped );
}
const int CSmokinParticleSource::GetNumParticles() const
{
	int result = 0;
	for ( std::list<SExtendedParticleSource>::const_iterator it = particles.begin(); it != particles.end(); ++it )
		result += it->pSource->GetNumParticles();
	return result;
}
void CSmokinParticleSource::FillParticleBuffer( SSimpleParticle *buff ) const
{
	for ( std::list<SExtendedParticleSource>::const_iterator it = particles.begin(); it != particles.end() ; it++  )
	{
		it->pSource->FillParticleBuffer( buff );
		buff += it->pSource->GetNumParticles();
	}
}
void CSmokinParticleSource::GetInfo( SParticleSourceInfo &info )
{
	SetStartTime( 0 );
	float nMaxCount = 0;
	float nMaxSize = 0;
	float	nAllCount = 0;
	float	nAllSize = 0;
	for ( int i = 0; i < GetEffectLifeTime(); i += 64 )
	{
		Update( i );
		float area = GetArea();
		nMaxCount = Max( nMaxCount, float( GetNumParticles() ) );
		nMaxSize = Max( nMaxSize, area );
		nAllCount += GetNumParticles();
		nAllSize += area;
	}	
	info.fMaxCount = nMaxCount;
	info.fMaxSize = nMaxSize / (  800.0f * 600.0f );
	info.fAverageSize = ( nAllSize * 64.0f) / ( GetEffectLifeTime() * 800.0f * 600.0f );
	info.fAverageCount = ( nAllCount  * 64.0f ) / GetEffectLifeTime() ;
}
void CSmokinParticleSource::Update( const NTimer::STime &time )
{
	NI_ASSERT_SLOW_T( pData != 0, "Updating uninitialized particle source!" );
	const int dt = time - nLastUpdateTime;
	if ( dt > 64 )
	{
		IParticleManager *pPM = GetSingleton<IParticleManager>();
		const int nStep = pData->nUpdateStep; 
		for ( int i = 0; i < dt; i += nStep )
		{
			const float fRelTime = (nLastUpdateTime + i - nStartTime) / float(pData->nLifeTime);
			const float fTempVal = ( lastError + pData->trackDensity.Integrate( &contextDensity, fRelTime ) * pData->nLifeTime ) * pData->fDensityCoeff;
			int nNumForGenerating = MINT( fTempVal );
			lastError = fTempVal - nNumForGenerating;
			CParticleGenerator::ResetGenerator( nNumForGenerating );
			while ( nNumForGenerating > 0 && pData->nLifeTime + nStartTime > time && !bStopped )
			{
				SExtendedParticleSource part;
				part.pSource = pPM->GetKeyBasedSource( (pData->szParticleEffectName + ".xml").c_str() );
				part.pSource->SetStartTime( nLastUpdateTime + i ); 
				const float area = pData->trackGenerateArea.GetValue( fRelTime ) * fScale;
				const CVec3 vPos = (*pfnGPPfunc)( area, vPosition );
				part.pSource->SetPos( vPos );
				const float fSpeed = pData->trackBeginSpeed.GetValue( fRelTime, pData->trackBeginSpeedRandomizer );

				const int nPhi = FSinCosMakeAngleChecked( GetRandomFromTrack( fRelTime, pData->trackBeginAngleRandomizer ) / 2 + fDirectionPhi );
				const int nTheta = FSinCosMakeAngleChecked( GetRandomFromTrack( fRelTime, pData->trackBeginAngleRandomizer ) / 2 + fDirectionTheta );


				part.vSpeed.x = FSinCalibrated( nTheta ) * FCosCalibrated( nPhi );
				part.vSpeed.y = FSinCalibrated( nTheta ) * FSinCalibrated( nPhi );
				part.vSpeed.z = FCosCalibrated( nTheta );
				CVec3 vRadialWind = vPos - vPosition;
				Normalize( &vRadialWind );
				part.vSpeed *= fSpeed;
				part.vSpeed += pData->fRadialWind * vRadialWind;
				part.vWind = pData->vWind;
				part.pSource->SetScale( fScale );
				pData->trackSpeed.CreateStartContext( &(part.contextSpeed), pData->trackSpeedRnd );
				pData->trackIntegralMass.CreateStartContext( &(part.contextZSpeed), pData->trackSpeedRnd );
				particles.push_back( part );
				--nNumForGenerating;
			}
			std::list<SExtendedParticleSource>::iterator it = particles.begin();
			while ( it != particles.end() )
			{
				it->pSource->Update( nLastUpdateTime + i );
				if ( it->pSource->IsFinished() )
				{
					it = particles.erase( it );
				}
				else
				{
					const float fTime = ( nLastUpdateTime + i - it->pSource->GetStartTime() ) / float( it->pSource->GetEffectLifeTime() );
					if ( fTime > it->contextSpeed.fTime && fTime < 1.0f )
					{
						const float fLifetime = it->pSource->GetEffectLifeTime();
						CVec3 vPos = it->pSource->GetPos();
						const float fSpeedCoeff = pData->trackSpeed.Integrate( &(it->contextSpeed), fTime, pData->trackSpeedRnd ) * fLifetime;
						vPos += ( it->vSpeed * fSpeedCoeff + it->vWind * 70.0f ) * fScale;
						vPos.z -= pData->trackIntegralMass.Integrate( &(it->contextZSpeed), fTime, pData->trackSpeedRnd ) * fLifetime * fLifetime * fScale;
						it->pSource->SetPos( vPos );
					}
					++it;
				}
			}
		}
		nLastUpdateTime += dt - ( dt % nStep );
	}
}
void CSmokinParticleSource::Init( SSmokinParticleSourceData *_pData )
{
	NI_ASSERT_SLOW_T( _pData != 0, "Unable to initialize particle source with empty data!" );
	pData = _pData;
	fScale = 1.0f;
	bStopped = false;
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
	pData->trackDensity.CreateStartContext( &contextDensity );
}
void CSmokinParticleSource::SetScale( float _fScale )
{
	fScale *= _fScale;
}
float CSmokinParticleSource::GetArea() const
{
	float result = 0;
	std::list<SExtendedParticleSource>::const_iterator it = particles.begin();
	while ( it != particles.end() )
	{
		result += it->pSource->GetArea();
	}
	return result;
}
void CSmokinParticleSource::Stop()
{
	bStopped = true;
}
int CSmokinParticleSource::GetOptimalUpdateTime() const
{
	return 16;
}
void CSmokinParticleSource::SetSuspendedState( bool bState )
{
	for ( std::list<SExtendedParticleSource>::iterator it = particles.begin(); it != particles.end() ; it++  )
		it->pSource->SetSuspendedState( bState );
}

int CSmokinParticleSource::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pData );
	saver.Add( 2, &nStartTime );
	saver.Add( 3, &nLastUpdateTime );
	saver.Add( 4, &vPosition );
	saver.Add( 5, &fDirectionPhi );
	saver.Add( 6, &fDirectionTheta );
	saver.Add( 7, &lastError );
	saver.Add( 9, &vDirection );
	saver.Add( 10, &fScale );
	saver.Add( 11, &bStopped );
	saver.Add( 12, &particles );
	saver.Add( 13, &contextDensity );
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
	return 0;
}
