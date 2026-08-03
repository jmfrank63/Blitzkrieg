#if !defined(__State__AI_General__)
#define __State__AI_General__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\RandomMapGen\Polygons_Types.h"

#include "InputMultiState.h"
#include "DrawingTools.h"

struct CAIGActivePoint
{
	int nIndex;
	CVec3 vDifference;
	bool bParcel;
	bool bArrow;
	bool isValid;

	CAIGActivePoint() : nIndex( 0 ), vDifference( VNULL3 ), bParcel( true ), bArrow( false ), isValid( false ) {} 
};

class CAIGSelectState : private IInputState
{
	friend class CInputMultiState;
	friend class CAIGState;
	
	class CAIGState* pParentState;

	CAIGSelectState() : pParentState( 0 )
  {
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	CAIGSelectState( CAIGState* _pParentState ) : pParentState( _pParentState )
	{
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	virtual void STDCALL OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
};

class CAIGEditState : public IInputState
{
	friend class CInputMultiState;
	friend class CAIGState;
	
	class CAIGState* pParentState;
	
	CAIGEditState() : pParentState( 0 )
  {
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}
	CAIGEditState( CAIGState* _pParentState ) : pParentState( _pParentState )
	{
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	virtual void STDCALL OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
};

class CAIGAddState : public IInputState
{
	friend class CInputMultiState;
	friend class CAIGState;
	
	class CAIGState* pParentState;
	
	CAIGAddState() : pParentState( 0 )
  {
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}
	CAIGAddState( CAIGState* _pParentState ) : pParentState( _pParentState )
	{
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

};

class CAIGState : public CInputMultiState
{
	enum INPUT_STATES_AIG
	{
		STATE_SELECT = 0,
		STATE_EDIT = 1,
		STATE_ADD = 2,
	};

protected:
	static const int INVALID_INDEX;
private:
	static const float PARCEL_POINT_RADIUS;
	static const float PARCEL_POINT_RADIUS_ARROW;
	static const int PARCEL_POINT_PARTS;
	
	static const float PARCEL_CENTER_POINT_RADIUS;
	static const int PARCEL_CENTER_POINT_PARTS;
	
	static const float PARCEL_ARROW_POINT_RADIUS;
	static const int PARCEL_ARROW_POINT_PARTS;

	static const float POSITION_POINT_RADIUS;
	static const float POSITION_POINT_RADIUS_ARROW;
	static const int POSITION_POINT_PARTS;
	
	static const float POSITION_CENTER_POINT_RADIUS;
	static const int POSITION_CENTER_POINT_PARTS;
	
	static const float POSITION_ARROW_POINT_RADIUS;
	static const int POSITION_ARROW_POINT_PARTS;

	static const SColor PARCEL_COLORS[3];
	static const SColor POSITION_COLORS[3];
	
	friend class CAIGSelectState;
	friend class CAIGEditState;
	friend class CAIGAddState;

protected:
	int nCurrentParcel;
private:
	CAIGActivePoint activePoint;

	CInputStateParameter stateParameter;
	CSceneDrawTool sceneDrawTool;
	

	virtual void STDCALL Enter();
	virtual void STDCALL Leave();
	virtual void STDCALL Update();

	virtual void STDCALL Draw( CTemplateEditorFrame* pFrame );

public:
	CAIGState()
	{
		int nStateNumber = CInputStateParameter::INVALID_STATE;
		
		CAIGSelectState *pAIGSelectState = new CAIGSelectState( this );
		nStateNumber = AddInputState( pAIGSelectState );
		NI_ASSERT_T( nStateNumber == STATE_SELECT, NStr::Format( "Wrong state number: %d (%d)", nStateNumber, STATE_SELECT ) );

		CAIGEditState *pAIGEditState = new CAIGEditState( this );
		nStateNumber = AddInputState( pAIGEditState );
		NI_ASSERT_T( nStateNumber == STATE_EDIT, NStr::Format( "Wrong state number: %d, (%d)", nStateNumber, STATE_EDIT ) );
		
		CAIGAddState *pAIGAddState = new CAIGAddState( this );
		nStateNumber = AddInputState( pAIGAddState );
		NI_ASSERT_T( nStateNumber == STATE_ADD, NStr::Format( "Wrong state number: %d, (%d)", nStateNumber, STATE_ADD ) );

		SetActiveState( STATE_SELECT );
	}
};
#endif // !defined(__State__AI_General__)
