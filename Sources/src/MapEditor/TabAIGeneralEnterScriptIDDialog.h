#if !defined(__Tabs__AIGeneral_EnterScriptID_Dialog__)
#define __Tabs__AIGeneral_EnterScriptID_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CTabAIGeneralEnterScriptIDDialog : public CDialog
{
public:
	CTabAIGeneralEnterScriptIDDialog( CWnd* pParent = NULL );

	enum { IDD = IDD_TAB_AI_GENERAL_ENTER_SCRIPT_ID };
	int		m_ScriptID;

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
	DECLARE_MESSAGE_MAP()
};
#endif // !defined(__Tabs__AIGeneral_EnterScriptID_Dialog__)
