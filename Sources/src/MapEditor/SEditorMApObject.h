
#if !defined(AFX_SEDITORMAPOBJECT_H__F284DFBF_8E87_40C0_94F9_A78B24FD9E26__INCLUDED_)
#define AFX_SEDITORMAPOBJECT_H__F284DFBF_8E87_40C0_94F9_A78B24FD9E26__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Common\MapObject.h"
#include "..\Misc\Manipulator.h"
#include "EditorObjectItem.h"
class SUnitEditorObjectItem : public SEditorObjectItem
{
	virtual IManipulator* GetManipulator();
};

class STrenchEditorObjectItem : public SEditorObjectItem
{
	virtual IManipulator* GetManipulator();
};

class SBuildingEditorObjectItem : public SEditorObjectItem
{
	virtual IManipulator* GetManipulator();
};
class CTrenchManipulator : public CManipulator
{
	OBJECT_MINIMAL_METHODS( CTrenchManipulator );
	SEditorObjectItem *m_obj;

public:
	CTrenchManipulator();
	
	void SetScriptID( const variant_t &value );		
	void GetScriptID( variant_t *pValue, int nIndex = -1 );	
	
	void SetUnitNumber( const variant_t &value );		
	void GetUnitNumber( variant_t *pValue, int nIndex = -1 );	// 

	void SetObject( SEditorObjectItem *p )			{ m_obj = p;}
};


class CUnitManipulator : public CManipulator
{
	static const std::string FORMATIONS_LABELS[5];

	OBJECT_MINIMAL_METHODS( CUnitManipulator );
	SEditorObjectItem *m_obj;

	std::string testString;
public:
	CUnitManipulator();
	void SetAngle( const variant_t &value );		
	void GetAngle( variant_t *pValue, int nIndex = -1 );	

	void SetPlayer( const variant_t &value );		
	void GetPlayer( variant_t *pValue, int nIndex = -1 );	
	
	void SetScriptID( const variant_t &value );		
	void GetScriptID( variant_t *pValue, int nIndex = -1 );	
	
	void SetScenarioUnit( const variant_t &value );		
	void GetScenarioUnit( variant_t *pValue, int nIndex = -1 );	

	void SetTestString( const variant_t &value );		
	void GetTestString( variant_t *pValue, int nIndex = -1 );	

	void SetFrameIndex( const variant_t &value );		
	void GetFrameIndex( variant_t *pValue, int nIndex = -1 );	

	void SetUnitNumber( const variant_t &value );		
	void GetUnitNumber( variant_t *pValue, int nIndex = -1 );	// 

	void SetHealth( const variant_t &value );		
	void GetHealth( variant_t *pValue, int nIndex = -1 );	// 

	void SetObject( SEditorObjectItem *p )			{ m_obj = p;}
};

/**
class CMultiUnitManipulator : public CManipulator
{
	OBJECT_MINIMAL_METHODS( CMultiUnitManipulator );
	std::vector<SEditorObjectItem*> m_objects;

public:
	CMultiUnitManipulator();
	void SetAngle( const variant_t &value );		
	void GetAngle( variant_t *pValue, int nIndex = -1 );	

	void SetPlayer( const variant_t &value );		
	void GetPlayer( variant_t *pValue, int nIndex = -1 );	

	void SetScriptID( const variant_t &value );		
	void GetScriptID( variant_t *pValue, int nIndex = -1 );

	void SetBehavior( const variant_t &value );		
	void GetBehavior( variant_t *pValue, int nIndex = -1 );	

	void AddObject( SEditorObjectItem *p )			{ m_objects.push_back( p );}
	void SetObject( SEditorObjectItem *p )			{ m_objects.clear(); AddObject( p ); }
};
/**/

class CBuildingManipulator : public CManipulator
{
	OBJECT_MINIMAL_METHODS( CBuildingManipulator );
	SEditorObjectItem *m_obj;
public:
	CBuildingManipulator();

	void SetPlayer( const variant_t &value );		
	void GetPlayer( variant_t *pValue, int nIndex = -1 );	
	
	void SetScriptID( const variant_t &value );		
	void GetScriptID( variant_t *pValue, int nIndex = -1 );	

	void SetUnitNumber( const variant_t &value );		
	void GetUnitNumber( variant_t *pValue, int nIndex = -1 );	// 

	void SetHealth( const variant_t &value );		
	void GetHealth( variant_t *pValue, int nIndex = -1 );	// 

	void SetObject( SEditorObjectItem *p )			{ m_obj = p;}
};


#endif // !defined(AFX_SEDITORMAPOBJECT_H__F284DFBF_8E87_40C0_94F9_A78B24FD9E26__INCLUDED_)
