#ifndef __UI_TIME_COUNTER_H__
#define __UI_TIME_COUNTER_H__
#include "UIBasic.h"

class CUITimeCounter : public CSimpleWindow
{
	DECLARE_SERIALIZE;
	DWORD dwBeginTime, dwRingTime;
	bool bNeedAnimate;							//����� �� ������������ ��������
	float fBegin, fEnd, fCurrent;		//��������� ��������
	bool bVertical;									//������������ ��� �������������� ��������
	DWORD dwCounterColor, dwBGColor, dwDisabledCounterColor;						//���� ��������

	const DWORD GetCounterColor();
public:
	CUITimeCounter() : dwBeginTime( 0 ), dwRingTime( 0 ), bNeedAnimate( false ),
		fBegin( 0 ), fEnd( 0 ), fCurrent( 0 ), bVertical( true ), dwDisabledCounterColor( 0xffffffff ), dwCounterColor( 0xffffffff ), dwBGColor( 0xff000000 ) {}
	virtual ~CUITimeCounter() {}
	
	virtual int STDCALL operator&( IDataTree &ss );

	virtual bool STDCALL Update( const NTimer::STime &currTime );
	virtual void STDCALL Draw( IGFX *pGFX );
	virtual void STDCALL Visit( interface ISceneVisitor *pVisitor );
	virtual bool STDCALL ProcessMessage( const SUIMessage &msg );
	virtual void ScaleLayout( const CVec2 &vScale );
};
class CUITimeCounterBridge : public IUITimeCounter, public CUITimeCounter
{
	OBJECT_NORMAL_METHODS( CUITimeCounterBridge );
public:
	DECLARE_SUPER( CUITimeCounter );
	DEFINE_UIELEMENT_BRIDGE;
};

#endif		//__UI_TIME_COUNTER_H__
