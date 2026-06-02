#if !defined(__InputMultiState__)
#define __InputMultiState__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "InputState.h"
class CInputMultiState : public IInputState
{
	std::vector<IInputState*> inputStates;	
	int nActiveInputState;
public:
	static const int INVALID_INPUT_STATE;
	
	virtual void STDCALL	Enter();
	virtual void STDCALL	Leave();
	virtual void STDCALL	Update();

	virtual void STDCALL	OnMouseMove			( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );

	virtual void STDCALL  OnLButtonDown		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnLButtonUp			( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnLButtonDblClk	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );

	virtual void STDCALL  OnRButtonDown		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnRButtonUp			( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnRButtonDblClk	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );

	virtual void STDCALL  OnMButtonDown		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnMButtonUp			( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnMButtonDblClk	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );

	virtual void STDCALL  OnKeyDown				( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
	virtual void STDCALL  OnObjectPaste		( CTemplateEditorFrame*	pFrame );

	virtual void STDCALL	Draw( CTemplateEditorFrame* pFrame );

public:
	CInputMultiState() : nActiveInputState( INVALID_INPUT_STATE ) {}
	~CInputMultiState()
	{
		for ( int nInputStateIndex = 0; nInputStateIndex < inputStates.size(); ++nInputStateIndex )
		{
			if ( inputStates[nInputStateIndex] )
			{
				delete ( inputStates[nInputStateIndex] );
				inputStates[nInputStateIndex] = 0;
			}
		}
	}
	
	int SetActiveState( int nNewInputState )
	{
		NI_ASSERT_TF( ( nNewInputState >= 0 ) && ( nNewInputState < inputStates.size() ),
									NStr::Format( "Invalid input state number: %d\n", nNewInputState ),
									INVALID_INPUT_STATE );

		if ( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ) )
		{
			inputStates[nActiveInputState]->Leave();
		}
		
		int nOldInputState = nActiveInputState;
		nActiveInputState = nNewInputState;
		
		if ( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ) )
		{
			inputStates[nActiveInputState]->Enter();
		}
		
		return nOldInputState;
	}
	
	int GetActiveState()
	{
		return nActiveInputState;
	}

	template<class TINPUTSTATE>
	int AddInputState( TINPUTSTATE* pDummyInputState ) 
	{
		TINPUTSTATE *pNewInputState = pDummyInputState;
		if ( !pNewInputState )
		{
			pNewInputState = new TINPUTSTATE();
		}
		inputStates.push_back( pNewInputState );
		nActiveInputState = inputStates.size() - 1;
		return nActiveInputState;
	}

	IInputState* GetInputState( int nInputState )
	{ 
		NI_ASSERT_TF( ( nInputState >= 0 ) && ( nInputState < inputStates.size() ),
									NStr::Format( "Invalid input state number: %d\n", nInputState ),
									0 );
		NI_ASSERT_TF( inputStates[nInputState] != 0,
									NStr::Format( "Invalid input state: %d, %x\n", nInputState, inputStates[nInputState] ),
									0 );
		return inputStates[nInputState];
	}
};
#endif // !defined(__InputMultiState__)
