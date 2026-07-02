#include "StdAfx.h"
#include "..\TreeItem.h"
#include "PropView.h"

#define ID_OI 515


BEGIN_MESSAGE_MAP(CPropView, SECControlBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()


CPropView::CPropView()
{
	pActiveTreeItem = 0;
}

CPropView::~CPropView()
{
}
int CPropView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  if (SECControlBar::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  RECT rect;
	
  GetClientRect( &rect );
  m_wndOI.Create( 0, "ObjectInspector", WS_CHILD | WS_VISIBLE, rect, this, ID_OI );
  m_wndOI.SetWindowPos( 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOSIZE );

	return 0;
}

BOOL CPropView::PreTranslateMessage( MSG* pMsg )
{
  if ( pMsg->message == WM_USER + 1 )
	{
		GetParent()->PostMessage( WM_USERCHANGEPARAM );
		UpdateValue( pMsg->wParam );
		return true;
	}

  return SECControlBar::PreTranslateMessage( pMsg );
}

BOOL CPropView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if ( ::IsWindow( m_wndOI.GetSafeHwnd() ) )
		return m_wndOI.SendMessage( WM_MOUSEWHEEL, MAKEWPARAM( nFlags, zDelta ), MAKELPARAM( pt.x, pt.y ) ) != 0;
	return SECControlBar::OnMouseWheel( nFlags, zDelta, pt );
}

void CPropView::OnSize(UINT nType, int cx, int cy) 
{
  SECControlBar::OnSize(nType, cx, cy);
  
  CRect rectInside;
  GetInsideRect(rectInside);
  ::SetWindowPos(m_wndOI, NULL, rectInside.left, rectInside.top,
    rectInside.Width(), rectInside.Height(),
    SWP_NOZORDER|SWP_NOACTIVATE);
}

/*
void CPropView::UpdatePropList()
{
  if ( !pPropMap )
    return;
  m_OIDlg.m_wndOI.ClearAll();
  if ( pPropMap->empty() )
    return;
  int nItemID = pPropMap->begin()->second->GetOwnerID();
  string szGroup = string( "General (ID = " ) + IToA( nItemID ) + " )";
  m_OIDlg.m_wndOI.SetGroup( 0, szGroup.c_str() );

  for ( CPropMap::const_iterator it = pPropMap->begin(); it != pPropMap->end(); ++it )
  {
    const SResTree *pRelation = theApp.GetResTree( it->second->GetRelation() );
    CVariant var;
    GroupID  gid = 0;
    DomenID  nViewType = it->second->GetViewType();
    bool     bReadOnly = false;
    if ( pRelation )
    {
      var = pRelation->pItemsTree->GetItemName( it->second->GetValue() );
      gid = it->second->GetGroup();
			nViewType = DT_RELATION;
      if ( gid )
        m_OIDlg.m_wndOI.SetGroup( gid, pRelation->szTabName, true );
    }
    else
    {
      var = it->second->GetValue();
    }
    string szPref = theApp.GetResSrcDir() + it->second->GetPrefix();
    m_OIDlg.m_wndOI.AddPropertiesValue( it->second->GetID(), nViewType, it->first, var, gid, bReadOnly, szPref.c_str() );
    if ( DT_COMBO == nViewType )
    {
      const vector<string> &v = it->second->GetStrings();
      for ( int i = 0; i < v.size(); ++i )
        m_OIDlg.m_wndOI.AddPropertyString( it->second->GetID(), v[i] );
    }
  }
  m_OIDlg.m_wndOI.Invalidate();
}
void CPropView::SetPropMap( const CPropMap *_pPropMap )
{ 
	m_OIDlg.m_wndOI.ClearAll();
  pPropMap = _pPropMap;
  UpdatePropList();
}
int CPropView::GetActiveProp( int nGroupID )
{
  return m_OIDlg.m_wndOI.GetActiveProp( nGroupID );
}
void CPropView::UpdateProperty( int nPropID )
{
  if ( !pPropMap )
    return;

  for ( CPropMap::const_iterator it = pPropMap->begin(); it != pPropMap->end(); ++it )
  {
    if ( it->second->GetID() == nPropID )
    {
      const SResTree *pRelation = theApp.GetResTree( it->second->GetRelation() );
      CVariant var;
      if ( pRelation )
      {
        var = pRelation->pItemsTree->GetItemName( it->second->GetValue() );
      }
      else
        var = it->second->GetValue();
      m_OIDlg.m_wndOI.SetPropertiesValue( nPropID, var );
    }
  }
}
void CPropView::SetActiveProp( int nPropID )
{
	m_OIDlg.m_wndOI.SetActiveProp( nPropID );
}
*/

void CPropView::SetItemProperty( const char *szItemName, CTreeItem *pProp )
{
	PropID nActiveProp = m_wndOI.GetMyActiveProp();
	if ( nActiveProp != -1 )
	{
		UpdateValue( nActiveProp );
	}
	
	m_wndOI.ClearAll();
	ASSERT( pProp != 0 );
	pActiveTreeItem = pProp;
	
	m_wndOI.SetGroup( 1, szItemName );
	for ( CPropVector::iterator it=pProp->values.begin(); it!=pProp->values.end(); ++it )
	{
		m_wndOI.AddPropertiesValue( it->nId, it->nDomenType, it->szDisplayName, it->value, 1 );
		if ( it->nDomenType == DT_COMBO || it->nDomenType == DT_BROWSE || it->nDomenType == DT_BROWSEDIR )
		{
			for ( vector<string>::iterator st=it->szStrings.begin(); st!=it->szStrings.end(); ++st )
				m_wndOI.AddPropertyString( it->nId, *st);
		}
	}
}

void CPropView::UpdateValue( PropID nID )
{
	if ( pActiveTreeItem )
	{
		string szName = m_wndOI.GetPropertyName( nID );
		if ( szName.size() == 0 )
			return;			//ERROR

		CPropVector::iterator it = pActiveTreeItem->values.begin();
		for ( ; it!=pActiveTreeItem->values.end(); ++it )
		{
			if ( nID == it->nId )
			{
				CVariant newValue = m_wndOI.GetPropertyValue( nID );
				if ( it->value != newValue )
					pActiveTreeItem->UpdateItemValue( nID, newValue );
				return;
			}
		}
	}
}
