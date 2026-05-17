#ifndef __IMSAVEMISSION_H__
#define __IMSAVEMISSION_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "BaseList.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CInterfaceIMSaveMission : public CInterfaceBaseList
{
	OBJECT_NORMAL_METHODS( CInterfaceIMSaveMission );
	bool bClosed;
	std::string szProspectiveFileName;
	//
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	// disable explicit destruction
	virtual ~CInterfaceIMSaveMission();
	
	std::unordered_map<int/*nUserData*/, std::string/*file name*/ > szSaves;

	void OnSelectionChanged();
	void Save();
protected:
	CInterfaceIMSaveMission() {}
	
	//������������� ���� �������
	virtual bool FillListItem( IUIListRow *pRow, const std::string &szFullFileName, bool *pSelectedItem = 0 );		//��������� ������� ������� � ������
	virtual bool OnOk( const std::string &szFullFileName );															//������������ ������ ����, ���������� �����

	virtual bool IsIgnoreSelection() const { return true; }														// user may not select, but enter to edit box
	virtual bool OnOk();
	virtual void STDCALL OnGetFocus( bool bFocus );
public:
	virtual bool STDCALL Init();
	virtual void STDCALL StartInterface();
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CICIMSaveMission : public CInterfaceCommandBase<CInterfaceIMSaveMission, MISSION_INTERFACE_IM_SAVE_MISSION>
{
	OBJECT_NORMAL_METHODS( CICIMSaveMission );
	
	virtual void PostCreate( IMainLoop *pML, CInterfaceIMSaveMission *pILM );
	//
	CICIMSaveMission() {}
public:
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __IMSAVEMISSION_H__
