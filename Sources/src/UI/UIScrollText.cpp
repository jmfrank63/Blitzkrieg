#include "StdAfx.h"
#include "UIMessages.h"
#include "UIScrollText.h"

//const int GLAD = 20;			//��� �������� ��������������
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollTextBox::SetWindowText( int nState, const WORD *pszText )
{
	NI_ASSERT_T( nState < states.size(), NStr::Format("Can't set window text for %d stats (max %d)", nState, states.size()) );
	IText *pText = states[nState].pGfxText->GetText();
	int nCurrentLength = pText->GetLength();
	pText->SetText( pszText );
	states[nState].pGfxText->SetText( pText );
	states[nState].pGfxText->SetColor( dwTextColor );

	// ����������, ������� ������� ���������� ��� ����������� ����� ������
	// ������ ����������, ����� ScrollBar ���������
	states[nState].pGfxText->SetWidth( wndRect.Width() - nLeftSpace - nRightSpace );
	int nNumLines = states[nState].pGfxText->GetNumLines();					//����� ����� �����
	
	int nLineHeigth = states[nState].pGfxText->GetLineSpace();			//������ ����� �������
	int nNumberOfScreenStrings = ( wndRect.Height() - nTopSpace - nBottomSpace ) / nLineHeigth;		//���������� �������, ������������ �� ������ ��������

	if ( !bScrollBarAlwaysVisible )
	{
		if ( nNumberOfScreenStrings > nNumLines )
		{
			pScrollBar->ShowWindow( UI_SW_HIDE );
			//CRAP{ CHECK IF IT WILL NOT LED TO BUGS
			//return;
			//CRAP}
		}
		else
			pScrollBar->ShowWindow( UI_SW_SHOW );
	}

	// �������������� ScrollBar
	states[nState].pGfxText->SetWidth( wndRect.Width() - pScrollBar->wndRect.Width() - nLeftSpace - nRightSpace );
	nNumLines = states[nState].pGfxText->GetNumLines();							//����� ����� �����

	nLineHeigth = states[nState].pGfxText->GetLineSpace();					//������ ����� �������
	nNumberOfScreenStrings = ( wndRect.Height() - nTopSpace - nBottomSpace ) / nLineHeigth;				//���������� �������, ������������ �� ������ ��������
	if ( !nNumberOfScreenStrings )
		nNumberOfScreenStrings++;																			//����� �� ���� ������� �� ����
	int nMax = nNumLines - nNumberOfScreenStrings;									//������������ ������� ��� ����������
	if ( nMax < 0 )
		nMax = 0;

	m_nY = 0;															// do display new text from the beginning

	UpdateScrollBar( nMax * states[nState].pGfxText->GetLineSpace(), 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollTextBox::	UpdateScrollBar( const int nMaxValue, const int nCurValue )
{
	pScrollBar->SetMinValue( 0 );
	pScrollBar->SetMaxValue( nMaxValue );
	pScrollBar->SetPosition( nCurValue );
	m_nY = - nCurValue;

	pScrollBar->SetStep( 1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollTextBox::AppendText( const WORD *pszText )
{
	bool bNeedScrollToEnd = pScrollBar->GetPosition() == pScrollBar->GetMaxValue();

	IText *pText = states[nCurrentState].pGfxText->GetText();
	std::wstring wszTemp = reinterpret_cast<const wchar_t*>(pText->GetString());
	wszTemp += reinterpret_cast<const wchar_t*>(pszText);

	SetWindowText( nCurrentState, reinterpret_cast<const WORD*>(wszTemp.c_str()) );
	
	//������� ������� ScrollBar, ���� ��� ����������
	if ( bNeedScrollToEnd )
	{
		pScrollBar->SetPosition( pScrollBar->GetMaxValue() );
		m_nY = -pScrollBar->GetPosition();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollTextBox::GetBorderRect( CTRect<float> *pBorderRect ) const
{
	*pBorderRect = wndRect;
	pBorderRect->left += nLeftSpace;
	pBorderRect->right -= nRightSpace;
	pBorderRect->top += nTopSpace;
	pBorderRect->bottom -= nBottomSpace;
	if ( pScrollBar->IsVisible() )
		pBorderRect->right -= pScrollBar->wndRect.Width();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollTextBox::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !nCmdShow )
		return;

	// ��� ������� ���������� �������������� ��� ��������� ���������, ������� ��� �� ��������� ���� �������� ��, �������� CUIScrollTextBox
	VisitBackground( pVisitor );
	
	// ������ �����
	for ( CWindowList::reverse_iterator ri = childList.rbegin(); ri != childList.rend(); ++ri )
		(*ri)->Visit( pVisitor );
	
	// ������ �����
	if ( states[nCurrentState].pGfxText )
	{
		CTRect<float> textRC;
		GetBorderRect( &textRC );
		pVisitor->VisitUIText( states[nCurrentState].pGfxText, textRC, m_nY, 0, FNT_FORMAT_JUSTIFY );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollTextBox::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;
	
	if ( !nCmdShow )
		return;

	//��� ������� ���������� �������������� ��� ��������� ���������, ������� ��� �� ��������� ���� �������� ��, �������� CUIScrollTextBox
	pGFX->SetShadingEffect( 3 );
	DrawBackground( pGFX );
	
	//������ �����
	for ( CWindowList::reverse_iterator ri=childList.rbegin(); ri!=childList.rend(); ri++ )
		(*ri)->Draw( pGFX );
	
	//������ �����
	if ( states[nCurrentState].pGfxText )
	{
		CTRect<float> textRC;
		GetBorderRect( &textRC );
		pGFX->DrawText( states[nCurrentState].pGfxText, textRC, m_nY, FNT_FORMAT_JUSTIFY );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIScrollTextBox::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CMultipleWindow*>(this) );
	
	//	saver.Add( "Left_Scrollbar", &bLeftScrollBar );
	//	saver.Add( "Scrollbar_Always_Visible", &bScrollBarAlwaysVisible );
	saver.Add( "ScrollBarWidth", &nScrollBarWidth );
	saver.Add( "LeftSpace", &nLeftSpace );
	saver.Add( "RightSpace", &nRightSpace );
	saver.Add( "TopSpace", &nTopSpace );
	saver.Add( "BottomSpace", &nBottomSpace );
	saver.Add( "SBVisible", &bScrollBarAlwaysVisible );

	if ( saver.IsReading() )
	{
		//�������������� pScrollBar
		pScrollBar = dynamic_cast<CUIScrollBar *>( GetChildByID(1) );
		NI_ASSERT_T( pScrollBar != 0, "can't find scroll bar" );

		for ( int i=0; i<states.size(); i++ )
		{
			if ( states[i].pGfxText )
			{
				states[i].pGfxText->EnableRedLine( bRedLine );
			}
		}
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIScrollTextBox::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMultipleWindow*>(this) );
	saver.Add( 2, &nScrollBarWidth );
	saver.Add( 4, &m_nY );
	saver.Add( 5, &nLeftSpace );
	saver.Add( 6, &nTopSpace );
	saver.Add( 7, &nBottomSpace );
	saver.Add( 8, &nRightSpace );
	saver.Add( 9, &bScrollBarAlwaysVisible );
	
	if ( !saver.IsReading() )
	{
		CPtr<IUIElement> pElement = dynamic_cast<IUIElement *> ( pScrollBar );
		saver.Add( 3, &pElement );
	}
	else
	{
		CPtr<IUIElement> pElement;
		saver.Add( 3, &pElement );
		pScrollBar = dynamic_cast<CUIScrollBar *> ( pElement.GetPtr() );
		NI_ASSERT_T( pScrollBar != 0, "No ScrollBar control, can not create CUIScrollTextBox" );
	}
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollTextBox::RepositionScrollbar()
{
	CVec2 size = pScrollBar->GetSize();
	pScrollBar->SetPos( CVec2(0, 0) );
	pScrollBar->SetSize( CVec2(size.x, GetSize().y ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollTextBox::RepositionText()
{
	//������� �����
	IText *pText = states[0].pGfxText->GetText();
	std::wstring szTempString = reinterpret_cast<const wchar_t*>(pText->GetString());
	SetWindowText( 0, reinterpret_cast<const WORD*>(szTempString.c_str()) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIScrollTextBox::Reposition( const CTRect<float> &rcParent )
{
	RepositionScrollbar();
	CMultipleWindow::Reposition( rcParent );
	RepositionText();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScrollTextBox::ProcessMessage( const SUIMessage &msg )
{
	//Scroll Text ������������ NOTIFY ��������� �� ScrollBar
	switch( msg.nMessageCode )
	{
	case UI_NOTIFY_POSITION_CHANGED:
		// �������� �����
		m_nY = -pScrollBar->GetPosition();

		return true;
	}
	
	return CMultipleWindow::ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIScrollTextBox::OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta )
{
	if ( !IsInside( vPos ) )
		return false;

	if ( !pScrollBar )
		return false;

	pScrollBar->SetPosition( pScrollBar->GetPosition() + fDelta*GetMouseWheelMultiplyer() );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
