#ifndef __GLOBALS_H__
#define __GLOBALS_H__
#pragma ONCE
interface ISingleton
{
	virtual bool STDCALL Register( int nID, IRefCount *pObj ) = 0;
	virtual bool STDCALL UnRegister( int nID ) = 0;
	virtual bool STDCALL UnRegister( IRefCount *pObj ) = 0;
	virtual IRefCount* STDCALL Get( int nID ) = 0;
	virtual int STDCALL GetAllObjects( IRefCount ***pBuffer, int *pnBufferSize ) = 0;
	virtual void STDCALL Done() = 0;
};
extern ISingleton *g_pGlobalSingleton;
inline ISingleton* GetSingletonGlobal() { return g_pGlobalSingleton; }
inline bool RegisterSingleton( int nID, IRefCount *pObj ) { return GetSingletonGlobal()->Register( nID, pObj ); }
inline bool UnRegisterSingleton( int nID ) { return GetSingletonGlobal()->UnRegister( nID ); }
inline bool UnRegisterSingleton( IRefCount *pObj ) { return GetSingletonGlobal()->UnRegister( pObj ); }
template <class TYPE>
	inline TYPE* GetSingleton( ISingleton *pSingleton ) { return static_cast<TYPE*>( pSingleton->Get(TYPE::tidTypeID) ); }
template <class TYPE>
	inline TYPE* GetSingleton() { return GetSingleton<TYPE>( GetSingletonGlobal() ); }
interface IGlobalVars : public IRefCount
{
	enum { tidTypeID = -1 };
	virtual const char* STDCALL GetVar( const char *pszValueName ) const = 0;
	virtual void STDCALL SetVar( const char *pszValueName, const char *pszValue ) = 0;
	virtual void STDCALL RemoveVar( const char *pszValueName ) = 0;
	virtual void STDCALL RemoveVarsByMatch( const char *pszValueMatch ) = 0;
	virtual const WORD* STDCALL GetWVar( const char *pszValueName ) const = 0;
	virtual void STDCALL SetVar( const char *pszValueName, const WORD *pszValue ) = 0;
	virtual void STDCALL RemoveWVar( const char *pszValueName ) = 0;
	virtual bool STDCALL DumpVars( const char *pszFileName ) = 0;
	
	virtual void STDCALL SerializeVarsByMatch( interface IDataTree *pSS, const char *pszValueMatch ) = 0;
};
inline const char* GetGlobalVar( const char *pszValueName, const char *defval = "" ) { const char *pszVal = GetSingleton<IGlobalVars>()->GetVar( pszValueName ); return pszVal == 0 ? defval : pszVal; }
inline int GetGlobalVar( const char *pszValueName, int defval ) { const char *pszVal = GetSingleton<IGlobalVars>()->GetVar( pszValueName ); return pszVal == 0 ? defval : NStr::ToInt( pszVal ); }
inline float GetGlobalVar( const char *pszValueName, float defval ) { const char *pszVal = GetSingleton<IGlobalVars>()->GetVar( pszValueName ); return pszVal == 0 ? defval : NStr::ToFloat( pszVal ); }
inline unsigned long GetGlobalVar( const char *pszValueName, unsigned long defval ) { const char *pszVal = GetSingleton<IGlobalVars>()->GetVar( pszValueName ); return pszVal == 0 ? defval : NStr::ToULong( pszVal ); }
inline void SetGlobalVar( const char *pszValueName, const char *pszValue ) { GetSingleton<IGlobalVars>()->SetVar( pszValueName, pszValue ); }
inline void SetGlobalVar( const char *pszValueName, int value ) { GetSingleton<IGlobalVars>()->SetVar( pszValueName, NStr::Format("%d", value) ); }
inline void SetGlobalVar( const char *pszValueName, float value ) { GetSingleton<IGlobalVars>()->SetVar( pszValueName, NStr::Format("%g", value) ); }
inline void SetGlobalVar( const char *pszValueName, unsigned long value ) { GetSingleton<IGlobalVars>()->SetVar( pszValueName, NStr::Format("%ul", value) ); }
inline void RemoveGlobalVar( const char *pszValueName ) { GetSingleton<IGlobalVars>()->RemoveVar( pszValueName ); }
inline const WORD* GetGlobalWVar( const char *pszValueName, const WORD *defval = 0 ) { const WORD *pszVal = GetSingleton<IGlobalVars>()->GetWVar( pszValueName ); return pszVal == 0 ? defval : pszVal; }
#ifdef _NATIVE_WCHAR_T_DEFINED
// bridge for native wchar_t; under /Zc:wchar_t- this would redefine the WORD* overload
inline const WORD* GetGlobalWVar( const char *pszValueName, const wchar_t *defval ) { return GetGlobalWVar( pszValueName, reinterpret_cast<const WORD*>( defval ) ); }
#endif
inline void SetGlobalVar( const char *pszValueName, const WORD *pszValue ) { GetSingleton<IGlobalVars>()->SetVar( pszValueName, pszValue ); }
inline void RemoveGlobalWVar( const char *pszValueName ) { GetSingleton<IGlobalVars>()->RemoveWVar( pszValueName ); }
enum
{
	CONSOLE_STREAM_WORLD		= 0,					// command to world
	CONSOLE_STREAM_SCRIPT		= 1,					// command to script
	CONSOLE_STREAM_CONSOLE	= 2,					// feedback to console (just to display)
	CONSOLE_STREAM_COMMAND	= 3,					// command, to parse in console
	CONSOLE_STREAM_CHAT			= 4,					// chat string
	CONSOLE_STREAM_NET_CHAT	= 5,					// net chat (to send by network)
	CONSOLE_STREAM_NETWORK	= 6,					// network commands
	CONSOLE_STREAM_MULTIPLAYER_CHECK = 7,
	CONSOLE_STREAM_UI_TO_MULTYPLAYER = 8,	// exchange ui data with multiplayer support
	CONSOLE_STREAM_MULTIPLAYER_TO_UI = 9,

	CONSOLE_STREAM_FORCE_DWORD = 0x7fffffff
};
interface IConsoleBuffer : public IRefCount
{
	enum { tidTypeID = -2 };
	virtual bool STDCALL Configure( const char *pszConfigure ) = 0;
	virtual void STDCALL Write( int nStreamID, const wchar_t *pszString, DWORD color = 0xffffffff, bool bBackupLog = false ) = 0;
	virtual void STDCALL WriteASCII( int nStreamID, const char *pszString, DWORD color = 0xffffffff, bool bBackupLog = false ) = 0;
	virtual const wchar_t* STDCALL Read( int nStreamID, DWORD *pColor = 0 ) = 0;
	virtual const char* STDCALL ReadASCII( int nStreamID, DWORD *pColor = 0 ) = 0;
	virtual bool STDCALL DumpLog( int nStreamID ) = 0;
};
extern void* (STDCALL *g_pfnGlobalGetTempRawBuffer)( int nAmount, int nBufferIndex );
template <class TYPE>
	TYPE* GetTempBufferN( int nAmount, int nIndex ) { return reinterpret_cast<TYPE*>( (*g_pfnGlobalGetTempRawBuffer)( nAmount*sizeof(TYPE), nIndex ) ); }
template <class TYPE>
	TYPE* GetTempBuffer( int nAmount ) { return GetTempBufferN<TYPE>( nAmount*sizeof(TYPE), 0 ); }
#endif // __GLOBALS_H__
