
#if !defined(AFX_SIMPLEWINDOW_H__31031DC1_1049_447B_8B96_F1354F976FCD__INCLUDED_)
#define AFX_SIMPLEWINDOW_H__31031DC1_1049_447B_8B96_F1354F976FCD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

enum ESubstate
{
	ES_NORMAL,
	ES_PUSHED,
	ES_MOUSE_OVER,
	ES_DISABLED,
};
struct SWindowSubState
{
	CPtr<IBackground> pBackground;									// background for this substats
	CPtr<IUIEffector> pEffectOnAppear;							// effect, that applied to the window when substate changed
	CPtr<IUIEffector> pEffect;											// effect applied when the state is active (after pEffectOnAppear)

	CPtr<IUIEffector> pCurrentEffect;
public:

	void Visit( interface ISceneVisitor * pVisitor )
	{
		if ( pCurrentEffect )
			pCurrentEffect->Visit( pVisitor );
		else if ( pBackground )
			pBackground->Visit( pVisitor );
	}
};
struct SWindowState
{
	std::vector<SWindowSubState> substates;
	int nSubState;																	// current active substate

	void Visit( interface ISceneVisitor * pVisitor )
	{
		if ( !substates.empty() )
			substates[nState].Visit( pVisitor );
	}
};
class CSimpleWindow  
{
	std::vector<SWindowState> states;
	int nState;

	
public:


	virtual void STDCALL Visit( interface ISceneVisitor * pVisitor )
	{
		states[nState].Visit( pVisitor );	
	}

	CSimpleWindow();
	virtual ~CSimpleWindow();

	IBackground * GetBackground() { return pBackground; }
};
#endif // !defined(AFX_SIMPLEWINDOW_H__31031DC1_1049_447B_8B96_F1354F976FCD__INCLUDED_)
