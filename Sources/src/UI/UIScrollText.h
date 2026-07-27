#ifndef __UI_SCROLL_TEXT_H__
#define __UI_SCROLL_TEXT_H__
#include "UIBasic.h"
#include "UISlider.h"
class CUIScrollTextBox : public CMultipleWindow
{
	DECLARE_SERIALIZE;
	CUIScrollBar *pScrollBar;				//���������������� �� ����� �������� � ������������ ��� ��������� ������� � �����������
	int nScrollBarWidth;
	int m_nY;												//����� �� Y �� ������ ������, ����� ����� ��������������
	int nLeftSpace, nRightSpace;		//������ ������ �������������� ����� �� ���� � ������ �� ����������
	int nTopSpace, nBottomSpace;		//������ ������ �������������� ������ � �����
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

	virtual bool STDCALL OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta ) = 0;
	
	virtual void STDCALL SetWindowText( int nState, const WORD *pszText );
#ifdef _NATIVE_WCHAR_T_DEFINED
	// bridge for native wchar_t; under /Zc:wchar_t- this would redefine the WORD* overload
	inline void STDCALL SetWindowText( int nState, const wchar_t *pszText )
	{
		static_assert( sizeof(wchar_t) == sizeof(WORD), "wchar_t and WORD size mismatch" );
		SetWindowText( nState, reinterpret_cast<const WORD*>( pszText ) );
	}
#endif
	virtual void STDCALL AppendText( const WORD *pszText );

	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL Reposition( const CTRect<float> &rcParent );
	virtual void ScaleLayout( const CVec2 &vScale );
	
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );

	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
};
class CUIScrollTextBoxBridge : public IUIScrollTextBox, public CUIScrollTextBox
{
	OBJECT_NORMAL_METHODS( CUIScrollTextBoxBridge );
	DECLARE_SUPER( CUIScrollTextBox );
	DEFINE_UICONTAINER_BRIDGE;

	virtual void STDCALL AppendText( const WORD *pszText ) { CSuper::AppendText( pszText ); }
};
#endif // __UI_SCROLL_TEXT_H__
