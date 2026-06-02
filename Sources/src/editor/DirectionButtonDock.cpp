#include "stdafx.h"
#include "DirectionButtonDock.h"

CDirectionButtonDockBar::CDirectionButtonDockBar()
{
}

CDirectionButtonDockBar::~CDirectionButtonDockBar()
{
}


BEGIN_MESSAGE_MAP(CDirectionButtonDockBar, SECControlBar)
ON_WM_CREATE()
ON_WM_SIZE()
END_MESSAGE_MAP()



int CDirectionButtonDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_BORDER;
	m_DirectionButton.Create( 0, "Direction button", dwStyle,
		CRect(0, 0, 0, 0), this, 3000 );
	return 0;
}

void CDirectionButtonDockBar::OnSize(UINT nType, int cx, int cy) 
{
	SECControlBar::OnSize(nType, cx, cy);
	
	if( m_DirectionButton.GetSafeHwnd() != NULL )
	{
		CRect r;
		GetInsideRect(r);
		m_DirectionButton.SetWindowPos( NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER|SWP_NOACTIVATE );
	}
}

BOOL CDirectionButtonDockBar::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
	case WM_ANGLE_CHANGED:
		GetParent()->PostMessage( WM_ANGLE_CHANGED );
		return true;
	}
	
	return SECControlBar::PreTranslateMessage( pMsg );
}

float CDirectionButtonDockBar::GetAngle()
{
	float fTemp = m_DirectionButton.GetAngle();
	fTemp -= FP_PI4;
	
	if ( fTemp < 0 )
		fTemp = FP_2PI + fTemp;
	
	return fTemp;
}

void CDirectionButtonDockBar::SetAngle( float fVal )
{
	fVal += FP_PI4;
	if ( fVal > FP_2PI )
		fVal -= FP_2PI;
	m_DirectionButton.SetAngle( fVal );
}

int CDirectionButtonDockBar::GetIntAngle()
{
	float fTemp = GetAngle();
	return fTemp * 65535 / ( FP_2PI );
}

void CDirectionButtonDockBar::SetIntAngle( int nVal )
{
	float fTemp = (float) nVal * FP_2PI / 65535;
	fTemp += FP_PI4;
	if ( fTemp > FP_PI )
		fTemp = FP_PI - fTemp;
	m_DirectionButton.SetAngle( fTemp );
}
