#ifndef __MAINLOOPCOMMANDS_H__
#define __MAINLOOPCOMMANDS_H__
#pragma ONCE
#include "iMain.h"
class CICSave : public IInterfaceCommand
{
	OBJECT_NORMAL_METHODS( CICSave );
	std::string szFileName;
	NTimer::STime timeDelayed;
	bool bAutoSave;												// autosave
	CICSave() : timeDelayed( 0 ), bAutoSave( false ) {  }
public:
	void STDCALL Exec( IMainLoop *pML );
	void STDCALL Configure( const char *pszConfig ) 
	{ 
		if ( !pszConfig ) return;
		std::vector<std::string> strings;
		NStr::SplitString( pszConfig, strings, ';' );
	
		if ( strings.size() == 2 ) 
		{
			szFileName = strings[0];
			bAutoSave = NStr::ToInt( strings[1] );
		}
		else
		{
			szFileName = pszConfig; 
		}
	}
	void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
	int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &szFileName );
		saver.Add( 2, &timeDelayed );
		return 0;
	}
};
class CICLoad : public IInterfaceCommand
{
	OBJECT_NORMAL_METHODS( CICLoad );
	std::string szFileName;
	NTimer::STime timeDelayed;
	CICLoad() : timeDelayed( 0 ) {  }
public:
	void STDCALL Exec( IMainLoop *pML );
	void STDCALL Configure( const char *pszConfig ) 
	{ 
		if ( !pszConfig ) return;
		szFileName = pszConfig; 
	}
	void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
	int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &szFileName );
		return 0;
	}
};
class CICPopInterface : public IInterfaceCommand
{
	OBJECT_NORMAL_METHODS( CICPopInterface );
	NTimer::STime timeDelayed;
	CICPopInterface() : timeDelayed( 0 ) {  }
public:
	void STDCALL Exec( IMainLoop *pML ) { pML->PopInterface(); }
	void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
	int STDCALL operator&( IStructureSaver &ss ) { return 0; }
};
class CICSendCommand : public IInterfaceCommand
{
	OBJECT_NORMAL_METHODS( CICSendCommand );
	int nCommand;													// command ID
	int nParam;														// command param
	NTimer::STime timeDelayed;
	CICSendCommand() : nCommand( -1 ), nParam( -1 ), timeDelayed( 0 ) {  }
public:
	void STDCALL Exec( IMainLoop *pML );
	void STDCALL Configure( const char *pszConfig );
	void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
	int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &nCommand );
		saver.Add( 2, &nParam );
		return 0;
	}
};
class CICEnableMessageProcessing : public IInterfaceCommand
{
	OBJECT_NORMAL_METHODS( CICEnableMessageProcessing );
	bool bEnable;
	NTimer::STime timeDelayed;
	CICEnableMessageProcessing() : bEnable( true ), timeDelayed( 0 ) {  }
public:
	void STDCALL Exec( IMainLoop *pML ) { pML->EnableMessageProcessing( bEnable ); }
	void STDCALL Configure( const char *pszConfig ) 
	{ 
		if ( !pszConfig ) return;
		bEnable = NStr::ToInt( pszConfig ) != 0; 
	}
	void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
	int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &bEnable );
		return 0;
	}
};
class CICExitGame : public IInterfaceCommand
{
	OBJECT_NORMAL_METHODS( CICExitGame );
	NTimer::STime timeDelayed;
	CICExitGame() : timeDelayed( 0 ) {  }
public:
	void STDCALL Exec( IMainLoop *pML );
	void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
};
class CICChangeMOD : public IInterfaceCommand
{
	OBJECT_NORMAL_METHODS( CICChangeMOD );
	NTimer::STime timeDelayed;
	std::string szMOD;
	CICChangeMOD() : timeDelayed( 0 ) {  }
public:
	void STDCALL Exec( IMainLoop *pML );
	void STDCALL Configure( const char *pszConfig );
	void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
	int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &szMOD );
		return 0;
	}
};
class CICPauseGame : public IInterfaceCommand
{
	OBJECT_NORMAL_METHODS( CICPauseGame );
	NTimer::STime timeDelayed;
	int nPauseReason;
	bool bSetPause;
	CICPauseGame() : timeDelayed( 0 ), nPauseReason( 0 ), bSetPause( 0 ) {  }
public:
	void STDCALL Exec( IMainLoop *pML );
	void STDCALL Configure( const char *pszConfig );
	void STDCALL SetDelayedTime( const NTimer::STime &timeToExecute ) { timeDelayed = timeToExecute; }
	NTimer::STime STDCALL GetDelayedTime() const { return timeDelayed; }
	int STDCALL operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &nPauseReason );
		saver.Add( 2, &bSetPause );
		return 0;
	}
};
#endif // __MAINLOOPCOMMANDS_H__
