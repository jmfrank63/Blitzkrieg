#ifndef __IMAIN_H__
#define __IMAIN_H__

// For GFXNativeWindow in InitializeWithWindow below. GFXPlatform.h is 24
// lines with no includes of its own, so this brings in the opaque window
// handle and nothing else of GFX - iMain.h is reached from Formats and
// RandomMapGen, which must not grow a renderer dependency.
#include "../GFX/GFXPlatform.h"
#pragma ONCE
#include "iMainClassIDs.h"
interface IInterfaceObject : public IRefCount
{
	virtual void STDCALL Step( bool bAppActive ) = 0;
	virtual bool STDCALL ProcessUIMessage( const SGameMessage &msg ) = 0;
	virtual bool STDCALL ProcessTextMessage( const STextMessage &msg ) = 0;
	virtual bool STDCALL GetMessage( SGameMessage *pMsg ) = 0;
};
interface IInterfaceBase : public IInterfaceObject
{
	virtual bool STDCALL Init() = 0;
	virtual void STDCALL Done() = 0;
	virtual void STDCALL OnGetFocus( bool bFocus ) = 0;
	virtual void STDCALL StartInterface() = 0;
};
interface IInterfaceCommand : public IRefCount
{
	virtual void STDCALL Exec( interface IMainLoop *pML ) = 0;
	virtual void STDCALL Configure( const char *pszConfig ) {  }
	virtual void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) = 0;
	virtual NTimer::STime STDCALL GetDelayedTime() const { return 0; }
};
interface IMainLoop : public IRefCount
{
	enum { tidTypeID = MAIN_MAIN_LOOP };
	virtual void STDCALL ConfigureNet( const int nAppID, const int nPort ) = 0;
	virtual const char* STDCALL GetBaseDir() const = 0;
	virtual bool STDCALL StepApp( bool bActive ) = 0; // return false on exit state
	virtual void STDCALL Command( IInterfaceCommand *pCommand ) = 0;
	virtual void STDCALL Command( int nCommandID, const char *pszConfiguration ) = 0;
	virtual void STDCALL ResetStack() = 0;
	virtual void STDCALL SetInterface( IInterfaceBase *pNewInterface ) = 0;
	virtual void STDCALL PushInterface( IInterfaceBase *pNewInterface ) = 0;
	virtual void STDCALL PopInterface() = 0;
	virtual IInterfaceBase* STDCALL GetInterface() const = 0;
	virtual void STDCALL Pause( const bool _bPause, const int _nPauseReason ) = 0;
	virtual bool STDCALL IsPaused() const = 0;
	virtual void STDCALL EnableMessageProcessing( const bool bEnable ) = 0;
	virtual void STDCALL ClearResources( const bool bClearAll = false ) = 0;
	virtual void STDCALL StoreScenarioTracker() = 0;
	virtual void STDCALL RestoreScenarioTracker() = 0;
	virtual void STDCALL SerializeConfig( const bool bRead, const DWORD dwSerialize ) = 0;
	virtual void STDCALL Serialize( IStructureSaver *pSS, interface IProgressHook *pHook = 0 ) = 0;
};
interface IFilesInspector : public IRefCount
{
	enum { tidTypeID = MAIN_FILES_INSPECTOR };
	virtual bool STDCALL AddEntry( const std::string &szName, interface IFilesInspectorEntry *pEntry ) = 0;
	virtual bool STDCALL RemoveEntry( const std::string &szName ) = 0;
	virtual interface IFilesInspectorEntry* STDCALL GetEntry( const std::string &szName ) = 0;
	virtual bool STDCALL InspectStorage( IDataStorage *pStorage ) = 0;
	virtual void STDCALL Clear() = 0;
};
interface IFilesInspectorEntry : public IRefCount
{
	virtual void STDCALL InspectStream( const std::string &szName ) = 0;
	virtual void STDCALL Clear() = 0;
};
interface IFilesInspectorEntryCollector : public IFilesInspectorEntry
{
	virtual void STDCALL Configure( const char *pszConfig ) = 0;
	virtual const std::vector<std::string>& STDCALL GetCollected() const = 0;
};
namespace NMain
{
	// Starts the engine on the window the renderer will draw into. The
	// four-handle form below forwards to this and is kept only so existing
	// callers compile; its other three arguments were never read.
	bool STDCALL InitializeWithWindow( GFXNativeWindow window );
	bool STDCALL Initialize( HWND hWnd3D, HWND hWndInput, HWND hWndSound, bool bGame );
	bool STDCALL Finalize();
	bool STDCALL IsInitialized();
	bool STDCALL CanLaunch();
	void SetupGlobalVarConsts( class CTableAccessor &table );
	// Reads ui\ModStyles\<mod folder>\consts.xml over the constants the loaded
	// mod shipped. Call it after SetupGlobalVarConsts; see the note there.
	void SetupModStyleConsts();
	const SModuleDescriptor* STDCALL GetModuleDesc( int nType );
	int STDCALL LoadAllModules( const char *pszPath );
	void STDCALL UnloadAllModules();
	bool STDCALL SwitchGame( bool bOn );
	const SModuleDescriptor* GetFirstModuleDesc();
	const SModuleDescriptor* GetNextModuleDesc();
	const std::string GetModuleFileNameByDesc( const SModuleDescriptor *pModule );
	bool SetGameDirectory();
	bool CheckBetaKey();
};
IMainLoop* STDCALL CreateMainLoop();
IObjectFactory* STDCALL GetMainObjectFactory();
#define SERIALIZE_CONFIG_OPTIONS		0x00000001
#define SERIALIZE_CONFIG_BINDS			0x00000002
#define SERIALIZE_CONFIG_HELPCALLS	0x00000004
bool SerializeConfig( const bool bRead, const DWORD dwSerialize );
interface IRPGStatsAutomagic : public IRefCount
{
public:
	virtual const char* STDCALL ToStr( const int nVal ) const = 0;
	virtual const int STDCALL ToInt( const char* pszVal ) const = 0;

	virtual bool STDCALL IsLastStr( const char* pszVal ) const = 0;
	virtual bool STDCALL IsLastInt( const int nVal ) const = 0;

	virtual const char* STDCALL GetFirstStr() const = 0;
	virtual const int STDCALL GetFirstInt() const = 0;

	virtual const char* STDCALL GetNextStr( const char* pszVal ) = 0;
	virtual const int STDCALL GetNextInt( const int nVal ) = 0;
};
#endif // __IMAIN_H__
