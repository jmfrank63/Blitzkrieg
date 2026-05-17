#ifndef __OPTIONSSYSTEMINTERNAL_H__
#define __OPTIONSSYSTEMINTERNAL_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "OptionSystem.h"
#include "..\Misc\VarSystemInternal.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SOption : public SSerialVariantT
{
	int nEditorType;											// editor type: 0 - checkbox, 1 - slider, 2 - droplist
	std::string szAction;									// action to do then data changed
	std::string szActionFill;							// action to fill with data
	DWORD dwFlags;												// flags
	int nOrder;														// sequential order of this opts
	SSerialVariantT defaultValue;					// 
	bool bInstantApply;										// option must be applied immidiately when changed.
	//
	SOption() : nEditorType( EOET_TEXT_ENTRY ), dwFlags( 0 ), nOrder( 0 ) {  }
	//
	void Set( const variant_t &var ) { *(static_cast<SSerialVariantT*>(this)) = var; }
	const variant_t& Get() const { return *(static_cast<const variant_t*>(this)); }
	//
	virtual int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.AddTypedSuper( 1, static_cast<SSerialVariantT*>(this) );
		saver.Add( 2, &nEditorType );
		saver.Add( 3, &szAction );
		saver.Add( 4, &szActionFill );
		saver.Add( 5, &dwFlags );
		saver.Add( 6, &nOrder );
		saver.Add( 7, &defaultValue );
		saver.Add( 8, &bInstantApply );
		return 0;
	}
	virtual int STDCALL operator&( IDataTree &ss )
	{
		CTreeAccessor saver = &ss;
		saver.Add( "EditorType", &nEditorType );
		saver.Add( "Flags", &dwFlags );
		saver.Add( "Order", &nOrder );
		saver.AddTypedSuper( static_cast<SSerialVariantT*>(this) );
		saver.Add( "Action", &szAction );
		saver.Add( "ActionFill", &szActionFill );
		saver.Add( "Default", &defaultValue );
		saver.Add( "InstantApply", &bInstantApply );
		return 0;
	}
};
interface IOption;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class COptionSystem : public CTVarSystem< SOption, CTRefCount<IOptionSystem> >
{
	OBJECT_SERVICE_METHODS( COptionSystem );
	typedef CTVarSystem< SOption, CTRefCount<IOptionSystem> > CBase;
	//
	mutable SOptionDesc descriptor;				// temporal descriptor to store options in
	mutable std::vector<SOptionDropListValue> droplist;	// temporal list for 'droplist' control (localized)

	void InnerSet( const std::string &szVarName, const variant_t &var );
public:
	COptionSystem();
	virtual ~COptionSystem() 
	{  
	}
	//
	virtual bool STDCALL Set( const std::string &szVarName, const variant_t &var );
	// get option descriptor
	virtual const SOptionDesc* STDCALL GetDesc( const std::string &szVarName ) const;
	// get values for option droplist editor type
	virtual const std::vector<SOptionDropListValue>& STDCALL GetDropValues( const std::string &szVarName ) const;
	// begin to iterate through all variables
	virtual IOptionSystemIterator* STDCALL CreateIterator( const DWORD dwMask = 0xffffffff );
	// serialize to configuration file
	virtual bool STDCALL SerializeConfig( IDataTree *pSS );
	virtual void STDCALL Init();
	virtual void STDCALL Repair( IDataTree *pSS, const bool bToDefault );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SOptionSorter
{
	void Sort( std::list<typename COptionSystem::CVarsMap::const_iterator> &vals );
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class COptionMaskAccepter
{
	const DWORD dwMask;
public:
	COptionMaskAccepter( const DWORD _dwMask ) : dwMask( _dwMask ) {  }
	const bool operator()( const SOption &opt ) const 
	{ 
		return dwMask == DWORD(-1) || (opt.dwFlags & dwMask) != 0; 
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class COptionSystemIterator : public CTVarSystemIterator<COptionSystem, CTRefCount<IOptionSystemIterator>, SOptionSorter, COptionMaskAccepter>
{
	typedef CTVarSystemIterator<COptionSystem, CTRefCount<IOptionSystemIterator>, SOptionSorter, COptionMaskAccepter> CBase;
	CPtr<COptionSystem> pOS;
	//
	COptionSystem* GetVS() const { return pOS; }
public:
	COptionSystemIterator( COptionSystem *pOS, const DWORD dwMask );
	virtual ~COptionSystemIterator() {  }
	// get option descriptor
	virtual const SOptionDesc* STDCALL GetDesc() const;
	// get values for option droplist editor type
	virtual const std::vector<SOptionDropListValue>& STDCALL GetDropValues() const;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __OPTIONSSYSTEMINTERNAL_H__
