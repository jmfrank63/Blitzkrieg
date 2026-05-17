#include "stdafx.h"

#include "ResizeDialog.h"
#include "..\RandomMapGen\Registry_Types.h"
#include "..\RandomMapGen\Resource_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const DWORD CResizeDialog::ANCHORE_LEFT					= 0x001;
const DWORD CResizeDialog::ANCHORE_TOP					= 0x002;
const DWORD CResizeDialog::ANCHORE_RIGHT				= 0x004;
const DWORD CResizeDialog::ANCHORE_BOTTOM				= 0x008;
const DWORD CResizeDialog::RESIZE_HOR						= 0x010;
const DWORD CResizeDialog::RESIZE_VER						= 0x020;
const DWORD CResizeDialog::ANCHORE_HOR_CENTER		= 0x040;
const DWORD CResizeDialog::ANCHORE_VER_CENTER		= 0x080;

const DWORD CResizeDialog::ANCHORE_LEFT_TOP					= CResizeDialog::ANCHORE_LEFT | CResizeDialog::ANCHORE_TOP;
const DWORD CResizeDialog::ANCHORE_RIGHT_TOP				= CResizeDialog::ANCHORE_RIGHT | CResizeDialog::ANCHORE_TOP;
const DWORD CResizeDialog::ANCHORE_LEFT_BOTTOM			= CResizeDialog::ANCHORE_LEFT | CResizeDialog::ANCHORE_BOTTOM;
const DWORD CResizeDialog::ANCHORE_RIGHT_BOTTOM			= CResizeDialog::ANCHORE_RIGHT | CResizeDialog::ANCHORE_BOTTOM;
const DWORD CResizeDialog::RESIZE_HOR_VER						= CResizeDialog::RESIZE_HOR | CResizeDialog::RESIZE_VER;
const DWORD CResizeDialog::ANCHORE_HOR_VER_CENTER		= CResizeDialog::ANCHORE_HOR_CENTER | CResizeDialog::ANCHORE_VER_CENTER;

const DWORD CResizeDialog::DEFAULT_STYLE = CResizeDialog::ANCHORE_LEFT_TOP;
const std::string CResizeDialog::szOptionsFileName = std::string( "Editor\\ResizeDialogStyles\\" );

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STDCALL CResizeDialog::SOptions::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;

	saver.Add( 1, &rect );	
	saver.Add( 2, &nParameters );	
	saver.Add( 3, &szParameters );	
	saver.Add( 4, &fParameters );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STDCALL CResizeDialog::SOptions::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss; 

	saver.Add( "Rect", &rect );	
	saver.Add( "IntParameterss", &nParameters );	
	saver.Add( "StringParameters", &szParameters );	
	saver.Add( "FloatParameters", &fParameters );

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CResizeDialog::CResizeDialog( UINT nIDTemplate, CWnd* pParent )
	: CDialog( nIDTemplate, pParent ), resizeDialogOriginalSize( 0, 0 )
{
	//{{AFX_DATA_INIT( CResizeDialog )
	//}}AFX_DATA_INIT
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CResizeDialog::DoDataExchange( CDataExchange* pDX )
{
	CDialog::DoDataExchange( pDX );
	//{{AFX_DATA_MAP(CResizeDialog)
	//}}AFX_DATA_MAP
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP( CResizeDialog, CDialog )
	//{{AFX_MSG_MAP( CResizeDialog )
	ON_WM_SIZE()
	ON_WM_SIZING()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CResizeDialog::OnInitDialog() 
{
	CDialog ::OnInitDialog();

	//�������� ������� � ������� ���������
	UpdateControlPositions();

	LoadResizeDialogOptions();
	if ( ( resizeDialogOptions.rect.Width() != 0 ) && ( resizeDialogOptions.rect.Height() != 0 ) )
	{
		MoveWindow( resizeDialogOptions.rect.minx,
								resizeDialogOptions.rect.miny,
								resizeDialogOptions.rect.Width(),
								resizeDialogOptions.rect.Height() );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CResizeDialog::UpdateControlPositions()
{
	RECT clientRect;
	GetClientRect( &clientRect );

 	resizeDialogOriginalSize.x = clientRect.right - clientRect.left;
	resizeDialogOriginalSize.y = clientRect.bottom - clientRect.top;

	for ( std::unordered_map<UINT, SControlStyle>::iterator controlStyleIterator = resizeDialogControlStyles.begin();
				controlStyleIterator != resizeDialogControlStyles.end();
				++controlStyleIterator )
	{
		if ( CWnd* pControlWnd = GetDlgItem( controlStyleIterator->first ) )
		{
			RECT controlWindowRect;
			pControlWnd->GetWindowRect( &controlWindowRect );
			ScreenToClient( &controlWindowRect );

			controlStyleIterator->second.position = CTRect<int>( controlWindowRect );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CResizeDialog::OnOK() 
{
	CRect rect;
	GetWindowRect( &rect );
	resizeDialogOptions.rect = rect;
	SaveResizeDialogOptions();
	CDialog::OnOK();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CResizeDialog::OnCancel() 
{
	CRect rect;
	GetWindowRect( &rect );
	resizeDialogOptions.rect = rect;
	SaveResizeDialogOptions();
	CDialog::OnCancel();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CResizeDialog::SetControlStyle( UINT nControlID, DWORD dwStyle, float fHorCenterAnchorRatio, float fVerCenterAnchorRatio, float fHorResizeRatio, float fVerResizeRatio )
{
	SControlStyle controlStyle;
	controlStyle.position = CTRect<int>( 0, 0 ,0, 0 );
	controlStyle.dwStyle = dwStyle;
	controlStyle.fHorCenterAnchorRatio = fHorCenterAnchorRatio; 
	controlStyle.fVerCenterAnchorRatio = fVerCenterAnchorRatio; 
	controlStyle.fHorResizeRatio = fHorResizeRatio;
	controlStyle.fVerResizeRatio = fVerResizeRatio;
	resizeDialogControlStyles[nControlID] = controlStyle;
}

//ANCHORE_LEFT				resize - ������������ ������ ���� ( ����������� ����� - 0.0f )
//ANCHORE_RIGHT				resize - ������������ ������� ���� ( ����������� ����� - 1.0f )
//ANCHORE_HOR_CENTER	resize - ������������ ����������� ����� ( ����������� ����� - fHorCenterAnchorRatio)
//RESIZE_HOR					������������� ������ ������ �� fHorResizeRatio
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CResizeDialog::OnSize( UINT nType, int cx, int cy ) 
{
	CDialog::OnSize( nType, cx, cy );
	
	CTPoint<int> dSize( ( cx - resizeDialogOriginalSize.x ), ( cy - resizeDialogOriginalSize.y ) );
	for ( std::unordered_map<UINT, SControlStyle>::iterator controlStyleIterator = resizeDialogControlStyles.begin();
				controlStyleIterator != resizeDialogControlStyles.end();
				++controlStyleIterator )
	{
		if ( CWnd* pControlWnd = GetDlgItem( controlStyleIterator->first ) )
		{
			RECT controlWindowRect;
			pControlWnd->GetWindowRect( &controlWindowRect );
			ScreenToClient( &controlWindowRect );

			CTRect<int> newPosition( controlStyleIterator->second.position );
			CTRect<int> oldPosition( controlStyleIterator->second.position );

			
			if ( ( controlStyleIterator->second.dwStyle & ANCHORE_LEFT ) == ANCHORE_LEFT )
			{
				if ( ( controlStyleIterator->second.dwStyle & RESIZE_HOR ) == RESIZE_HOR )
				{
					newPosition.minx = oldPosition.minx + dSize.x * 0.0f;
					newPosition.maxx = oldPosition.maxx + dSize.x * controlStyleIterator->second.fHorResizeRatio;
				}
				else
				{
					//do nothing
					newPosition.minx = oldPosition.minx + dSize.x * 0.0f;
					newPosition.maxx = oldPosition.maxx + dSize.x * 0.0f;
				}
			}
			else if ( ( controlStyleIterator->second.dwStyle & ANCHORE_RIGHT ) == ANCHORE_RIGHT )
			{
				if ( ( controlStyleIterator->second.dwStyle & RESIZE_HOR ) == RESIZE_HOR )
				{
					newPosition.minx = oldPosition.minx + dSize.x * ( 1.0f - controlStyleIterator->second.fHorResizeRatio );
					newPosition.maxx = oldPosition.maxx + dSize.x * 1.0f;
				}
				else
				{
					newPosition.minx = oldPosition.minx + dSize.x * 1.0f;
					newPosition.maxx = oldPosition.maxx + dSize.x * 1.0f;
				}
			
			}
			else if ( ( controlStyleIterator->second.dwStyle & ANCHORE_HOR_CENTER ) == ANCHORE_HOR_CENTER )
			{
				if ( ( controlStyleIterator->second.dwStyle & RESIZE_HOR ) == RESIZE_HOR )
				{
					newPosition.minx = oldPosition.minx + dSize.x * ( controlStyleIterator->second.fHorCenterAnchorRatio - ( controlStyleIterator->second.fHorResizeRatio / 2.0f ) );
					newPosition.maxx = oldPosition.maxx + dSize.x * ( controlStyleIterator->second.fHorCenterAnchorRatio + ( controlStyleIterator->second.fHorResizeRatio / 2.0f ) );
				}
				else
				{
					newPosition.minx = oldPosition.minx + dSize.x * controlStyleIterator->second.fHorCenterAnchorRatio;
					newPosition.maxx = oldPosition.maxx + dSize.x * controlStyleIterator->second.fHorCenterAnchorRatio;
				}
			}
			//
			if ( ( controlStyleIterator->second.dwStyle & ANCHORE_TOP ) == ANCHORE_TOP )
			{
				if ( ( controlStyleIterator->second.dwStyle & RESIZE_VER ) == RESIZE_VER )
				{
					newPosition.miny = oldPosition.miny + dSize.y * 0.0f;
					newPosition.maxy = oldPosition.maxy + dSize.y * controlStyleIterator->second.fVerResizeRatio;
				}
				else
				{
					//do nothing
					newPosition.miny = oldPosition.miny + dSize.y * 0.0f;
					newPosition.maxy = oldPosition.maxy + dSize.y * 0.0f;
				}
			}
			else if ( ( controlStyleIterator->second.dwStyle & ANCHORE_BOTTOM ) == ANCHORE_BOTTOM )
			{
				if ( ( controlStyleIterator->second.dwStyle & RESIZE_VER ) == RESIZE_VER )
				{
					newPosition.miny = oldPosition.miny + dSize.y * ( 1.0f - controlStyleIterator->second.fVerResizeRatio );
					newPosition.maxy = oldPosition.maxy + dSize.y * 1.0f;
				}
				else
				{
					newPosition.miny = oldPosition.miny + dSize.y * 1.0f;
					newPosition.maxy = oldPosition.maxy + dSize.y * 1.0f;
				}
			
			}
			else if ( ( controlStyleIterator->second.dwStyle & ANCHORE_VER_CENTER ) == ANCHORE_VER_CENTER )
			{
				if ( ( controlStyleIterator->second.dwStyle & RESIZE_VER ) == RESIZE_VER )
				{
					newPosition.miny = oldPosition.miny + dSize.y * ( controlStyleIterator->second.fVerCenterAnchorRatio - ( controlStyleIterator->second.fVerResizeRatio / 2.0f ) );
					newPosition.maxy = oldPosition.maxy + dSize.y * ( controlStyleIterator->second.fVerCenterAnchorRatio + ( controlStyleIterator->second.fVerResizeRatio / 2.0f ) );
				}
				else
				{
					newPosition.miny = oldPosition.miny + dSize.y * controlStyleIterator->second.fVerCenterAnchorRatio;
					newPosition.maxy = oldPosition.maxy + dSize.y * controlStyleIterator->second.fVerCenterAnchorRatio;
				}
			}
			
			//
			if ( newPosition.minx > newPosition.maxx )
			{
				 newPosition.maxx = newPosition.minx;
			}
			if ( newPosition.miny > newPosition.maxy )
			{
				 newPosition.maxy = newPosition.miny;
			}
			//
			pControlWnd->MoveWindow( newPosition.minx, newPosition.miny, newPosition.Width(), newPosition.Height() );
		}
	}

  InvalidateRect( 0, true );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CResizeDialog::OnSizing( UINT fwSide, LPRECT pRect ) 
{
	CDialog::OnSizing( fwSide, pRect );
	
	//CRect mainWndRect;
	//GetClientRect( &mainWndRect );
	//CRect newWndRect;
	//GetWindowRect( &newWndRect );

	RECT clientRect;
	GetClientRect( &clientRect );
	RECT windowRect;
	GetWindowRect( &windowRect );


	LONG nXSize = clientRect.right - clientRect.left;
	LONG nYSize = clientRect.bottom - clientRect.top;
	nXSize = ( pRect->right - pRect->left ) - ( ( windowRect.right - windowRect.left ) - nXSize );
	nYSize = ( pRect->bottom - pRect->top ) - ( ( windowRect.bottom - windowRect.top ) - nYSize );

	if ( nXSize < GetMinimumXDimension() )
	{
	if ( ( fwSide == WMSZ_LEFT ) ||
		   ( fwSide == WMSZ_TOPLEFT ) ||
		   ( fwSide == WMSZ_BOTTOMLEFT ) )
		{
			pRect->left = pRect->right - GetMinimumXDimension() - ( ( pRect->right - pRect->left ) - nXSize );
		}
		else
		{
			pRect->right = pRect->left + GetMinimumXDimension() + ( ( pRect->right - pRect->left ) - nXSize );
		}
	}
	if ( nYSize < GetMinimumYDimension() )
	{
		if ( (fwSide == WMSZ_TOP ) ||
		     ( fwSide == WMSZ_TOPLEFT ) ||
		     ( fwSide == WMSZ_TOPRIGHT ) )
		{
			pRect->top = pRect->bottom - GetMinimumYDimension() - ( ( pRect->bottom - pRect->top ) - nYSize );
		}
		else
		{
			pRect->bottom = pRect->top + GetMinimumYDimension() + ( ( pRect->bottom - pRect->top ) - nYSize );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CResizeDialog::LoadResizeDialogOptions()
{
	if ( SerializeToRegistry() )
	{
		std::string szRegistryKey = GetRegistryKey();
		if ( !szRegistryKey.empty() )
		{
			CRegistrySection registrySection( HKEY_CURRENT_USER, KEY_READ, szRegistryKey.c_str() );
			
			registrySection.LoadRect( _T( "Rect" ), _T( "%d" ), &( resizeDialogOptions.rect ), CTRect<int>( 0, 0, 0, 0 ) );
			int nParameters = 0;
			int szParameters = 0;
			int fParameters = 0;
			registrySection.LoadNumber( _T( "nParams" ), _T( "%d" ), &nParameters, 0 );
			registrySection.LoadNumber( _T( "szParams" ), _T( "%d" ), &szParameters, 0 );
			registrySection.LoadNumber( _T( "fParams" ), _T( "%d" ), &fParameters, 0 );
			for ( int nParameterIndex = 0; nParameterIndex < nParameters; ++nParameterIndex )
			{
				resizeDialogOptions.nParameters.push_back( 0 );
				std::string szFormat = NStr::Format( _T( "nParam%d" ), nParameterIndex );
				registrySection.LoadNumber( szFormat.c_str(), _T( "%d" ), &( resizeDialogOptions.nParameters[nParameterIndex] ), 0 );
			}
			for ( int nParameterIndex = 0; nParameterIndex < szParameters; ++nParameterIndex )
			{
				resizeDialogOptions.szParameters.push_back( "" );
				std::string szFormat = NStr::Format( _T( "szParam%d" ), nParameterIndex );
				registrySection.LoadString( szFormat.c_str(), &( resizeDialogOptions.szParameters[nParameterIndex] ), "" );
			}
			for ( int nParameterIndex = 0; nParameterIndex < fParameters; ++nParameterIndex )
			{
				resizeDialogOptions.fParameters.push_back( 0.0f );
				std::string szFormat = NStr::Format( _T( "fParam%d" ), nParameterIndex );
				registrySection.LoadNumber( szFormat.c_str(), _T( "%g" ), &( resizeDialogOptions.fParameters[nParameterIndex] ), 0.0f );
			}
		}
	}
	else
	{
		std::string szLabel = GetXMLOptionsLabel();
		if ( !szLabel.empty() )
		{
			LoadDataResource( szOptionsFileName + szLabel, "", false, 0, szLabel, resizeDialogOptions );
		}
	}
}
	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CResizeDialog::SaveResizeDialogOptions()
{
	if ( SerializeToRegistry() )
	{
		std::string szRegistryKey = GetRegistryKey();
		if ( !szRegistryKey.empty() )
		{
			CRegistrySection registrySection( HKEY_CURRENT_USER, KEY_WRITE, szRegistryKey.c_str() );
			
			registrySection.SaveRect( _T( "Rect" ), _T( "%d" ), resizeDialogOptions.rect );
			registrySection.SaveNumber( _T( "nParams" ), _T( "%d" ), resizeDialogOptions.nParameters.size() );
			registrySection.SaveNumber( _T( "szParams" ), _T( "%d" ), resizeDialogOptions.szParameters.size() );
			registrySection.SaveNumber( _T( "fParams" ), _T( "%d" ), resizeDialogOptions.fParameters.size() );
			for ( int nParameterIndex = 0; nParameterIndex < resizeDialogOptions.nParameters.size(); ++nParameterIndex )
			{
				std::string szFormat = NStr::Format( _T ( "nParam%d" ), nParameterIndex );
				registrySection.SaveNumber( szFormat.c_str(), _T( "%d" ), resizeDialogOptions.nParameters[nParameterIndex] );
			}
			for ( int nParameterIndex = 0; nParameterIndex < resizeDialogOptions.szParameters.size(); ++nParameterIndex )
			{
				std::string szFormat = NStr::Format( _T( "szParam%d" ), nParameterIndex );
				registrySection.SaveString( szFormat.c_str(), resizeDialogOptions.szParameters[nParameterIndex] );
			}
			for ( int nParameterIndex = 0; nParameterIndex < resizeDialogOptions.fParameters.size(); ++nParameterIndex )
			{
				std::string szFormat = NStr::Format( _T( "fParam%d" ), nParameterIndex );
				registrySection.SaveNumber( szFormat.c_str(), _T( "%g" ), resizeDialogOptions.fParameters[nParameterIndex] );
			}
		}
	}
	else
	{
		std::string szLabel = GetXMLOptionsLabel();
		if ( !szLabel.empty() )
		{
			SaveDataResource( szOptionsFileName + szLabel, "", false, 0, szLabel, resizeDialogOptions );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// basement storage  
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CResizeDialog::OnPaint() 
{
	CPaintDC dc( this );
	
	if ( GetDrawGripper() )
	{
		CRect clientRect;
		GetClientRect( &clientRect );

		clientRect.left = clientRect.right - ::GetSystemMetrics( SM_CXHSCROLL );
		clientRect.top = clientRect.bottom - ::GetSystemMetrics( SM_CYVSCROLL );

		dc.DrawFrameControl( clientRect, DFC_SCROLL, DFCS_SCROLLSIZEGRIP );
	}
}
