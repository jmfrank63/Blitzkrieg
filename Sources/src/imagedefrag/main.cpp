#include "StdAfx.h"
#include "..\\Formats\\FmtSprite.h"
#include "..\\Formats\\FmtTerrain.h"
#include "..\\Image\\Image.h"
#include "..\\RandomMapGen\\IB_Types.h"
#include "..\\RandomMapGen\\MapInfo_Types.h"
#include "..\\RandomMapGen\\RMG_Types.h"
#include "..\\Misc\\FileUtils.h"


typedef IImage* PIImage;
bool LoadImageLibrary()
{
  char buffer[2048];
  ::GetCurrentDirectory( 2048, buffer );
  std::string szCurrentFolder = buffer;
  if ( szCurrentFolder.empty() )
  {
    szCurrentFolder = ".\\";
  }
  else if ( szCurrentFolder[szCurrentFolder.size() - 1] != '\\' )
  {
    szCurrentFolder += '\\';
  }
  HMODULE hImage = ::LoadLibrary( (szCurrentFolder + "image.dll").c_str() );
  if ( hImage )
  {
    GETMODULEDESCRIPTOR pfnGetModuleDescriptor = reinterpret_cast<GETMODULEDESCRIPTOR>( ::GetProcAddress( hImage, "GetModuleDescriptor" ) );
    if ( pfnGetModuleDescriptor )
    {
      const SModuleDescriptor *pDesc = (*pfnGetModuleDescriptor)();
      if ( pDesc && pDesc->pFactory )
      {
        IImageProcessor *pIP = CreateObject<IImageProcessor>( pDesc->pFactory, IImageProcessor::tidTypeID );
        if ( pIP )
        {
          RegisterSingleton( IImageProcessor::tidTypeID, pIP );
          return true;
        }
      }
    }
  }
  return false;
}

int CalculateWeight( const std::string &originalImage, DWORD dwMinAlpha )
{
  CPtr<IDataStream> pOriginalImageStream = OpenFileStream( originalImage.c_str(), STREAM_ACCESS_READ );
  NI_ASSERT_TF( pOriginalImageStream != 0, NStr::Format("Can't open image file \"%s\"", originalImage.c_str()), return 0xDEAD );

  IImageProcessor *pIP = GetImageProcessor();
  CPtr<IImage> pOriginalImage = pIP->LoadImage( pOriginalImageStream );
  CImageAccessor imageAccessor = pOriginalImage;

	QWORD red = 0;
	QWORD blue = 0;
	QWORD green = 0;
	QWORD count = 0;
	for ( int nYindex = 0; nYindex < pOriginalImage->GetSizeY(); ++nYindex )
	{
    for ( int nXindex = 0; nXindex < pOriginalImage->GetSizeX(); ++nXindex )
		{
      SColor &rColor = imageAccessor[nYindex][nXindex];
			if ( rColor.a >= dwMinAlpha )
			{
				red += rColor.r;
				blue += rColor.g;
				green += rColor.b;
				++count;
			}
    }
  }
	red /= count;
	blue /= count;
	green /= count;

  return 0;
}

int DrawLines( const std::string &originalImage, const std::string &outputImage, DWORD dwMinAlpha )
{
  CPtr<IDataStream> pOriginalImageStream = OpenFileStream( originalImage.c_str(), STREAM_ACCESS_READ );
  NI_ASSERT_TF( pOriginalImageStream != 0, NStr::Format("Can't open image file \"%s\"", originalImage.c_str()), return 0xDEAD );

  IImageProcessor *pIP = GetImageProcessor();
  CPtr<IImage> pOriginalImage = pIP->LoadImage( pOriginalImageStream );
  CImageAccessor imageAccessor = pOriginalImage;

	for ( int lineIndex = fCellSizeX; lineIndex < pOriginalImage->GetSizeX(); lineIndex += fCellSizeX * 2 )
	{
		for ( int nYindex = 0; nYindex < pOriginalImage->GetSizeY(); ++nYindex )
		{
			int nXindex = lineIndex + ( nYindex * 2 );
			if ( ( nXindex >= 0 ) && ( nXindex < pOriginalImage->GetSizeX() ) )
			{
				SColor &rColor = imageAccessor[nYindex][nXindex];
				rColor.r = 0xFF;
				rColor.g = 0xFF;
				rColor.b = 0xFF;
				rColor.a = 0xFF;
			}
			nXindex = lineIndex - ( nYindex * 2 );
			if ( ( nXindex >= 0 ) && ( nXindex < pOriginalImage->GetSizeX() ) )
			{
				SColor &rColor = imageAccessor[nYindex][nXindex];
				rColor.r = 0xFF;
				rColor.g = 0xFF;
				rColor.b = 0xFF;
				rColor.a = 0xFF;
			}
		}
	}

	for ( int lineIndex = fCellSizeY; lineIndex < pOriginalImage->GetSizeY(); lineIndex += fCellSizeY * 2 )
	{
		for ( int nYindex = lineIndex; nYindex < pOriginalImage->GetSizeY(); ++nYindex )
		{
			int nXindex = ( nYindex - lineIndex )  * 2;
			if ( ( nXindex >= 0 ) && ( nXindex < pOriginalImage->GetSizeX() ) )
			{
				SColor &rColor = imageAccessor[nYindex][nXindex];
				rColor.r = 0xFF;
				rColor.g = 0xFF;
				rColor.b = 0xFF;
				rColor.a = 0xFF;
			}
			nXindex =  pOriginalImage->GetSizeX() - ( nYindex - lineIndex ) * 2 - 1;
			if ( ( nXindex >= 0 ) && ( nXindex < pOriginalImage->GetSizeX() ) )
			{
				SColor &rColor = imageAccessor[nYindex][nXindex];
				rColor.r = 0xFF;
				rColor.g = 0xFF;
				rColor.b = 0xFF;
				rColor.a = 0xFF;
			}
		}
	}

	for ( int lineIndex = fCellSizeX; lineIndex < pOriginalImage->GetSizeX(); lineIndex += fCellSizeX * 2 )
	{
		for ( int nYindex = 0; nYindex < pOriginalImage->GetSizeY(); ++nYindex )
		{
			int nXindex = lineIndex + ( nYindex * 2 );
			if ( ( nXindex >= 0 ) && ( nXindex < pOriginalImage->GetSizeX() ) )
			{
				SColor &rColor = imageAccessor[nYindex][nXindex];
				rColor.r = 0xFF;
				rColor.g = 0xFF;
				rColor.b = 0xFF;
				rColor.a = 0xFF;
			}
			nXindex = lineIndex - ( nYindex * 2 );
			if ( ( nXindex >= 0 ) && ( nXindex < pOriginalImage->GetSizeX() ) )
			{
				SColor &rColor = imageAccessor[nYindex][nXindex];
				rColor.r = 0xFF;
				rColor.g = 0xFF;
				rColor.b = 0xFF;
				rColor.a = 0xFF;
			}
		}
	}

  CPtr<IDataStream> pOutputImageStream = CreateFileStream( outputImage.c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pOutputImageStream != 0, NStr::Format("Can't open image file \"%s\" to write", outputImage.c_str()), return 0xDEAD );
  pIP->SaveImageAsTGA( pOutputImageStream, pOriginalImage );
	return 0;
}

int Pack( const std::string &originalImage, const std::string &packImage, DWORD alpha )
{
  CPtr<IDataStream> pOriginalImageStream = OpenFileStream( ( originalImage + ".tga" ).c_str(), STREAM_ACCESS_READ );
  NI_ASSERT_TF( pOriginalImageStream != 0, NStr::Format("Can't open image file \"%s\"", originalImage.c_str()), return 0xDEAD );

  CPtr<IDataStream> pPackImageStream = CreateFileStream( ( packImage + ".tga" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pPackImageStream != 0, NStr::Format("Can't open image file \"%s\" to write", packImage.c_str()), return 0xDEAD );
  
  std::string packStructure = packImage;
  CPtr<IDataStream> pPackStructureStream = CreateFileStream( ( packStructure + ".sp" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pPackStructureStream != 0, NStr::Format("Can't open structure file \"%s\" to write", packStructure.c_str()), return 0xDEAD );

  IImageProcessor *pIP = GetImageProcessor();
  CPtr<IImage> pOriginalImage = pIP->LoadImage( pOriginalImageStream );
  CTPoint<int> originalImageLeftTop( 0, 0 );

	CSpritesPackBuilder::SPackParameter packParameter;
	packParameter.pImage = pOriginalImage;
	packParameter.center.x = 252;
	packParameter.center.y = 288;
	/**
	packParameter.lockedTiles.SetSizes( 8, 6 );
	packParameter.lockedTiles.Set( RMGC_UNLOCKED );
	
	packParameter.lockedTiles[0][0] = RMGC_LOCKED;
	packParameter.lockedTiles[1][0] = RMGC_LOCKED;
	packParameter.lockedTiles[2][0] = RMGC_LOCKED;
	packParameter.lockedTiles[0][1] = RMGC_LOCKED;
	packParameter.lockedTiles[1][1] = RMGC_LOCKED;
	packParameter.lockedTiles[2][1] = RMGC_LOCKED;
	
	packParameter.lockedTiles[0][3] = RMGC_LOCKED;
	packParameter.lockedTiles[1][3] = RMGC_LOCKED;
	packParameter.lockedTiles[2][3] = RMGC_LOCKED;

	packParameter.lockedTiles[0][4] = RMGC_LOCKED;
	packParameter.lockedTiles[1][4] = RMGC_LOCKED;
	packParameter.lockedTiles[2][4] = RMGC_LOCKED;
	packParameter.lockedTiles[3][4] = RMGC_LOCKED;
	packParameter.lockedTiles[4][4] = RMGC_LOCKED;
	packParameter.lockedTiles[5][4] = RMGC_LOCKED;
	packParameter.lockedTiles[0][5] = RMGC_LOCKED;
	packParameter.lockedTiles[1][5] = RMGC_LOCKED;
	packParameter.lockedTiles[2][5] = RMGC_LOCKED;
	packParameter.lockedTiles[3][5] = RMGC_LOCKED;
	packParameter.lockedTiles[4][5] = RMGC_LOCKED;
	packParameter.lockedTiles[5][5] = RMGC_LOCKED;

	packParameter.lockedTiles[1][6] = RMGC_LOCKED;
	packParameter.lockedTiles[2][6] = RMGC_LOCKED;
	packParameter.lockedTiles[3][6] = RMGC_LOCKED;
	packParameter.lockedTiles[4][6] = RMGC_LOCKED;
	packParameter.lockedTiles[5][6] = RMGC_LOCKED;

	packParameter.lockedTiles[2][7] = RMGC_LOCKED;
	packParameter.lockedTiles[3][7] = RMGC_LOCKED;
	packParameter.lockedTiles[4][7] = RMGC_LOCKED;
	packParameter.lockedTiles[5][7] = RMGC_LOCKED;

	packParameter.lockedTilesCenter.x = -112;
	packParameter.lockedTilesCenter.y = -40;
	/**/
	packParameter.dwMinAlpha = 1;

	SSpritesPack spritesPack;
	DWORD dwSignature = SSpritesPack::SIGNATURE;

  CPtr<IImage> pPackedImage = CSpritesPackBuilder::Pack( &spritesPack, packParameter, 256, 5 );
  if( pPackedImage == 0 )
  {
    printf( "Error. SImagePack::CreateImagePack() == 0\n" );
    return 0xDEAD;
  }
	pIP->SaveImageAsTGA( pPackImageStream, pPackedImage );

	CPtr<IStructureSaver> pSS = CreateStructureSaver( pPackStructureStream, IStructureSaver::WRITE );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &spritesPack );
	saver.Add( 127, &dwSignature );
  return 0;
}

int Unpack( const std::string &packImage, const std::string &originalImage, DWORD alpha )
{
  CPtr<IDataStream> pPackImageStream = OpenFileStream( ( packImage + ".tga" ).c_str(), STREAM_ACCESS_READ );
  NI_ASSERT_TF( pPackImageStream != 0, NStr::Format("Can't open image file \"%s\"", packImage.c_str()), return 0xDEAD );

  std::string packStructure = packImage;
  CPtr<IDataStream> pPackStructureStream = OpenFileStream( ( packStructure + ".sp" ).c_str(), STREAM_ACCESS_READ );
  NI_ASSERT_TF( pPackStructureStream != 0, NStr::Format("Can't open structure file \"%s\"", packStructure.c_str()), return 0xDEAD );

  CPtr<IDataStream> pOriginalImageEdgeStream = CreateFileStream( ( originalImage + "_edge.tga" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pOriginalImageEdgeStream != 0, NStr::Format("Can't open image file \"%s\" to write", originalImage.c_str()), return 0xDEAD );

  CPtr<IDataStream> pOriginalImageBoundsStream = CreateFileStream( ( originalImage + "_bounds.tga" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pOriginalImageBoundsStream != 0, NStr::Format("Can't open image file \"%s\" to write", originalImage.c_str()), return 0xDEAD );

  CPtr<IDataStream> pOriginalImageInEdgeStream = CreateFileStream( ( originalImage + "_inedge.tga" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pOriginalImageInEdgeStream != 0, NStr::Format("Can't open image file \"%s\" to write", originalImage.c_str()), return 0xDEAD );

  CPtr<IDataStream> pOriginalImageDepthStream = CreateFileStream( ( originalImage + "_depth.tga" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pOriginalImageDepthStream != 0, NStr::Format("Can't open image file \"%s\" to write", originalImage.c_str()), return 0xDEAD );

  CPtr<IDataStream> pOriginalImageAlphaStream = CreateFileStream( ( originalImage + "_alpha.tga" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pOriginalImageAlphaStream != 0, NStr::Format("Can't open image file \"%s\" to write", originalImage.c_str()), return 0xDEAD );

	CPtr<IDataStream> pOriginalImageStream = CreateFileStream( ( originalImage + ".tga" ).c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pOriginalImageStream != 0, NStr::Format("Can't open image file \"%s\" to write", originalImage.c_str()), return 0xDEAD );

	SSpritesPack spritesPack;
	DWORD dwSignature = 0;
  CPtr<IStructureSaver> pSS = CreateStructureSaver( pPackStructureStream, IStructureSaver::READ );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &spritesPack );
	saver.Add( 127, &dwSignature );
	if ( dwSignature == SSpritesPack::SIGNATURE )
	{
		
		IImageProcessor *pIP = GetImageProcessor();
		CPtr<IImage> pPackImage = pIP->LoadImage( pPackImageStream );

		CPtr<IImage> pOriginalImageEdge = CSpritesPackBuilder::UnpackAndMarkEdge( &spritesPack, pPackImage, 0 );
		if( pOriginalImageEdge == 0 )
		{
			printf( "Error. SImagePack::UnpackImage() == 0\n" );
			return 0xDEAD;
		}
		pIP->SaveImageAsTGA( pOriginalImageEdgeStream, pOriginalImageEdge );

		CPtr<IImage> pOriginalImageBounds = CSpritesPackBuilder::UnpackAndMarkBounds( &spritesPack, pPackImage, 0 );
		if( pOriginalImageBounds == 0 )
		{
			printf( "Error. SImagePack::UnpackImage() == 0\n" );
			return 0xDEAD;
		}
		pIP->SaveImageAsTGA( pOriginalImageBoundsStream, pOriginalImageBounds );

		CPtr<IImage> pOriginalImageInEdge = CSpritesPackBuilder::UnpackAndMarkInEdge( &spritesPack, pPackImage, 0 );
		if( pOriginalImageInEdge == 0 )
		{
			printf( "Error. SImagePack::UnpackImage() == 0\n" );
			return 0xDEAD;
		}
		pIP->SaveImageAsTGA( pOriginalImageInEdgeStream, pOriginalImageInEdge );

		CPtr<IImage> pOriginalImageDepth = CSpritesPackBuilder::UnpackAndMarkDepth( &spritesPack, pPackImage, 0 );
		if( pOriginalImageDepth == 0 )
		{
			printf( "Error. SImagePack::UnpackImage() == 0\n" );
			return 0xDEAD;
		}
		pIP->SaveImageAsTGA( pOriginalImageDepthStream, pOriginalImageDepth );


		CPtr<IImage> pOriginalImageAlpha = CSpritesPackBuilder::UnpackAndMarkAlpha( &spritesPack, pPackImage, 0, 1, 255 );
		if( pOriginalImageAlpha == 0 )
		{
			printf( "Error. SImagePack::UnpackImage() == 0\n" );
			return 0xDEAD;
		}
		pIP->SaveImageAsTGA( pOriginalImageAlphaStream, pOriginalImageAlpha );

		CTRect<int> bounds( 0, 0, 0, 0 );
		CPtr<IImage> pOriginalImage = CSpritesPackBuilder::Unpack( &spritesPack, pPackImage, 0, &bounds );
		if( pOriginalImage == 0 )
		{
			printf( "Error. SImagePack::UnpackImage() == 0\n" );
			return 0xDEAD;
		}

		{
			CImageAccessor originalImageAccessor = pOriginalImage;
			for ( int nIndex = -10; nIndex <= 10; ++nIndex )
			{
				{
					int nXPos = nIndex - bounds.minx;
					int nYPos = -bounds.miny;
					if ( ( nXPos >= 0 ) &&
							 ( nYPos >= 0 ) &&
							 ( nXPos < pOriginalImage->GetSizeX() ) &&
							 ( nYPos < pOriginalImage->GetSizeY() ) )
					{
						SColor &rColor = originalImageAccessor[nYPos][nXPos];
						rColor.r = 255;
						rColor.g = 0;
						rColor.b = 0;
					}
				}
				{
					int nXPos = -bounds.minx;
					int nYPos = nIndex - bounds.miny;
					if ( ( nXPos >= 0 ) &&
							 ( nYPos >= 0 ) &&
							 ( nXPos < pOriginalImage->GetSizeX() ) &&
							 ( nYPos < pOriginalImage->GetSizeY() ) )
					{
						SColor &rColor = originalImageAccessor[nYPos][nXPos];
						rColor.r = 255;
						rColor.g = 0;
						rColor.b = 0;
					}
				}
			}
		}
		pIP->SaveImageAsTGA( pOriginalImageStream, pOriginalImage );
	}
  return 0;
}

int MultyPack( const std::string &originalFolder, const std::string &packFolder, const std::string &originalFile, DWORD alpha )
{
/**
	std::string packImage = packFolder + "\\" + originalFile;
  CPtr<IDataStream> pPackImageStream = CreateFileStream( packImage.c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pPackImageStream != 0, NStr::Format("Can't open image file \"%s\" to write", packImage.c_str()), return 0xDEAD );
  
  std::string packStructure = packImage + ".pack";
  CPtr<IDataStream> pPackStructureStream = CreateFileStream( packStructure.c_str(), STREAM_ACCESS_WRITE );
  NI_ASSERT_TF( pPackStructureStream != 0, NStr::Format("Can't open structure file \"%s\" to write", packStructure.c_str()), return 0xDEAD );

	IImageProcessor *pIP = GetImageProcessor();

	CPackedImageInfo::TPackParameters packParameters;

  std::string findFilePattern = originalFolder + "\\*.tga";
	for ( NFile::CFileIterator fileIterator( findFilePattern.c_str() ); !fileIterator.IsEnd(); ++fileIterator )
	{
		if ( !fileIterator.IsDirectory() && !fileIterator.IsDots() )
		{
      CPtr<IDataStream> pOriginalImageStream = OpenFileStream( ( fileIterator.GetFilePath() ).c_str(), STREAM_ACCESS_READ );
      NI_ASSERT_TF( pOriginalImageStream != 0, NStr::Format("Can't open image file \"%s\"", ( fileIterator.GetFilePath() ).c_str() ), return 0xDEAD );

			packParameters.push_back( CPackedImageInfo::SPackParameter() );
			packParameters.back().pImage = pIP->LoadImage( pOriginalImageStream );
			packParameters.back().leftTop = CTPoint<int>( 100, 100 );
			packParameters.back().dwMinAlpha = 1;
		}
	}

  if ( packParameters.empty() )
  {
    printf( "<%s> subfolders not found.\n", originalFolder);
    return -1;
  }

  CPackedImageInfo imagePack;
  CPtr<IImage> pPackedImage = imagePack.Pack( packParameters, 32, 2 );
  if( pPackedImage == 0 )
  {
    printf( "Error. SImagePack::CreateImagePack() == 0\n" );
    return 0xDEAD;
  }
	pIP->SaveImageAsTGA( pPackImageStream, pPackedImage );

	CPtr<IStructureSaver> pSS = CreateStructureSaver( pPackStructureStream, IStructureSaver::WRITE );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &imagePack );

  printf( "Pack: %d files (%s)\n",  packParameters.size(), originalFile );
/**/
  return 0;
}

int MultyUnpack( const std::string &packFolder, const std::string &originalFolder, const std::string &packFile, DWORD alpha )
{
/**
  std::string  packImage = packFolder + "\\" + packFile;
  CPtr<IDataStream> pPackImageStream = OpenFileStream( packImage.c_str(), STREAM_ACCESS_READ );
  NI_ASSERT_TF( pPackImageStream != 0, NStr::Format("Can't open image file \"%s\"", packImage.c_str()), return 0xDEAD );

  std::string packStructure = packImage + ".pack";
  CPtr<IDataStream> pPackStructureStream = OpenFileStream( packStructure.c_str(), STREAM_ACCESS_READ );
  NI_ASSERT_TF( pPackStructureStream != 0, NStr::Format("Can't open structure file \"%s\"", packStructure.c_str()), return 0xDEAD );

  CPackedImageInfo imagePack;
  CPtr<IStructureSaver> pSS = CreateStructureSaver( pPackStructureStream, IStructureSaver::READ );
	CSaverAccessor saver = pSS;
	saver.Add( 1, &imagePack );

  IImageProcessor *pIP = GetImageProcessor();
  CPtr<IImage> pPackImage = pIP->LoadImage( pPackImageStream );
  for( int index = 0; index < imagePack.packedImages.size(); ++index )
  {
    char number[0xF];
    sprintf(number, "%03d", index);
    std::string originalImage = originalFolder + "\\" + number + "_"+ packFile;

    CPtr<IDataStream> pOriginalImageStream = CreateFileStream( originalImage.c_str(), STREAM_ACCESS_WRITE );
    NI_ASSERT_TF( pOriginalImageStream != 0, NStr::Format("Can't open image file \"%s\" to write", originalImage.c_str()), return 0xDEAD );

    CPtr<IImage> pOriginalImage = imagePack.Unpack( pPackImage, index );
    if( pOriginalImage == 0 )
    {
      printf( "Error. SImagePack::UnpackImage() == 0\n" );
      return 0xDEAD;
    }
	  if ( ( pOriginalImage->GetSizeX() != 0 ) && ( pOriginalImage->GetSizeY() != 0 ) )
		{
			pIP->SaveImageAsTGA( pOriginalImageStream, pOriginalImage );
		}
  }
  printf( "Unpack: %d files (%s)\n",  imagePack.packedImages.size(), packFile );
/**/
  return 0;
}

int main( int argc, char* argv[] )
{
	if ( argc < 4 )
  {
		printf( "MiniMap Creator 1.0\n" );
		printf( "Written by Michael Melnikov\n" );
		printf( "(C) Nival Interactive, 2001\n" );
		printf( "Usage: CreateMiniMaps ImagePacker.exe <image size> <folder name> <file pattern>\n" );
    return -1;
  }

	if( !LoadImageLibrary() )
  {
    printf( "Error. LoadImageLibrary() == 0\n" );
    return 0xDEAD;
  }
	
	CPtr<IDataStorage> pStorage = OpenStorage( "C:\\a7\\data\\*.pak", STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
	RegisterSingleton( IDataStorage::tidTypeID, pStorage );

	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( "objects.xml", STREAM_ACCESS_READ );
	CPtr<IObjectsDB> pODB = CreateObjectsDB();
	pODB->LoadDB();
	RegisterSingleton( IObjectsDB::tidTypeID, pODB );
  
	int nImageSize;
	sscanf(argv[1], "%d", &nImageSize );
  std::string findFileFolder = argv[2];
	std::string findFilePattern = argv[3];

	std::vector<std::string> allFiles;
	{
		NFile::CGetAllFiles allFilesFunctional( &allFiles );
		NFile::EnumerateFiles( findFileFolder.c_str(), findFilePattern.c_str(), allFilesFunctional, true );
	}
	for ( int nFileIndex = 0; nFileIndex < allFiles.size(); ++nFileIndex )
	{
	printf( "File: %s... ", allFiles[nFileIndex].c_str() );
		std::string szMapNameExt = allFiles[nFileIndex].substr( allFiles[nFileIndex].rfind( '.' ),  allFiles[nFileIndex].size() );
		std::string szMiniMapName = allFiles[nFileIndex].substr( 0,  allFiles[nFileIndex].rfind( '.' ) );
		NStr::ToLower( szMapNameExt );
		CMapInfo mapInfo;
		if ( 	szMapNameExt.compare( ".bzm" ) == 0 )
		{
			CPtr<IDataStream> pStreamBZM = OpenFileStream( ( szMiniMapName + ".bzm" ).c_str(), STREAM_ACCESS_READ );
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBZM, IStructureSaver::READ );
			CSaverAccessor saver = pSaver;
			saver.Add( 1, &mapInfo );
		}
		else if ( szMapNameExt.compare( ".xml" ) == 0 )
		{
			CPtr<IDataStream> pStreamXML = OpenFileStream( ( szMiniMapName + ".xml" ).c_str(), STREAM_ACCESS_READ );
			CTreeAccessor saver = CreateDataTreeSaver( pStreamXML, IDataTree::READ );
			saver.AddTypedSuper( &mapInfo );
		}
		else
		{
			printf( "Unknown format (%s)!\n", szMapNameExt.c_str() );
			continue;
		}
		CRMImageCreateParameterList imageCreateParameterList;
		imageCreateParameterList.push_back( SRMImageCreateParameter( szMiniMapName, CTPoint<int>( nImageSize, nImageSize ), true ) );
		bool bResult = mapInfo.CreateMiniMapImage( imageCreateParameterList );
		printf( "Done %d.\n", bResult );
  }
  printf( "Total: %d files.\n", allFiles.size() );
	return 0;
/*
	if ( ( argc < 3 ) ||
       ( ( argv[1][0] != 'l' ) &&
         ( argv[1][0] != 'L' ) &&
         ( argv[1][0] != 'm' ) &&
         ( argv[1][0] != 'M' ) &&
         ( argv[1][0] != 'w' ) &&
         ( argv[1][0] != 'W' ) &&
         ( argv[1][0] != 'p' ) &&
         ( argv[1][0] != 'P' ) &&
         ( argv[1][0] != 'u' ) &&
         ( argv[1][0] != 'U' ) ) )
  {
    printf( "Image Packer 1.1\n" );
    printf( "Written by Michael Melnikov\n" );
    printf( "(C) Nival Interactive, 2001\n" );
    printf( "Usage: ImagePacker.exe <mp|mu|p|u|e|t|w|l|r>XXX <input folder name> <output folder name> <file name>\n" );
    printf( "mp - multy pack image\n");
    printf( "mu - multy unpack image\n");
    printf( "p - pack image\n");
    printf( "u - unpack image\n");
    printf( "w - calculate picture color weight, in this case <input folder name> = <input folder image>\n" );
    printf( "l - draw lines\n" );
    printf( "in all cases XXX - minimal alpha\n" );
    return -1;
  }
  
	if( !LoadImageLibrary() )
  {
    printf( "Error. LoadImageLibrary() == 0\n" );
    return 0xDEAD;
  }

	std::string szFileName;
	std::string szInputFolder;
	std::string szInputImage;

	std::string szOutputFolder;
	std::string szOutputImage;

	if( ( argv[1][0] == 'w' ) ||
      ( argv[1][0] == 'W' ) )
  {
		szInputImage = argv[2];
	}
	else 
	{
		szFileName = argv[4];
		szInputFolder = argv[2];
		szInputImage = szInputFolder + "\\" + szFileName;
		
		szOutputFolder = argv[3];
		szOutputImage = szOutputFolder + "\\" + szFileName;
	}

  DWORD nOverallTimeCount = ::GetTickCount();

  int nStatus = 0;
  if( ( argv[1][0] == 'm' ) ||
      ( argv[1][0] == 'M' ) )
  {
    char charPrefix0 = 0x00;
    char charPrefix1 = 0x00;
    DWORD alpha = 1;
    sscanf(argv[1], "%c%c%d", &charPrefix0, &charPrefix1, &alpha);
    if( ( argv[1][1] == 'p' ) ||
        ( argv[1][1] == 'P' ) )
    {
      nStatus = MultyPack( szInputFolder, szOutputFolder, szFileName, alpha );
    }
    else
    {
      nStatus = MultyUnpack( szInputFolder, szOutputFolder, szFileName, alpha );
    }
  }
  else
  {
    char charPrefix = 0x00;
    DWORD alpha = 1;
    sscanf(argv[1], "%c%d", &charPrefix, &alpha);
		if( ( argv[1][0] == 'l' ) ||
        ( argv[1][0] == 'L' ) )
		{
      nStatus = DrawLines( szInputImage, szOutputImage, alpha );
		}
		else if( ( argv[1][0] == 'w' ) ||
             ( argv[1][0] == 'W' ) )
    {
      nStatus = CalculateWeight( szInputImage, alpha );
    }
    else if( ( argv[1][0] == 'p' ) ||
             ( argv[1][0] == 'P' ) )
    {
      nStatus = Pack( szInputImage, szOutputImage, alpha );
    }
    else
    {
      nStatus = Unpack( szInputImage, szOutputImage, alpha );
    }
  }
  nOverallTimeCount = ::GetTickCount() - nOverallTimeCount;
  printf( "Completed in %d ms\n", nOverallTimeCount );
  return nStatus;
/**/
}
/**

	int count = 5;
	SWeightVector<std::string> items;
	std::vector<int> indices;

	items.push_back( "string", 0 );
	items.push_back( "string", 0 );
	items.push_back( "string", 0 );
	items.push_back( "string", 0 );
	items.push_back( "string", 0 );

	for ( int index = 0; index < count; ++index )
	{
		indices.push_back( 0 );
	}
	{
		for ( int index = 0; index < count; ++index )
		{
			indices[index] = 0;
		}

		DWORD nOverallTimeCount = ::GetTickCount();
		for ( int index = 0; index < ( count * 100 ); ++index )
		{
			int randomIndex = items.GetRandomIndex( false );
			++( indices[randomIndex] );
		}
		nOverallTimeCount = ::GetTickCount() - nOverallTimeCount;
		NStr::DebugTrace( "Completed in %d ms\n", nOverallTimeCount );

		for ( int index = 0; index < count; ++index )
		{
			NStr::DebugTrace( "count[%d] %d\n", index, indices[index] );
		}
	}	
	
	{
		for ( int index = 0; index < count; ++index )
		{
			indices[index] = 0;
		}

		DWORD nOverallTimeCount = ::GetTickCount();
		for ( int index = 0; index < ( count * 100 ); ++index )
		{
			int randomIndex = items.GetRandomIndex( true );
			++( indices[randomIndex] );
		}
		nOverallTimeCount = ::GetTickCount() - nOverallTimeCount;
		NStr::DebugTrace( "Completed in %d ms\n", nOverallTimeCount );

	
		for ( int index = 0; index < count; ++index )
		{
			NStr::DebugTrace( "count[%d] %d\n", index, indices[index] );
		}
	}
/**/