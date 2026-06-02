#ifndef __FRAMESELECTION_H__
#define __FRAMESELECTION_H__
#pragma ONCE
class CFrameSelection : public IFrameSelection
{
	OBJECT_MINIMAL_METHODS( CFrameSelection );
	CVec3 vBegin;
	CVec3 vEnd;
	bool bActive;
public:
	CFrameSelection() : vBegin( VNULL3 ), vEnd( VNULL3 ), bActive( false ) {  }
	virtual void STDCALL Begin( const CVec3 &point ) { vBegin = vEnd = point; bActive = true; }
	virtual void STDCALL End() { bActive = false; }
	virtual void STDCALL Update( const CVec3 &point ) { if ( bActive ) vEnd = point; }
	virtual void STDCALL Reset() { vBegin = vEnd = VNULL3; bActive = false; }

	virtual CVec3 STDCALL GetBeginPoint() { return vBegin; }
	virtual CVec3 STDCALL GetEndPoint() { return vEnd; }
	virtual bool STDCALL IsActive() { return bActive; }
	virtual bool STDCALL Update( const NTimer::STime &time, bool bForced = false ) { return true; }
	virtual bool STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( ISceneVisitor *pVisitor, int nType = -1 );
};
#endif // __FRAMESELECTION_H__
