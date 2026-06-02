#ifndef __INPUTHELPER_H__
#define __INPUTHELPER_H__
namespace NInput
{
struct SRegisterCommandEntry
{
	const char *pszName;
	int nID;
};
class CCommandRegistrator
{
	const SRegisterCommandEntry *pRegisters;
	CPtr<IInput> pInput;
	void RegisterCommands( const SRegisterCommandEntry *_pRegisters )
	{
		NI_ASSERT_T( (pInput != 0) && (_pRegisters != 0), "can't register commands with empty input or registers" );
		pRegisters = _pRegisters;
		for ( ; _pRegisters->pszName != 0; ++_pRegisters )
			pInput->RegisterCommand( _pRegisters->pszName, _pRegisters->nID );
	}
	void UnregisterCommands()
	{
		if ( pRegisters )
		{
			for ( ; pRegisters->pszName != 0; ++pRegisters )
				pInput->UnRegisterCommand( pRegisters->pszName );
		}
	}
public:
	CCommandRegistrator() : pRegisters( 0 ) {  }
	CCommandRegistrator( IInput *_pInput, const SRegisterCommandEntry *_pRegisters ) : pInput( _pInput ) { RegisterCommands( _pRegisters ); }
	~CCommandRegistrator() { UnregisterCommands(); }
	void Init( IInput *_pInput ) { pInput = _pInput; }
	void Init( IInput *_pInput, const SRegisterCommandEntry *_pRegisters ) { pInput = _pInput; (*this) = _pRegisters; }
	void operator=( const SRegisterCommandEntry *_pRegisters ) { UnregisterCommands(); RegisterCommands( _pRegisters ); }
};
};
#endif // __INPUTHELPER_H__
