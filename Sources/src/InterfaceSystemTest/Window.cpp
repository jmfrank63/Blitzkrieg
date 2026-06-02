
#include "stdafx.h"
#include "Window.h"


#include "WindowMultiBkg.h"
#include "Background.h"
#include "UIScreen.h"
#include "WindowTextView.h"
#include "WindowEditLine.h"
BASIC_REGISTER_CLASS(CWindow);
bool SWindowCompare::operator()( const CDCPtr<CWindow> &o1, const CDCPtr<CWindow> &o2 ) const
{ 
	return o1->GetPriority() < o2->GetPriority(); 
}


void CWindow::InitStatic()
{
	handleMap["UI_SHOW_WINDOW"] = CUIMessageHandler( CWindow::ShowWindow );
	handleMap["MC_TEXT_MODE"] = CUIMessageHandler( CWindow::SwitchTextMode );
}
void CWindow::Init( int TEST )
{
	szTooltip = "tooltip1";
	
	bVisible = true;
	nPriority = 100;
	vChildPos = CVec2( 0,0 );
	vSize = CVec2( 100, 100 );
	nVerAllign = EPA_LOW_END;
	nHorAllign = EPA_LOW_END;
	
	if ( TEST == 1 )
	{
		szName = "Parent1";
	}
	else
		szName = "Child1";

	SetBackground( new CBackgroundPlainTexture );
}
int CWindow::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;

}
void CWindow::SetBackground( IBackground *_pBackground )
{	
	pBackground = _pBackground; 
	if ( pBackground )
		pBackground->SetPos( vScreenPos, vSize );
}
class CScreen * CWindow::GetScreen()
{
	if ( GetParent() == 0 )
		return dynamic_cast<CScreen*>( this );
	else return GetParent()->GetScreen();
}
CWindow* CWindow::GetParent() 
{ 
	return pParent; 
}
void CWindow::RemoveFocus()
{
	if ( pFocused )
	{
		pFocused->RemoveFocus();
		pFocused = 0;
	}
}
void CWindow::SetFocused( CWindow *pChild, const bool bFocus )
{
	if ( bFocus )
	{
		if ( pFocused && (CWindow*)pFocused != (CWindow*)pChild )
			pFocused->RemoveFocus();
		pFocused = pChild;
	}
	else
		pFocused = 0;
	if ( GetParent() )
		GetParent()->SetFocused( this, bFocus );
}
void CWindow::SetModal( CWindow *pChild )
{
	pModal = pChild;
	if ( GetParent() )
		GetParent()->SetModal( this );
}
bool CWindow::IsVisible() const 
{ 
	return bVisible; 
}
int CWindow::GetPriority() const 
{ 
	return nPriority; 
}
const std::string& CWindow::GetName() const 
{ 
	return szName; 
}
void CWindow::SetName( const std::string &_szName )
{
	szName = _szName;
}
int CWindow::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;

	saver.Add( "Position", &vChildPos );
	int nV = nVerAllign;
	saver.Add( "VerAlign", &nV );
	nVerAllign = (EPositionAllign) nV;
	int nH = nHorAllign;
	saver.Add( "HorAlign", &nH );
	nHorAllign = (EPositionAllign) nH;
	saver.Add( "Size", &vSize );
	saver.Add( "Name", &szName );
	saver.Add( "Priority", &nPriority );
	saver.Add( "Background", &pBackground );
	saver.Add( "ToolTip", &szTooltip );
	saver.Add( "Visible", &bVisible );
	
	std::list< CObj<CWindow> > cl;

	if ( saver.IsReading() )
	{
		saver.Add( "Children", &cl );
		for ( std::list< CObj<CWindow> >::iterator it = cl.begin(); it != cl.end(); ++it )
			AddChild( *it );
	}
	else
	{
		for ( int i = 0; i < drawOrder.Size(); ++i )
			cl.push_back( drawOrder[i] );

		saver.Add( "Children", &cl );
	}

	return 0;
}
void CWindow::Init()
{
	for ( int i = 0; i < drawOrder.Size(); ++i )
		drawOrder[i]->Init();
}
void CWindow::Reposition( const CTRect<float> &parentRect )
{
	const CVec2 vParentPos( parentRect.left, parentRect.top );
	const CVec2 vParentSize( parentRect.right-parentRect.left, parentRect.bottom - parentRect.top );
	switch( nHorAllign )
	{
	case 	EPA_LOW_END:
		vScreenPos.x = vParentPos.x + vChildPos.x;
		break;
	case 	ERA_CENTER:
		vScreenPos.x = vParentPos.x + (vParentSize.x - vSize.x ) / 2 + vChildPos.x;
		break;
	case 	EPA_HIGH_END:
		vScreenPos.x = vParentPos.x + vParentSize.x - vSize.x + vChildPos.x;
		break;
	}
	switch( nVerAllign )
	{
	case 	EPA_LOW_END:
		vScreenPos.y = vParentPos.y + vChildPos.y;
		break;
	case 	ERA_CENTER:
		vScreenPos.y = vParentPos.y + (vParentSize.y - vSize.y ) / 2 + vChildPos.y;
		break;
	case 	EPA_HIGH_END:
		vScreenPos.y = vParentPos.y + vParentSize.y - vSize.y + vChildPos.y;
		break;
	}
	
	if ( pBackground )
		pBackground->SetPos( vScreenPos, vSize );
	RepositionChildren();
}
void CWindow::RepositionChildren()
{
	RECT rCurrent;
	rCurrent.top = vScreenPos.y;
	rCurrent.left = vScreenPos.x;
	rCurrent.right = vScreenPos.x + vSize.x;
	rCurrent.bottom = vScreenPos.y + vSize.y;
	for ( int i = 0; i < drawOrder.Size(); ++i )
	{
		drawOrder[i]->Reposition( rCurrent );
	}
}
void CWindow::GetPlacement( int *pX, int *pY, int *pSizeX, int *pSizeY ) const
{
	if ( pX )
		*pX = vChildPos.x;
	if ( pY )
		*pY = vChildPos.y;
	if ( pSizeX )
		*pSizeX = vSize.x;
	if ( pSizeY )
		*pSizeY = vSize.y;
}
void CWindow::FillWindowRect( CTRect<float> *pRect ) const
{
	pRect->top = vScreenPos.y;
	pRect->left = vScreenPos.x;
	pRect->right = vSize.x + vScreenPos.x;
	pRect->bottom = vSize.y + vScreenPos.y;
}
void CWindow::SetPlacement( int x, int y, int sizeX, int sizeY, const DWORD flags ) 
{
	if ( flags & EWPF_POS_X )
	{
		vScreenPos.x += x - vChildPos.x;
		vChildPos.x = x;
	}
	if ( flags & EWPF_POS_Y )
	{
		vScreenPos.y += y - vChildPos.y;
		vChildPos.y = y;
	}
	if ( flags & EWPF_SIZE_X )
		vSize.x = sizeX;
	if ( flags & EWPF_SIZE_Y )
		vSize.y = sizeY;

	if ( pBackground && flags != 0 )
		pBackground->SetPos( vScreenPos, vSize );
	
	RepositionChildren();
}

void CWindow::AddChild( CWindow *pWnd )
{
	if ( drawOrder.GetReserved() <= drawOrder.Size() + 1 )
		drawOrder.Reserve( drawOrder.Size() + Max( 10, drawOrder.Size() ) );
	drawOrder.Push( pWnd ); 
	pWnd->SetParent( this );
	children.insert( pWnd->GetName() );
}
/*void CWindow::RemoveChild( const std::string &_szName )
{
	CChildren::iterator it = children.find( _szName );
	if ( it != children.end() )
	{
		drawOrder.Remove( it->second );
		children.erase( it );
	}
}*/
CWindow* CWindow::GetChild( const std::string &_szName )
{
	if ( _szName.empty() ) return 0;
	
	CChildren::iterator it = children.find( _szName );
	if ( it != children.end() )
	{
		for ( int i = 0; i < drawOrder.Size(); ++i )
			if ( _szName == drawOrder[i]->GetName() )
				return drawOrder[i];
	}
	return 0;
}
CWindow* CWindow::GetDeepChild( const std::string &_szName )
{
	CWindow *pRet = GetChild( _szName );
	if ( !pRet ) // not immidiate child
	{
		for ( int i = drawOrder.Size() -1; i >= 0; --i )
			if ( pRet = drawOrder[i]->GetDeepChild( _szName ) )
				return pRet;
	}
	return 0;
}
void CWindow::SetParent( CWindow *_pParent )
{
	pParent = _pParent;
}
void CWindow::OnButtonDown( const CVec2 &vPos, const int nButton )
{
	NI_ASSERT_T( nButton >= 0, NStr::Format( "don't understand such buttons %i", nButton) );
	pressed.resize( Max( pressed.size(), (unsigned int)nButton + 1 ) );

	pressed[nButton] = PickInternal( vPos );
	if ( pressed[nButton] )
		pressed[nButton]->OnButtonDown( vPos, nButton );
}
void CWindow::OnButtonUp( const CVec2 &vPos, const int nButton )
{
	NI_ASSERT_T( nButton >= 0, NStr::Format( "don't understand such buttons %i", nButton) );
	pressed.resize( Max( pressed.size(), (unsigned int)nButton + 1 ) );
	if ( pressed[nButton] )
		pressed[nButton]->OnButtonUp( vPos, nButton );
	pressed[nButton] = 0;
}
void CWindow::OnButtonDblClk( const CVec2 &vPos, const int nButton )
{
	NI_ASSERT_T( nButton >= 0, NStr::Format( "don't understand such buttons %i", nButton) );
}
void CWindow::OnMouseMove( const CVec2 &_vPos, const int nButton )
{
	switch( nButton )
	{
	case MSTATE_FREE:		// no button pressed
		{
			CWindow *pNewHighligted = 0;
			if ( _vPos.x < 0 || _vPos.y < 0 )
			{
				if ( pHighlighted )
					pHighlighted->OnMouseMove( _vPos, nButton );
				pHighlighted = 0;
			}
			else if ( !pHighlighted )
			{
				pHighlighted = PickInternal( _vPos );
			}
			else if ( (pNewHighligted = PickInternal( _vPos )) != pHighlighted )
			{
				if ( pHighlighted )	// remove highlight
					pHighlighted->OnMouseMove( CVec2(-1,-1), nButton );
				pHighlighted = pNewHighligted;
			}
		}
		
		if ( pHighlighted )
			pHighlighted->OnMouseMove( _vPos, nButton );
		break;
	default:
		if ( pFocused )
			pFocused->OnMouseMove( _vPos, nButton );
	
		if ( pModal )
			pModal->OnMouseMove( _vPos, nButton );
		else
		{
			for ( int i = drawOrder.Size() - 1; i >=0; --i )
			{
				if ( drawOrder[i] != pFocused )
					drawOrder[i]->OnMouseMove( _vPos, nButton );
			}
		}
	}
}
void CWindow::OnChar( const wchar_t chr )
{
	if ( pFocused )
		pFocused->OnChar( chr );
}
CWindow* CWindow::PickInternal( const CVec2 &vPos )
{
	for ( int i = drawOrder.Size() - 1; i >= 0 ; --i )
	{
		if ( drawOrder[i]->IsInside( vPos ) ) 
			return drawOrder[i];
	}
	return 0;
}
IWindow* CWindow::Pick( const CVec2 &vPos )
{
	return PickInternal( vPos );
}
IManipulator* CWindow::GetManipulator()
{
	NI_ASSERT_T( false, "not implemented" );
	return 0;
}
IText* CWindow::GetHelpContext()
{
	if ( !szTooltip.empty() )
	{
		NI_ASSERT_T( false, "not implemented" );
		return 0;
	}
	return 0;
}
void CWindow::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !IsVisible() )
		return;

	if ( pBackground )
		pBackground->Visit( pVisitor );
	
	for ( int i = 0; i < drawOrder.Size(); ++i )
	{
		if ( drawOrder[i]->IsVisible() )
			drawOrder[i]->Visit( pVisitor );
	}
}
bool CWindow::IsInside( const CVec2 &vPos ) const
{
	return vPos.x >= vScreenPos.x && 
				 vPos.y >= vScreenPos.y &&
				 vPos.x < vScreenPos.x + vSize.x &&
				 vPos.y < vScreenPos.y + vSize.y;
}
bool CWindow::ProcessMessage( const struct SBUIMessage &msg )
{
	int nSize = handleMap.size();
	HM_TYPE::iterator it = handleMap.find( msg.szMessageID );
	bool bRes = false;
	
	NI_ASSERT_T( it != handleMap.end(), NStr::Format( "window recieves unregistered message \"%s\"", msg.szMessageID.c_str() ) );
	if ( it != handleMap.end() )
	{
		bRes = it->second.Execute( msg, this );
	}

	if ( !bRes ) // this window doesn't process message, direct it to children
	{
		for ( int i = drawOrder.Size() - 1; i >= 0; --i )
			if ( drawOrder[i]->ProcessMessage( msg ) )
				return true;
	}
	return false;	
}
