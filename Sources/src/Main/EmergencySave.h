#ifndef __EMERGENCYSAVE_H__
#define __EMERGENCYSAVE_H__
#pragma ONCE


class CEmergencySave : public IBaseCommand
{
	OBJECT_MINIMAL_METHODS( CEmergencySave );
	CPtr<IMainLoop> pMainLoop;						// main loop to call save from
	CPtr<IStructureSaver> pSS;						// emergency save structure saver
public:
	CEmergencySave( IMainLoop *_pMainLoop, IStructureSaver *_pSS )
		: pMainLoop( _pMainLoop ), pSS( _pSS ) {  }
	virtual void STDCALL Do()
	{
		try
		{
			pMainLoop->operator&( *pSS );
			pSS = 0;
		}
		catch ( ... )
		{
			NPlatform::ShowError( "ERROR", "Unable to execute 'emergency save' command" );
		}
	}
	virtual void STDCALL UnDo() {  }
	virtual bool STDCALL CanUnDo() { return false; }
};
#endif // __EMERGENCYSAVE_H__
