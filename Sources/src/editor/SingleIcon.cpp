#include "StdAfx.h"
#include "..\\Image\Image.h"

#include "SingleIcon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CSingleIcon::CSingleIcon()
{
	m_fB = m_fC = m_fG = 0;
	m_nSizeX = m_nSizeY = 100;
}

CSingleIcon::~CSingleIcon()
{
}


BEGIN_MESSAGE_MAP(CSingleIcon, CWnd)
	ON_WM_PAINT()
	ON_WM_CREATE()
END_MESSAGE_MAP()

int CSingleIcon::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	return 0;
}


void CSingleIcon::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	RECT rc;
	GetClientRect( &rc );
  dc.FillSolidRect( &rc, RGB( 255, 255, 255 ) );

	CDC memDC;
	memDC.CreateCompatibleDC( &dc );
	CBitmap *pOldBitmap = memDC.SelectObject( &m_bmp );
	dc.BitBlt( rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, &memDC, 0, 0, SRCCOPY );
	memDC.SelectObject(pOldBitmap);

}

void CSingleIcon::LoadBitmap( const char *pszFullFileName, const char *pszInvalidFileName )
{
	szImageName = pszFullFileName;
	IImageProcessor *pImageProcessor = GetImageProcessor();
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	CPtr<IDataStream> pStream = pStorage->OpenStream( pszFullFileName, STREAM_ACCESS_READ );
	if ( pStream == 0 )
	{
		pStream = OpenFileStream( pszInvalidFileName, STREAM_ACCESS_READ );
		if ( pStream == 0 )
			return;
	}

	CPtr<IImage> pImage = pImageProcessor->LoadImage( pStream );
	if ( !pImage )
		return;
	if ( m_fB != 0 || m_fC != 0 || m_fG != 0 )
		pImage = pImageProcessor->CreateGammaCorrection( pImage, m_fB, m_fC, m_fG );

	int nSizeX = pImage->GetSizeX();
	int nSizeY = pImage->GetSizeY();
	double fRateX = (double) m_nSizeX/nSizeX;
	double fRateY = (double) m_nSizeY/nSizeY;
	double fRate = Min( fRateX, fRateY );
	CPtr<IImage> pScaleImage = pImageProcessor->CreateScale( pImage, fRate, ISM_LANCZOS3 /* ISM_LANCZOS3 */ );
	NI_ASSERT( pScaleImage != 0 );
	
	nSizeX = pScaleImage->GetSizeX();
	nSizeY = pScaleImage->GetSizeY();
	
	if ( nSizeY < m_nSizeY )
	{
		int nUp = (m_nSizeY - nSizeY)/2;
		CPtr<IImage> pCenteredImage = pImageProcessor->CreateImage( m_nSizeX, m_nSizeY );
		pCenteredImage->Set( 0 );
		RECT rc = { 0, 0, nSizeX, nSizeY };
		pCenteredImage->CopyFrom( pScaleImage, &rc, 0, nUp );
		pScaleImage = pCenteredImage;
	}
	else if ( nSizeX < m_nSizeX )
	{
		int nLeft = (m_nSizeX - nSizeX)/2;
		CPtr<IImage> pCenteredImage = pImageProcessor->CreateImage( m_nSizeX, m_nSizeY );
		pCenteredImage->Set( 0 );
		RECT rc = { 0, 0, nSizeX, nSizeY };
		pCenteredImage->CopyFrom( pScaleImage, &rc, nLeft, 0 );
		pScaleImage = pCenteredImage;
	}
	
	nSizeX = pScaleImage->GetSizeX();
	nSizeY = pScaleImage->GetSizeY();
	
	BITMAPINFO bmi;
	bmi.bmiHeader.biSize  = sizeof( bmi.bmiHeader );
	bmi.bmiHeader.biWidth  = nSizeX;
	bmi.bmiHeader.biHeight = -nSizeY;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = 0;
	bmi.bmiHeader.biClrUsed = 0;
	
	CDC *pDC = GetDC();
	HBITMAP hbm = CreateCompatibleBitmap( pDC->m_hDC, nSizeX, nSizeY );
	SetDIBits( pDC->m_hDC, hbm, 0, nSizeY, pScaleImage->GetLFB(), &bmi, DIB_RGB_COLORS );
	
	ReleaseDC( pDC );
	m_bmp.Detach();
	m_bmp.Attach( hbm );
	InvalidateRect( NULL );
}

void CSingleIcon::ApplyGammaCorrection( float fBrightness, float fContrast, float fGamma )
{
	m_fB = fBrightness;
	m_fC = fContrast;
	m_fG = fGamma;

	LoadBitmap( szImageName.c_str(), "" );
}
