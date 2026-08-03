#if !defined(__VectorStripeObjects__State__)
#define __VectorStripeObjects__State__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "InputMultiState.h"
#include "DrawingTools.h"

#include "../Image/Image.h"
#include "../Formats/fmtMap.h"
#include "../RandomMapGen/Polygons_Types.h"
#include "../RandomMapGen/RMG_Types.h"
#include "../RandomMapGen/VSO_Types.h"

struct CVectorStripeActivePoint
{
	int nIndex;
	CVec3 vDifference;
	float fOpacity;
	bool bControlPoint;
	bool bBegin;
	bool isValid;

	CVectorStripeActivePoint() : nIndex( 0 ), vDifference( VNULL3 ), fOpacity( 0.0f ), bControlPoint( true ), bBegin( true ), isValid( false ) {} 
};

class CVSOSelectState : private IInputState
{
	friend class CInputMultiState;
	friend class CVSOState;
	
	class CVSOState* pParentState;
	
	CVSOSelectState() : pParentState( 0 )
  {
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}
	CVSOSelectState( CVSOState* _pParentState ) : pParentState( _pParentState )
	{
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	virtual void STDCALL OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnRButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
};

class CVSOEditState : public IInputState
{
	friend class CInputMultiState;
	friend class CVSOState;
	
	class CVSOState* pParentState;
	
	CVSOEditState() : pParentState( 0 )
  {
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}
	CVSOEditState( CVSOState* _pParentState )
		: pParentState( _pParentState )
	{
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	virtual void STDCALL OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnRButtonDown( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnRButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
};

class CVSOAddState : public IInputState
{
	friend class CInputMultiState;
	friend class CVSOState;
	
	class CVSOState* pParentState;
	
	CVSOAddState() : pParentState( 0 )
  {
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}
	CVSOAddState( CVSOState* _pParentState )
		: pParentState( _pParentState )
	{
		NI_ASSERT_T( pParentState != 0,
								 NStr::Format( "Invalid parameter: %x", pParentState ) );
	}

	virtual void STDCALL OnMouseMove( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnLButtonDblClk( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnRButtonUp( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
};

class CVSOState : public CInputMultiState
{
	enum VSO_INPUT_STATES
	{
		STATE_SELECT = 0,
		STATE_EDIT = 1,
		STATE_ADD = 2,
	};

	friend class CVSOSelectState;
	friend class CVSOEditState;
	friend class CVSOAddState;

protected:
	static const int INVALID_INDEX;
private:
	static const float CONTROL_POINT_RADIUS;
	static const int CONTROL_POINT_PARTS;
	static const float KEY_POINT_RADIUS;
	static const int KEY_POINT_PARTS;
	static const SColor CONTROL_POINT_COLOR;
	static const SColor KEY_POINT_COLOR;
	static const SColor POINT_COLOR;
	static const SColor POLYGON_COLOR;
	static const float OPACITY_DELIMITER;

protected:
	int nCurrentVSO;
	std::list<CVec3> addPoints;
private:
	CVectorStripeActivePoint activePoint;
	CVSOBuilder::SBackupKeyPoints backupKeyPoints;
	std::vector<int> selectedIndices;
	int nSelectedIndex;

	void ValidateSelectedIndex()
	{
		if ( ( nSelectedIndex  < 0 ) || ( nSelectedIndex >= selectedIndices.size() ) )
		{
			nSelectedIndex = 0;
		}
	}

	CInputStateParameter stateParameter;
	CSceneDrawTool sceneDrawTool;
	
	virtual void STDCALL Enter();
	virtual void STDCALL Leave();
	virtual void STDCALL Update();

	virtual bool CreateVSO( SVectorStripeObject *pVSO ) = 0;
	virtual void AddVSO( const SVectorStripeObject &rVSO ) = 0;
	virtual void UpdateVSO( int nID ) = 0;
	virtual void RemoveVSO( int nID ) = 0;
	virtual SVectorStripeObject* GetVSO( int nVSOIndex ) = 0;
	virtual SVectorStripeObject* GetCurrentVSO() = 0;
	virtual bool IsCurrentVSOValid() = 0;
	virtual class CTabVOVSODialog* GetTabVOVSODialog( class CTemplateEditorFrame *pFrame ) = 0;
	virtual CMapInfo::TERRAIN_HIT_TEST_TYPE GetTerrainHitTestType() = 0;
	virtual bool NeedUpdateAI() { return false; }

	virtual void STDCALL Draw( CTemplateEditorFrame* pFrame );

public:
	CVSOState() : nCurrentVSO ( INVALID_INDEX ), nSelectedIndex( INVALID_INDEX )
	{
		int nStateNumber = CInputStateParameter::INVALID_STATE;
		
		CVSOSelectState *pVSOSelectState = new CVSOSelectState( this );
		nStateNumber = AddInputState( pVSOSelectState );
		NI_ASSERT_T( nStateNumber == STATE_SELECT, NStr::Format( "Wrong state number: %d (%d)", nStateNumber, STATE_SELECT ) );

		CVSOEditState *pVSOEditState = new CVSOEditState( this );
		nStateNumber = AddInputState( pVSOEditState );
		NI_ASSERT_T( nStateNumber == STATE_EDIT, NStr::Format( "Wrong state number: %d, (%d)", nStateNumber, STATE_EDIT ) );
		
		CVSOAddState *pVSOAddState = new CVSOAddState( this );
		nStateNumber = AddInputState( pVSOAddState );
		NI_ASSERT_T( nStateNumber == STATE_ADD, NStr::Format( "Wrong state number: %d, (%d)", nStateNumber, STATE_ADD ) );

		SetActiveState( STATE_SELECT );
	}
};

class CRoads3DState : public CVSOState
{
	virtual bool CreateVSO( SVectorStripeObject *pRoad3D );
	virtual void AddVSO( const SVectorStripeObject &rVSO );
	virtual void UpdateVSO( int nID );
	virtual void RemoveVSO( int nID );
	virtual SVectorStripeObject* GetVSO( int nVSOIndex );
	virtual SVectorStripeObject* GetCurrentVSO();
	virtual bool IsCurrentVSOValid();
	virtual class CTabVOVSODialog* GetTabVOVSODialog( class CTemplateEditorFrame *pFrame );
	virtual CMapInfo::TERRAIN_HIT_TEST_TYPE GetTerrainHitTestType();
	virtual bool NeedUpdateAI() { return false; }
};

class CRiversState : public CVSOState
{
	virtual bool CreateVSO( SVectorStripeObject *pRiver );
	virtual void AddVSO( const SVectorStripeObject &rVSO );
	virtual void UpdateVSO( int nID );
	virtual void RemoveVSO( int nID );
	virtual SVectorStripeObject* GetVSO( int nVSOIndex );
	virtual SVectorStripeObject* GetCurrentVSO();
	virtual bool IsCurrentVSOValid();
	virtual class CTabVOVSODialog* GetTabVOVSODialog( class CTemplateEditorFrame *pFrame );
	virtual CMapInfo::TERRAIN_HIT_TEST_TYPE GetTerrainHitTestType();
	virtual bool NeedUpdateAI() { return true; }
};
#endif // !defined(__VectorStripeObjects__State__)
