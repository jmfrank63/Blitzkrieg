
#if !defined(AFX_OBJECTPLACERSTATE_H__DB9EBB7D_57AE_4ABB_B281_3350C4494410__INCLUDED_)
#define AFX_OBJECTPLACERSTATE_H__DB9EBB7D_57AE_4ABB_B281_3350C4494410__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "InputState.h"

class CObjectPlacerState : public IInputState
{
	CInputStateParameter stateParameter;

	virtual void STDCALL Enter();
	virtual void STDCALL Leave();
	virtual void STDCALL Update();

	virtual void STDCALL	OnMouseMove			( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );

	virtual void STDCALL	OnLButtonDown		( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL	OnLButtonUp			( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );
	virtual void STDCALL	OnLButtonDblClk	( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );

	virtual void STDCALL	OnRButtonUp			( UINT nFlags, const CTPoint<int> &rMousePoint, CTemplateEditorFrame* pFrame );

	virtual void STDCALL	OnKeyDown				( UINT nChar, UINT nRepCnt, UINT nFlags, CTemplateEditorFrame* pFrame );
	virtual void STDCALL	OnObjectPaste		( CTemplateEditorFrame*	pFrame );
	public:
		void CalculateShiftsForMultiSelect( CTemplateEditorFrame *frame, const CVec3 &center = VNULL3, const CVec3 &rShift = VNULL3 );
		
		void CalculateObjectSelection(   std::vector< struct SMapObject*> &objects, CTemplateEditorFrame *frame, const CVec3 &center = VNULL3, bool bSquad = false );

		std::vector<SMapObject*> 	GetObjectsSelection(   std::vector< struct SMapObject*> &objects, CTemplateEditorFrame *frame, bool *pbSquad = 0 );
		std::vector<SMapObject*>	GetObjectsSelection(  SMapObject* object, CTemplateEditorFrame *frame, bool *pbSquad = 0 );

		void											FilterObjects( std::vector< struct SMapObject*> &objects, CTemplateEditorFrame *frame )	;
  
		void ClearAllSelection( CTemplateEditorFrame * );
		void		PlacePasteGroup( const std::string &name, CTemplateEditorFrame* );
public:
	void UpdatePropertie( struct SEditorObjectItem *ptr, CTemplateEditorFrame* );
	CObjectPlacerState();

private:
	bool CheckForInserting( std::vector< struct SMapObject* > &v, struct  SMapObject *p, int *pType = 0 );
};
#endif // !defined(AFX_OBJECTPLACERSTATE_H__DB9EBB7D_57AE_4ABB_B281_3350C4494410__INCLUDED_)
