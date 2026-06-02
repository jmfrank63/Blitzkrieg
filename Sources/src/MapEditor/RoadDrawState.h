
#if !defined(AFX_ROADDRAWSTATE_H__0BCF6866_121A_4459_8337_B2EDCC515941__INCLUDED_)
#define AFX_ROADDRAWSTATE_H__0BCF6866_121A_4459_8337_B2EDCC515941__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "InputState.h"
#include "TemplateEditorFrame1.h"
#include "..\Formats\fmtMap.h"

namespace roadStateConsts
{ 
	const int nTrench	= CTemplateEditorFrame::STATE_VO_ENTRENCHMENTS;
	const int nFence	= CTemplateEditorFrame::STATE_SO_FENCES;
	const int nBridge	= CTemplateEditorFrame::STATE_SO_BRIDGES;
};

class CRoadDrawState : public IInputState
{
	CInputStateParameter stateParameter;

	virtual void STDCALL Enter();
	virtual void STDCALL Leave();
	virtual void STDCALL Update();

	virtual void STDCALL	OnMouseMove			(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnLButtonDown		(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnLButtonUp			(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnLButtonDblClk	(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pframe );
	virtual void STDCALL  OnRButtonUp			(UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnKeyDown				(UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );

	std::vector<GPoint>								m_pointForTrench;

	std::vector< interface IVisObj*>	m_highlightedObjects;
	CVec2															m_lastScreenMouse;

	std::vector< CVec2 > GetPointsForBridge( CVec2  &begin,  CVec2  &end, CTemplateEditorFrame* frame );

	struct SBridgeSpanObject											*tmpSpan;			

public:
	CRoadDrawState() : tmpSpan( 0 ) {}
};

#endif // !defined(AFX_ROADDRAWSTATE_H__0BCF6866_121A_4459_8337_B2EDCC515941__INCLUDED_)
