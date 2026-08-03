#ifndef __PLAYMOVIEINTERFACE_H__
#define __PLAYMOVIEINTERFACE_H__
#pragma ONCE
#include "..\Common\InterfaceScreenBase.h"
#include "..\Input\InputHelper.h"
#include "iMission.h"
class CPlayMovieInterface : public CInterfaceScreenBase
{
	OBJECT_NORMAL_METHODS( CPlayMovieInterface );
	DECLARE_SERIALIZE;
	struct SMovie
	{
		std::string szFileName;							// file name (with respect to 'data' subdir)
		DWORD dwFrameDelay;									// interframe delay for this movie (for 'slideshow' mode)
		bool bCanInterupt;									// can we interupt this movie with mouse/SPACE/ENTER/ESC buttons
		bool bCanSkipFrame;									// can we skip frame (for 'slideshow' mode)
		int operator&( IDataTree &ss )
		{
			CTreeAccessor saver = &ss;
			saver.Add( "FileName", &szFileName );
			saver.Add( "FrameDelay", &dwFrameDelay );
			saver.Add( "CanInterupt", &bCanInterupt );
			saver.Add( "CanSkipFrame", &bCanSkipFrame );
			return 0;
		}
		int operator&( IStructureSaver &ss )
		{
			CSaverAccessor saver = &ss;
			saver.Add( 1, &szFileName );
			saver.Add( 2, &dwFrameDelay );
			saver.Add( 3, &bCanInterupt );
			saver.Add( 4, &bCanSkipFrame );
			return 0;
		}
	};
	NInput::CCommandRegistrator movieMsgs;
	CPtr<IVideoPlayer> pPlayer;						// video player with current movie
	std::vector<SMovie> movies;						// all movies to play
	int nCurrMovie;												// current movie to play
	int nNextInterfaceCommandTypeID;
	std::string szNextInterfaceCommandConfig;
	bool PlayMovie();
	void StartNextInterface();
	virtual bool STDCALL ProcessUIMessage( const SGameMessage &msg );
	virtual bool STDCALL ProcessMessage( const SGameMessage &msg );
	virtual bool OpenCurtains() { return false; }
public:	
	CPlayMovieInterface();
	virtual ~CPlayMovieInterface();
	void LoadMovieSequence( const std::string &szFileName );
	void SetNextInterface( const int nTypeID, const std::string &szConfig );
	virtual bool STDCALL Init();
	virtual void STDCALL Done();
	virtual void STDCALL Step( bool bAppActive );
	virtual void STDCALL OnGetFocus( bool bFocus );
};
class CICPlayMovie : public CInterfaceCommandBase<CPlayMovieInterface, MISSION_INTERFACE_VIDEO>
{
	OBJECT_NORMAL_METHODS( CICPlayMovie );
	DECLARE_SERIALIZE;
	std::string szSequenceName;
	int nNextICTypeID;
	std::string szNextICConfig;
	virtual void PreCreate( IMainLoop *pML ) { pML->PopInterface(); }
	virtual void PostCreate( IMainLoop *pML, CPlayMovieInterface *pInterface );
	CICPlayMovie() {  }
public:
	virtual void STDCALL Configure( const char *pszConfig );
};
#endif // __PLAYMOVIEINTERFACE_H__
