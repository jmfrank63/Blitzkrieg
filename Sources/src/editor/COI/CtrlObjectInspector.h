#if !defined(AFX_CTRLOBJECTINSPECTOR_H__A6751B03_5DCC_4993_8D98_89E650E73626__INCLUDED_)
#define AFX_CTRLOBJECTINSPECTOR_H__A6751B03_5DCC_4993_8D98_89E650E73626__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Variant.h"
using namespace std;
int ConvertFromDomenTypeToRef( int nDomenType );

typedef std::list<string> CStlStringList;

typedef int PropID;
typedef int DomenID;
typedef int GroupID;

const PropID PropIDEmpty = -1;
const GroupID GroupIDEmpty = -1;
const GroupID GroupDefault = -2;

const int N_BORDER = 2;
const int N_TEXT_BORDER = 3;

enum
{
	DT_ERROR = 0,
	DT_DEC,
	DT_HEX,
	DT_STR,
	DT_BOOL,
  DT_BROWSE,
  DT_BROWSEDIR,
  DT_COMBO,
	DT_COLOR,
	DT_FLOAT,
	
	DT_ANIMATION_REF,
	DT_FUNC_PARTICLE_REF,
	DT_EFFECT_REF,
	DT_WEAPON_REF,
	DT_SOLDIER_REF,
	DT_ACTION_REF,
	DT_SCENARIO_MISSION_REF,
	DT_TEMPLATE_MISSION_REF,
	DT_CHAPTER_REF,
	DT_SOUND_REF,
	DT_SETTING_REF,
	DT_ASK_REF,
	DT_DEATH_REF,
	DT_CRATER_REF,
	DT_MAP_REF,
	DT_MUSIC_REF,
	DT_MOVIE_REF,
	DT_PARTICLE_TEXTURE_REF,
	DT_WATER_TEXTURE_REF,
	DT_ROAD_TEXTURE_REF,

	DT_CUSTOM
};

struct SCOIProperties
{
  PropID  idProp;
	DomenID idDomen;
	GroupID idGroup;
	CVariant varValue;
	string  strName;
  bool    bReadOnly;
  vector<string> szStrs;  // поля комбо-бокса
};
typedef std::map<PropID, SCOIProperties> CCOIPropMap; // хранить все свойства по ID
typedef std::list<SCOIProperties*> CCOIPropPtrs;

struct SCOIGroup
{
	bool    isExpand;
	bool    isVisible;
  bool    bRadioGroup;
  PropID  iActiveProp;  // если это радио-группа, то здесь сохраняется текущий активный элемент
	CCOIPropPtrs aPorops;
	string strGroupName;
	SCOIGroup() : isExpand(true), isVisible(true), strGroupName("Unnamed") {}
};
typedef std::map<GroupID, SCOIGroup> CCOIGpoupMap;

struct SCOIPaintElem
{
	SCOIProperties *pProp;
	SCOIGroup *pGroup;
	SCOIPaintElem() : pProp(0), pGroup(0) {}
};

typedef std::vector<SCOIPaintElem> CCOIPaintElemVector;


struct SCOICustomListDomen
{
	CStlStringList aValueSet;
};


class COIEdit;
class COICombo;
class COIBrowseEdit;
class COIReferenceEdit;
class COIColorEdit;
class COIRelEdit;

class CCtrlObjectInspector : public CWnd
{
private:
	bool		m_haveFocus;
	CSize		m_sizeClient;
	int			m_nLineHeight;
	CFont		m_fntDef, m_fntDefBold;
	COIEdit		*const m_pEdit;
	COICombo	*const m_pCombo;
  COIBrowseEdit *const m_pBEdit;
  COIReferenceEdit *const m_pReference;
	COIColorEdit  *const m_pCEdit;
	CButton		m_button;
	int			m_nSplitterPos;
  CWnd    *pActiveWnd;
	CImageList imlIcons;

	CCOIPropMap		m_mapProps;
	CCOIGpoupMap	m_mapGroups;	// все группы
	CCOIPaintElemVector m_aPaintElems;

	int m_nFirstElem;	// первая строчка которая видна на экране из m_aPaintElems
	int	m_nCurVirtualLine; // текущий выбранный элемент из m_aPaintElems
	int m_nCurGroup;
	bool bDraggingSplitter;

	void Init();
	void MakePaintList();
	void UpdateScrollers( int nFirstVirtualLine = -1 );

	CRect GetPaintColPartRect( int nPaintLine, int nCol );
	CRect GetTextColPartRect( int nPaintLine, int nCol, bool bHasIcon = false );
	CRect GetPaintLineRect( int nPaintLine );
	void ProcessKeyInput( UINT nChar );
	void SelectRow( int nVirtualLine, bool needHide = false );
	void ExpandGroup( bool needInverse = false, bool isExpand = true );
	void LooseFocus();
	void DrawPlus( CDC* pDC, int nLine, bool isPlus );

	CRect	GetPlusRect( int nPaintLine ) const	{ int nSideSize = ( m_nLineHeight / 4 ); return CRect( nSideSize, nSideSize + nPaintLine * m_nLineHeight, m_nLineHeight - nSideSize, (nPaintLine+1) * m_nLineHeight - nSideSize ); }
	int		GetPaintLine( const CPoint &point ) const { return ( point.y / m_nLineHeight ); } // 0 - if then click on caption
	int		GetCol( const CPoint &point ) const { return point.x > m_nSplitterPos; }
	int		GetLineCount() const				{ return m_sizeClient.cy / m_nLineHeight - 1; }
	int		PaintLineToVirtual( int nPaintLine ) const { return m_nFirstElem + nPaintLine - 1; }
	int		VirtualToPaintLine(int nVirtualLine) const { return nVirtualLine - m_nFirstElem + 1; }
	SCOIPaintElem *GetVirtualElem( int nElem )		{ return ( nElem > -1 && nElem < m_aPaintElems.size() ) ? &m_aPaintElems[nElem] : 0; }

public:
	CCtrlObjectInspector();

public:

public:
	void ClearAll();
	
	void AddCustomDomen( PropID idNewDomen, int eBaseDomenType );
	void AddCustomListDomen( PropID idNewDomen, int eBaseDomenType, CStlStringList );
	bool IsValidDomen( DomenID idDomen );

	void SetGroup( GroupID idGroup, const string strName, bool bRadioGroup = false );
	bool AddPropertiesValue( PropID idProp, DomenID idDomen, const string strName, const CVariant &var, 
                           GroupID idGroup = GroupDefault, bool bReadOnly = false );
  void AddPropertyString( PropID idProp, const string &szStr );
  CVariant GetPropertyValue( PropID idProp );
  string   GetPropertyName( PropID idProp );
	bool SetPropertiesValue( PropID idProp, const CVariant &var );
	void SelectProperties( PropID nID );
	PropID HitTest( CPoint ptClient );	// PropIDEmpty if not click in properties
  PropID GetActiveProp( int nGroupID );
	void   SetActiveProp( PropID nID );

	PropID GetMyActiveProp();		//RR
	
	virtual void OnPropertiesChangeOK( PropID idProp, const CVariant &var ) {}
	virtual void OnEditCustomDomen( DomenID idDomen, CVariant &var ) {}

	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

public:
	virtual ~CCtrlObjectInspector();

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	DECLARE_MESSAGE_MAP()
};



#endif // !defined(AFX_CTRLOBJECTINSPECTOR_H__A6751B03_5DCC_4993_8D98_89E650E73626__INCLUDED_)
