#if !defined(__State__Terrain_Fields__)
#define __State__Terrain_Fields__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "InputMultiState.h"
#include "DrawingTools.h"
#include "MapSoundInfo.h"

class CFieldsSelectState : private IInputState
{
	friend class CInputMultiState;
	friend class CFieldsState;
	
	class CFieldsState* pParentState;

	CFieldsSelectState() : pParentState( 0 )
  {
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	CFieldsSelectState( CFieldsState* _pParentState ) : pParentState( _pParentState )
	{
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	virtual void STDCALL OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
};

class CFieldsEditState : public IInputState
{
	friend class CInputMultiState;
	friend class CFieldsState;
	
	class CFieldsState* pParentState;
	
	CFieldsEditState() : pParentState( 0 )
  {
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}
	CFieldsEditState( CFieldsState* _pParentState ) : pParentState( _pParentState )
	{
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	virtual void STDCALL OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
};

class CFieldsAddState : public IInputState
{
	friend class CInputMultiState;
	friend class CFieldsState;
	
	class CFieldsState* pParentState;
	
	CFieldsAddState() : pParentState( 0 )
  {
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}
	CFieldsAddState( CFieldsState* _pParentState ) : pParentState( _pParentState )
	{
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	virtual void STDCALL OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnRButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
};

class CFieldsState : public CInputMultiState
{
	enum INPUT_STATES_FIELDS
	{
		STATE_SELECT = 0,
		STATE_EDIT = 1,
		STATE_ADD = 2,
	};

	friend class CFieldsSelectState;
	friend class CFieldsEditState;
	friend class CFieldsAddState;

	static const int INVALID_INDEX;
	static const float POINT_RADIUS;
	static const int POINT_PARTS;
	static const float LINE_SEGMENT;
	static const SColor POINT_COLOR;
	static const SColor LINE_COLOR;

	CVec3 vDifference;
	std::vector<CVec3> points;
	int nCurrentPoint;

	CInputStateParameter stateParameter;
	CSceneDrawTool sceneDrawTool;
	
	virtual void STDCALL Enter();
	virtual void STDCALL Leave();
	virtual void STDCALL Update();

	virtual void STDCALL  Draw( CTemplateEditorFrame* pFrame );

public:
	CFieldsState() : vDifference( VNULL3 ), nCurrentPoint( INVALID_INDEX )
	{
		int nStateNumber = CInputStateParameter::INVALID_STATE;
		
		CFieldsSelectState *pFieldsSelectState = new CFieldsSelectState( this );
		nStateNumber = AddInputState( pFieldsSelectState );
		NI_ASSERT_T( nStateNumber == STATE_SELECT, NStr::Format( "Wrong state number: %d (%d)", nStateNumber, STATE_SELECT ) );

		CFieldsEditState *pFieldsEditState = new CFieldsEditState( this );
		nStateNumber = AddInputState( pFieldsEditState );
		NI_ASSERT_T( nStateNumber == STATE_EDIT, NStr::Format( "Wrong state number: %d, (%d)", nStateNumber, STATE_EDIT ) );
		
		CFieldsAddState *pFieldsAddState = new CFieldsAddState( this );
		nStateNumber = AddInputState( pFieldsAddState );
		NI_ASSERT_T( nStateNumber == STATE_ADD, NStr::Format( "Wrong state number: %d, (%d)", nStateNumber, STATE_ADD ) );

		SetActiveState( STATE_SELECT );
	}

	void PlaceField( bool bPlace );
};
#endif // !defined(__State__Terrain_Fields__)
