#ifndef __UI_TREE_H__
#define __UI_TREE_H__
#include "UIBasic.h"
#include "UISlider.h"

#ifdef OLD
struct SUITreeItem : public IUITreeItem
{
	OBJECT_NORMAL_METHODS( SUITreeItem );
	DECLARE_SERIALIZE;

	CPtr<IUIContainer> pTreeCtrl;
	CPtr<IUIElement> pIcon;
	CPtr<IUIElement> pInfo;
	CPtr<SUITreeItem> pParent;
	int nData;

public:
	typedef std::list< CObj<SUITreeItem> > CItemList;
private:
	CItemList treeItems;
	
public:
	SUITreeItem() : nData( 0 ) {}

	virtual IUITreeItem* STDCALL AddTreeItem( IUIElement *_pIcon, IUIElement *_pInfo, int nUserData );
	virtual IUITreeItem* STDCALL GetParentTreeItem() { return pParent; }

	virtual IUITreeIterator STDCALL GetBegin() { return treeItems.begin(); }
	virtual IUITreeIterator STDCALL GetEnd() { return treeItems.end(); }
	virtual int STDCALL GetUserData() { return nData; }
	
	virtual int STDCALL operator&( IDataTree &ss );
};
class CUITree : public CMultipleWindow
{
	DECLARE_SERIALIZE;
	CUIScrollBar *pScrollBar;				//инициализируетс€ во врем€ загрузки и используетс€ дл€ ускорени€ доступа к компонентам

	int nLeftSpace;
	int nTopSpace;
	int nItemHeight;
	int nHSubSpace;
	int nVSubSpace;
	bool bScrollBarAlwaysVisible;
	int nScrollBarWidth;

	CPtr<SUITreeItem> pRoot;
	CPtr<SUITreeItem> pSelection;

	std::vector<SWindowSubRect> selSubRects;
	CPtr<IGFXTexture> pSelectionTexture;				// внешний вид - текстура

	void UpdateAll();
	void UpdateItemsCoordinates();				//ќбновл€ет координаты всех внутренних item
	void UpdateScrollBarStatus();					//¬ызываетс€ чтобы проверить, нужно ли отображать ScrollBar и обновлени€ его состо€ни€
	void OnUserChangeScrollBarPosition();	//¬ызываетс€ дл€ пересчета в ответ на изменени€ в статусе ScrollBar

public:
	CUITree() : pScrollBar( 0 ), nLeftSpace( 10 ), nTopSpace( 5 ), nItemHeight( 30 ),
		nHSubSpace( 2 ), nVSubSpace( 2 ), nScrollBarWidth( 30 ), bScrollBarAlwaysVisible( true ) {}
	virtual ~CUITree();

	virtual void STDCALL Reposition( const CTRect<float> &rcParent );

	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );

	virtual int STDCALL operator&( IDataTree &ss );

	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	
	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState );
	
	virtual SUITreeItem* STDCALL GetRootTreeItem() { return pRoot; }
	virtual void STDCALL SetSelectionItem( SUITreeItem *pTreeItem );
	virtual SUITreeItem* STDCALL GetSelectionItem() { return pSelection; }
};
class CUITreeBridge : public IUITreeControl, public CUITree
{
	OBJECT_NORMAL_METHODS( CUITreeBridge );
	DECLARE_SUPER( CUITree );
	DEFINE_UICONTAINER_BRIDGE;
};
#endif	//OLD
#endif //__UI_TREE_H__
