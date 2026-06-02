#ifndef __UI_NUMBER_INDICATOR_H__
#define __UI_NUMBER_INDICATOR_H__
#include "UIBasic.h"
class CUINumberIndicator : public CSimpleWindow
{
	DECLARE_SERIALIZE;
	struct SValueColor
	{
		DECLARE_SERIALIZE;
	public:
		virtual int STDCALL operator&( IDataTree &ss );
		bool operator < ( const SValueColor &v ) const { return fVal < v.fVal; }
		float fVal;
		DWORD dwColor;
	};
	std::vector<SValueColor> valueColors;
	float m_fVal;

	void SortValues();

public:
	CUINumberIndicator() : m_fVal( 0.0f ) {}
	~CUINumberIndicator() {}

	virtual void STDCALL Draw( interface IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	virtual int STDCALL operator&( IDataTree &ss );
	virtual void STDCALL SetValue( float fVal );
	virtual void STDCALL ClearColors() { valueColors.clear(); }
	virtual void STDCALL SetColor( float fVal, DWORD dwColor );
};
class CUINumberIndicatorBridge : public IUINumberIndicator, public CUINumberIndicator
{
	OBJECT_NORMAL_METHODS( CUINumberIndicatorBridge );
	DECLARE_SUPER( CUINumberIndicator );
	DEFINE_UIELEMENT_BRIDGE;
	virtual void STDCALL SetValue( float fVal ) { CSuper::SetValue( fVal ); }
	virtual void STDCALL ClearColors() { CSuper::ClearColors(); }
	virtual void STDCALL SetColor( float fVal, DWORD dwColor ) { CSuper::SetColor( fVal, dwColor ); }
};
#endif // __UI_NUMBER_INDICATOR_H__
