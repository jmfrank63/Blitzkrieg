#ifndef __AI_EDITOR_INTERNAL_H__
#define __AI_EDITOR_INTERNAL_H__
#pragma ONCE
#include "AILogic.h"
class CAIEditor : public IAIEditor
{
	OBJECT_NORMAL_METHODS( CAIEditor );
	DECLARE_SERIALIZE;

	CPtr<ISegmentTimer> pGameSegment;
	bool IsRectInsideOfMap( const struct SRect &unitRect ) const;
public:
	virtual void STDCALL Init( const struct STerrainInfo &terrainInfo );
	virtual void STDCALL Clear();	
	
	virtual bool STDCALL IsFormation( IRefCount *pObject ) const;
	virtual void STDCALL GetUnitsInFormation( IRefCount *pObject, IRefCount ***pUnits, int *pnLen );
	virtual IRefCount* STDCALL GetFormationOfUnit( IRefCount *pObject );

	virtual bool STDCALL AddNewObject( const SMapObjectInfo &object, IRefCount **pObject );
	virtual bool STDCALL AddNewEntrencment( IRefCount** segments, const int nLen, IRefCount **pObject );
	virtual void STDCALL LoadEntrenchments( const std::vector<SEntrenchmentInfo> &entrenchments );	

	virtual bool STDCALL MoveObject( IRefCount *pObject, short x, short y );
	virtual void STDCALL DeleteObject( IRefCount *pObject );
	virtual void STDCALL DamageObject( IRefCount *pObject, const float fHP );
	
	virtual bool STDCALL TurnObject( IRefCount *pObject, const WORD wDir );

	virtual float STDCALL GetObjectHP( IRefCount *pObject );
	virtual int STDCALL GetObjectScriptID( IRefCount *pObject );
	
	virtual void STDCALL HandOutLinks();
	virtual IRefCount* STDCALL LinkToAI( const int ID );
	virtual int STDCALL AIToLink( IRefCount *pObj );
	
	virtual const CVec2& STDCALL GetCenter( IRefCount *pObj ) const;
	virtual const WORD STDCALL GetDir( IRefCount *pObj ) const;
	
	virtual const int STDCALL GetUnitDBID( IRefCount *pObj ) const;
	
	virtual bool STDCALL IsObjectInsideOfMap( const struct SMapObjectInfo &object ) const;
	virtual bool STDCALL CanAddObject( const struct SMapObjectInfo &object ) const;
	
	virtual void STDCALL ApplyPattern( const struct SVAPattern &rPattern );
	virtual void STDCALL UpdateAllHeights();

	virtual bool STDCALL ToggleShow( const int nShowType );
	
	virtual void STDCALL UpdateTerrain( const CTRect<int> &rect, const struct STerrainInfo &terrainInfo );

	virtual void STDCALL RecalcPassabilityForPlayer( CArray2D<BYTE> *array, const int nPlayer );
	
	virtual void STDCALL SetPlayer( IRefCount *pObj, const int nPlayer );
	virtual void STDCALL SetDiplomacies( const std::vector<BYTE> &playerParty );

	virtual void STDCALL DeleteRiver( const SVectorStripeObject &river );
	virtual void STDCALL AddRiver( const SVectorStripeObject &river );
};
#endif // __AI_EDITOR_INTERNAL_H__
