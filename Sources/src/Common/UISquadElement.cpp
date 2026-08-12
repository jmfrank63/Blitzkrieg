#include "StdAfx.h"

#include "UISquadElement.h"

#include "../GFX/GFX.H"
#include "../Scene/Scene.h"
#include "../GameTT/iMission.h"
#include "../GameTT/WorldClient.h"
#include "../Input/Input.h"
#include "Icons.h"
BASIC_REGISTER_CLASS( CUISquadElement );
void CUIUnitObserver::AddIcon( const int nType, ISceneIcon *pIcon )
{
	RemoveIcon( nType );
	icons.push_back( SIconDesc(nType, pIcon) );
}
void CUIUnitObserver::RemoveIcon( const int nType )
{
	for ( std::list<SIconDesc>::iterator it = icons.begin(); it != icons.end(); ++it )
	{
		if ( it->nType == nType ) 
		{
			icons.erase( it );
			return;
		}
	}
}
void CUIUnitObserver::UpdateHP( const float fValue )
{
	fHP = fValue;
}
void CUIUnitObserver::RemoveUnit()
{
	if ( pSquad ) 
		pSquad->RemovePassanger( this );
}
int CUIUnitObserver::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &icons );
	saver.Add( 2, &fHP );
	saver.Add( 3, &pSquad );
	saver.Add( 4, &pMOUnit );
	return 0;
}
CUISquadElement::CUISquadElement()
{
	nID = -1;
	bWindowActive = true;
	nCmdShow = 1;
	nPositionFlag = UIPLACE_LEFT | UIPLACE_TOP;
	bSelected = false;
	pSquadIcon = 0;
	vAppliedLayoutScale = CVec2( 1.0f, 1.0f );
}
void CUISquadElement::ScaleLayout( const CVec2 &vScale )
{
	vAppliedLayoutScale.x *= vScale.x;
	vAppliedLayoutScale.y *= vScale.y;
	vPos.x *= vScale.x;
	vPos.y *= vScale.y;
	vSize.x *= vScale.x;
	vSize.y *= vScale.y;
}
bool CUISquadElement::Update( const NTimer::STime &currTime )
{
	return false;
}
void CUISquadElement::Reposition( const CTRect<float> &rcParent )
{
	switch ( nPositionFlag & 0xf )
	{
		case UIPLACE_LEFT:
			rcWindow.x1 = rcParent.x1 + vPos.x;
			rcWindow.x2 = rcWindow.x1 + vSize.x;
			break;
		case UIPLACE_RIGHT:
			rcWindow.x1 = rcParent.x2 - vPos.x;
			rcWindow.x2 = rcWindow.x1 + vSize.x;
			break;
		case UIPLACE_HMID:
			rcWindow.x1 = (int) (rcParent.x1 + vPos.x + rcParent.Width()/2 - vSize.x/2);
			rcWindow.x2 = (int) (rcParent.x1 + vPos.x + rcParent.Width()/2 + vSize.x/2);
			break;
	}

	switch ( nPositionFlag & 0xf0 )
	{
		case UIPLACE_TOP:
			rcWindow.y1 = rcParent.y1 + vPos.y;
			rcWindow.y2 = rcWindow.y1 + vSize.y;
			break;
		case UIPLACE_BOTTOM:
			rcWindow.y1 = rcParent.y2 - vPos.y;
			rcWindow.y2 = rcWindow.y1 + vSize.y;
			break;
		case UIPLACE_VMID:
			rcWindow.y1 = (int) (rcParent.y1 + vPos.y + rcParent.Height()/2 - vSize.y/2);
			rcWindow.y2 = (int) (rcParent.y1 + vPos.y + rcParent.Height()/2 + vSize.y/2);
			break;
	}
}
CIconsVisitor visitor;
#define HP_BAR_WIDTH 2
#define ICONS_IN_COLUMN 3

void CUISquadElement::Visit( interface ISceneVisitor *pVisitor )
{
	if ( pSquadIcon == 0 )
	{
		pSquadIcon = GetSingleton<ITextureManager>()->GetTexture( "UI\\container" );
	}
	float fX = rcWindow.x1;
	const float fY = rcWindow.y2;
	SGFXRect2 rect;
	for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		SGFXRect2 rect;
		rect.rect.Set( fX, fY - rcWindow.Height(), fX + rcWindow.Height(), fY);
		rect.maps.Set( 0, 0, 1, 1 );
		rect.maps.Move( 0.5f / pSquadIcon->GetSizeX(0), 0.5f / pSquadIcon->GetSizeY(0) );
		if ( !(*(passangers.begin()))->GetMOUnit()->CanSelect() )
		{
			rect.color = 0xff808080;
		}
		pVisitor->VisitUIRects( pSquadIcon, 3, &rect, 1 );
		fX += rcWindow.Height();
	}
	vSize.x = fX;
	rcWindow.x2 = rcWindow.x1 + vSize.x;
	fX = rcWindow.x1;
	for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		const float fHP = (*it)->GetHP();
		rect.rect.Set( fX + 1, fY - 1 - HP_BAR_WIDTH, fX + (rcWindow.Height() - 2) * fHP, fY - 1 );
		rect.maps.SetEmpty();
		rect.color = MakeHPBarColor( fHP ); //((DWORD)(0xff00ff00 * fHP))|((DWORD)(0xffff0000 * (1 - fHP)));
		pVisitor->VisitUIRects( 0, 3, &rect, 1 );
		fX += rcWindow.Height();
	}
	fX = rcWindow.x1;
	int iconHeight = (rcWindow.Height() - 4) / ICONS_IN_COLUMN;
	for ( CPassangersList::const_iterator it = passangers.begin(); it != passangers.end(); ++it )
	{
		const std::list<SIconDesc> &icons = (*it)->GetIcons();
		int i = 0;
		for ( std::list<SIconDesc>::const_iterator icon = icons.begin(); icon != icons.end(); ++icon )
		{
			visitor.Clear();
			icon->pIcon->Visit( &visitor, 0x80000000 );
			int xOffset = 1 + iconHeight * ( i / ICONS_IN_COLUMN );
			if ( visitor.pInfo != 0 )
			{
				const SSpriteInfo *pInfo = visitor.pInfo;
				rect.rect.Set( fX + xOffset, fY - 4 - iconHeight * (ICONS_IN_COLUMN - i), fX + xOffset + iconHeight - 1, fY - 5 - iconHeight * (ICONS_IN_COLUMN - 1 - i) );
				rect.maps = pInfo->maps;
				rect.color = 0xffffffff;
				rect.specular = 0xff000000;
				pVisitor->VisitUIRects( pInfo->pTexture, 3, &rect, 1 );
				++i;
			}
		}
		fX += rcWindow.Height();
	}
}
void CUISquadElement::Draw( interface IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't call Draw() directly - use visit pattern" );
}
void CUISquadElement::SetWindowTexture( IGFXTexture *pTexture )
{
}
void CUISquadElement::SetWindowMap( const CTRect<float> &maps )
{
}
void CUISquadElement::SetWindowPlacement( const CVec2 *pvPos, const CVec2 *pvSize )
{
	if ( pvPos ) 
		vPos = *pvPos;
	if ( pvSize ) 
		vSize = *pvSize;
}
void CUISquadElement::SetWindowID( int _nID )
{
	nID = _nID;
}
void CUISquadElement::SetBoundRect( const CTRect<float> &rc )
{
}
bool CUISquadElement::IsInside( const CVec2 &vPos ) 
{
	return rcWindow.IsInside( vPos );
}
bool CUISquadElement::OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState )
{
	return false;
}
void CUISquadElement::SetParent( interface IUIContainer *_pParent )
{
	pParent = _pParent;
}
IUIContainer* CUISquadElement::GetParent()
{
	return pParent;
}
void CUISquadElement::SetWindowText( int nState, const WORD *pszText )
{
}
const WORD* CUISquadElement::GetWindowText( int nState )
{
	return 0;
}
void CUISquadElement::SetTextColor( DWORD dwColor )
{
}
interface IText* CUISquadElement::GetHelpContext( const CVec2 &vPos, CTRect<float> *pRect )
{
	return 0;
}
void CUISquadElement::SetHelpContext( int nState, const WORD *pszToolTipText )
{
}
void CUISquadElement::SetFocus( bool bFocus )
{
}
void CUISquadElement::EnableWindow( bool bEnable )
{
	bWindowActive = bEnable;
}
bool CUISquadElement::IsWindowEnabled()
{
	return bWindowActive;
}
void CUISquadElement::SetState( int nState, bool bNotify )
{
}
int  CUISquadElement::GetState()
{
	return 0;
}
bool CUISquadElement::IsVisible()
{
	return nCmdShow;
}
int CUISquadElement::GetVisibleState()
{
	return nCmdShow;
}
void CUISquadElement::ShowWindow( int _nCmdShow )
{
	nCmdShow = _nCmdShow;
}
int CUISquadElement:: GetWindowID()
{
	return nID;
}
int CUISquadElement::GetWindowPlacement( CVec2 *pPos, CVec2 *pSize, CTRect<float> *pScreenRect )
{
	if ( pPos ) 
		*pPos = vPos;
	if ( pSize ) 
		*pSize = vSize;
	if ( pScreenRect ) 
		*pScreenRect = rcWindow;
	return nPositionFlag;
}
int CUISquadElement::GetPositionFlag()
{
	return nPositionFlag;
}
bool CUISquadElement::ProcessMessage( const SUIMessage &msg )
{
	return false;
}
IUIElement* CUISquadElement::PickElement( const CVec2 &vPos, int nRecursion )
{
	return IsInside(vPos) ? this : 0;
}
IManipulator* CUISquadElement::GetManipulator()
{
	return 0;
}
int CUISquadElement::operator&( IDataTree &ss )
{
	return 0;
}
int CUISquadElement::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &rcWindow );
	saver.Add( 2, &nPositionFlag );
	saver.Add( 3, &vPos );
	saver.Add( 4, &vSize );
	saver.Add( 5, &nID );
	saver.Add( 6, &pParent );
	saver.Add( 7, &bWindowActive );
	saver.Add( 8, &nCmdShow );
	saver.Add( 9, &bSelected );
	saver.Add( 10, &passangers );
	saver.Add( 12, &pSquadIcon );
	return 0;
}
bool CUISquadElement::OnLButtonDblClk( const CVec2 &vPos )
{
	return IsInside( vPos );
}
bool CUISquadElement::OnMouseMove( const CVec2 &vPos, EMouseState mouseState )
{
	return IsInside( vPos );
}
bool CUISquadElement::OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta )
{
	return IsInside( vPos );
}
bool CUISquadElement::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	bSelected = !bSelected;
	if ( !passangers.empty() )
	{
		SGameMessage msg = SGameMessage( 0 );
		msg.nEventID = bSelected ? WCC_UI_SQUAD_SEL : WCC_UI_SQUAD_DESEL;
		if ( bSelected )
		{
			CUIUnitObserver *observer = *(passangers.begin());
			if ( observer->GetMOUnit()->CanSelect() )
			{
				msg.nParam = static_cast<int>( reinterpret_cast<std::uintptr_t>( observer->GetMOUnit() ) );
				GetSingleton<IInput>()->AddMessage( msg );
			}
		}
		else
		{
			for ( CPassangersList::iterator it = passangers.begin(); it != passangers.end(); ++it )
			{
				CUIUnitObserver *observer = *(it);
				if ( observer->GetMOUnit()->CanSelect() )
				{
					msg.nParam = static_cast<int>( reinterpret_cast<std::uintptr_t>( observer->GetMOUnit() ) );
					GetSingleton<IInput>()->AddMessage( msg );
				}
			}
		}
	}
	return true;
}
bool CUISquadElement::OnLButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	return true;
}
bool CUISquadElement::OnRButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	return true;
}
bool CUISquadElement::OnRButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	return true;
}
void CUISquadElement::AddPassanger( IUnitStateObserver *pObserver )
{
	passangers.push_back( dynamic_cast<CUIUnitObserver*>(pObserver) );
}
void CUISquadElement::RemovePassanger( IUnitStateObserver *pObserver )
{
	passangers.remove( dynamic_cast<CUIUnitObserver*>(pObserver) );
	if ( passangers.empty() ) 
		GetSingleton<IInput>()->AddMessage( SGameMessage(MC_UPDATE_WHO_IN_CONTAINER) );
}
int CUISquadElement::GetPassangerCount()
{
	return passangers.size();
}
