
#include "StdAfx.h"
#include "PlanePathTest.h"
#include "PlanePathTestDlg.h"
#include "..\Misc\HPTimer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


static const float R = 60;
static const float fMaxSpeed = 100;
static const int nCenter = 350;

static float fX = 0;
static float fY = 0;
static float fZ = 0;
static float fZoom = 0;

CPoint _P( const CVec3 &_v )
{

	CVec3 v(_v);

	v = CVec3( cos(fZ) * v.x + sin(fZ) * v.y, - sin(fZ) * v.x + cos(fZ) * v.y, v.z );

	v = CVec3( cos(fY) * v.x + sin(fY) * v.z, v.y, -sin(fY) * v.x + cos(fY) * v.z );

	v = CVec3( v.x, cos(fX) * v.y + sin(fX) * v.z, - sin(fX) * v.y + cos(fX) * v.z );

	return CPoint( nCenter + fZoom * (v.x - v.z / sqrt(2.0f)) , nCenter + fZoom*( v.y - v.z / sqrt(2.0f)) );
}


CPlanePathTestDlg::CPlanePathTestDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPlanePathTestDlg::IDD, pParent)
{
	m_StartSpeed = 50;
	m_FinalSpeed = 50;
	m_PathProgress = 0;
	m_XAngle = 50;
	m_YAngle = 50;
	m_ZAngle = 50;
	m_Zoom = 50;
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPlanePathTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Slider(pDX, IDC_SLIDER1, m_StartSpeed);
	DDX_Slider(pDX, IDC_SLIDER2, m_FinalSpeed);
	DDX_Slider(pDX, IDC_SLIDER3, m_PathProgress);
	DDX_Slider(pDX, IDC_SLIDER4, m_XAngle);
	DDX_Slider(pDX, IDC_SLIDER5, m_YAngle);
	DDX_Slider(pDX, IDC_SLIDER6, m_ZAngle);
	DDX_Slider(pDX, IDC_SLIDER7, m_Zoom);
}

BEGIN_MESSAGE_MAP(CPlanePathTestDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_HSCROLL()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_BN_CLICKED(IDC_BUTTON1, OnRecalc)
	ON_WM_VSCROLL()
END_MESSAGE_MAP()


BOOL CPlanePathTestDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	

	/*
	+	x0	{152.149, 2089.12, 500.000}
+	x1	{150.919, 2072.24, 500.000}
*/

	x0 = CVec3( 0, 0, 0 );
	v0 = CVec3( -100, -100, 0 );
	
	x1 = CVec3( -180, -180, 90 );
	v1 = CVec3( 0, 0, 0 );


	x2 = CVec3( 0, 0, 0 );
	v2 = CVec3( 0, -100, 0 );
	pBest = new CPathFractionArcLine3D;
	pBest1 = new CPathFractionArcLine3D;

	Recalc();
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CPlanePathTestDlg::Recalc()
{
	fX = 2 * PI * m_XAngle / 100.0f - PI;
	fY = 2 * PI * m_YAngle / 100.0f - PI;
	fZ = 2 * PI * m_ZAngle / 100.0f - PI;
	fZoom = m_Zoom / 50.0f;

	const float fStartRadius = R * m_StartSpeed / fMaxSpeed;
	const float fFinalRadius = R * m_FinalSpeed / fMaxSpeed;

	pBest->Init( x0, v0, x1, fStartRadius );
	pBest->DoSubstitute( 0 );

	pBest1->Init( pBest->GetPoint(pBest->GetLength()), pBest->GetTangent(pBest->GetLength()), x2, fFinalRadius );
	pBest1->DoSubstitute( 0 );
}

void CPlanePathTestDlg::DrawPointValue( CPaintDC &dc, const CVec3 &point ) const
{
	CString szVal;
	szVal.Format( "%.0f,%.0f,%.0f", point.x, point.y, point.z );
	
	CPoint p( _P(point) );
	CRect r( p.x, p.y, 50, 30 );
	
	dc.DrawText( szVal, r, DT_LEFT|DT_NOCLIP    );

	dc.MoveTo( _P(point - CVec3(0,0,5)) );
	dc.LineTo( _P(point + CVec3(0,0,5)) );

	dc.MoveTo( _P(point - CVec3(0,5,0)) );
	dc.LineTo( _P(point + CVec3(0,5,0)) );

	dc.MoveTo( _P(point - CVec3(5,0,0)) );
	dc.LineTo( _P(point + CVec3(5,0,0)) );
}

void CPlanePathTestDlg::Draw()
{
	CPaintDC dc(this); // device context for painting

	CPen pen( PS_SOLID, 2, 0x00ff8888 );
	CPen pen1( PS_SOLID, 1, 0x0000ff );
	CPen pen2( PS_SOLID, 3, 0x00ff00 );
	CPen pen3( PS_SOLID, 3, 0x0000ff );
	CPen pen4( PS_SOLID, 3, 0xff00ff );
	CPen pen5( PS_SOLID, 3, 0xffffff );
	CPen * pOldPen = dc.SelectObject( &pen );

	dc.SelectObject( &pen5 );
	dc.MoveTo( _P(CVec3(0,0,0)) );
	dc.LineTo( _P(CVec3(50,0,0)) );
	dc.MoveTo( _P(CVec3(0,0,0)) );
	dc.LineTo( _P(CVec3(0,50,0)) );
	dc.MoveTo( _P(CVec3(0,0,0)) );
	dc.LineTo( _P(CVec3(0,0,50)) );

	dc.SelectObject( &pen );
	CVec3 vNorm0 = v0;
	CVec3 vNorm1 = v1;

	Normalize( &vNorm0 );
	Normalize( &vNorm1 );

	DrawPointValue( dc, x0 );
	dc.MoveTo( _P( x0 ));
	dc.LineTo( _P( x0 + vNorm0 * 50 ) );
	DrawPointValue( dc, x1 );
	dc.MoveTo( _P( x1 ) );
	dc.LineTo( _P( x1 + vNorm1 * 50 )  );

	DrawPointValue( dc, x2 );
	dc.MoveTo( _P( x2 ) );
	dc.LineTo( _P( x2 + vNorm1 * 50 )  );


	dc.SelectObject( &pen2 );
	CVec3 vPoint;
	CVec3 vTangent;

	vPoint = pBest->GetPoint( 0 );
	vTangent = pBest->GetTangent( 0 );
	dc.MoveTo( _P( vPoint ) );
	for ( float f = 1; f < pBest->GetLength(); ++f )
	{
		vPoint = pBest->GetPoint( f );
		vTangent = pBest->GetTangent( f );
		
		dc.LineTo( _P( vPoint ) );
		dc.MoveTo( _P( vPoint ) );
	}

	vPoint = pBest1->GetPoint( 0 );
	vTangent = pBest1->GetTangent( 0 );
	dc.MoveTo( _P( vPoint ) );
	for ( float f = 1; f < pBest1->GetLength(); ++f )
	{
		vPoint = pBest1->GetPoint( f );
		vTangent = pBest1->GetTangent( f );
		
		dc.LineTo( _P( vPoint ) );
		dc.MoveTo( _P( vPoint ) );
	}

	
	const float _fPlanePos = m_PathProgress * (pBest->GetLength() +pBest1->GetLength())/ 100.0f;
	IPathFraction *pCur = 0;
	float fPlanePos;
	
	if ( _fPlanePos > pBest->GetLength() )
	{
		pCur = pBest1;
		fPlanePos = _fPlanePos - pBest->GetLength();
	}
	else
	{
		pCur = pBest;
		fPlanePos = _fPlanePos;
	}

	{
		dc.SelectObject( &pen3 );
		vPoint = pCur->GetPoint( fPlanePos );
		vTangent = pCur->GetTangent( fPlanePos );
		Normalize( &vTangent );
		CVec3 vPerp = pCur->GetNormale( fPlanePos );
		Normalize( &vPerp );
		dc.MoveTo( _P(vPoint + vPerp * 20) );
		dc.LineTo( _P(vPoint) );
		dc.MoveTo( _P(vPoint) );
		dc.LineTo( _P(vPoint + vTangent * 20) );
	}

	dc.SelectObject( pOldPen );
}


void CPlanePathTestDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		Draw();
		CDialog::OnPaint();
	}
}

HCURSOR CPlanePathTestDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CPlanePathTestDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	UpdateData( true );
	Recalc();	
	UpdateWindow();
	RedrawWindow();
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CPlanePathTestDlg::OnLButtonDown(UINT nFlags, CPoint point) 
{
	x1.x  = point.x - nCenter;
	x1.y  = point.y - nCenter;

	Recalc();	
	UpdateWindow();
	RedrawWindow();
	CDialog::OnLButtonDown(nFlags, point);
}

void CPlanePathTestDlg::OnRButtonDown(UINT nFlags, CPoint point) 
{
	
	x2.x  = point.x - nCenter;
	x2.y  = point.y - nCenter;

	Recalc();
	UpdateWindow();
	RedrawWindow();
	CDialog::OnRButtonDown(nFlags, point);
}

void CPlanePathTestDlg::OnMouseMove(UINT nFlags, CPoint point) 
{
	if ( nFlags & MK_LBUTTON )
	{
		OnLButtonDown( nFlags, point );
	}
	else if ( nFlags & MK_RBUTTON )
	{
		OnRButtonDown( nFlags, point );
	}
	
	CDialog::OnMouseMove(nFlags, point);
}

void CPlanePathTestDlg::OnRecalc() 
{
	Recalc();
}

void CPlanePathTestDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	UpdateData( true );
	Recalc();	
	UpdateWindow();
	RedrawWindow();
	CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}
