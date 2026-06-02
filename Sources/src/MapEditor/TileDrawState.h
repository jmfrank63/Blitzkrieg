
#if !defined(AFX_TILEDRAWSTATE_H__E03A5D60_0B7E_4E60_8B6D_17ACD7F2B3DE__INCLUDED_)
#define AFX_TILEDRAWSTATE_H__E03A5D60_0B7E_4E60_8B6D_17ACD7F2B3DE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "InputState.h"

class CTileDrawState  : public IInputState
{
	CInputStateParameter stateParameter;

	virtual void STDCALL Enter();
	virtual void STDCALL Leave();
	virtual void STDCALL Update();

	virtual void STDCALL	OnMouseMove		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame*	pFrame );
	virtual void STDCALL  OnLButtonDown	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnLButtonUp		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
};

#endif // !defined(AFX_TILEDRAWSTATE_H__E03A5D60_0B7E_4E60_8B6D_17ACD7F2B3DE__INCLUDED_)
