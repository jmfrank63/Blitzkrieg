#ifndef __CAMERA_H__
#define __CAMERA_H__
#pragma ONCE
#include "..\Input\Input.h"
class CCamera : public ICamera
{
	OBJECT_COMPLETE_METHODS( CCamera );
	DECLARE_SERIALIZE;
	struct SEarthQuake
	{
		float fAmplitude;
		float fAttenuation;
		float fDuration;
		float fTime;
		SEarthQuake() {  }
		SEarthQuake( float _fAmplitude, float _fAttenuation, float _fDuration )
			: fAmplitude( _fAmplitude ), fAttenuation( _fAttenuation ), fDuration( _fDuration ), fTime( 0 ) {  }
	};
	CVec3 vAnchor;												// anchor point on the terrain's surface
	CVec3 vAnchor1;												// то, на что надо ориентироватьс
	float fRod;														// distance from the anchor to the camera
	float fPitch, fYaw;										// yaw and pitch of the camera
	CVec3 vPos;														// temporary storage for position
	CVec2 vScrollSpeed;										// additional scrolling speed
	CTRect<float> rcBounds;								// bounds rect
	float fEQAttenuation;									// earthquake wave attenuation
	float fEQPeriod;											// earthquake wave period
	NTimer::STime timeEQDuration;					// earthquake duration
	std::list<SEarthQuake> earthquakes;		// earthquakes
	CPtr<ITimeSlider> pTimeSlider;
	CPtr<IInputSlider> pFwd, pStrafe, pZoom;
	CVec3 vLastAnchor;										// last position
	NTimer::STime timeLast;								// last time of the position above
public:
	CCamera();
	virtual void STDCALL Init( ISingleton *pSingleton );
	virtual void STDCALL SetBounds( int x1, int y1, int x2, int y2 ) { rcBounds.Set( x1, y1, x2, y2 ); }
	virtual void STDCALL SetPlacement( const CVec3 &vAnchor, float fDist, float fPitch, float fYaw );
	virtual void STDCALL SetAnchor( const CVec3 &_vAnchor ) { vAnchor = _vAnchor; }
	virtual const SHMatrix STDCALL GetPlacement() const;
	virtual const CVec3 STDCALL GetPos() const { return vPos; }
	virtual const CVec3 STDCALL GetAnchor() { return vAnchor1; }
	virtual void STDCALL GetLastPos( CVec3 *pvAnchor, NTimer::STime *pTime ) const { *pvAnchor = vLastAnchor; *pTime = timeLast; }
	virtual void STDCALL ResetSliders();
	virtual void STDCALL SetScrollSpeedX( float fSpeed ) { vScrollSpeed.x = fSpeed; }
	virtual void STDCALL SetScrollSpeedY( float fSpeed ) { vScrollSpeed.y = fSpeed; }
	virtual void STDCALL AddEarthquake( const CVec3 &vPos, const float fPower );
	virtual void STDCALL Update();
};
#endif // __CAMERA_H__
