#ifndef __INPUTBIND_H__
#define __INPUTBIND_H__
#pragma ONCE
class CInputBind : public CTRefCount<IInputBind>
{
	OBJECT_SERVICE_METHODS( CInputBind );
	std::string szCommand;
	EInputBindActivationType eType;
	std::vector<std::string> controls;
public:
	virtual void Clear() { szCommand.clear(); controls.clear(); }
	virtual void STDCALL AddControl( const char *pszControl ) { controls.push_back( pszControl ); }
	virtual void STDCALL SetCommand( const char *pszCommand, const EInputBindActivationType _eType )
	{
		szCommand = pszCommand;
		eType = _eType;
	}

	virtual int STDCALL GetNumControls() const { return controls.size(); }
	virtual const char* STDCALL GetControl( const int nIndex ) const { return controls[nIndex].c_str(); }
	virtual const char* STDCALL GetCommand() const { return szCommand.c_str(); }
	virtual EInputBindActivationType STDCALL GetActivationType() const { return eType; }
};
#endif // __INPUTBIND_H__
