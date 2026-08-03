
#include "StdAfx.h"
#include "DirectionButton.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CDirectionButton::CDirectionButton()
{
	fAngle = 0.0f;
}

CDirectionButton::~CDirectionButton()
{
}

BEGIN_MESSAGE_MAP(CDirectionButton, CWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
END_MESSAGE_MAP()


int CDirectionButton::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

void CDirectionButton::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	RECT clientRC;
	GetClientRect( &clientRC );
  dc.FillSolidRect( &clientRC, GetSysColor( COLOR_MENU ) );

	int cx = clientRC.right/2;
	int cy = clientRC.bottom/2;

	int cxTmp = clientRC.right/2 - 5;
	int cyTmp = clientRC.bottom/2 - 5;

	POINT p1;
	POINT p2;

	dc.MoveTo( cx, cy );

	if ( cx >= cy )
	{
		cx = cy * cos( fAngle );
		cy = cy * sin( fAngle ) / 2 ;
	
		p1.x = sqrt( cx*cx+cy*cy ) * cos( fAngle + PI / 10.5f );
		p2.x = sqrt( cx*cx+cy*cy ) * cos( fAngle - PI / 10.5f );

		p1.y = sqrt( cx*cx+cy*cy ) * sin( fAngle + PI / 10.5f ) ;
		p2.y = sqrt( cx*cx+cy*cy ) * sin( fAngle - PI / 10.5f ) ; 

	}
	else
	{
		cy = cx * sin( fAngle ) / 2;
		cx = cx * cos( fAngle );

		p1.x = cxTmp * cos( fAngle + PI / 10.5f );
		p2.x = cxTmp * cos( fAngle - PI / 10.5f );

		p1.y = cxTmp * sin( fAngle + PI / 10.5f ) / 2;
		p2.y = cxTmp * sin( fAngle - PI / 10.5f ) / 2;
	}
	dc.LineTo( cx + clientRC.right/2, clientRC.bottom/2 - cy );

/*	dc.MoveTo( cx + clientRC.right/2, clientRC.bottom/2 - cy  );
	dc.LineTo( p1.x + clientRC.right/2, clientRC.bottom/2 - p1.y );

	dc.MoveTo( cx + clientRC.right/2, clientRC.bottom/2 - cy  );
	dc.LineTo( p2.x + clientRC.right/2, clientRC.bottom/2 - p2.y );*/
	
	
	{
		CFont font;
		font.CreateFont( 12, 0, 0, 0, 200, FALSE, FALSE, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,   
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial Cyr" );
		CFont* def_font = dc.SelectObject( &font);
		dc.SetTextColor( RGB( 0, 0, 0 ) );
		dc.SetBkMode( TRANSPARENT );
		char  str[200] ;
		float angle  = fAngle ;
		angle = angle < 0 ? ( 2 * PI - fabs( angle ) ) : angle;
		angle = angle > PI / 4 ? angle - PI / 4 : 2 * PI - ( PI / 4 -  angle ) ;
		sprintf( str, "%.2f " , ( angle * 180 ) / PI  );
		dc.TextOut( clientRC.left , clientRC.top + 3, str , strlen( str ) );
		font.DeleteObject(); 
	}





/*	LOGBRUSH logBrush;
	logBrush.lbStyle = BS_HATCHED;
	logBrush.lbColor = RGB(255, 0, 0);
	logBrush.lbHatch = HS_CROSS;
	CBrush brush;
	brush.CreateBrushIndirect( &logBrush );
	CBrush *pOldBrush = (CBrush *) dc.SelectObject( &brush );
	dc.SetBkColor( RGB(255, 0, 0) );
	dc.Ellipse( cx + clientRC.right/2 - 5, clientRC.bottom/2 - cy - 5, cx + clientRC.right/2 + 5, clientRC.bottom/2 - cy + 5 );
	dc.SelectObject( pOldBrush );*/
}


int CDirectionButton::GetQuadrant()
{
	if ( fAngle >= 0 && fAngle < PI/4 )
		return 0;
	if ( fAngle >= PI/4 && fAngle < PI/2 )
		return 1;
	if ( fAngle >= PI/2 && fAngle < 1.5*PI )
		return 2;
	if ( fAngle >= 1.5*PI && fAngle <= PI )
		return 3;

	if ( fAngle <= 0 && fAngle > -PI/4 )
		return 7;
	if ( fAngle <= PI/4 && fAngle > -PI/2 )
		return 6;
	if ( fAngle <= PI/2 && fAngle > -1.5*PI )
		return 5;
	if ( fAngle <= 1.5*PI && fAngle > PI )
		return 4;

	return 0;
}

void CDirectionButton::OnLButtonDown(UINT nFlags, CPoint pt) 
{
	RECT clientRC;
	GetClientRect( &clientRC );
	int cx, cy;		//расстояния от текущей позиции курсора до центра кнопки
	cx = pt.x - clientRC.right/2;
	cy = clientRC.bottom/2 - pt.y;
	
	fAngle = atan2( cy, cx );
	GetParent()->PostMessage( WM_ANGLE_CHANGED );
	Invalidate();
	SetCapture();

	CWnd::OnLButtonDown(nFlags, pt);
}

void CDirectionButton::OnMouseMove(UINT nFlags, CPoint pt) 
{
	if ( nFlags & MK_LBUTTON )
	{
		RECT clientRC;
		GetClientRect( &clientRC );
		int cx, cy;		//расстояния от текущей позиции курсора до центра кнопки
		cx = pt.x - clientRC.right/2;
		cy = clientRC.bottom/2 - pt.y;
		
		fAngle = atan2( cy, cx );
		RedrawWindow();
		Invalidate();
		GetParent()->PostMessage( WM_ANGLE_CHANGED );
	}
	CWnd::OnMouseMove(nFlags, pt);
}

void CDirectionButton::OnLButtonUp(UINT nFlags, CPoint point) 
{
	ReleaseCapture();
	CWnd::OnLButtonUp(nFlags, point);
}
