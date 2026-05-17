#include "StdAfx.h"
#include "..\GameTT\CommonId.h"
#include "UISlider.h"
#include "UIMessages.h"

const int TIME_TO_SCROLL = 50;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUISlider::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CSimpleWindow*>(this) );
	saver.Add( 2, &m_nMin );
	saver.Add( 3, &m_nMax );
	saver.Add( 4, &m_nPos );
	saver.Add( 5, &m_nStep );
	saver.Add( 6, &m_nElevatorWidth );
	saver.Add( 7, &m_nLineWidth );
	saver.Add( 8, &bVertical );
	saver.Add( 9, &pSliderTexture );
	saver.Add( 10, &sliderMapa );
	saver.Add( 11, &m_nKeyStep );
	saver.Add( 12, &m_nPrevPos );
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUISlider::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CSimpleWindow*>(this) );
	saver.Add( "Min", &m_nMin );
	saver.Add( "Max", &m_nMax );
	saver.Add( "Position", &m_nPos );
	saver.Add( "Step", &m_nStep );
	saver.Add( "Elevator_Width", &m_nElevatorWidth );
	saver.Add( "Width", &m_nLineWidth );
	saver.Add( "Vertical", &bVertical );
	saver.Add( "KeyStep", &m_nKeyStep );
	if ( saver.IsReading() )
		LoadTextureAndMap( &saver, &pSliderTexture, "Elevator_Texture", &sliderMapa, "Elevator_Maps" );
	else
		SaveTextureAndMap( &saver, pSliderTexture, "Elevator_Texture", sliderMapa, "Elevator_Maps" );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUISlider::OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState )
{
	if ( !IsVisible() )
		return false;
	if ( !bPressed )
		return false;

	switch ( nVirtualKey )
	{
		case VK_UP:
		case VK_LEFT:
		{
			DecPosition( m_nKeyStep );
			NotifyPositionChanged();
			return true;
		}

		case VK_DOWN:
		case VK_RIGHT:
		{
			IncPosition( m_nKeyStep );
			NotifyPositionChanged();
			return true;
		}

		default:
			return false;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUISlider::ProcessMessage( const SUIMessage &msg )
{
	//ScrollBar ������������ NOTIFY ��������� �� �����
	switch( msg.nMessageCode )
	{
	case MESSAGE_KEY_UP:
		{
			DecPosition( m_nKeyStep );
			NotifyPositionChanged();
			return true;
		}
	case MESSAGE_KEY_DOWN:
		{
			IncPosition( m_nKeyStep );
			NotifyPositionChanged();
			return true;
		}
	}

	return CSimpleWindow::ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUISlider::ComputeElevatorCoord()
{
	const CTRect<float> rc = GetScreenRect();

	if ( !bVertical )
	{
		if ( m_nMax == m_nMin )
			return rc.left;
		return rc.left + (rc.right-rc.left-m_nElevatorWidth)*m_nPos/(m_nMax-m_nMin);
	}
	else
	{
		if ( m_nMax == m_nMin )
			return rc.top;
		return rc.top + (rc.bottom-rc.top-m_nElevatorWidth)*m_nPos/(m_nMax-m_nMin);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUISlider::NotifyPositionChanged()
{
	if ( m_nPrevPos == m_nPos )
		return;

	m_nPrevPos = m_nPos;
	SUIMessage msg;
	msg.nMessageCode = UI_NOTIFY_POSITION_CHANGED;
	msg.nFirst = GetWindowID();
	msg.nSecond = m_nPos;
	GetParent()->ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUISlider::UpdatePosition( int nCoord )
{
	const CTRect<float> rc = GetScreenRect();

	if ( !bVertical )
	{
		if ( nCoord >= rc.right - m_nElevatorWidth/2 )
		{
			m_nPos = m_nMax;
			NotifyPositionChanged();
			return;
		}
		if ( nCoord <= rc.left + m_nElevatorWidth/2 )
		{
			m_nPos = m_nMin;
			NotifyPositionChanged();
			return;
		}

		float nNumberOfPositions = (float) ( m_nMax - m_nMin ) / m_nStep;
		float nSizeOfInterval = (float)  ( rc.right - rc.left - m_nElevatorWidth ) / ( nNumberOfPositions );
		m_nPos = (float) ( nCoord - rc.left - m_nElevatorWidth/2 + nSizeOfInterval/2 ) / nSizeOfInterval;
		m_nPos *= m_nStep;
	}
	else
	{
		if ( nCoord >= rc.bottom - m_nElevatorWidth/2 )
		{
			m_nPos = m_nMax;
			NotifyPositionChanged();
			return;
		}
		if ( nCoord <= rc.top + m_nElevatorWidth/2 )
		{
			m_nPos = m_nMin;
			NotifyPositionChanged();
			return;
		}
		
		float nNumberOfPositions = (float) ( m_nMax - m_nMin ) / m_nStep;
		float nSizeOfInterval = (float)  ( rc.bottom - rc.top - m_nElevatorWidth ) / ( nNumberOfPositions );
		m_nPos = (float) ( nCoord - rc.top - m_nElevatorWidth/2 + nSizeOfInterval/2 ) / nSizeOfInterval;
//		m_nPos = (float) ( nCoord - rc.top ) / nSizeOfInterval;
		m_nPos *= m_nStep;
	}

	if ( m_nPos > m_nMax )
		m_nPos = m_nMax;
	else if ( m_nPos < m_nMin )
		m_nPos = m_nMin;

	NotifyPositionChanged();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUISlider::Visit( interface ISceneVisitor *pVisitor )
{
	CSimpleWindow::Visit( pVisitor );

	if ( m_nMin < m_nMax )
	{
		// ������ ������ ��������
		// ����� �������������� ���     nMin <= nPosition <= nMax
		SGFXRect2 rect;
		rect.rect = GetScreenRect();
		if ( !bVertical )
		{
			rect.rect.left = ComputeElevatorCoord();
			rect.rect.right = rect.rect.left + m_nElevatorWidth;
		}
		else
		{
			rect.rect.top = ComputeElevatorCoord();
			rect.rect.bottom = rect.rect.top + m_nElevatorWidth;
		}
		rect.maps = sliderMapa;
		rect.fZ = 0.0f;
		
		pVisitor->VisitUIRects( pSliderTexture, 3, &rect, 1 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUISlider::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;

	CSimpleWindow::Draw( pGFX );

	if ( m_nMin < m_nMax )
	{
		//������ ������ ��������
		//����� �������������� ���     nMin <= nPosition <= nMax
		pGFX->SetShadingEffect( 3 );
		SGFXRect2 rect;
		rect.rect = GetScreenRect();
		if ( !bVertical )
		{
			rect.rect.left = ComputeElevatorCoord();
			rect.rect.right = rect.rect.left + m_nElevatorWidth;
		}
		else
		{
			rect.rect.top = ComputeElevatorCoord();
			rect.rect.bottom = rect.rect.top + m_nElevatorWidth;
		}
		rect.maps = sliderMapa;
		pGFX->SetTexture( 0, pSliderTexture );
		rect.fZ = 0.0f;
		
		pGFX->DrawRects( &rect, 1 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUISlider::OnMouseMove( const CVec2 &vPos, EMouseState mouseState )
{
	bool bRes = CSimpleWindow::OnMouseMove( vPos, mouseState );

	if ( mouseState == E_MOUSE_FREE )
		return bRes;

	//��� ������, ��� bRes true ����� �������� ����� ���� ����������, � ������ ����� ��� ������ ������, ��� ������ ����������� ����
	//���� ����� ������ ����� ������
	if ( mouseState & E_LBUTTONDOWN )
	{
		if ( !bVertical )
		{
			UpdatePosition( vPos.x );
		}
		else
		{
			UpdatePosition( vPos.y );
		}
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUISlider::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	bool bRes = CSimpleWindow::OnMouseMove( vPos, mouseState );

	if ( mouseState == E_MOUSE_FREE || !bRes )
		return bRes;

	//��� ������, ��� bRes true ����� �������� ����� ���� ����������, � ������ ����� ��� ������ ������, ��� ������ ����������� ����
	//���� ����� ������ ����� ������
	if ( mouseState & E_LBUTTONDOWN )
	{
		if ( !bVertical )
		{
			UpdatePosition( vPos.x );
		}
		else
		{
			UpdatePosition( vPos.y );
		}
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUISlider::SetPosition( int nPos )
{
	if ( nPos <= m_nMin )
	{
		m_nPos = m_nMin;
	}
	else if ( nPos >= m_nMax )
	{
		m_nPos = m_nMax;
	}
	else
		m_nPos = nPos;
	NotifyPositionChanged();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUISlider::SetMinValue( int nMin ) 
{ 
	m_nMin = nMin; 

	if ( m_nPos < m_nMin )
		SetPosition( m_nMin );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUISlider::SetMaxValue( int nMax ) 
{ 
	m_nMax = nMax; 
	if ( m_nPos > m_nMax )
		SetPosition( m_nMax );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollBar::NotifyPositionChanged()
{
	SUIMessage msg;
	msg.nMessageCode = UI_NOTIFY_POSITION_CHANGED;
	msg.nFirst = GetWindowID();
	msg.nSecond = pSlider->GetPosition();
	GetParent()->ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScrollBar::ProcessMessage( const SUIMessage &msg )
{
	//ScrollBar ������������ NOTIFY ��������� �� �����
	switch( msg.nMessageCode )
	{
	case UI_NOTIFY_WINDOW_CLICKED:
		if ( msg.nFirst == 1 )			//min ������
		{
			dwLastUpdateTime = GetSingleton<IGameTimer>()->GetAbsTime();
			pSlider->DecPosition( m_nButtonStep );
			NotifyPositionChanged();
			GetSingleton<IScene>()->AddSound( "int_scroller", VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );

		}
		else if ( msg.nFirst == 2 )
		{
			dwLastUpdateTime = GetSingleton<IGameTimer>()->GetAbsTime();
			pSlider->IncPosition( m_nButtonStep );
			NotifyPositionChanged();
			GetSingleton<IScene>()->AddSound( "int_scroller", VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
		}
		else
			NI_ASSERT_T( 0, "Invalid notify message in ScrollBar" );
		return true;

	case UI_NOTIFY_POSITION_CHANGED:
		NotifyPositionChanged();
		return true;
	}

	return CMultipleWindow::ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollBar::Reposition( const CTRect<float> &rcParent )
{
	CSimpleWindow::Reposition( rcParent );
	
	CTRect<float> rc, newRc, myRc;
	myRc = GetScreenRect();
	
	//������ ������� ������ ������� ��� ������������ ScrollBar
	if ( !IsVertical() )
	{
		rc = pMinButton->GetScreenRect();
		newRc.left = myRc.left;
		newRc.right = newRc.left + pMinButton->GetSize().x;
		newRc.top = myRc.top;
		newRc.bottom = myRc.bottom;
		pMinButton->SetScreenRect( newRc );
		pMinButton->SetPos( CVec2( 0, 0 ) );
		pMinButton->SetSize( CVec2( pMinButton->GetSize().x, myRc.Height() ) );
		pMinButton->UpdateSubRects();

		rc = pMaxButton->GetScreenRect();
		newRc.right = myRc.right;
		newRc.left = newRc.right - pMaxButton->GetSize().x;
		pMaxButton->SetScreenRect( newRc );
		pMaxButton->SetPos( CVec2( myRc.Width() - pMaxButton->GetSize().x, 0 ) );
		pMaxButton->SetSize( CVec2( pMaxButton->GetSize().x, myRc.Height() ) );
		pMaxButton->UpdateSubRects();
		
		rc = pSlider->GetScreenRect();
		newRc.right = newRc.left;
		newRc.left = myRc.left + pMinButton->GetSize().x;
		pSlider->SetScreenRect( newRc );
		pSlider->SetPos( CVec2( pMinButton->GetSize().x, 0 ) );
		pSlider->SetSize( CVec2( myRc.Width() - pMinButton->GetSize().x - pMaxButton->GetSize().x, myRc.Height() ) );
//		pSlider->UpdateSubRects();
	}
	else
	{
		rc = pMinButton->GetScreenRect();
		newRc.top = myRc.top;
		newRc.bottom = newRc.top + pMinButton->GetSize().y;
		newRc.left = myRc.left;
		newRc.right = myRc.right;
		pMinButton->SetScreenRect( newRc );
		pMinButton->SetPos( CVec2( 0, 0 ) );
		pMinButton->SetSize( CVec2( myRc.Width(), pMinButton->GetSize().y ) );
		pMinButton->UpdateSubRects();
		
		rc = pMaxButton->GetScreenRect();
		newRc.bottom = myRc.bottom;
		newRc.top = newRc.bottom - pMaxButton->GetSize().y;
		pMaxButton->SetScreenRect( newRc );
		pMaxButton->SetPos( CVec2( 0, myRc.Height() - pMaxButton->GetSize().y ) );
		pMaxButton->SetSize( CVec2( myRc.Width(), pMaxButton->GetSize().y ) );
		pMaxButton->UpdateSubRects();
		
		rc = pSlider->GetScreenRect();
		newRc.bottom = newRc.top;
		newRc.top = myRc.top + pMinButton->GetSize().y;
		pSlider->SetScreenRect( newRc );
		pSlider->SetPos( CVec2( 0, pMinButton->GetSize().y ) );
		pSlider->SetSize( CVec2( myRc.Width(), myRc.Height() - pMinButton->GetSize().y - pMaxButton->GetSize().y ) );
//		pSlider->UpdateSubRects();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIScrollBar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMultipleWindow*>(this) );
	saver.Add( 2, &m_nButtonStep );
	
	if ( !saver.IsReading() )
	{
		CPtr<IUIElement> pElement = dynamic_cast<IUIElement *> ( pMinButton );
		saver.Add( 3, &pElement );
		pElement = dynamic_cast<IUIElement *> ( pMaxButton );
		saver.Add( 4, &pElement );
		pElement = dynamic_cast<IUIElement *> ( pSlider );
		saver.Add( 5, &pElement );
	}
	else
	{
		CPtr<IUIElement> pElement;
		saver.Add( 3, &pElement );
		pMinButton = dynamic_cast<CUIButton *> ( pElement.GetPtr() );
		saver.Add( 4, &pElement );
		pMaxButton = dynamic_cast<CUIButton *> ( pElement.GetPtr() );
		saver.Add( 5, &pElement );
		pSlider = dynamic_cast<CUISlider *> ( pElement.GetPtr() );
		NI_ASSERT_T( pMinButton && pMaxButton && pSlider, "Invalid data for ScrollBar, can not create elements" );
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIScrollBar::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CMultipleWindow*>(this) );
	saver.Add( "ButtonStep", &m_nButtonStep );
	
	if ( saver.IsReading() )
	{
		//�������������� ���������� ����������
		pMinButton = dynamic_cast<CUIButton *>( GetChildByID(1) );
		pMaxButton = dynamic_cast<CUIButton *>( GetChildByID(2) );
		pSlider = dynamic_cast<CUISlider *>( GetChildByID(3) );
	}
	NI_ASSERT_T( pMinButton && pMaxButton && pSlider, "Invalid data for ScrollBar, can not create elements" );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScrollBar::Update( const NTimer::STime &currTime )
{
	//��������, ����� min ��� max ������ � ������� ������������
	if ( pMinButton->GetCurrentSubState() == E_PUSHED_STATE )
	{
		if ( currTime - dwLastUpdateTime > TIME_TO_SCROLL )
		{
			dwLastUpdateTime += TIME_TO_SCROLL;
			pSlider->DecPosition( m_nButtonStep );
			NotifyPositionChanged();
		}
		return true;
	}

	if ( pMaxButton->GetCurrentSubState() == E_PUSHED_STATE )
	{
		if ( currTime - dwLastUpdateTime > TIME_TO_SCROLL )
		{
			dwLastUpdateTime += TIME_TO_SCROLL;
			pSlider->IncPosition( m_nButtonStep );
			NotifyPositionChanged();
		}
		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
