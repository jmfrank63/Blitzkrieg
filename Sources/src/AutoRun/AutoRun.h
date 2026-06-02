#if !defined(__AUTO_RUN_APPLICATION__)
#define __AUTO_RUN_APPLICATION__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

class CAutoRunApp : public CWinApp
{
public:
	CAutoRunApp();

	public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
#endif // !defined(__AUTO_RUN_APPLICATION__)
