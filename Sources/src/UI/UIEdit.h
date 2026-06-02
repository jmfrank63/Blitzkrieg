#ifndef __UI_EDIT_BOX_H__
#define __UI_EDIT_BOX_H__
#include "UIBasic.h"
class CUIEditBox : public CSimpleWindow
{
	DECLARE_SERIALIZE;
	int nCursorPos;									//позиция курсора в текущей редактируемой строке
	bool bShowCursor;								//для мигания курсора
	bool bFocused;									//для отображения курсора
	DWORD dwLastCursorAnimatedTime;	//для анимации курсора
	int m_nBeginSel;								//начало выделения
	int m_nEndSel;									//конец выделения
	DWORD dwSelColor;								//цвет для выделенного текста
	int m_nBeginDragSel;						//начало выделения мышкой
	bool bNumericMode;							//вводятся только числа
	bool bGameSpySymbols;						//ограничение на вводимые символы
	bool bLocalPlayerNameMode;			// local player's name allowed symbols
	bool bFileNameSymbols;					//символы доступные для имени файла
	int nMaxLength;									//если эта переменная установлена, то включено ограничение на количество символов в тексте

	std::wstring wszFullText;
	int nBeginText;		//с этой позиции начинается отображение текста szFullText
	bool bTextScroll;	//если установлена эта переменная, то можно вводить текст шире поля edit box

	bool IsValidSymbol( int nAsciiCode );
public:
	CUIEditBox() : nCursorPos( 0 ), dwLastCursorAnimatedTime( 0 ), bShowCursor( 0 ), bFocused( 0 ), bTextScroll( 0 ), nMaxLength( -1 ), bGameSpySymbols( 0 ), bFileNameSymbols( 0 ),
		m_nBeginSel( -1 ), m_nEndSel( -1 ), dwSelColor( 0xff2e401b ), m_nBeginDragSel( -1 ), nBeginText( 0 ), bNumericMode( 0 ), bLocalPlayerNameMode ( false ) {}
	~CUIEditBox() {}

	virtual bool STDCALL OnMouseMove( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnRButtonDown( const CVec2 &vPos, EMouseState mouseState );

	virtual void STDCALL SetWindowText( int nState, const WORD *pszText );
	virtual void STDCALL SetFocus( bool bFocus );
	virtual void STDCALL SetCursor( int nPos );
	virtual int  STDCALL GetCursor() { return nCursorPos; }
	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	virtual bool STDCALL Update( const NTimer::STime &currTime );
	virtual void STDCALL SetSel( int nBegin, int nEnd ) { m_nBeginSel = nBegin; m_nEndSel = nEnd; }
	virtual void STDCALL GetSel( int *nBegin, int *nEnd ) { *nBegin = m_nBeginSel; *nEnd = m_nEndSel; }
	virtual void STDCALL SetMaxLength( const int nLength ) { nMaxLength = nLength; }

	virtual int STDCALL operator&( IDataTree &ss );
	virtual bool STDCALL OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState );
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );
private:
	bool DeleteSelection();
	int GetSelection( int nX );
	void NotifyTextChanged();
	void EnsureCursorVisible();
	bool IsTextInsideEditBox();
};
class CUIEditBoxBridge : public IUIEditBox, public CUIEditBox
{
	OBJECT_NORMAL_METHODS( CUIEditBoxBridge );
	DECLARE_SUPER( CUIEditBox );
	DEFINE_UIELEMENT_BRIDGE;

	virtual void STDCALL SetCursor( int nPos ) { CSuper::SetCursor( nPos ); }
	virtual int  STDCALL GetCursor() { return CSuper::GetCursor(); }
	virtual void STDCALL SetSel( int nBegin, int nEnd ) { CSuper::SetSel( nBegin, nEnd ); }
	virtual void STDCALL GetSel( int *nBegin, int *nEnd ) { CSuper::GetSel( nBegin, nEnd ); }
	virtual void STDCALL SetMaxLength( const int nLength ) { CSuper::SetMaxLength( nLength ); }
};
#endif // __UI_EDIT_BOX_H__
