#ifndef __PROPERTY_DOCK_H__
#define __PROPERTY_DOCK_H__

#include "MTree ctrl\FrameTree.h"


class CPropertyDockBar : public SECControlBar
{
	std::map< std::string, HTREEITEM > m_insertedNodes;
public:
	CPropertyDockBar();
	virtual ~CPropertyDockBar();
	
public:
	
public:
	void AddObjectWithProp( IManipulator *ptr );
	void ClearVariables();
	
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	
public:
	
private:
	CFrameTree m_tree;
	CPtr<IManipulator> m_pCurrentObject;
	

	std::map<std::string, HTREEITEM> m_varHandles;
	int		GetVariable( std::string &name );
	void	AddRootVariable( std::string &str, int variable );
	void	AddManipulatorVariable( std::string &str, IManipulator *ptr ); // добавляет пустые промежуточные nod'ы + конечный( редактируемый ) node
	
	HTREEITEM	AddEmptyNode( std::string &str, HTREEITEM hPARoot = TVI_ROOT ); // node который не содержит данных
	HTREEITEM	AddPropertieNode( std::string &str, std::string &propName,IManipulator *ptr, HTREEITEM hPARoot = TVI_ROOT ); 
	
	
protected:
	
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnKeyframeDeleteNode();
	afx_msg void OnKeyframeResetAll();
	DECLARE_MESSAGE_MAP()
};



#endif	//__PROPERTY_DOCK_H__
