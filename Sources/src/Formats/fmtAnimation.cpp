#include "StdAfx.h"

#include "../Anim/Animation.h"
#include "fmtAnimation.h"
bool SSpriteAnimationFormat::Load( const bool bPreLoad )
{
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
	CSaverAccessor saver = pSaver;
	DWORD dwSignature = 0;
	saver.Add( 127, &dwSignature );
	if ( dwSignature != 0 ) 
		return false;
	saver.Add( 1, this );
	return true;
}
int SSpriteAnimationFormat::SSpriteAnimation::SDir::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &frames );
	return 0;
}
int SSpriteAnimationFormat::SSpriteAnimation::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &rects );
	saver.Add( 2, &dirs );
	saver.Add( 3, &nFrameTime );
	saver.Add( 4, &fSpeed );
	saver.Add( 5, &bCycled );
	return 0;
}
int SSpriteAnimationFormat::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &animations );
	return 0;
}
