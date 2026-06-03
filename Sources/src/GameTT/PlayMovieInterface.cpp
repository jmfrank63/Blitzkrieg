#include "StdAfx.h"

#include "PlayMovieInterface.h"

#include "iMission.h"
#include "..\Misc\HPTimer.h"
int CICPlayMovie::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &szSequenceName );
	saver.Add( 2, &nNextICTypeID );
	saver.Add( 3, &szNextICConfig );
	return 0;
}
void CICPlayMovie::Configure( const char *pszConfig )
{
	if ( !pszConfig ) return;
	std::vector<std::string> szStrings;
	NStr::SplitString( pszConfig, szStrings, ';' );
	if ( szStrings.size() >= 1 ) 
		szSequenceName = szStrings[0];
	if ( szStrings.size() >= 2 ) 
		nNextICTypeID = NStr::ToInt( szStrings[1] );
	else
		nNextICTypeID = 0;
	if ( szStrings.size() >= 3 ) 
	{
		for ( int i = 2; i < szStrings.size(); ++i )
			szNextICConfig += szStrings[i] + ";";
		if ( !szNextICConfig.empty() ) 
			szNextICConfig.resize( szNextICConfig.size() - 1 );
	}
	else
		szNextICConfig.clear();
}
void CICPlayMovie::PostCreate( IMainLoop *pML, CPlayMovieInterface *pInterface ) 
{ 
	if ( GetGlobalVar("novideo", 0) != 0 )
	{
		CPtr<CPlayMovieInterface> pInt = pInterface;
		pInterface->Done();
		if ( nNextICTypeID != 0 ) 
			pML->Command( nNextICTypeID, szNextICConfig.c_str() );
		else
			pML->Command( MAIN_COMMAND_EXIT_GAME, 0 );
	}
	else
	{
		pInterface->LoadMovieSequence( (szSequenceName + ".xml").c_str() );
		pInterface->SetNextInterface( nNextICTypeID, szNextICConfig );
		pML->PushInterface( pInterface ); 
	}
}
static const NInput::SRegisterCommandEntry movieCommands[] = 
{
	{ "movie_skip_sequence"	,	MC_MOVIE_SKIP_SEQUENCE	},
	{ "movie_skip_movie"		, MC_MOVIE_SKIP_MOVIE			},
	{ "movie_skip_frame"		, MC_MOVIE_SKIP_FRAME			},
	{ 0											,	0												}
};
CPlayMovieInterface::CPlayMovieInterface()
: CInterfaceScreenBase( "InterMission" )
{
	nCurrMovie = -1;
	nNextInterfaceCommandTypeID = -1;
}
CPlayMovieInterface::~CPlayMovieInterface()
{
}
void CPlayMovieInterface::LoadMovieSequence( const std::string &szFileName )
{
	movies.clear();
	if ( GetGlobalVar("novideo", 0) != 0 )
	{
		nCurrMovie = 1000000000;
		return;
	}
	if ( CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream(szFileName.c_str(), STREAM_ACCESS_READ) )
	{
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
		saver.Add( "Movies", &movies );
		const std::string szBaseDirName = GetSingleton<IDataStorage>()->GetName();
		for ( std::vector<SMovie>::iterator it = movies.begin(); it != movies.end(); ++it )
			it->szFileName = szBaseDirName + it->szFileName;
	}
	nCurrMovie = movies.empty() ? -1 : 0;
}
void CPlayMovieInterface::SetNextInterface( const int nTypeID, const std::string &szConfig )
{
	nNextInterfaceCommandTypeID = nTypeID;
	szNextInterfaceCommandConfig = szConfig;
}
bool CPlayMovieInterface::Init()
{
	CInterfaceScreenBase::Init();
	movieMsgs.Init( pInput, movieCommands );
	SetBindSection( "play_movies" );
	GetSingleton<ICursor>()->Show( false );
	pScene->RemoveSceneObject( 0 );
	while ( pScene->ToggleShow(SCENE_SHOW_HAZE) != false );
	return true;
}
void CPlayMovieInterface::Done() 
{
	CInterfaceScreenBase::Done();
	pScene->RemoveSceneObject( pPlayer );
	GetSingleton<ICursor>()->Show( true );
}
bool CPlayMovieInterface::ProcessUIMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID )
	{
		case MC_MOVIE_SKIP_SEQUENCE:
		case MC_MOVIE_SKIP_MOVIE:
			return ProcessMessage( msg );
		default:
			return CInterfaceScreenBase::ProcessUIMessage( msg );
	}
}
bool CPlayMovieInterface::ProcessMessage( const SGameMessage &msg )
{
	switch ( msg.nEventID ) 
	{
		case MC_MOVIE_SKIP_SEQUENCE:
			if ( pPlayer && ((nCurrMovie >= 0) && (nCurrMovie < movies.size())) && movies[nCurrMovie].bCanInterupt ) 
			{
				pPlayer->Stop();
				nCurrMovie = 1000000000;
			}
			break;
		case MC_MOVIE_SKIP_MOVIE:
			if ( pPlayer && ((nCurrMovie >= 0) && (nCurrMovie < movies.size())) && movies[nCurrMovie].bCanInterupt ) 
				pPlayer->Stop();
			break;
		default:
			AddMessage( msg );
			return false;
	}
	return true;
}
void CPlayMovieInterface::Step( bool bAppActive )
{
	CInterfaceScreenBase::Step( bAppActive );
	if ( pPlayer ) 
	{
		if ( !pPlayer->IsPlaying() ) 
		{
			pScene->RemoveSceneObject( pPlayer );
			++nCurrMovie;
			if ( PlayMovie() == false ) 
				StartNextInterface();						// exit this screen...
		}
	}
	else if ( PlayMovie() == false ) 
		StartNextInterface();								// exit this screen...
}
void CPlayMovieInterface::StartNextInterface()
{
	if ( nNextInterfaceCommandTypeID != 0 ) 
		GetSingleton<IMainLoop>()->Command( nNextInterfaceCommandTypeID, szNextInterfaceCommandConfig.c_str() );
	else
		GetSingleton<IMainLoop>()->Command( MAIN_COMMAND_EXIT_GAME, 0 );
}
bool CPlayMovieInterface::PlayMovie()
{
	if ( (nCurrMovie < 0) || (nCurrMovie >= movies.size()) )
		return false;
	pPlayer = CreateObject<IVideoPlayer>( SCENE_VIDEO_PLAYER );
	const CTRect<long> rcScreen = pGFX->GetScreenRect();
	pPlayer->SetDstRect( rcScreen, true );
	const double fClockRate = NHPTimer::GetClockRate();
	const std::string szMovieName = movies[nCurrMovie].szFileName + ( fClockRate > 900000000 ? ".bik" : "_l.bik" );
	int nMovieLength = pPlayer->Play( szMovieName.c_str(), 0, pGFX, GetSingleton<ISFX>() );
	if ( nMovieLength == 0 ) 
		nMovieLength = pPlayer->Play( (movies[nCurrMovie].szFileName + ".bik").c_str(), 0, pGFX, GetSingleton<ISFX>() );
	if ( nMovieLength == 0 ) 
	{
		pPlayer = 0;
		return false;
	}
	pScene->AddSceneObject( pPlayer );
	return true;
}
void CPlayMovieInterface::OnGetFocus( bool bFocus )
{
	CInterfaceScreenBase::OnGetFocus( bFocus );
	if ( bFocus ) 
		SuspendAILogic( true );
}
int CPlayMovieInterface::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CInterfaceScreenBase*>(this) );
	saver.Add( 2, &movies );
	saver.Add( 3, &nCurrMovie );
	saver.Add( 4, &pPlayer );
	return 0;
}
