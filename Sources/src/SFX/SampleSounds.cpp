#include "StdAfx.h"

#include "AudioFmodCompat.h"
#include "SampleSounds.h"

void CSoundSample::Close()
{
	if ( sample )
		FSOUND_Sample_Free( static_cast<FSOUND_SAMPLE*>(sample) );
	sample = 0;
}
void CSoundSample::SetSample( void *_sample )
{
	Close();
	sample = _sample;
	if ( sample )
		FSOUND_Sample_SetMinMaxDistance( static_cast<FSOUND_SAMPLE*>(sample), fMinDistance, 1000000000.0f );
}
void CSoundSample::Set3D( bool b3D )
{
	nMode = b3D ? FSOUND_HW3D : FSOUND_2D;
}
void CSoundSample::SetMinDistance( float _fMinDistance )
{
	fMinDistance = _fMinDistance;
	if ( sample )
		FSOUND_Sample_SetMinMaxDistance( static_cast<FSOUND_SAMPLE*>(sample), fMinDistance, 1000000000.0f );
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
		FSOUND_Sample_SetMode( static_cast<FSOUND_SAMPLE*>(sample), bEnable ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF );
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
	FSOUND_SAMPLE *sample = FSOUND_Sample_Load( FSOUND_UNMANAGED, &(buffer[0]), GetMode() | FSOUND_LOADMEMORY, 0, nSize );
	if ( sample == 0 )
		return false;
	SetSample( sample );
	return true;
}
bool CBaseSound::IsPlaying()
{
	if ( (nChannel != -1) && FSOUND_IsPlaying(nChannel) )
		return FSOUND_GetCurrentSample( nChannel ) == static_cast<FSOUND_SAMPLE*>(pSample->GetInternalContainer());
	else
		return false;
}
void CBaseSound::SetLooping( bool bEnable, int nStart, int nEnd )
{
	FSOUND_SAMPLE *sample = static_cast<FSOUND_SAMPLE*>(pSample->GetInternalContainer());
	FSOUND_Sample_SetMode( static_cast<FSOUND_SAMPLE*>(sample), bEnable ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF );
	if ( (nStart != -1) && (nEnd != -1) )
		FSOUND_Sample_SetLoopPoints( sample, nStart, nEnd );
}
unsigned int CBaseSound::GetLenght()
{
	return FSOUND_Sample_GetLength( static_cast<FSOUND_SAMPLE*>(pSample->GetInternalContainer()) );
}
unsigned int CBaseSound::GetSampleRate()
{
	int freq = 44000;
	FSOUND_Sample_GetDefaults( static_cast<FSOUND_SAMPLE*>(pSample->GetInternalContainer()), &freq, 0, 0, 0 );
	return freq;
}
int CSound2D::Visit( interface ISFXVisitor *pVisitor )
{
	return pVisitor->VisitSound2D( this );
}
int CSound2D::Play()
{
	int nChannel = FSOUND_PlaySound( FSOUND_FREE, static_cast<FSOUND_SAMPLE*>(GetSample()->GetInternalContainer()) );
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
			FSOUND_3D_SetAttributes( GetChannel(), (float *) vLocalPos.m, 0 );			//0 потому что мы не используем доплеровский эффект
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
			FSOUND_3D_SetAttributes( GetChannel(), (float *) vLocalPos.m, 0 );
		}
	}
	vPos = vLocalPos;
}
int CSound3D::Play()
{
	FSOUND_SAMPLE *sample = static_cast<FSOUND_SAMPLE*>(GetSample()->GetInternalContainer());
	int nChannel = -1;
	if ( FSOUND_SAMPLE *sample = static_cast<FSOUND_SAMPLE*>(GetSample()->GetInternalContainer()) )
	{

		nChannel = FSOUND_PlaySound( FSOUND_FREE, sample );

	}
	SetChannel( nChannel );
	return nChannel;
}

