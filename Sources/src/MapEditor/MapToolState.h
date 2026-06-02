#if !defined(AFX_MAPTOOLSTATE_H__EE853783_30FB_4FBE_BB6D_DFC01BAB7653__INCLUDED_)
#define AFX_MAPTOOLSTATE_H__EE853783_30FB_4FBE_BB6D_DFC01BAB7653__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "InputState.h"
namespace toolStateConsts
{ 
	const int nRepair = 0;
	const int nArea = 1;

	const int nRectType = 0;
	const int nCircleType = 1;
}

class CMapToolState : public IInputState
{
	CInputStateParameter stateParameter;

	virtual void STDCALL Enter();
	virtual void STDCALL Leave();
	virtual void STDCALL Update();

	virtual void STDCALL  OnLButtonDown	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnRButtonDown	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnMButtonDown	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnLButtonUp		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL	OnMouseMove		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
};
#endif // !defined(AFX_MAPTOOLSTATE_H__EE853783_30FB_4FBE_BB6D_DFC01BAB7653__INCLUDED_)
