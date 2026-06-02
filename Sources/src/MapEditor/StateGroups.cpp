#include "stdafx.h"
#include "editor.h"
#include "StateGroups.h"
#include "frames.h"
#include "GameWnd.h"
#include "MainFrm.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

void CGroupsState::Enter()
{
	if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
	{
		pFrame->RedrawWindow();
	}
}

void CGroupsState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	if ( stateParameter.Update( CInputStateParameter::ISE_MOUSEMOVE, rMousePoint, pFrame ) )
	{
	}
}
