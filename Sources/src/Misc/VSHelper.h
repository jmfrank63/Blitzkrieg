#ifndef __VSHELPER_H__
#define __VSHELPER_H__
#pragma ONCE
namespace NVar
{
template <class TYPE>
	inline const TYPE GetTypedVar( IVarSystem *pVS, const std::string &szVarName, const TYPE &defval )
	{
		variant_t var;
		if ( pVS->Get(szVarName, &var) == false )
			return defval;
		else
			return var;
	}
inline const std::string GetVar( IVarSystem *pVS, const std::string &szVarName, const std::string &defval = "" ) 
{ 
	variant_t var;
	if ( pVS->Get(szVarName, &var) == false )
		return defval;
	else
		return std::string( (const char*)(bstr_t(var)) );
}
inline const std::wstring GetVar( IVarSystem *pVS, const std::string &szVarName, const std::wstring &defval = L"" ) 
{ 
	variant_t var;
	if ( pVS->Get(szVarName, &var) == false )
		return defval;
	else
		return std::wstring( (const wchar_t*)(bstr_t(var)) );
}
inline const bool GetVar( IVarSystem *pVS, const std::string &szVarName, const bool defval = false ) 
{
	return GetTypedVar( pVS, szVarName, defval );
}
inline const BYTE GetVar( IVarSystem *pVS, const std::string &szVarName, const BYTE defval = 0 ) 
{
	return GetTypedVar( pVS, szVarName, defval );
}
inline const short GetVar( IVarSystem *pVS, const std::string &szVarName, const short defval = 0 ) 
{
	return GetTypedVar( pVS, szVarName, defval );
}
inline const long GetVar( IVarSystem *pVS, const std::string &szVarName, const long defval = 0 ) 
{
	return GetTypedVar( pVS, szVarName, defval );
}
inline const int GetVar( IVarSystem *pVS, const std::string &szVarName, const int defval = 0 ) 
{
	return GetTypedVar( pVS, szVarName, long(defval) );
}
inline const float GetVar( IVarSystem *pVS, const std::string &szVarName, const float defval = 0 ) 
{
	return GetTypedVar( pVS, szVarName, defval );
}
inline const double GetVar( IVarSystem *pVS, const std::string &szVarName, const double defval = 0 ) 
{
	return GetTypedVar( pVS, szVarName, defval );
}
template <class TYPE>
	inline void SetTypedVar( IVarSystem *pVS, const std::string &szVarName, const TYPE &value )
	{
		pVS->Set( szVarName, variant_t( value ) );
	}
inline void SetVar( IVarSystem *pVS, const std::string &szVarName, const std::string &defval ) 
{ 
	SetTypedVar( pVS, szVarName, defval.c_str() );
}
inline void SetVar( IVarSystem *pVS, const std::string &szVarName, const std::wstring &defval ) 
{ 
	SetTypedVar( pVS, szVarName, defval.c_str() );
}
inline void SetVar( IVarSystem *pVS, const std::string &szVarName, const bool &defval ) 
{
	SetTypedVar( pVS, szVarName, defval );
}
inline void SetVar( IVarSystem *pVS, const std::string &szVarName, const BYTE &defval ) 
{
	SetTypedVar( pVS, szVarName, defval );
}
inline void SetVar( IVarSystem *pVS, const std::string &szVarName, const short &defval ) 
{
	SetTypedVar( pVS, szVarName, defval );
}
inline void SetVar( IVarSystem *pVS, const std::string &szVarName, const long &defval ) 
{
	SetTypedVar( pVS, szVarName, defval );
}
inline void SetVar( IVarSystem *pVS, const std::string &szVarName, const int &defval ) 
{
	SetTypedVar( pVS, szVarName, long(defval) );
}
inline void SetVar( IVarSystem *pVS, const std::string &szVarName, const float &defval ) 
{
	SetTypedVar( pVS, szVarName, defval );
}
inline void SetVar( IVarSystem *pVS, const std::string &szVarName, const double &defval ) 
{
	SetTypedVar( pVS, szVarName, defval );
}
inline const bool RemoveVar( IVarSystem *pVS, const std::string &szVarName )
{
	return pVS->Remove( szVarName );
}
inline const bool RemoveVarByMatch( IVarSystem *pVS, const std::string &szVarName )
{
	return pVS->RemoveByMatch( szVarName );
}
void ReApply( CPtr<IVarSystem> pVS, CPtr<IVarIterator> pIt )
{
	for ( ; !pIt->IsEnd(); pIt->Next() )
	{
		variant_t varName, varValue;
		pIt->Get( &varName, &varValue );
		const std::string szVarName = (const char*)bstr_t( varName );
		pVS->Set( szVarName, varValue );
	}
}
};
#endif // __VSHELPER_H__
