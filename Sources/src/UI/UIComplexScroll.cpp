#include "StdAfx.h"

#include "UIComplexScroll.h"
#include "UIMessages.h"
#include "UISlider.h"
#include "UIDialog.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum
{
	E_ELEMENT_CONTAINER				= 5,
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIComplexScroll::UpdateScrollBar( const int nMaxValue, const int nCurValue )
{
	pScrollBar->SetMinValue( 0 );
	pScrollBar->SetMaxValue( nMaxValue );
	pScrollBar->SetPosition( nCurValue );
	m_nY = - nCurValue;

	pScrollBar->SetStep( 1 );
	
	UpdatePosition();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIComplexScroll::CUIComplexScroll()
: nCurrentPosToAdd( 0 ), m_nY( 0 )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIComplexScroll::GetBorderRect( CTRect<float> *pBorderRect ) const
{
	GetWindowRect( pBorderRect );
	pBorderRect->left += nLeftSpace;
	pBorderRect->right -= nRightSpace;
	pBorderRect->top += nTopSpace;
	pBorderRect->bottom -= nBottomSpace;
	if ( pScrollBar->IsVisible() )
		pBorderRect->right -= pScrollBar->GetWidth();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIComplexScroll::Draw( IGFX *pGFX )
{
	NI_ASSERT_T( false, "wrong call" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIComplexScroll::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !GetCmdShow() )
		return;

	// ��� ������� ���������� �������������� ��� ��������� ���������, ������� ��� �� ��������� ���� �������� ��, �������� CUIComplexScroll
	VisitBackground( pVisitor );
	
	CWindowList & children = GetChildList();
	// ������ �����
	for ( CWindowList::reverse_iterator ri = children.rbegin(); ri != children.rend(); ++ri )
		(*ri)->Visit( pVisitor );
	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIComplexScroll::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CMultipleWindow*>(this) );
	saver.Add( "ScrollBarWidth", &nScrollBarWidth );
	saver.Add( "LeftSpace", &nLeftSpace );
	saver.Add( "RightSpace", &nRightSpace );
	saver.Add( "TopSpace", &nTopSpace );
	saver.Add( "BottomSpace", &nBottomSpace );
	saver.Add( "SBVisible", &bScrollBarAlwaysVisible );
	saver.Add( "VSubSpace", &nVSubSpace );

	if ( saver.IsReading() )
	{
		//�������������� pScrollBar
		pScrollBar = dynamic_cast<CUIScrollBar *>( GetChildByID(1) );
		NI_ASSERT_T( pScrollBar != 0, "can't find scroll bar" );
		
		pItemContainer = checked_cast<IUIDialog *>( GetChildByID(E_ELEMENT_CONTAINER) );
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIComplexScroll::operator&( IStructureSaver &ss )
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
	saver.Add( 10, &pItemContainer );
	saver.Add( 11, &nVSubSpace );
	
	if ( !saver.IsReading() )
	{
		CPtr<IUIElement> pElement = dynamic_cast<IUIElement *> ( pScrollBar );
		saver.Add( 3, &pElement );
		pElement = pItemContainer;
		saver.Add( 4, &pElement );
	}
	else
	{
		CPtr<IUIElement> pElement;
		saver.Add( 3, &pElement );
		pScrollBar = dynamic_cast<CUIScrollBar *> ( pElement.GetPtr() );
		saver.Add( 4, &pElement );
		pItemContainer = dynamic_cast_ptr<IUIDialog *>( pElement );
		NI_ASSERT_T( pScrollBar != 0, "No ScrollBar control, can not create CUIComplexScroll" );
	}
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIComplexScroll::RepositionScrollbar()
{
	CVec2 size = pScrollBar->GetSize();
	pScrollBar->SetPos( CVec2(0, 0) );
	pScrollBar->SetSize( CVec2( size.x, GetSize().y ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIComplexScroll::Reposition( const CTRect<float> &rcParent )
{
	RepositionScrollbar();
	CMultipleWindow::Reposition( rcParent );

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIComplexScroll::ProcessMessage( const SUIMessage &msg )
{
	//Scroll Text ������������ NOTIFY ��������� �� ScrollBar
	switch( msg.nMessageCode )
	{
	case UI_NOTIFY_POSITION_CHANGED:
		UpdatePosition();

		return true;
	}
	
	return CMultipleWindow::ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIComplexScroll::UpdatePosition()
{
	CVec2 vPos;
	m_nY = - pScrollBar->GetPosition();
	pItemContainer->GetWindowPlacement( &vPos, 0, 0 );
	vPos.y = m_nY + nTopSpace;
	pItemContainer->SetWindowPlacement( &vPos, 0 );
	
	CTRect<float> rc;
	GetParent()->GetWindowPlacement( 0, 0, &rc );
	Reposition( rc );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIComplexScroll::OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta )
{
	if ( !IsInside( vPos ) )
		return false;

	if ( !pScrollBar )
		return false;

	pScrollBar->SetPosition( pScrollBar->GetPosition() + fDelta*GetMouseWheelMultiplyer() );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIComplexScroll::AddItem( IUIElement *pElement, const bool bResizeToFitText )
{
	// bound rect for container
	CTRect<float> rect;
	GetBorderRect( &rect );
	
	if ( bResizeToFitText )
	{
		CVec2 vSize( rect.Width(), rect.Height() );
		pElement->SetWindowPlacement( 0, &vSize );

		// resize element by X and see the size of 
		int nNewX = rect.Width(), nNewY = rect.Height();
		pElement->GetTextSize( pElement->GetState(), &nNewX, &nNewY );
		const CVec2 vNewSize( nNewX, nNewY );
		pElement->SetWindowPlacement( 0, &vNewSize );
	}

	// add window to current position in container
	const CVec2 vPos( 0, nCurrentPosToAdd );
	pElement->SetWindowPlacement( &vPos, 0 );
	pItemContainer->AddChild( pElement );

	CVec2 vElementSize;
	pElement->GetWindowPlacement( 0, &vElementSize, 0 );

	
	// resize container to fit new element
	CVec2 vContainerSize;
	pItemContainer->GetWindowPlacement( 0, &vContainerSize, 0 );
	vContainerSize.y = nCurrentPosToAdd + vElementSize.y;
	
	const CVec2 vContainerPos( nLeftSpace, nTopSpace );
	pItemContainer->SetWindowPlacement( &vContainerPos, &vContainerSize );

	const int nSize = Max( vContainerSize.y - rect.Height(), 0.0f );
	UpdateScrollBar( nSize, 0 );
	pItemContainer->SetBoundRect( rect );

	nCurrentPosToAdd = vContainerSize.y + nVSubSpace;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIComplexScroll::Clear()
{
	pItemContainer->RemoveAllChildren();
	nCurrentPosToAdd = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
