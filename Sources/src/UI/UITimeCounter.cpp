#include "StdAfx.h"

#include "UIMessages.h"
#include "UITimeCounter.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUITimeCounter::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CSimpleWindow*>(this) );
	saver.Add( "Begin", &fBegin );
	saver.Add( "End", &fEnd );
	saver.Add( "Vertical", &bVertical );
	saver.Add( "Color", &dwCounterColor );
	if ( saver.IsReading() )
		dwDisabledCounterColor = dwCounterColor;
	saver.Add( "ColorDisabled", &dwDisabledCounterColor );
	saver.Add( "BGColor", &dwBGColor );

	if ( saver.IsReading() )
		fCurrent = fEnd;
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUITimeCounter::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CSimpleWindow*>(this) );
	saver.Add( 2, &dwBeginTime );
	saver.Add( 3, &dwRingTime );
	saver.Add( 4, &bNeedAnimate );
	saver.Add( 5, &fBegin );
	saver.Add( 6, &fEnd );
	saver.Add( 7, &fCurrent );
	saver.Add( 8, &bVertical );
	saver.Add( 9, &dwCounterColor );
	saver.Add( 10, &dwBGColor );
	saver.Add( 11, &dwDisabledCounterColor );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUITimeCounter::ProcessMessage( const SUIMessage &msg )
{
	bool bRes = CSimpleWindow::ProcessMessage( msg );
	if ( bRes )
		return bRes;

	switch ( msg.nMessageCode )
	{
		case UI_SET_ANIMATION_TIME:
			if ( msg.nFirst == GetWindowID() )
			{
				EnableWindow( false );				//��������� �� ����� �� ����� ��������������
				if ( msg.nFirst == -1 )
				{
					//button is disabled
					bNeedAnimate = false;
					fCurrent = fBegin;
					return true;
				}
				
				bNeedAnimate = true;
				dwBeginTime = GetSingleton<IGameTimer>()->GetGameTime();
				dwRingTime = dwBeginTime + msg.nSecond;
				fCurrent = fBegin;
				return true;
			}
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUITimeCounter::Visit( interface ISceneVisitor *pVisitor )
{
	// ������ ��������
	CTRect<float> screenRC = GetScreenRect();
	SGFXRect2 rc;
	rc.rect = screenRC;
	rc.color = dwBGColor;
	rc.fZ = 0;
	pVisitor->VisitUIRects( 0, 3, &rc, 1 );

	//������ �������
	if ( !bVertical )
	{
		rc.rect.left = screenRC.left + fBegin;
		rc.rect.right = screenRC.left + fCurrent;
	}
	else
	{
		rc.rect.bottom = screenRC.bottom - fBegin;
		rc.rect.top = screenRC.bottom - fCurrent;
	}
	rc.color = GetCounterColor();
	rc.fZ = 0;
	pVisitor->VisitUIRects( 0, 3, &rc, 1 );
	
	CSimpleWindow::Visit( pVisitor );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const DWORD CUITimeCounter::GetCounterColor()
{
	return IsWindowEnabled() ? dwCounterColor : dwDisabledCounterColor;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUITimeCounter::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;
	//������ ��������
	pGFX->SetShadingEffect( 3 );
	pGFX->SetTexture( 0, 0 );
	CTRect<float> screenRC = GetScreenRect();
	SGFXRect2 rc;
	rc.rect = screenRC;
	rc.color = dwBGColor;
	rc.fZ = 0;
	pGFX->DrawRects( &rc, 1 );

	//������ �������
	if ( !bVertical )
	{
		rc.rect.left = screenRC.left + fBegin;
		rc.rect.right = screenRC.left + fCurrent;
	}
	else
	{
		rc.rect.bottom = screenRC.bottom - fBegin;
		rc.rect.top = screenRC.bottom - fCurrent;
	}
	rc.color = GetCounterColor();
	rc.fZ = 0;
	pGFX->DrawRects( &rc, 1 );
	
	CSimpleWindow::Draw( pGFX );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUITimeCounter::Update( const NTimer::STime &currTime )
{
	if ( !bNeedAnimate )
		return true;

	DWORD dwGameTime = GetSingleton<IGameTimer>()->GetGameTime();
	if ( dwGameTime > dwRingTime )
	{
		fCurrent = fEnd;
		bNeedAnimate = false;
//		EnableWindow( true );			//������ ���������� �������� ������ �� ������� ��������� �����
		return true;
	}

	float k = (float) ( dwGameTime - dwBeginTime ) / ( dwRingTime - dwBeginTime );
	if ( k < 0 )
		k = 0;

	fCurrent = fBegin + k * ( fEnd - fBegin );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
