
#include "stdafx.h"
#include "editor.h"
#include "InputMultiState.h"

#include "..\Scene\Scene.h"
#include "..\Scene\Terrain.h"

#include "frames.h"
#include "GameWnd.h"
#include "MapEditorBarWnd.h"
#include "TemplateEditorFrame1.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

const int CInputMultiState::INVALID_INPUT_STATE = -1;

void CInputMultiState::Enter()
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->Enter();
}

void CInputMultiState::Leave()
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->Leave();
}

void CInputMultiState::Update()
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->Update();
}

void CInputMultiState::Draw( CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->Draw( pFrame );
}


void CInputMultiState::OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnMouseMove( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnLButtonDown( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnLButtonUp( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnLButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnLButtonDblClk( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnRButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnRButtonDown( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnRButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnRButtonUp( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnRButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnRButtonDblClk( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnMButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnMButtonDown( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnMButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnMButtonUp( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnMButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnMButtonDblClk( nFlags, rMousePoint, pFrame );
}

void CInputMultiState::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnKeyDown( nChar, nRepCnt, nFlags, pFrame );
}

void CInputMultiState::OnObjectPaste( CTemplateEditorFrame* pFrame )
{
	NI_ASSERT_T( ( nActiveInputState >= 0 ) && ( nActiveInputState < inputStates.size() ),
							 NStr::Format( "Invalid input state number: %d\n", nActiveInputState ) );
	inputStates[nActiveInputState]->OnObjectPaste( pFrame );
}


