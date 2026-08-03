
#include "StdAfx.h"
#include "WindowMessageHandle.h"
#include "Window.h"
#include "IUIInternal.h"
#include "../Input/Input.h"

IMPLEMENT_HANDLE_MAP(CWindow)
IMPLEMENT_MESSAGE_HANDLER(CWindow,ShowWindow)
{
	CWindow *pChild = GetChild( msg.szParam );
	if ( pChild )
		pChild->ShowWindow( msg.nParam );
	return pChild;
}
IMPLEMENT_MESSAGE_HANDLER(CWindow,SwitchTextMode)
{
	if ( msg.nParam )
	{
		GetSingleton<IInput>()->SetTextMode( INPUT_TEXT_MODE_TEXTONLY );
	}
	else
	{
		GetSingleton<IInput>()->SetTextMode( INPUT_TEXT_MODE_NOTEXT );
	}
	return true;
}
/*IMPLEMENT_MESSAGE_HANDLER(CWindow,EnableWindow)
{
	CWindow *pChild = GetChild( cmd.szParam );
	if ( pChild )
		pChild->EnableWindow( cmd.nParam );
	return pChild;
}*/