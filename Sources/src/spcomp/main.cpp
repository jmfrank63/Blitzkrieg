#include "StdAfx.h"

#include "..\Common\fmtAnimation.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline long Width( const RECT &rc ) { return rc.right - rc.left; }
inline long Height( const RECT &rc ) { return rc.bottom - rc.top; }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//��������������� ����������
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SDirDesc
{
	std::vector<short> frames;						// frame indices for this direction sequence
	CVec2 ptFrameShift;										// shift for all frames, which participates in this dir sequence
};
struct SAnimationDesc 
{
	std::string szName;										// animation name
	int nFirstSprite;
	int nLastSprite;
	std::vector<SDirDesc> dirs;						// direction descritions
	std::unordered_map<int, CVec2> frames;			// each frame unique shift
	int nFrameTime;												// general one frame show time
	CVec2 ptFrameShift;										// general one frame shift
	float fSpeed;													// translation speed (for animations with movement)
	bool bCycled;													// cycled or one-shot animation
};

/*const int nBoundValue = 1000000;
RECT AnalyzeSubrect( const CImageAccessor &image, const RECT &rect )
{
	int minx = nBoundValue, miny = nBoundValue, maxx = -nBoundValue, maxy = -nBoundValue;
	bool bNoYBefore = true;
	for ( int i=rect.top; i <= rect.bottom; ++i )
	{
		bool bEmptyLine = true;
		bool bNoXBefore = true;
		for ( int j=rect.left; j <= rect.right; ++j )
		{
			BYTE a = image[i][j].a;
			if ( image[i][j].a != 0 )
			{
				if ( bNoXBefore )
					minx = Min( minx, j ); 
				bNoXBefore = false;
				maxx = Max( maxx, j );
				bEmptyLine = false;
			}
		}
		if ( !bEmptyLine )
		{
			if ( bNoYBefore )
				miny = i;
			bNoYBefore = false;
			maxy = Max( maxy, i );
		}
	}
	RECT ret = { minx, miny, maxx, maxy };
	return ret;
}*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Serialize( CTreeAccessor *pFile, SDirDesc *pData )
{
	pFile->AddDataContainer( "seq", &pData->frames );
	pFile->AddObject( "shift", &pData->ptFrameShift );
}
void Serialize( CTreeAccessor *pFile, SAnimationDesc *pData )
{
	pFile->AddObject( "name", &pData->szName );
	pFile->AddContainer( "dirs", &pData->dirs );
	pFile->AddContainer( "frames", &pData->frames );
	pFile->AddData( "frame_time", &pData->nFrameTime );
	pFile->AddObject( "shift", &pData->ptFrameShift );
	pFile->AddData( "speed", &pData->fSpeed );
	pFile->AddData( "cycled", &pData->bCycled );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// spcomp.exe <sprite sequence dir> <output name>
int main( int argc, char *argv[] )
{
	if ( argc != 3 )
	{
		printf( "Sprite composing utility\n" );
		printf( "Written by Yuri V. Blazhevich\n" );
		printf( "(C) Nival Interactive, 2000\n" );
		printf( "\tUsage:  spcomp.exe <sprite sequence desc dir> <output name>\n" );
		return 0xDEAD;
	}
	//
	char buff[2048];
	std::string szSpriteDir = argv[1];
	if ( szSpriteDir[szSpriteDir.size() - 1] != '\\' )
		szSpriteDir += '\\';
	CPtr<IDataStorage> pStorage = OpenStorage( szSpriteDir.c_str(), STREAM_ACCESS_READ | STREAM_ACCESS_WRITE );
	//
	//
	//
	//
	//
	std::vector<IImage*> images;
	images.reserve( 1000 );
	IImageProcessor *pIP = GetImageProcessor();
	// read .ini file with animation description info
	CPtr<IDataBase> pDB = OpenDataBase( szSpriteDir.c_str(), TABLE_ACCESS_READ );
	CTableAccessor reg = pDB->OpenTable( "anims.txt", TABLE_ACCESS_READ );
	std::vector<std::string> szSections;
	reg.GetRowNames( szSections );
	std::vector<SAnimationDesc> animdescs( szSections.size() );
	std::vector<float> fvals;
	std::vector<short> nvals;
	for ( int i=0; i<szSections.size(); ++i )
	{
		// read animation general info
		SAnimationDesc &anim = animdescs[i];
		anim.szName = szSections[i];
		anim.nFrameTime = reg.GetInt( szSections[i].c_str(), "frametime", 125 );
		anim.fSpeed = reg.GetFloat( szSections[i].c_str(), "speed", 0 );
		anim.bCycled = reg.GetInt( szSections[i].c_str(), "cycled", true );
		int nNumDirs = reg.GetInt( szSections[i].c_str(), "dirs", 0 );
		NI_ASSERT_TF( nNumDirs > 0, NStr::Format("number of dirs must be >0 (in animation \"%s\")", szSections[i].c_str()), return 0xDEAD );
		anim.dirs.resize( nNumDirs );
		//
		reg.GetArray( szSections[i].c_str(), "shift", fvals );
		if ( fvals.size() == 2 )
			anim.ptFrameShift.Set( fvals[0], fvals[1] );
		else
			NI_ASSERT_TF( fvals.size() == 0, NStr::Format("wrong number of components in the general frame shift for animation \"%s\"", szSections[i].c_str()), return 0xDEAD );
		// read dirs info
		for ( int j=0; j<anim.dirs.size(); ++j )
		{
			SDirDesc &dir = anim.dirs[j];
			reg.GetArray( szSections[i].c_str(), NStr::Format("dir%d seq", j), dir.frames );
			NI_ASSERT_TF( dir.frames.size() > 0, NStr::Format("number of frames must be >0 (for dir %d of animation \"%s\")", j, szSections[i].c_str()), return 0xDEAD );
			reg.GetArray( szSections[i].c_str(), NStr::Format("dir%d shift", j), fvals );
			if ( fvals.size() == 2 )
				dir.ptFrameShift.Set( fvals[0], fvals[1] );
			else
				dir.ptFrameShift = anim.ptFrameShift;
			// read frames info
			for ( int k=0; k<dir.frames.size(); ++k )
			{
				reg.GetArray( szSections[i].c_str(), NStr::Format("frame%d shift", dir.frames[k]), fvals );
				if ( fvals.size() == 2 )
					anim.frames[ dir.frames[k] ].Set( fvals[0], fvals[1] );
				else
					anim.frames[ dir.frames[k] ] = dir.ptFrameShift;
			}
		}
	}
	//
	int nFirstFrame = 0, nLastFrame = 0;
	SSpriteAnimationFormat animations;
	for ( int i=0; i<animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		SSpriteAnimationFormat::SSpriteAnimation *pAnimation = &( animations.animations[animdesc.szName] );
		pAnimation->fSpeed = animdesc.fSpeed;
		pAnimation->bCycled = animdesc.bCycled;
		pAnimation->dirs.resize( animdesc.dirs.size() );
		pAnimation->nFrameTime = animdesc.nFrameTime;
		for ( int j=0; j<pAnimation->dirs.size(); ++j )
			pAnimation->dirs[j].frames = animdesc.dirs[j].frames;
		// CRAP{ ������ ������������ ��� � �������� ������������� ��� ����� 
		pAnimation->rects.resize( animdesc.frames.size() );
		// CRAP}
		for ( int j=0; j<pAnimation->rects.size(); ++j )
		{
			if ( animdesc.szName == "default" )
				strcpy( buff, "1.tga" );
			else
				sprintf( buff, "%s\\%.3d.tga", animdesc.szName.c_str(), j );
			CPtr<IDataStream> pStream = pStorage->OpenStream( buff, STREAM_ACCESS_READ );
			NI_ASSERT_TF( pStream != 0, NStr::Format("Can't open image \"%s\"", buff), break );
			IImage *pImage = pIP->LoadImage( pStream );
			NI_ASSERT_TF( pImage != 0, NStr::Format("Can't load image from the stream \"%s\"", buff), return 0xDEAD );
			pImage->AddRef();
			images.push_back( pImage );
			++nLastFrame;
		}
		animdesc.nFirstSprite = nFirstFrame;
		animdesc.nLastSprite = nLastFrame;
		nFirstFrame = nLastFrame;
	}
	//
	std::vector<RECT> rects		 ( images.size() );
	std::vector<RECT> rectsMain( images.size() );
	CPtr<IImage> pImage = pIP->ComposeImages( &(images[0]), &(rects[0]), &(rectsMain[0]), images.size() );

	// write tga image
	std::string szFileName = argv[2];
	szFileName += ".tga";
	CPtr<IDataStream> pStream = pStorage->CreateStream( szFileName.c_str(), STREAM_ACCESS_WRITE );
	pIP->SaveImageAsTGA( pStream, pImage );
	pStream = 0;
	//
	CImageAccessor image = pImage;
	//
	// 
	//
	// compose prite animation data and write it
	for ( int i=0; i<animdescs.size(); ++i )
	{
		SAnimationDesc &animdesc = animdescs[i];
		SSpriteAnimationFormat::SSpriteAnimation *pAnimation = &( animations.animations[animdesc.szName] );
		NI_ASSERT_T( pAnimation->rects.size() == animdesc.nLastSprite - animdesc.nFirstSprite, NStr::Format("Not enought sprites for animation \"%s\"", animdesc.szName.c_str()) );
		for ( int j=0; j<pAnimation->rects.size(); ++j )
		{
			const RECT &rcSubRect = rects[ animdesc.nFirstSprite + j ];
			pAnimation->rects[j].maps.x1 = ( float( rcSubRect.left   ) + 0.5f ) / float( pImage->GetSizeX() );
			pAnimation->rects[j].maps.x2 = ( float( rcSubRect.right  ) + 0.5f ) / float( pImage->GetSizeX() );
			pAnimation->rects[j].maps.y1 = ( float( rcSubRect.top    ) + 0.5f ) / float( pImage->GetSizeY() );
			pAnimation->rects[j].maps.y2 = ( float( rcSubRect.bottom ) + 0.5f ) / float( pImage->GetSizeY() );
			RECT &rcBase = rectsMain[ animdesc.nFirstSprite + j  ];
			pAnimation->rects[j].rect.Set( rcSubRect.left - rcBase.left - animdesc.frames[j].x,
				                             rcSubRect.top - rcBase.top - animdesc.frames[j].y, 
				                             rcSubRect.right - rcBase.left - animdesc.frames[j].x, 
																		 rcSubRect.bottom - rcBase.top - animdesc.frames[j].y );
		}
	}
	// sprite animation data
	szFileName = argv[2];
	szFileName += ".san";
	pStream = pStorage->CreateStream( szFileName.c_str(), STREAM_ACCESS_WRITE );
	NI_ASSERT_TF( pStream != 0, NStr::Format("Can't write sprite animation data to the stream \"%s\"", szFileName.c_str()), return 0xDEAD );
	{
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::WRITE );
		CSaverAccessor saver = pSaver;
		saver.AddObject( 1, &animations );
	}
	pStream = 0;
	//
	for ( int i=0; i<images.size(); ++i )
		images[i]->Release();
	//
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
