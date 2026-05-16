#ifndef __GLOBALS_H__
#define __GLOBALS_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** singleton class holder
// ** класс, предназначенный для хранения в себе указателей на объекты, которые бывают в единственном числе.
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface ISingleton
{
	// register singleton object for global access
	virtual bool STDCALL Register( int nID, IRefCount *pObj ) = 0;
	// unregister singleton object by ID
	virtual bool STDCALL UnRegister( int nID ) = 0;
	// unregister singleton object by pointer
	virtual bool STDCALL UnRegister( IRefCount *pObj ) = 0;
	// get singleton object by ID
	virtual IRefCount* STDCALL Get( int nID ) = 0;
	// get all registered objects. NOTE: this function uses 'temp buffer 0'
	virtual int STDCALL GetAllObjects( IRefCount ***pBuffer, int *pnBufferSize ) = 0;
	// done - release all objects
	virtual void STDCALL Done() = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern ISingleton *g_pGlobalSingleton;
inline ISingleton* GetSingletonGlobal() { return g_pGlobalSingleton; }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// helper functions для работы с зранилищем "глобальных" (общедоступных) объектов
// зарегистрировать singleton в глобальном хранилище
inline bool RegisterSingleton( int nID, IRefCount *pObj ) { return GetSingletonGlobal()->Register( nID, pObj ); }
inline bool UnRegisterSingleton( int nID ) { return GetSingletonGlobal()->UnRegister( nID ); }
inline bool UnRegisterSingleton( IRefCount *pObj ) { return GetSingletonGlobal()->UnRegister( pObj ); }
// получить singleton по типу из глобального хранилища.
// singleton должен иметь enum с одним полем 'tidTypeID', которое содержит его константу
// и под этой константой он уже зарегистрирован в глобальном хранилище
template <class TYPE>
	inline TYPE* GetSingleton( ISingleton *pSingleton ) { return static_cast<TYPE*>( pSingleton->Get(TYPE::tidTypeID) ); }
template <class TYPE>
	inline TYPE* GetSingleton() { return GetSingleton<TYPE>( GetSingletonGlobal() ); }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** Global Vars - global pseudo-variables system
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
interface IGlobalVars : public IRefCount
{
	enum { tidTypeID = -1 };
	//
	virtual const char* STDCALL GetVar( const char *pszValueName ) const = 0;
	virtual void STDCALL SetVar( const char *pszValueName, const char *pszValue ) = 0;
	virtual void STDCALL RemoveVar( const char *pszValueName ) = 0;
	virtual void STDCALL RemoveVarsByMatch( const char *pszValueMatch ) = 0;
	//
	virtual const WORD* STDCALL GetWVar( const char *pszValueName ) const = 0;
	virtual void STDCALL SetVar( const char *pszValueName, const WORD *pszValue ) = 0;
	virtual void STDCALL RemoveWVar( const char *pszValueName ) = 0;
	// dump vars
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
inline const WORD* GetGlobalWVar( const char *pszValueName, const wchar_t *defval ) { static_assert( sizeof(wchar_t) == sizeof(WORD), "wchar_t and WORD size mismatch" ); return GetGlobalWVar( pszValueName, reinterpret_cast<const WORD*>( defval ) ); }
inline void SetGlobalVar( const char *pszValueName, const WORD *pszValue ) { GetSingleton<IGlobalVars>()->SetVar( pszValueName, pszValue ); }
inline void RemoveGlobalWVar( const char *pszValueName ) { GetSingleton<IGlobalVars>()->RemoveWVar( pszValueName ); }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** console buffer system
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//
interface IConsoleBuffer : public IRefCount
{
	enum { tidTypeID = -2 };
	// configure console buffer
	virtual bool STDCALL Configure( const char *pszConfigure ) = 0;
	// write string to console's stream
	virtual void STDCALL Write( int nStreamID, const wchar_t *pszString, DWORD color = 0xffffffff, bool bBackupLog = false ) = 0;
	// write string to console's stream. doesn't support any locales - just for english text
	virtual void STDCALL WriteASCII( int nStreamID, const char *pszString, DWORD color = 0xffffffff, bool bBackupLog = false ) = 0;
	// read string from console's stream
	virtual const wchar_t* STDCALL Read( int nStreamID, DWORD *pColor = 0 ) = 0;
	// read string from console's stream. doesn't support any locales - just for english text
	virtual const char* STDCALL ReadASCII( int nStreamID, DWORD *pColor = 0 ) = 0;
	// dump console's stream log to the previously configured output devices
	virtual bool STDCALL DumpLog( int nStreamID ) = 0;
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** temporal buffer - special storage for temporal (fire'n'forgot) data
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern void* (STDCALL *g_pfnGlobalGetTempRawBuffer)( int nAmount, int nBufferIndex );
template <class TYPE>
	TYPE* GetTempBufferN( int nAmount, int nIndex ) { return reinterpret_cast<TYPE*>( (*g_pfnGlobalGetTempRawBuffer)( nAmount*sizeof(TYPE), nIndex ) ); }
template <class TYPE>
	TYPE* GetTempBuffer( int nAmount ) { return GetTempBufferN<TYPE>( nAmount*sizeof(TYPE), 0 ); }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __GLOBALS_H__
