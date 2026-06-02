#include "StdAfx.h"

#include "fmtSprite.h"
bool SSpritesPack::Load( const bool bPreLoad )
{
	const std::string szStreamName = GetSharedResourceFullName();
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( szStreamName.c_str(), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
	CSaverAccessor saver = pSaver;
	DWORD dwSignature = -1;
	saver.Add( 127, &dwSignature );
	if ( dwSignature != SSpritesPack::SIGNATURE ) 
		return false;
	saver.Add( 1, this );
	return true;
}
int SSpritesPack::SSprite::SEdge::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &bHorizontal );
	saver.Add( 2, &edges );
	saver.Add( 3, &rcBoundBox );
	return 0;
}
int SSpritesPack::SSprite::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &squares );
	saver.Add( 2, &edge );
	saver.Add( 3, &center );
	if ( saver.IsReading() ) 
	{
		fMinDepth = 0;
		for ( CSquaresList::const_iterator it = squares.begin(); it != squares.end(); ++it )
		{
			if ( (it->vLeftTop.x <= 0) && (it->vLeftTop.x + it->fSize >= 0) &&
			     (it->vLeftTop.y <= 0) && (it->vLeftTop.y + it->fSize >= 0) )
			{
				fMinDepth = Min( fMinDepth, it->fDepthLeft );
				fMinDepth = Min( fMinDepth, it->fDepthRight );
			}
		}
		fMinDepth = fabs( fMinDepth );
	}
	return 0;
}
int SSpritesPack::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &sprites );
	return 0;
}
const bool SSpritesPack::SSprite::SEdge::IsInside( const CVec2 &vPos ) const
{
	if ( !rcBoundBox.IsInside( vPos ) )
	{
		return false;
	}

  if( edges.GetSize() == 0 )
	{
		return false;
	}
  
	short offset = 0;
  short index = 0; 
  short count = 0;
  
	CTPoint<short> pos( static_cast<short>( vPos.x - rcBoundBox.minx ), static_cast<short>( vPos.y - rcBoundBox.miny ) );
	
	if ( bHorizontal )
  {
    index = pos.y; 
    offset = pos.x;
  }
  else
  {
    index = pos.x; 
    offset = pos.y;
  }
  
	while( count < edges.GetLineSize( index ) )
  {
    if ( edges[index][count] == offset )
		{
			return true;
		}
    else if ( edges[index][count] < offset )
		{
			++count;
		}
    else
		{
			return ( ( count % 2 ) > 0 );
		}
  }
  
	return false;
}
