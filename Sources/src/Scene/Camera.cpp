#include "StdAfx.h"

#include "../Input/Input.h"
#include "Camera.h"
#include "../Common/PauseGame.h"
CCamera::CCamera() 
{
	vAnchor = VNULL3;
	fRod = 1000;
	fPitch = -ToRadian( 90.0f + 45.0f );
	fYaw = 0; 
	vScrollSpeed.Set( 0, 0 );
	rcBounds.SetEmpty();
	fEQAttenuation = 5.0f;
	fEQPeriod = 8.0f;
	timeEQDuration = 1000;
	vLastAnchor = VNULL3;
	timeLast = 0;
}
void CCamera::Init( ISingleton *pSingleton )
{
	IInput *pInput = GetSingleton<IInput>( pSingleton );
	pFwd = pInput->CreateSlider( "camera_forward", 200.0f );
	pStrafe = pInput->CreateSlider( "camera_strafe", 100.0f );
	pZoom = pInput->CreateSlider( "camera_zoom", 100.0f );
	ISingleTimer *pTimer = GetSingleton<IGameTimer>()->GetAbsTimer();
	pTimeSlider = pTimer->CreateSlider();
	CTableAccessor table = NDB::OpenDataTable( "consts.xml" );
	if ( (IDataTable*)table == 0 )
	{
		NStr::DebugTrace( "Camera: failed to open consts.xml, using default earthquake constants\n" );
		return;
	}
	fEQAttenuation = table.GetFloat( "Scene", "Camera.Earthquake.Attenuation", 5 );
	fEQPeriod = table.GetFloat( "Scene", "Camera.Earthquake.Period", 8 );
	timeEQDuration = table.GetInt( "Scene", "Camera.Earthquake.Duration", 5000 );
}
void CCamera::SetPlacement( const CVec3 &_vAnchor, float _fDist, float _fPitch, float _fYaw )
{
	vAnchor = _vAnchor;
	vAnchor.z = 0;
	fRod = _fDist;
	fPitch = _fPitch;
	fYaw = _fYaw;
}
void CCamera::ResetSliders()
{
	pFwd->Reset();
	pStrafe->Reset();
	pZoom->Reset();
	pTimeSlider->Reset();
}
void CCamera::Update()
{
	if ( GetSingleton<IGameTimer>()->GetPauseReason() > PAUSE_TYPE_NO_CONTROL ) 
	{
		pTimeSlider->Reset();
		pFwd->Reset();
		pStrafe->Reset();
		pZoom->Reset();
	}
	const float fTimeDiff = pTimeSlider->GetDelta();
	float fStrafe = pStrafe->GetDelta() + fTimeDiff*vScrollSpeed.x;
	float fFwd = pFwd->GetDelta() + fTimeDiff*vScrollSpeed.y * 2;
	// BK_INPUT_TRACE: every non-zero camera move, paired with the slider
	// activate/deactivate lines from the input binder - a move with no key
	// held means a slider bind never received its release.
	if ( ( fabsf( fStrafe ) > 0.001f || fabsf( fFwd ) > 0.001f ) && getenv( "BK_INPUT_TRACE" ) )
		fprintf( stderr, "BK_INPUT_TRACE: camera strafe=%.2f fwd=%.2f anchor=(%.0f,%.0f)\n", fStrafe, fFwd, vAnchor.x, vAnchor.y );
	float fZoom = pZoom->GetDelta();
	CQuat quat = CQuat( fYaw, V3_AXIS_Z ) * CQuat( fPitch, V3_AXIS_X );
	CVec3 vAxisX, vAxisY;
	quat.GetZAxis( &vAxisY );
	vAxisY.z = 0;
	Normalize( &vAxisY );
	vAnchor += vAxisY * fFwd;
	quat.GetXAxis( &vAxisX );
	vAxisX.z = 0;
	Normalize( &vAxisX );
	vAnchor += vAxisX * fStrafe;
	fRod += fZoom;
	if ( !rcBounds.IsEmpty() )
	{
		vAnchor.x = Clamp( vAnchor.x, rcBounds.x1, rcBounds.x2 );
		vAnchor.y = Clamp( vAnchor.y, rcBounds.y1, rcBounds.y2 );
	}
	float fComponentX = vAxisX * vAnchor;
	float fComponentY = vAxisY * vAnchor;
	fComponentX = int( fComponentX );
	fComponentY = int( fComponentY / 2.0f ) * 2.0f;

	vAnchor1 = fComponentX*vAxisX + fComponentY*vAxisY;

	CVec3 vDir;
	quat.GetZAxis( &vDir );
	vPos = vAnchor1 - vDir*fRod;
	if ( fabs2(vAnchor1 - vLastAnchor) > 4 ) 
	{
		vLastAnchor = vAnchor1;
		timeLast = GetSingleton<IGameTimer>()->GetAbsTime();
	}
	if ( !earthquakes.empty() ) 
	{
		float fVal = 0;
		for ( std::list<SEarthQuake>::iterator it = earthquakes.begin(); it != earthquakes.end(); )
		{
			const float fTime = it->fTime * 0.001f;
			fVal += exp( -fTime*it->fAttenuation ) * cos( fEQPeriod*FP_2PI*fTime ) * it->fAmplitude;
			it->fTime += fTimeDiff;
			if ( it->fTime >= it->fDuration )
				it = earthquakes.erase( it );
			else
				++it;
		}
		vPos.z += fVal;
		vAnchor1.z += fVal;
	}
}
const SHMatrix CCamera::GetPlacement() const
{
	CQuat quat = CQuat( fYaw, V3_AXIS_Z ) * CQuat( fPitch, V3_AXIS_X );
	SHMatrix matrix;
	CreateViewMatrixRH( &matrix, vPos, quat );
	return matrix;
}
static const float FP_MIN_QUAKE_RADIUS = 8.0f*fWorldCellSize;
static const float FP_MAX_QUAKE_RADIUS = 200.0f*fWorldCellSize;
void CCamera::AddEarthquake( const CVec3 &vPos, const float fPower )
{
	if ( fPower <= 0 ) 
		return;
	const float fDist = fabs( vPos.x - vAnchor1.x, vPos.y - vAnchor1.y );
	if ( fDist < FP_MAX_QUAKE_RADIUS )
	{
		float fCoeff = fPower;
		if ( fDist > FP_MIN_QUAKE_RADIUS )
			fCoeff *= ( 1.0f - ( fDist - FP_MIN_QUAKE_RADIUS ) / ( FP_MAX_QUAKE_RADIUS - FP_MIN_QUAKE_RADIUS ) );
		earthquakes.push_back( SEarthQuake(fPower*fCoeff, fEQAttenuation, timeEQDuration) );
	}
}
int CCamera::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vAnchor );
	saver.Add( 2, &fRod );
	saver.Add( 3, &fPitch );
	saver.Add( 4, &fYaw );
	saver.Add( 5, &vPos );
	saver.Add( 6, &pTimeSlider );
	saver.Add( 7, &rcBounds );
	saver.Add( 8, &vLastAnchor );
	saver.Add( 9, &timeLast );
	saver.Add( 10, &earthquakes );
	saver.Add( 11, &fEQAttenuation );
	saver.Add( 12, &fEQPeriod );
	saver.Add( 13, &timeEQDuration );
	return 0;
}
