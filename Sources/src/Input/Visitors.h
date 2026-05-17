#ifndef __VISITORS_H__
#define __VISITORS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CCombo;
struct SCommand;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSetBindSectionVisitor : public IInputVisitor
{
	const std::string szBindSection;
	std::unordered_set<CCombo*, SDefaultPtrHash> combos;
public:
	CSetBindSectionVisitor( const std::string &_szBindSection ) : szBindSection( _szBindSection ) {  }
	//
	virtual bool STDCALL VisitControl( class CControl *pControl );
	virtual bool STDCALL VisitCombo( CCombo *pCombo );
	virtual bool STDCALL VisitBind( class CBind *pBind ) { return false; }
	virtual bool STDCALL VisitCommand( struct SCommand *pCommand ) { return false; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFindBindVisitor : public IInputVisitor
{
	struct SBindStats
	{
		int nCounter;
		CCombo *pCombo;
		//
		SBindStats() : nCounter( 0 ), pCombo( 0 ) {  }
	};
	//
	typedef std::unordered_map<const CControl*, std::list<CBind*>, SDefaultPtrHash> CControlsMap;
	typedef std::unordered_map<CBind*, SBindStats, SDefaultPtrHash> CBindsMap;
	const SCommand *pCommand2Find;				// command to find
	const EInputBindActivationType eType;	// command activation type to find
	CControlsMap controls;								// controls of the bind to find
	CBindsMap binds;											// how many times each bind was found. if this number are equal to number of controls, 
	                                      // then this is the bind we've searched for
	//
	CControl *pCurrControl;								// currently checked control
	CCombo *pCurrCombo;										// currently checked combo
	// found bind and combo
	mutable CCombo *pFoundCombo;					// 
	mutable CBind *pFoundBind;						//
public:
	CFindBindVisitor( const SCommand *_pCommand, const EInputBindActivationType _eType, 
		                const std::vector<const CControl*> &_controls );
	//
	virtual bool STDCALL VisitControl( class CControl *pControl );
	virtual bool STDCALL VisitCombo( CCombo *pCombo );
	virtual bool STDCALL VisitBind( class CBind *pBind );
	virtual bool STDCALL VisitCommand( struct SCommand *pCommand );
	//
	void FinalCheck() const;
	CCombo* GetCombo() const { FinalCheck(); return pFoundCombo; }
	CBind* GetBind() const { FinalCheck(); return pFoundBind; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRemoveComboVisitor : public IInputVisitor
{
	CPtr<CCombo> pCombo2Remove;
public:
	CRemoveComboVisitor( CCombo *_pCombo ) : pCombo2Remove( _pCombo ) {  }
	//
	virtual bool STDCALL VisitControl( class CControl *pControl );
	virtual bool STDCALL VisitCombo( CCombo *pCombo );
	virtual bool STDCALL VisitBind( class CBind *pBind ) { return true; }
	virtual bool STDCALL VisitCommand( struct SCommand *pCommand ) { return false; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __VISITORS_H__
