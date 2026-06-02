#if !defined(__3DTerrainState__)
#define __3DTerrainState__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "InputState.h"
#include "DrawingTools.h"

class CDrawShadeState : public IInputState
{
	CInputStateParameter stateParameter;
	CSceneDrawTool sceneDrawTool;

	float fTileHeight;
	float fAverageHeight;
	bool isTileHeightValid;
	bool bLeaved;

	void Update( CTemplateEditorFrame* pFrame, bool bUpdateTileHeight = true );
	void UpdateZ( CTemplateEditorFrame* pFrame );

	virtual void STDCALL Enter();
	virtual void STDCALL Leave();
	virtual void STDCALL Update();

	virtual void STDCALL	OnMouseMove		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnLButtonDown	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnLButtonUp		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnRButtonDown	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnRButtonUp		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnMButtonDown	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnMButtonUp		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );

	virtual void STDCALL Draw( CTemplateEditorFrame* pFrame );

public:
	CDrawShadeState()
		: fTileHeight( 0.0f ), fAverageHeight( 0.0f ), isTileHeightValid( false ), bLeaved( true ) {}
};
#endif // !defined(__3DTerrainState__)
