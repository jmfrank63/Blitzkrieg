// SoundVerifycation.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "..\Misc\FileUtils.h"
#include <iostream>
#include "..\..\sdk\FMOD\api\inc\fmod.h"
#include "..\..\sdk\FMOD\api\inc\fmod_errors.h"
#include <stdio.h>
 #include <conio.h>

// filename to store bad sounds
//
std::string szCurrentBeingReading = "current_sound.txt";
std::string szBadSounds = "bad_sounds.txt";

typedef std::unordered_set<std::string> CBadFiles;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CEnumFiles
{
	CBadFiles *pBadFiles;
public:
	CEnumFiles( CBadFiles *_pBadFiles )
		: pBadFiles( _pBadFiles ) { }

	void operator()( const class NFile::CFileIterator &fileIt ) const
	{
		//std::cout<<"+";
		if ( fileIt.IsDirectory() ) 
			return;

		const std::string szFileName = fileIt.GetFilePath();
	//	std::cout<<szFileName<<"\n";
		
		CBadFiles::const_iterator it = pBadFiles->find( szFileName );
		if ( it == pBadFiles->end() )
		{
			// write the file name to recently read,
			// try to open soud with FMOD,
			// if it fails then we gon another bad sound.
			// if all OK  - iterate furter.
			NFile::CFile currentSound;
			if ( !currentSound.Open( szCurrentBeingReading.c_str(), NFile::CFile::modeReadWrite|NFile::CFile::modeCreate ) )
			{
				std::cout<<"cannot open file with latest read files \""<<szCurrentBeingReading<<"\"\n";
				return;
			}
			currentSound.SetLength( 0 );
			currentSound.Write( szFileName.c_str(), szFileName.size() );
			currentSound.Flush();
			currentSound.Close();

			FSOUND_SAMPLE *samp1 = FSOUND_Sample_Load( FSOUND_UNMANAGED, szFileName.c_str(), FSOUND_NORMAL, 0, 0 );
			/*if ( !samp1 )
			{
				std::cout<<"Error loading sample\n";
				exit(1);
			}*/
		//	const int nChannel = FSOUND_PlaySound( FSOUND_FREE, samp1 );
			FSOUND_Sample_Free( samp1 );
		}
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool InitFMOD()
{
	if (FSOUND_GetVersion() < FMOD_VERSION)
	{
		printf("Error : You are using the wrong DLL version!  You should be using FMOD %.02f\n", FMOD_VERSION);
		return 1;
	}

	FSOUND_SetOutput(FSOUND_OUTPUT_DSOUND);
	FSOUND_SetDriver(0);					/* Select sound card (0 = default) */

	/*
	    INITIALIZE
	*/
	if (!FSOUND_Init(44100, 32, FSOUND_INIT_USEDEFAULTMIDISYNTH))
	{
		printf("Error!\n");
		printf("%s\n", FMOD_ErrorString(FSOUND_GetError()));
		return 1;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main( int argc, char* argv[] )
{
	if ( argc < 2 ) 
	{
		std::cout<<"need directory name to search in\n";
		return 1;
	}
	
	const std::string szDataDirectory = argv[1];
	std::cout<<"+++++++++++++++++++++++\n"<<"searching in "<<szDataDirectory<<"\n"<<"+++++++++++++++++++++++\n";

	if ( InitFMOD() ) return 1;

	// store last being read file
	NFile::CFile currentSound;
	if ( currentSound.Open( szCurrentBeingReading.c_str(), NFile::CFile::modeReadWrite ) )
	{
		// last being read - is bad
		if ( currentSound.GetLength() )
		{
			// bad sounds list file
			NFile::CFile badSounds;
			if ( !badSounds.Open( szBadSounds.c_str(), NFile::CFile::modeReadWrite) &&
						!badSounds.Open( szBadSounds.c_str(), NFile::CFile::modeReadWrite|NFile::CFile::modeCreate  ))
			{
				std::cout<<"cannot open file to store bad sounds \""<<szBadSounds<<"\"\n";
				return 1;
			}
			// read last bad sound filename
			std::vector<char> buffer;
			buffer.resize( currentSound.GetLength() );
			if ( buffer.size() != currentSound.Read( &buffer[0], buffer.size() ) )
			{
				std::cout<<"error reading file";
				return 1;
			}
			// write it 
			
			badSounds.Seek( 0, NFile::CFile::end );

			const int nSize = buffer.size();
			badSounds.Write( &nSize, sizeof( nSize ) );
			if ( buffer.size() != badSounds.Write( &buffer[0], nSize ) )
			{
				std::cout<<"error writing file";
				return 1;
			}
			badSounds.Flush();
			badSounds.Close();
		}
		currentSound.SetLength( 0 );
		currentSound.Close();
	}	
	

	// prepear bad sounds hash
	CBadFiles badSoundFileNames;
	NFile::CFile badSounds;
	if ( badSounds.Open( szBadSounds.c_str(), NFile::CFile::modeRead) )
	{
		int nSize;
		std::vector<char> buff;
		
		while( badSounds.GetLength() != badSounds.GetPosition() )
		{
			if ( sizeof(nSize) != badSounds.Read( &nSize, sizeof(nSize) ) )
			{
				break;
			}
			buff.resize( nSize + 1 ); // for trailing 0
			if ( nSize != badSounds.Read( &buff[0], nSize ) )
			{
				break;
			}
			buff[nSize] = 0;
			badSoundFileNames.insert( &buff[0] );
		}
		badSounds.Close();
	}

	CEnumFiles ef( &badSoundFileNames );
	NFile::EnumerateFiles( szDataDirectory.c_str(), "*.wav", ef, true );

	if ( badSounds.Open( szBadSounds.c_str(), NFile::CFile::modeWrite|NFile::CFile::modeCreate ) )
	{
		badSounds.SetLength( 0 );
		for ( CBadFiles::const_iterator it = badSoundFileNames.begin(); it != badSoundFileNames.end(); ++it )
		{
			badSounds.Write( it->c_str(), it->size() );
			badSounds.Write( "\n", 1 );
		}
		badSounds.Close();
	}

	NFile::CFile::Remove( szCurrentBeingReading.c_str() );
	return 0;
}

