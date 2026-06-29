#include "StdAfx.h"

#include "AudioBackend.h"
#include "SampleSounds.h"

void CSoundSample::Close()
{
	if ( sample )
		NAudioBackend::FreeSample( sample );
	sample = 0;
}
void CSoundSample::SetSample( void *_sample )
{
	Close();
	sample = _sample;
	if ( sample )
		NAudioBackend::SetSampleMinDistance( sample, fMinDistance );
}
void CSoundSample::Set3D( bool b3D )
{
	nMode = b3D ? NAudioBackend::GetSampleMode3D() : NAudioBackend::GetSampleMode2D();
}
void CSoundSample::SetMinDistance( float _fMinDistance )
{
	fMinDistance = _fMinDistance;
	if ( sample )
		NAudioBackend::SetSampleMinDistance( sample, fMinDistance );
}
void CSoundSample::SwapData( ISharedResource *pResource )
{
	CSoundSample *pRes = dynamic_cast<CSoundSample*>( pResource );
	NI_ASSERT_TF( pRes != 0, NStr::Format("shared resource is not a \"%s\"", typeid(*this).name()), return );
	std::swap( sample, pRes->sample );
}
void CSoundSample::SetLoop( bool bEnable )
{
	bLooped = bEnable;
	if ( sample )
		NAudioBackend::SetSampleLoop( sample, bEnable );
}
bool CSoundSample::Load( const bool bPreLoad )
{
	if ( (sample != 0) || bPreLoad )
		return true;
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	const int nSize = pStream->GetSize();
	std::vector<char> buffer( nSize );
	const int nCheck = pStream->Read( &(buffer[0]), nSize );
	NI_ASSERT_SLOW_TF( nCheck == nSize, "Readed size doesn't match requested", return false );
	void *sample = NAudioBackend::LoadSampleFromMemory( &(buffer[0]), nSize, GetMode() );
	if ( sample == 0 )
		return false;
	SetSample( sample );
	return true;
}
bool CBaseSound::IsPlaying()
{
	return NAudioBackend::IsChannelPlayingSample( nChannel, pSample->GetInternalContainer() );
}
void CBaseSound::SetLooping( bool bEnable, int nStart, int nEnd )
{
	void *sample = pSample->GetInternalContainer();
	NAudioBackend::SetSampleLoop( sample, bEnable );
	if ( (nStart != -1) && (nEnd != -1) )
		NAudioBackend::SetSampleLoopPoints( sample, nStart, nEnd );
}
unsigned int CBaseSound::GetLenght()
{
	return NAudioBackend::GetSampleLength( pSample->GetInternalContainer() );
}
unsigned int CBaseSound::GetSampleRate()
{
	return NAudioBackend::GetSampleRate( pSample->GetInternalContainer() );
}
int CSound2D::Visit( interface ISFXVisitor *pVisitor )
{
	return pVisitor->VisitSound2D( this );
}
int CSound2D::Play()
{
	int nChannel = NAudioBackend::PlaySample( GetSample()->GetInternalContainer() );
	SetChannel( nChannel );
	return nChannel;
}
int CSound3D::Visit( interface ISFXVisitor *pVisitor )
{
	return pVisitor->VisitSound3D( this, vPos );
}
void CSound3D::SetPosition( const CVec3 &vPos3 )
{
	CVec3 vLocalPos( vPos3.x, vPos3.z, vPos3.y );
	if ( IsPlaying() )
	{
		if ( !bDopplerFlag )
			NAudioBackend::SetChannel3DAttributes( GetChannel(), vLocalPos );			//0 потому что мы не используем доплеровский эффект
		else
		{
			/*
			NTimer::STime time = GetSingleton<IGameTimer>()->GetGameTime();
			float fSpeed = sqrt( (vPos3.x-vLastPos.x)*(vPos3.x-vLastPos.x) + (vPos3.y-vLastPos.y)*(vPos3.y-vLastPos.y) + (vPos3.z-vLastPos.z)*(vPos3.z-vLastPos.z) );
			float fDeltaTime = (float) (time - lastUpdateTime) / 1000.0f;		//delta t in seconds
			CVec3 vSpeed;
			vSpeed.x = (vPos3.x - vLastPos.x) / fDeltaTime;
			vSpeed.y = (vPos3.y - vLastPos.y) / fDeltaTime;
			vSpeed.z = (vPos3.z - vLastPos.z) / fDeltaTime;
			vLastPos = vPos3;
			*/
			NAudioBackend::SetChannel3DAttributes( GetChannel(), vLocalPos );
		}
	}
	vPos = vLocalPos;
}
int CSound3D::Play()
{
	void *sample = GetSample()->GetInternalContainer();
	int nChannel = -1;
	if ( sample )
		nChannel = NAudioBackend::PlaySample( sample );
	SetChannel( nChannel );
	return nChannel;
}

