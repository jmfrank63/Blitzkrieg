
#include "stdafx.h"
#include "editor.h"
#include "GameWnd.h"
#include "MainFrm.h"
#include "frames.h"

static void AppendEditorInputTrace( const char *pszText )
{
	char szTempPath[MAX_PATH] = { 0 };
	GetTempPathA( MAX_PATH, szTempPath );
	std::string szTraceFile = std::string( szTempPath ) + "blitzkrieg_editor_input_trace.txt";
	FILE *pFile = fopen( szTraceFile.c_str(), "a" );
	if ( pFile )
	{
		fputs( pszText, pFile );
		fclose( pFile );
	}
}


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CGameWnd::CGameWnd()
{
}

CGameWnd::~CGameWnd()
{
}


BEGIN_MESSAGE_MAP(CGameWnd, CWnd)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MBUTTONDBLCLK()
	ON_WM_MOUSEWHEEL()
	ON_WM_PAINT()
	ON_WM_CREATE()
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

static CWnd* GetMouseTargetWnd( CGameWnd *pGameWnd )
{
	if ( CParentFrame *pFrame = g_frameManager.GetActiveWnd() )
		if ( ::IsWindow( pFrame->GetSafeHwnd() ) )
			return pFrame;

	if ( pGameWnd != 0 && pGameWnd->m_mainFramePtr != 0 )
	{
		if ( CMDIChildWnd *pActiveChild = pGameWnd->m_mainFramePtr->MDIGetActive() )
		{
			if ( ::IsWindow( pActiveChild->GetSafeHwnd() ) )
				return pActiveChild;
		}
	}
	return 0;
}

static void ForwardMouseMessage( CGameWnd *pGameWnd, UINT nMessage, UINT nFlags, CPoint point )
{
	CWnd *pTarget = GetMouseTargetWnd( pGameWnd );
	if ( pTarget == 0 )
	{
		NStr::DebugTrace( "EDITOR_INPUT GameWnd drop msg=0x%x flags=0x%x pt=(%d,%d) target=null\n", nMessage, nFlags, point.x, point.y );
		AppendEditorInputTrace( NStr::Format( "GameWnd drop msg=0x%x flags=0x%x pt=(%d,%d) target=null\n", nMessage, nFlags, point.x, point.y ) );
		return;
	}
	if ( nMessage != WM_MOUSEMOVE )
	{
		NStr::DebugTrace( "EDITOR_INPUT GameWnd msg=0x%x flags=0x%x pt=(%d,%d) target=0x%p\n", nMessage, nFlags, point.x, point.y, pTarget->GetSafeHwnd() );
		AppendEditorInputTrace( NStr::Format( "GameWnd msg=0x%x flags=0x%x pt=(%d,%d) target=0x%p\n", nMessage, nFlags, point.x, point.y, pTarget->GetSafeHwnd() ) );
	}
	CPoint targetPoint = point;
	pGameWnd->ClientToScreen( &targetPoint );
	pTarget->ScreenToClient( &targetPoint );
	::SendMessage( pTarget->GetSafeHwnd(), nMessage, nFlags, MAKELPARAM(targetPoint.x, targetPoint.y) );
}


void CGameWnd::OnMouseMove(UINT nFlags, CPoint point) 
{
	ForwardMouseMessage( this, WM_MOUSEMOVE, nFlags, point );
}

void CGameWnd::OnLButtonDown(UINT nFlags, CPoint point) 
{
	ForwardMouseMessage( this, WM_LBUTTONDOWN, nFlags, point );
}

void CGameWnd::OnLButtonUp(UINT nFlags, CPoint point) 
{
	ForwardMouseMessage( this, WM_LBUTTONUP, nFlags, point );
}

void CGameWnd::OnRButtonDown(UINT nFlags, CPoint point) 
{
	ForwardMouseMessage( this, WM_RBUTTONDOWN, nFlags, point );
}

void CGameWnd::OnRButtonUp(UINT nFlags, CPoint point) 
{
	ForwardMouseMessage( this, WM_RBUTTONUP, nFlags, point );
}

void CGameWnd::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	ForwardMouseMessage( this, WM_RBUTTONDBLCLK, nFlags, point );
}

void CGameWnd::OnMButtonDown(UINT nFlags, CPoint point)
{
	ForwardMouseMessage( this, WM_MBUTTONDOWN, nFlags, point );
}

void CGameWnd::OnMButtonUp(UINT nFlags, CPoint point)
{
	ForwardMouseMessage( this, WM_MBUTTONUP, nFlags, point );
}

void CGameWnd::OnMButtonDblClk(UINT nFlags, CPoint point)
{
	ForwardMouseMessage( this, WM_MBUTTONDBLCLK, nFlags, point );
}

BOOL CGameWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if ( CWnd *pTarget = GetMouseTargetWnd( this ) )
	{
		NStr::DebugTrace( "EDITOR_INPUT GameWnd wheel flags=0x%x delta=%d pt=(%d,%d) target=0x%p\n", nFlags, zDelta, pt.x, pt.y, pTarget->GetSafeHwnd() );
		AppendEditorInputTrace( NStr::Format( "GameWnd wheel flags=0x%x delta=%d pt=(%d,%d) target=0x%p\n", nFlags, zDelta, pt.x, pt.y, pTarget->GetSafeHwnd() ) );
		::SendMessage( pTarget->GetSafeHwnd(), WM_MOUSEWHEEL, MAKEWPARAM( nFlags, zDelta ), MAKELPARAM( pt.x, pt.y ) );
		return TRUE;
	}
	NStr::DebugTrace( "EDITOR_INPUT GameWnd wheel drop flags=0x%x delta=%d pt=(%d,%d) target=null\n", nFlags, zDelta, pt.x, pt.y );
	AppendEditorInputTrace( NStr::Format( "GameWnd wheel drop flags=0x%x delta=%d pt=(%d,%d) target=null\n", nFlags, zDelta, pt.x, pt.y ) );
	return CWnd::OnMouseWheel( nFlags, zDelta, pt );
}

void CGameWnd::OnPaint() 
{

	ValidateRect( 0 );
}

BOOL CGameWnd::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
/*
	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
*/
/*
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
*/
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		0, HBRUSH(COLOR_WINDOW+1), NULL);

	return TRUE;
}

int CGameWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	m_mainFramePtr = static_cast<CMainFrame *>( GetParent() ); // first time this is MainFrame
	
 	return 0;
}

void CGameWnd::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	ForwardMouseMessage( this, WM_LBUTTONDBLCLK, nFlags, point );
}
