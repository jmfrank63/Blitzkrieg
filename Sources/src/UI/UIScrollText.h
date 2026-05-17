#ifndef __UI_SCROLL_TEXT_H__
#define __UI_SCROLL_TEXT_H__
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "UIBasic.h"
#include "UISlider.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIScrollTextBox : public CMultipleWindow
{
	DECLARE_SERIALIZE;
	CUIScrollBar *pScrollBar;				//инициализируется во время загрузки и используется для ускорения доступа к компонентам
	int nScrollBarWidth;
	int m_nY;												//сдвиг по Y от начала текста, чтобы текст скроллировался
	int nLeftSpace, nRightSpace;		//отступ текста соответственно слева от края и справа от скроллбара
	int nTopSpace, nBottomSpace;		//отступ текста соответственно сверху и снизу
	bool bScrollBarAlwaysVisible;
protected:
	int GetY() const { return m_nY; }
	void GetBorderRect( CTRect<float> *pBorderRect ) const;
	void RepositionScrollbar();
	virtual void RepositionText();
	void UpdateScrollBar( const int nMaxValue, const int nCurValue );
public:
	
	CUIScrollTextBox() : nScrollBarWidth( 30 ), m_nY( 0 ), nLeftSpace( 4 ), nRightSpace( 4 ), nTopSpace( 0 ), nBottomSpace( 0 ), bScrollBarAlwaysVisible( true ) {}
	~CUIScrollTextBox() {}

	//mouse wheel
	virtual bool STDCALL OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta ) = 0;
	
	// text
	virtual void STDCALL SetWindowText( int nState, const WORD *pszText );
	inline void STDCALL SetWindowText( int nState, const wchar_t *pszText ) 
	{ 
		static_assert( sizeof(wchar_t) == sizeof(WORD), "wchar_t and WORD size mismatch" ); 
		SetWindowText( nState, reinterpret_cast<const WORD*>( pszText ) ); 
	}
	virtual void STDCALL AppendText( const WORD *pszText );

	// serializing...
	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL Reposition( const CTRect<float> &rcParent );
	
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );

	// drawing
	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CUIScrollTextBoxBridge : public IUIScrollTextBox, public CUIScrollTextBox
{
	OBJECT_NORMAL_METHODS( CUIScrollTextBoxBridge );
	DECLARE_SUPER( CUIScrollTextBox );
	DEFINE_UICONTAINER_BRIDGE;

	virtual void STDCALL AppendText( const WORD *pszText ) { CSuper::AppendText( pszText ); }
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __UI_SCROLL_TEXT_H__
