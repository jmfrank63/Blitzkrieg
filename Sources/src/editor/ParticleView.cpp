#include "StdAfx.h"
#include "ParticleView.h"
#include "ParticleFrm.h"
#include "frames.h"


CParticleView::CParticleView()
{
}

CParticleView::~CParticleView()
{
}


BEGIN_MESSAGE_MAP(CParticleView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CParticleView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CParticleView::OnPaint() 
{
	CParticleFrame *pFrame = static_cast<CParticleFrame *> ( g_frameManager.GetFrame( CFrameManager::E_PARTICLE_FRAME ) );
	if ( pFrame->IsRunning() )
	{
		RECT valRC;
		((CWnd *) g_frameManager.GetGameWnd())->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		pFrame->GFXDraw();
	}
	else
	{
		CWnd::OnPaint();
		ValidateRect( 0 );
	}
}
