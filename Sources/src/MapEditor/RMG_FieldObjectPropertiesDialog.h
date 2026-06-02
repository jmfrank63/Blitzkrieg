#if !defined(__RMG_Field_Object_Properties_Dialog__)
#define __RMG_Field_Object_Properties_Dialog__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Resource.h"
#include "ResizeDialog.h"

class CRMGFieldObjectPropertiesDialog : public CResizeDialog
{
public:
	static const int VIS_TYPES_COUNT;
	static const int GAME_TYPES_COUNT;
	static const int SQUAD_TYPES_COUNT;
	static const int BUILDING_TYPES_COUNT;
	static const char* VIS_TYPES[];
	static const char* GAME_TYPES[];
	static const char* SQUAD_TYPES[];
	static const char* BUILDING_TYPES[];

	CRMGFieldObjectPropertiesDialog( CWnd* pParent = NULL );
	
	enum { IDD = IDD_RMG_CF_OS_OBJECT_PROPERTIES };
	CStatic	m_Icon;
	CString	m_szName;
	CString	m_szPath;
	CString	m_szStats;
	CString	m_szVisType;
	CString	m_szGameType;
	CString	m_szWeight;

	bool bDisableEditWeight;
	HICON hIcon;

	protected:
	virtual void DoDataExchange( CDataExchange* pDX );

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()

protected:
	virtual bool GetDrawGripper() { return false; }
public:
	static std::string GetObjectStats( IObjectsDB *pODB, const SGDBObjectDesc *pObjectDesc );
};

#endif // !defined(__RMG_Field_Object_Properties_Dialog__)
