#ifndef __UI_LIST_H__
#define __UI_LIST_H__
#include "UIBasic.h"
#include "UISlider.h"
struct SColumnProperties
{
	DECLARE_SERIALIZE;
public:
	int nWidth;									//ширина столбца
	std::string szFileName;			//XML файл из которого создаются внутренние элементы
	int nSorterType;
	SColumnProperties() : nWidth( 0 ), nSorterType( 0 ) {}
	
	virtual int STDCALL operator&( IDataTree &ss );
};
typedef std::vector<SColumnProperties> CVectorOfColumnProperties;
struct SUIListRow : public IUIListRow
{
	OBJECT_NORMAL_METHODS( SUIListRow );
	DECLARE_SERIALIZE;
public:
	typedef std::vector< CPtr<IUIElement> > CUIListSubItems;
	CUIListSubItems subItems;
	int nUserData;
	
	SUIListRow() : nUserData( 0 ) {}
	virtual int STDCALL GetNumberOfElements() const { return subItems.size(); }
	virtual IUIElement* STDCALL GetElement( int nIndex ) const;
	virtual void STDCALL SetUserData( int nData ) { nUserData = nData; }
	virtual int  STDCALL GetUserData() const { return nUserData; }
	
};
typedef std::vector< CPtr<SUIListRow> > CUIListItems;
struct SUIListHeader : public IUIListRow
{
	OBJECT_NORMAL_METHODS( SUIListHeader );
	DECLARE_SERIALIZE;
public:
	struct SColumn
	{
		CPtr<IUIElement> pElement;
		CPtr<IUIListSorter> pSorter;
		int operator&( IStructureSaver &ss )
		{
			CSaverAccessor saver = &ss;
			saver.Add( 1, &pElement );
			saver.Add( 2, &pSorter );
			return 0;
		}
	};
	typedef std::vector< SColumn > CUIListHeaderItems;
	CUIListHeaderItems subItems;
	int nUserData;

	SUIListHeader() : nUserData( 0 ) {}
	virtual int STDCALL GetNumberOfElements() const { return subItems.size(); }
	virtual IUIElement* STDCALL GetElement( int nIndex ) const;
	virtual void STDCALL SetUserData( int nData ) { nUserData = nData; }
	virtual int  STDCALL GetUserData() const { return nUserData; }
};
class CUIList : public CMultipleWindow
{
	DECLARE_SERIALIZE;
	CObj<IUIScrollBar> pScrollBar;				//инициализируется во время загрузки и используется для ускорения доступа к компонентам

	int nLeftSpace;												//отступ item слева и справа от края контрола
	int nTopSpace;												//отступ item от низа header сверху и от низа контрола снизу
	int nHeaderTopSpace;									//отступ header от верха контрола
	int nItemHeight;											//высота одного item
	int nHSubSpace;												//расстояние между двумя subitems по горизонтали
	int nVSubSpace;												//расстояние между двумя items по вертикали
	bool bLeftScrollBar;
	bool bScrollBarAlwaysVisible;
	int nHeaderSize;											//размер header по вертикали, если > 0 то есть заголовок
	int nScrollBarWidth;
	int nSelection;
	int nSortedHeaderIndex;
	bool bSortAscending;

	SUIListHeader headers;
	CUIListItems listItems;
	CVectorOfColumnProperties columnProperties;

	std::vector<SWindowSubRect> selSubRects;
	CPtr<IGFXTexture> pSelectionTexture;				// внешний вид - текстура

	void UpdateItemsCoordinates();				//Обновляет координаты всех внутренних item
	void UpdateScrollBarStatus();					//Вызывается чтобы проверить, нужно ли отображать ScrollBar и обновления его состояния
	void EnsureSelectionVisible();				//Чтобы selection стал полностью видимым, перемещает позицию скроллбара.

	IUIElement* CreateComponent( const char *pszFileName );
	CVec2 GetComponentSize( const char *pszFileName );		//возвращает размер элемента
	void InitItemHeight();								//Вызывается из сериализации, чтобы рассчитать высоту строчки

	void NotifySelectionChanged();
	void NotifyDoubleClick( int nItem );
	void RemoveFocusFromItem( int nIndex );
	void MoveSelectionItemUp();

	void InitSortFunctors();
public:
	CUIList();
	virtual ~CUIList();

	virtual bool STDCALL OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta ) = 0;

	virtual void STDCALL Reposition( const CTRect<float> &rcParent );
	virtual void ScaleLayout( const CVec2 &vScale );

	virtual bool STDCALL OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState );
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );

	virtual int STDCALL operator&( IDataTree &ss );

	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );

	virtual bool STDCALL OnLButtonDblClk( const CVec2 &vPos );
	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState );
	
	virtual int STDCALL GetNumberOfItems() { return listItems.size(); }
	virtual void STDCALL AddItem( int nData = 0 );			//добавляет новую строчку VectorElements в конец списка
	virtual void STDCALL RemoveItem( int nIndex );			//удаляет строчку из конца списка
	virtual IUIListRow* STDCALL GetItem( int nIndex );
	virtual int STDCALL GetItemByID( int nID );
	virtual void STDCALL SetSelectionItem( int nSel );
	virtual int STDCALL GetSelectionItem() { return nSelection; }
	virtual void STDCALL InitialUpdate();
	virtual void STDCALL SetSortFunctor( int nColumn, IUIListSorter *pSorter );
	virtual bool STDCALL Sort( int nColumn, const int nSortType = 0 );
	virtual bool STDCALL ReSort();
	
	/*
	int GetNumberOfItems() { return listItems.size(); }
	void SetNumberOfItems( int n );
	*/
};
class CUIListBridge : public IUIListControl, public CUIList
{
	OBJECT_NORMAL_METHODS( CUIListBridge );
	DECLARE_SUPER( CUIList );
	DEFINE_UICONTAINER_BRIDGE;
	virtual int STDCALL GetNumberOfItems() { return CSuper::GetNumberOfItems(); }
	virtual void STDCALL AddItem( int nData = 0 ) { CSuper::AddItem( nData ); }
	virtual void STDCALL RemoveItem( int nIndex ) { CSuper::RemoveItem( nIndex ); }
	virtual IUIListRow* STDCALL GetItem( int nIndex ) { return CSuper::GetItem( nIndex ); }
	virtual int STDCALL GetItemByID( int nID ) { return CSuper::GetItemByID( nID ); }
	virtual void STDCALL SetSelectionItem( int nSel ) { CSuper::SetSelectionItem( nSel ); }
	virtual int STDCALL GetSelectionItem() { return CSuper::GetSelectionItem(); }
	virtual void STDCALL InitialUpdate() { CSuper::InitialUpdate(); }
	virtual void STDCALL SetSortFunctor( int nColumn, IUIListSorter *pSorter ) { CSuper::SetSortFunctor( nColumn, pSorter ); }
	virtual bool STDCALL Sort( int nColumn, const int nSortType ) { return CSuper::Sort( nColumn, nSortType ); }
	virtual bool STDCALL ReSort() { return CSuper::ReSort(); }
};
#endif //__UI_LIST_H__
