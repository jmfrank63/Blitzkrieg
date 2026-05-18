#ifndef __OBJVISOBJ_H__
#define __OBJVISOBJ_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Icon.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TBase>
class CTObjVisObj : public TBase
{
protected:
	typedef CTObjVisObj<TBase> CObjVisObj;
private:
	//
	DWORD dwGameType;											// object game type
	CVec3 vPos;														// unit's position
	int nDirection;												// 2D direction
	bool bVisible;												// is this object visible?
	int nPriority;												// this object priority
	EVisObjSelectionState selectionState;	// selection state
	// icons
	CIconsList icons;
protected:
	DWORD dwLastUpdateTime;								// last time, update was calculated
	//
	virtual void RepositionIcons() {  }
	void RepositionIconsLocal( const DWORD placement, const CTRect<float> &rcRect, const float fAddZ = 20 ) { ::RepositionIconsLocal( icons, placement, rcRect, fAddZ ); }
	void VisitIcons( ISceneVisitor *pVisitor )
	{
		for ( CIconsList::iterator it = icons.begin(); it != icons.end(); ++it )
		{
			it->pIcon->Reposition( vPos );
			it->pIcon->Visit( pVisitor );
		}
	}
	//
	void SetPos( const CVec3 &_vPos ) { vPos = _vPos; }
	const CVec3& GetPos() const { return vPos; }
	const int GetPriority() const { return nPriority; }
	const DWORD GetGameType( const DWORD dwType ) const { return dwGameType == 0 ? dwType : dwGameType; }
	//
	void SetDir( const int _nDirection ) { nDirection = _nDirection % 65536; }
	const int GetDir() const { return nDirection; }
	//
	virtual ~CTObjVisObj() {  }
public:
	CTObjVisObj()
	{
		dwGameType = 0;
		vPos = VNULL3;
		nDirection = 0;
		dwLastUpdateTime = -1;
		bVisible = true;
		nPriority = 0;
		selectionState = SGVOSS_UNSELECTED;
	}
	//
	// placement
	//
	virtual void STDCALL SetPlacement( const CVec3 &_vPos, const int _nDir ) { SetPosition( _vPos ); SetDirection( _nDir ); }
	virtual const CVec3& STDCALL GetPosition() const { return vPos; }
	virtual int STDCALL GetDirection() const { return nDirection; }
	// 
	// selection
	//
	virtual EVisObjSelectionState STDCALL GetSelectionState() const { return selectionState; }
	virtual void STDCALL Select( EVisObjSelectionState state ) 
	{ 
		selectionState = state;
		for ( CIconsList::iterator it = icons.begin(); it != icons.end(); ++it )
		{
			if ( it->nID < 10100 ) 
				it->pIcon->SetAlpha( selectionState == SGVOSS_SELECTED ? 0xff : 0x60 );
		}
	}
	//
	// visibility, priority, game type
	//
	virtual bool STDCALL IsVisible() const { return bVisible; }
	virtual void STDCALL SetVisible( const bool _bVisible ) { bVisible = _bVisible; }
	virtual void STDCALL SetPriority( const int _nPriority ) { nPriority = _nPriority; }
	virtual void STDCALL SetGameType( const DWORD dwType ) { dwGameType = dwType; }
	//
	// icons
	//
	virtual void STDCALL AddIcon( ISceneIcon *pIcon, int nID, const CVec3 &vAddValue, const CVec3 &vAddStep, 
		                            int nPriority, DWORD placement, bool bReposition = true )
	{
		if ( pIcon )
		{
			// remove old icon with similar ID
			for ( CIconsList::iterator it = icons.begin(); it != icons.end(); ++it )
			{
				if ( it->nID == nID )
				{
					icons.erase( it );
					break;
				}
			}
			//
			icons.push_back( SIconDesc() );
			icons.back().pIcon = pIcon;
			icons.back().nID = nID;
			icons.back().nPriority = nPriority;
			icons.back().placement = placement;
			icons.back().vAddValue = vAddValue;
			icons.back().vAddStep = vAddStep;

			pIcon->SetAlpha( selectionState == SGVOSS_SELECTED ? 0xff : 0x60 );
		}
		if ( bReposition ) 
			RepositionIcons();
	}
	virtual void STDCALL RemoveIcon( int nID, bool bReposition = true )
	{
		if ( nID == -1 )
			icons.clear();
		else
		{
			for ( CIconsList::iterator it = icons.begin(); it != icons.end(); ++it )
			{
				if ( it->nID == nID )
				{
					icons.erase( it );
					break;
				}
			}
		}
		//
		if ( bReposition ) 
			RepositionIcons();
	}
	virtual ISceneIcon* STDCALL GetIcon( int nID ) const
	{
		for ( CIconsList::const_iterator it = icons.begin(); it != icons.end(); ++it )
		{
			if ( it->nID == nID )
				return it->pIcon;
		}
		return 0;
	}
	//
	// serialization
	//
	virtual int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 3, &dwGameType );
		saver.Add( 4, &vPos );
		saver.Add( 5, &nDirection );
		saver.Add( 6, &dwLastUpdateTime );
		saver.Add( 7, &bVisible );
		saver.Add( 8, &nPriority );
		saver.Add( 9, &selectionState );
		saver.Add( 10, &icons );
		if ( saver.IsReading() )
			Select( selectionState );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OBJVISOBJ_H__