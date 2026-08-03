
#if !defined(AFX_IUNDOREDOCMD_H__ABC35A7B_CD4D_48AC_A1E7_000DE1228BEA__INCLUDED_)
#define AFX_IUNDOREDOCMD_H__ABC35A7B_CD4D_48AC_A1E7_000DE1228BEA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include "../Formats/fmtMap.h"
class CTemplateEditorFrame;
struct SMapObject;
class IUndoRedoCmd  
{
public:
	virtual void Undo() = 0;
};
struct STileRedoCmdInfo
{
	BYTE oldTile;
	BYTE newTile;

	int posX;
	int posY;
	STileRedoCmdInfo() {}
	STileRedoCmdInfo( int x, int y, BYTE tile,BYTE tile2 ) 
	{
		newTile = tile;
		oldTile = tile2;
		posX = x; 
		posY = y;
	}
};
class CTileRedoCmd : public IUndoRedoCmd  
{
	CTemplateEditorFrame *m_ptr;
	std::vector<STileRedoCmdInfo> m_data;
public:
	virtual void Undo();
	void		Init( CTemplateEditorFrame *ptr, std::vector<STileRedoCmdInfo> &inf )
	{
		m_ptr = ptr;
		m_data = inf;
	}

};
class CDellObjRedoCmd : public IUndoRedoCmd  
{
	CTemplateEditorFrame *m_ptr;
	SGDBObjectDesc desc;
	int player; 
	int scriptId; 
	bool bScenarioUnit;
	int frameIndex;
	CPoint position;
	int m_dir;
public:
	virtual void Undo();
	void		Init( CTemplateEditorFrame *ptr, SGDBObjectDesc &d, int p, CPoint &pos, int dir, int Id, bool _bScenarioUnit, int Index )
	{
		desc = d;
		m_ptr = ptr;
		player = p; 
		position = pos;
		m_dir = dir;
		scriptId = Id ;
		bScenarioUnit = _bScenarioUnit;
		frameIndex = Index;
	}
};
interface IVisObj;
class CAddObjRedoCmd : public IUndoRedoCmd  
{
	CTemplateEditorFrame *m_ptr;
	SMapObject					 *m_obj;
public:
	virtual void Undo();
	void		Init( CTemplateEditorFrame *ptr, SMapObject *obj )
	{
		m_ptr = ptr;
		m_obj = obj;
	}
};
class CAddMultiObjRedoCmd : public IUndoRedoCmd  
{
	CTemplateEditorFrame *m_ptr;
	std::vector<SMapObject*> m_objs;
public:
	virtual void Undo();
	void		Init( CTemplateEditorFrame *ptr, const std::vector<SMapObject*> &obj )
	{
		m_ptr = ptr;
		m_objs = obj;
	}
};
class CMoveObjRedoCmd : public IUndoRedoCmd  
{
	CTemplateEditorFrame *m_ptr;
	SMapObject						*m_obj;
	CPoint								m_oldPos;
	CPoint								m_newPos;
public:
	virtual void Undo();
	void		Init( CTemplateEditorFrame *ptr, SMapObject *obj, CPoint	&oldPos, CPoint	&newPos )
	{
		m_ptr = ptr;
		m_obj = obj;
		m_oldPos = oldPos;
		m_newPos = newPos;
	}
};
class CPutRoadRedoCmd : public IUndoRedoCmd  
{
	CTemplateEditorFrame *m_ptr;
	SRoadItem							m_item;
public:
	virtual void Undo();
	void		Init( CTemplateEditorFrame *ptr, SRoadItem &item )
	{
		m_ptr = ptr;
		m_item = item;
	}
};
class CDellRoadRedoCmd : public IUndoRedoCmd  
{
	CTemplateEditorFrame *m_ptr;
	SRoadItem							m_item;
public:
	virtual void Undo();
	void		Init( CTemplateEditorFrame *ptr, SRoadItem &item )
	{
		m_ptr = ptr;
		m_item = item;
	}
};
struct SObjectDellDisciption
{
	SObjectDellDisciption(){}
	SObjectDellDisciption( SGDBObjectDesc &desc, int player,	CVec3 &position, int dir, int scriptId, bool bScenarioUnit, int frameIndex )
	{
		m_desc = desc;
		m_player	= player; 
		m_position = position;
		m_dir	= dir;
		m_scriptID = scriptId;
		m_bScenarioUnit = bScenarioUnit;
		m_frameIndex = frameIndex;
	}
	SGDBObjectDesc m_desc;
	int m_player; 
	CVec3 m_position;
	int m_dir;
	int m_scriptID;
	bool m_bScenarioUnit;
	int m_frameIndex;
	std::string szBehavior;

};
class CDellMultiObjRedoCmd : public IUndoRedoCmd  
{
	CTemplateEditorFrame *m_ptr;
	std::vector<SObjectDellDisciption> m_objects;
public:
	virtual void Undo();
	void	Init( CTemplateEditorFrame *ptr, std::vector<SObjectDellDisciption> &objects )
	{
		m_ptr = ptr;
		m_objects = objects;
	}
};
#endif // !defined(AFX_IUNDOREDOCMD_H__ABC35A7B_CD4D_48AC_A1E7_000DE1228BEA__INCLUDED_)
