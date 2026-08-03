#include "StdAfx.h"
#include "AutoRun.h"
#include "AutoRunDialog.h"
#include "StrProc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const DWORD CAutoRunDialog::FINISH_TIMER_ID = 1;
const DWORD CAutoRunDialog::FINISH_TIMER_INTERVAL = 1000;
const DWORD CAutoRunDialog::FINISH_TIMER_MAX_COUNT = 10;

bool CAutoRunDialog::CheckGameApp( LPCSTR pszMainClass, LPCSTR pszMainTitle )
{
  HWND hwndFind;
	hwndFind = ::FindWindow( pszMainClass, pszMainTitle );
  if ( hwndFind )
  {
    return false;
  }
  return true;
}

bool CAutoRunDialog::CheckPreviousApp( LPCSTR pszMainClass, LPCSTR pszMainTitle )
{
  HWND hwndFind, hwndLast, hwndForeGround;
  DWORD dwFindID, dwForeGroundID;
  hwndFind = ::FindWindow( pszMainClass, pszMainTitle );
  if ( hwndFind )
  {
    hwndForeGround = ::GetForegroundWindow();
    dwForeGroundID = ::GetWindowThreadProcessId( hwndForeGround, 0 );
    dwFindID = ::GetWindowThreadProcessId( hwndFind, 0 );
    if ( ( dwFindID != dwForeGroundID ) || ::IsIconic( hwndFind ) )
    {
      hwndLast = ::GetLastActivePopup( hwndFind );
      if ( ::IsIconic( hwndLast ) )
      {
				::ShowWindow( hwndLast, SW_RESTORE );
			}
      ::SetForegroundWindow( hwndLast );
    }
    return false;
  }
  return true;
}

void CAutoRunDialog::SetFinishTimer()
{
  KillFinishTimer();
  dwFinishTimer = SetTimer( FINISH_TIMER_ID, FINISH_TIMER_INTERVAL, 0 );
}

void CAutoRunDialog::KillFinishTimer()
{
  if ( dwFinishTimer != 0 )
	{
		KillTimer( dwFinishTimer );
		dwFinishTimer = 0;
	}
}

void CAutoRunDialog::OnFinishTimer()
{
	if ( !CheckGameApp( 0, menuSelector.GetRunningGameTitle() ) )
	{
		CDialog::OnOK();
	}
	else if ( !CheckGameApp( 0, menuSelector.GetRunningInstallTitle() ) )
	{
		CDialog::OnOK();
	}
	else
	{
		if ( dwFinishTimerCount < FINISH_TIMER_MAX_COUNT )
		{
			++dwFinishTimerCount;
			SetFinishTimer();
		}
	}
}

CAutoRunDialog::CAutoRunDialog( CWnd* pParent )
	: CDialog( CAutoRunDialog::IDD, pParent ), lastMousePoint( 0, 0 ), lastMouseFlags( 0 ), bMoveWindow( false ), dwFinishTimer( 0 ), dwFinishTimerCount( 0 )
{
	m_hIcon = AfxGetApp()->LoadIcon( IDI_AUTORUN_ICON );
}

void CAutoRunDialog::DoDataExchange( CDataExchange* pDX )
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAutoRunDialog, CDialog)
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_KEYDOWN()
	ON_WM_TIMER()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
END_MESSAGE_MAP()

bool CAutoRunDialog::Load()
{
	if ( !menuSelector.Load( GetDesktopWindow() ) )
	{
		MessageBox( NStr::Format( "Can't find <%s> file.", menuSelector.DATA_FILE_NAME ), "Error!", MB_ICONSTOP | MB_OK ); 
		return false;
	}
	
	if ( !CheckGameApp( 0, menuSelector.GetRunningGameTitle() ) )
	{
		return false;
	}
	
	if ( !CheckGameApp( 0, menuSelector.GetRunningInstallTitle() ) )
	{
		return false;
	}

	if ( !CheckPreviousApp( 0, menuSelector.GetTitle() ) )
	{
		return false;
	}
	return true;
}

BOOL CAutoRunDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	const CARBitmap* pARBitmap = menuSelector.dataStorage.GetBitmap( menuSelector.mainSection.szBackgroundImageFileName );
	if ( pARBitmap )
	{
		CRect windowRect;
		CRect clientRect;
		GetWindowRect( &windowRect );
		GetClientRect( &clientRect );
		
		const CPoint &rSize = pARBitmap->GetSize();

		int nWidth = ::GetSystemMetrics( SM_CXSCREEN );
		int nHeight = ::GetSystemMetrics( SM_CYSCREEN );
		int nDialogWidth = rSize.x + ( windowRect.Width() - clientRect.Width() );
		int nDialogHeight = rSize.y + ( windowRect.Height() - clientRect.Height() );
		MoveWindow ( ( ( nWidth - nDialogWidth ) / 2 ), ( ( nHeight - nDialogHeight ) / 2 ), nDialogWidth, nDialogHeight ); 
	}

	if ( menuSelector.mainSection.bShowToolTips )
	{
		tooltips.Create( this );
		tooltips.SetDelayTime( TTDT_AUTOPOP, 6000 );
		tooltips.SetDelayTime( TTDT_INITIAL, 1300 );
		tooltips.SetDelayTime( TTDT_RESHOW, 10 );   
		tooltips.Activate( true );

		menuSelector.FillToolTips( &tooltips, CPoint( -1, -1 ) );
	}
	menuSelector.PlayStartSound();

	SetWindowText( menuSelector.GetTitle() );
	SetIcon( m_hIcon, true );			// Set big icon
	SetIcon( m_hIcon, false );		// Set small icon
	return true;
}

void CAutoRunDialog::OnPaint() 
{
	CPaintDC paintDC( this );

	CRect clientRect;
	GetClientRect( &clientRect );

	CDC dc;
	dc.CreateCompatibleDC( &paintDC );
	CBitmap bmp;
	bmp.CreateCompatibleBitmap( &paintDC, clientRect.Width(), clientRect.Height() );
	CBitmap *pOldBitmap = dc.SelectObject( &bmp );
	
	menuSelector.DrawBackgroundAndLogos( &dc );
	menuSelector.DrawMenu( &dc, lastMousePoint, lastMouseFlags );

	paintDC.BitBlt( clientRect.left, clientRect.top, clientRect.Width(), clientRect.Height(), &dc, 0, 0, SRCCOPY );
	dc.SelectObject( pOldBitmap );
}

void CAutoRunDialog::OnMouseMove( UINT nFlags, CPoint point ) 
{
	lastMouseFlags = nFlags;
	CDialog::OnMouseMove(nFlags, point);
	if ( bMoveWindow )
	{
		CRect windowRect;
		GetWindowRect( & windowRect );
		MoveWindow( windowRect.left + ( point.x - lastMousePoint.x ), windowRect.top + ( point.y - lastMousePoint.y ), windowRect.Width(), windowRect.Height(), true );
	}
	else
	{
		lastMousePoint = point;
		RedrawWindow( 0, 0, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE );
	}
}

void CAutoRunDialog::OnLButtonUp( UINT nFlags, CPoint point ) 
{
	CDialog::OnLButtonUp(nFlags, point);
	lastMousePoint = point;
	lastMouseFlags = nFlags;
	bMoveWindow = false;
	if ( menuSelector.Action( this, lastMousePoint ) )
	{
		CDialog::OnOK();
		return;
	}
	if ( menuSelector.mainSection.bShowToolTips )
	{
		menuSelector.FillToolTips( &tooltips, point );
	}
	RedrawWindow( 0, 0, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE );
}

void CAutoRunDialog::OnLButtonDown( UINT nFlags, CPoint point ) 
{
	CDialog::OnLButtonDown(nFlags, point);
	lastMousePoint = point;
	lastMouseFlags = nFlags;
	if ( menuSelector.HitTest( lastMousePoint ) >= 0 )
	{
		bMoveWindow = false;
		RedrawWindow( 0, 0, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE );
	}
	else
	{
		bMoveWindow = true;
	}
}

void CAutoRunDialog::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags ) 
{
	if ( nChar == VK_RETURN )
	{
		OnLButtonUp( lastMouseFlags, lastMousePoint );
		return;
	}
	else if ( nChar == VK_ESCAPE )
	{
		if ( menuSelector.ReturnMenu() )
		{
			CDialog::OnOK();
		}
		else
		{
			if ( menuSelector.mainSection.bShowToolTips )
			{
				menuSelector.FillToolTips( &tooltips, lastMousePoint );
			}
			RedrawWindow( 0, 0, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE );
		}
		return;
	}
	else
	{
		CDialog::OnKeyDown(nChar, nRepCnt, nFlags);
	}
}

void CAutoRunDialog::OnOK() 
{
	OnKeyDown( VK_RETURN, 0, 0 );
}

void CAutoRunDialog::OnCancel() 
{
	OnKeyDown( VK_ESCAPE, 0, 0 );
}

BOOL CAutoRunDialog::PreTranslateMessage( MSG* pMsg ) 
{
	if ( IsWindow( tooltips ) )
	{
		if ( ( pMsg->message == WM_MOUSEMOVE ) ||
				 ( pMsg->message == WM_LBUTTONDOWN ) ||
				 ( pMsg->message == WM_LBUTTONUP ) ||
				 ( pMsg->message == WM_RBUTTONDOWN ) ||
				 ( pMsg->message == WM_MBUTTONDOWN ) ||
				 ( pMsg->message == WM_RBUTTONUP ) ||
				 ( pMsg->message == WM_MBUTTONUP ) )
		{
			tooltips.RelayEvent( pMsg );
		}
	}
	return CDialog::PreTranslateMessage( pMsg );
}

void CAutoRunDialog::OnTimer(UINT nIDEvent) 
{
  if ( nIDEvent == FINISH_TIMER_ID )
	{
		OnFinishTimer();
	}
	CDialog::OnTimer(nIDEvent);
}

void CAutoRunDialog::OnClose() 
{
	CDialog::OnOK();
}
