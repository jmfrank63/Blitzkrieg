#ifndef __KOSTYA_TREEITEM_H__
#define __KOSTYA_TREEITEM_H__
#include <vector>
#include <string>
#include <afxdisp.h>
#include "../../Misc/Basic.h"



class CMultiTree;
class CMultiTreeEditBox;
class CTreeItemComboBox;
class CMultiTreeSlider;
class CPercentDialog;

interface ITreeItem
{
	virtual				std::string				GetItemName ()											= 0;
	virtual				void							SetItemName ( std::string str)			= 0;
	virtual				LPARAM						GetData()														= 0;
	virtual				void							SetData( LPARAM param)							= 0;
	virtual				bool							ifBold()														= 0;
	virtual				void							SetBoldStyle( bool b)								= 0;
	virtual				std::string			  GetNormalProperty()									= 0;

	virtual				bool							BuildEditor( HTREEITEM *item,
																	CMultiTree *treePtr )								= 0;
	virtual				bool							KillEditor ()												= 0;
	virtual ~ITreeItem() {}
};
class CSimpleTreeItem : public ITreeItem
{
	std::string m_str;
	LPARAM m_data;
	bool m_bold;
public:
	bool bKilleditor;
	std::string				GetItemName() 											{ return m_str;				}
	void							SetItemName( std::string str )			{ m_str = str;				}
	LPARAM						GetData()														{ return m_data;			}
	void							SetData( LPARAM param )							{ m_data = param;			}
	bool							ifBold()														{ return m_bold;			}
	void							SetBoldStyle( bool b )							{	m_bold = b;					}
	CSimpleTreeItem() : bKilleditor( false ) {}
	~CSimpleTreeItem() {}
};

class CSTreeItem : public CSimpleTreeItem
{
	COleVariant m_var;
	CMultiTreeEditBox	*m_editCtl;
public:
	std::string GetNormalProperty()									
	{ 
		COleVariant tmp = m_var;
		tmp.ChangeType( VT_BSTR, NULL );
		std::string str = CString( tmp.bstrVal );	
		return str;
	}
	bool BuildEditor( HTREEITEM *item, CMultiTree *treePtr );												
	bool KillEditor();											
	void SetOleData( COleVariant var ) { m_var = var; }
	COleVariant GetOleData() const { return m_var; }
	~CSTreeItem() {}
};

class CTrueFalseTreeItem : public CSimpleTreeItem
{
	bool m_var;
	CTreeItemComboBox	*m_ComboBoxCtrl;
public:
	std::string GetNormalProperty()									{ return m_var ? "true": "false" ;}
	bool BuildEditor( HTREEITEM *item, CMultiTree *treePtr );												
	bool KillEditor();											
	void SetBoolData( bool var ) { m_var = var;  }
	bool GetBoolData() const { return m_var; }
	~CTrueFalseTreeItem() {}
};


class CProcentTreeItem: public CSimpleTreeItem
{
private:
	short m_procent;
	CPercentDialog *m_SliderDlg;
public:
	std::string GetNormalProperty()
	{	
		COleVariant tmp = COleVariant( m_procent );
		tmp.ChangeType( VT_BSTR, NULL );
		std::string str = CString( tmp.bstrVal );	
		return str + std::string( "%" );
	}
	bool BuildEditor( HTREEITEM *item, CMultiTree *treePtr );												
	bool KillEditor();											
	void SetValue( int var ) { m_procent = var;  }
	short GetValue() const { return m_procent; }
	~CProcentTreeItem() {}
};

class CNumComboBoxTreeItem : public CSimpleTreeItem
{
private:
	int m_var;
	CTreeItemComboBox	*m_ComboBoxCtrl;
public:
	std::string GetNormalProperty();
	bool BuildEditor( HTREEITEM *item,	CMultiTree *treePtr );												
	bool KillEditor();											
	void SetNumData( int var ) { m_var = var;  }
	int GetNumData() const { return m_var; }
	~CNumComboBoxTreeItem() {}
};

class CEmptyTreeItem : public CSimpleTreeItem
{
public:
	std::string	 GetNormalProperty() { return ""; }
	bool BuildEditor( HTREEITEM *item,	CMultiTree *treePtr );												
	bool KillEditor();
	~CEmptyTreeItem() {}
};
class CSimplePropertieTreeItem
{
protected:
	CPtr<IManipulator> m_pManipulator;
	std::string m_propName;
public:
	virtual void Setup( std::string &name, IManipulator *ptr )							
	{	
		m_propName = name ;
		m_pManipulator = ptr;
	}
	virtual ~CSimplePropertieTreeItem() {}
};

class CPropertieTreeItem : public CSimpleTreeItem, public CSimplePropertieTreeItem
{
	CMultiTreeEditBox	*m_editCtl;
public:
	std::string GetNormalProperty()								
	{
		COleVariant oletmp;
		variant_t tmp;
		m_pManipulator->GetValue( m_propName.c_str(), &tmp );
		oletmp = tmp;
		oletmp.ChangeType( VT_BSTR, NULL );
		std::string str = CString( oletmp.bstrVal );	
		return str;
	}

	bool BuildEditor( HTREEITEM *item,	CMultiTree *treePtr );												
	bool KillEditor();

	~CPropertieTreeItem() {}
};
class CComboBoxTreeItemPropertieTreeItem : public CSimpleTreeItem, public CSimplePropertieTreeItem
{
	CTreeItemComboBox	*m_ComboBoxCtrl;
public:
	std::string GetNormalProperty()				
	{
		variant_t tmp;
		COleVariant oletmp;
		m_pManipulator->GetValue( m_propName.c_str(), &tmp );
		oletmp = tmp;
		oletmp.ChangeType( VT_BSTR, NULL );
		std::string str = CString( oletmp.bstrVal );	
		return str;
	}

	bool BuildEditor( HTREEITEM *item,	CMultiTree *treePtr );												
	bool KillEditor();
	~CComboBoxTreeItemPropertieTreeItem() {}
};
class CDirChosePropertieTreeItem: public CSimpleTreeItem, public CSimplePropertieTreeItem
{
public:
	std::string GetNormalProperty()								
	{
		variant_t tmp;
		COleVariant oletmp;
		m_pManipulator->GetValue( m_propName.c_str(), &tmp );
		oletmp = tmp;
		oletmp.ChangeType( VT_BSTR, NULL );
		std::string str = CString( oletmp.bstrVal );
		return str;
	}
	
	bool BuildEditor( HTREEITEM *item,	CMultiTree *treePtr );												
	bool KillEditor();
	~CDirChosePropertieTreeItem() {}
};
class CFileChosePropertieTreeItem: public CSimpleTreeItem, public CSimplePropertieTreeItem
{
public:
	std::string GetNormalProperty()
	{
		COleVariant tmp;
		variant_t var_t;
		m_pManipulator->GetValue( m_propName.c_str(), &var_t );
		tmp = var_t;
		tmp.ChangeType( VT_BSTR, NULL );
		std::string str = CString( tmp.bstrVal );
		return str;
	}
	bool BuildEditor( HTREEITEM *item,	CMultiTree *treePtr );												
	bool KillEditor();

	~CFileChosePropertieTreeItem() {}
};

class CUnitsPropertieTreeItem: public CSimpleTreeItem, public CSimplePropertieTreeItem
{
public:
	std::string GetNormalProperty()
	{
		COleVariant tmp;
		variant_t var_t;
		m_pManipulator->GetValue( m_propName.c_str(), &var_t );
		tmp = var_t;
		tmp.ChangeType( VT_BSTR, NULL );
		std::string str = CString( tmp.bstrVal );
		return str;
	}
	bool BuildEditor( HTREEITEM *item,	CMultiTree *treePtr ) ;												
	bool KillEditor()	;
	~CUnitsPropertieTreeItem() {}
};
#endif // __KOSTYA_TREEITEM_H__


