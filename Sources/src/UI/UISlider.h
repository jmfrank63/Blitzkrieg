#ifndef __UI_SLIDER_H__
#define __UI_SLIDER_H__

#include "UIButton.h"
class CUISlider : public CSimpleWindow
{
	DECLARE_SERIALIZE;
	int m_nMin, m_nMax, m_nStep;
	int m_nPrevPos, m_nPos;
	int m_nKeyStep;													//keyboard step
	int m_nElevatorWidth;										//размер элеватора вдоль линейки
	int m_nLineWidth;												//ширина линейки
	bool bVertical;													//если true то вертикальный слайдер
	bool bSelElevator;											// выделен ли элеватор
	
	CPtr<IGFXTexture> pSliderTexture;				// внешний вид - текстура
	CTRect<float> sliderMapa;
	
	int ComputeElevatorCoord();
	void UpdatePosition( int nCoord );

	void NotifyPositionChanged();
public:
	CUISlider() : m_nMin( 0 ), m_nMax( 0 ), m_nPos( 0 ), m_nStep( 0 ), m_nKeyStep( 20 ), m_nElevatorWidth( 0 ),
		m_nLineWidth( 0 ), bVertical( false ), bSelElevator( false ), m_nPrevPos( -13 ) {}
	virtual ~CUISlider() {}

	virtual bool STDCALL OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState );
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );

	virtual int STDCALL operator&( IDataTree &ss );
	virtual void ScaleLayout( const CVec2 &vScale );

	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );

	virtual bool STDCALL OnMouseMove( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnLButtonDown( const CVec2 &vPos, EMouseState mouseState );
	virtual bool STDCALL OnLButtonUp( const CVec2 &vPos, EMouseState mouseState ) { return true; }
	
	void SetPosition( int nPos );
	int GetPosition() { return m_nPos; }
	int GetMinValue() { return m_nMin; }
	int GetMaxValue() { return m_nMax; }

	void IncPosition( int nStep ) { m_nPos += nStep; if ( m_nPos > m_nMax ) m_nPos = m_nMax; }
	void DecPosition( int nStep ) { m_nPos -= nStep; if ( m_nPos < m_nMin ) m_nPos = m_nMin; }
	bool IsVertical() { return bVertical; }

	void SetMinValue( int nMin );
	void SetMaxValue( int nMax );
	void SetStep( int nStep ) { m_nStep = nStep; }
};
class CUISliderBridge : public IUISlider, public CUISlider
{
	OBJECT_NORMAL_METHODS( CUISliderBridge );
public:
	DECLARE_SUPER( CUISlider );
	DEFINE_UIELEMENT_BRIDGE;
	virtual void STDCALL SetMinValue( int nVal ) { CSuper::SetMinValue( nVal ); }
	virtual void STDCALL SetMaxValue( int nVal ) { CSuper::SetMaxValue( nVal ); }
	virtual void STDCALL SetStep( int nVal ) { CSuper::SetStep( nVal ); }
	virtual void STDCALL SetPosition( int nPos ) { CSuper::SetPosition( nPos ); }
	virtual int STDCALL GetPosition() { return CSuper::GetPosition(); }
};
class CUIScrollBar : public CMultipleWindow
{
	DECLARE_SERIALIZE;
	CUIButton *pMinButton;			//инициализируется во время загрузки и используется для ускорения доступа к компонентам
	CUIButton *pMaxButton;
	CUISlider *pSlider;
	int m_nButtonStep;					//сдвиг при нажатии на кнопку
	DWORD dwLastUpdateTime;
	
	void NotifyPositionChanged();
	bool IsVertical() { return pSlider->IsVertical(); }
public:
	CUIScrollBar() : pMinButton( 0 ), pMaxButton( 0 ), pSlider( 0 ), m_nButtonStep( 1 ), dwLastUpdateTime( 0 ) {}
	~CUIScrollBar() {}
	
	virtual void STDCALL Reposition( const CTRect<float> &rcParent );
	virtual void ScaleLayout( const CVec2 &vScale );
	/*
	virtual void STDCALL SetState( int nState );
	virtual int  STDCALL GetState() { return nCurrentState; }
	*/
	
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );
	virtual bool STDCALL Update( const NTimer::STime &currTime );
	
	virtual int STDCALL operator&( IDataTree &ss );
	
	void SetPosition( int nPos ) { pSlider->SetPosition( nPos ); }
	int GetPosition() { return pSlider->GetPosition(); }
	int GetMinValue() { return pSlider->GetMinValue(); }
	int GetMaxValue() { return pSlider->GetMaxValue(); }

	void SetMinValue( int nMin ) { pSlider->SetMinValue( nMin ); }
	void SetMaxValue( int nMax ) { pSlider->SetMaxValue( nMax ); }
	void SetStep( int nStep ) { pSlider->SetStep( nStep ); }
	void SetButtonStep( int nVal ) { m_nButtonStep = nVal; }
};

class CUIScrollBarBridge : public IUIScrollBar, public CUIScrollBar
{
	OBJECT_NORMAL_METHODS( CUIScrollBarBridge );
	DECLARE_SUPER( CUIScrollBar );
	DEFINE_UICONTAINER_BRIDGE;
	virtual void STDCALL SetMinValue( int nVal ) { CSuper::SetMinValue( nVal ); }
	virtual void STDCALL SetMaxValue( int nVal ) { CSuper::SetMaxValue( nVal ); }
	virtual void STDCALL SetStep( int nVal ) { CSuper::SetStep( nVal ); }
	virtual void STDCALL SetButtonStep( int nVal ) { CSuper::SetButtonStep( nVal ); }
	virtual void STDCALL SetPosition( int nPos ) { CSuper::SetPosition( nPos ); }
	virtual int STDCALL GetPosition() { return CSuper::GetPosition(); }
};
#endif		//__UI_SLIDER_H__
