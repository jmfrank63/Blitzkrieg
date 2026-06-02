#ifndef __UI_COMBOBOX_H__
#define __UI_COMBOBOX_H__
#include "UIBasic.h"
#include "UISlider.h"

class CUIComboBox : public CMultipleWindow
{
	DECLARE_SERIALIZE;
	int nVSubSpace;
	int nItemLeftSpace;
	CWindowList items;
	bool bComboShown;
	CTRect<float> saveRect;					//сохраненный размер окна, необходим для восстановления состояния после схлопывания окошек

	int nSelItem;										//выделенный item
/*
	std::vector<SWindowSubRect> selSubRects;
	CPtr<IGFXTexture> pSelectionTexture;				// внешний вид - текстура
*/

	void UpdateItemsCoordinates();				//Обновляет координаты всех внутренних item
	void ShowCombo( bool bShow );

public:
	CUIComboBox() : nVSubSpace( 2 ), nItemLeftSpace( 0 ), nSelItem( -1 ), bComboShown( false ) {}

	virtual void STDCALL Reposition( const CTRect<float> &rcParent );

	virtual int STDCALL operator&( IDataTree &ss );

	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );

	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState );
	
	virtual void STDCALL AddItem( IUIElement *pElement );
	virtual int STDCALL GetSelectionItem() { return nSelItem; }
	virtual void STDCALL SetSelectionItem( int nItem );
	virtual int STDCALL GetNumberOfItems() { return items.size(); }
	virtual IUIElement* STDCALL GetItem( int nItem );
	virtual void STDCALL Clear();
};
class CUIComboBoxBridge : public IUIComboBox, public CUIComboBox
{
	OBJECT_NORMAL_METHODS( CUIComboBoxBridge );
	DECLARE_SUPER( CUIComboBox );
	DEFINE_UICONTAINER_BRIDGE;
	
	virtual void STDCALL AddItem( IUIElement *pElement ) { CSuper::AddItem( pElement ); }
	virtual int STDCALL GetSelectionItem() { return CSuper::GetSelectionItem(); }
	virtual void STDCALL SetSelectionItem( int nItem ) { CSuper::SetSelectionItem( nItem ); }
	virtual int STDCALL GetNumberOfItems() { return CSuper::GetNumberOfItems(); }
	virtual IUIElement* STDCALL GetItem( int nItem ) { return CSuper::GetItem( nItem ); }
	virtual void STDCALL Clear() { CSuper::Clear(); }
};
#endif //__UI_COMBOBOX_H__
