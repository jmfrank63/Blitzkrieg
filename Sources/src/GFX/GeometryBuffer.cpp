#include "StdAfx.h"

#include "GeometryBuffer.h"
D3DPRIMITIVETYPE GFXPrimitiveToD3D( EGFXPrimitiveType type )
{
	switch ( type )
	{
		case GFXPT_POINTLIST:     return D3DPT_POINTLIST;
		case GFXPT_LINELIST:      return D3DPT_LINELIST;
		case GFXPT_LINESTRIP:     return D3DPT_LINESTRIP;
		case GFXPT_TRIANGLELIST:  return D3DPT_TRIANGLELIST;
		case GFXPT_TRIANGLESTRIP: return D3DPT_TRIANGLESTRIP;
	}
	NI_ASSERT_T( 0, NStr::Format("Unknown primitive type %d", type) );
	return D3DPT_TRIANGLELIST;
}
int GetNumPrimitives( D3DPRIMITIVETYPE type, int nNumElements )
{
  switch ( type )
  {
    case D3DPT_POINTLIST:     return nNumElements;
	  case D3DPT_LINELIST:      return nNumElements / 2;
	  case D3DPT_LINESTRIP:     return nNumElements - 1;
	  case D3DPT_TRIANGLELIST:  return nNumElements / 3;
	  case D3DPT_TRIANGLESTRIP: return nNumElements - 2;
  }
	return 0;
}
int GetVertexSize( DWORD dwFormat )
{
	/*
	CPow2Allocator alloc( 65536 );
	std::list<SRangeLimits> ranges;
	for ( int i=0; i<10000; ++i )
	{
		int nAmount = rand() % 32768;
		SRangeLimits range;
		if ( alloc.Allocate( nAmount, &range ) == EAV_SUCCESS )
		{
			ranges.push_back( range );
		}
		else if ( !ranges.empty() )
		{
			alloc.Free( ranges.front() );
			ranges.pop_front();
		}
		int nAllocated = 0;
		for ( std::list<SRangeLimits>::const_iterator it = ranges.begin(); it != ranges.end(); ++it )
			nAllocated += it->second;
		float fEffect = float( nAllocated ) / float( 65536 - alloc.GetFree() );

	}
	NStr::DebugTrace( "num blocks = %d. Allocated %d\n", alloc.GetNumBlocks(), 65536 - alloc.GetFree() );
	alloc.TestRanges();
	while ( !ranges.empty() ) 
	{
		alloc.Free( ranges.back() );
		ranges.pop_back();
		alloc.TestRanges();
	}
	NStr::DebugTrace( "num blocks = %d. Allocated %d\n", alloc.GetNumBlocks(), 65536 - alloc.GetFree() );

	alloc.TestRanges();
	*/

	DWORD dwSize = 0;
	if ( dwFormat & D3DFVF_XYZ )
		dwSize += 3 * sizeof( float );
	else if ( dwFormat & D3DFVF_XYZRHW )
		dwSize += 4 * sizeof( float );
	else if ( dwFormat & D3DFVF_XYZB1 )
		dwSize += 4 * sizeof( float );
	else if ( dwFormat & D3DFVF_XYZB2 )
		dwSize += 5 * sizeof( float );
	else if ( dwFormat & D3DFVF_XYZB3 )
		dwSize += 6 * sizeof( float );
	else if ( dwFormat & D3DFVF_XYZB4 )
		dwSize += 7 * sizeof( float );
	else if ( dwFormat & D3DFVF_XYZB5 )
		dwSize += 8 * sizeof( float );
	if ( dwFormat & D3DFVF_NORMAL )
		dwSize += 3 * sizeof( float );
	if ( dwFormat & D3DFVF_DIFFUSE  )
		dwSize += sizeof( DWORD );
	if ( dwFormat & D3DFVF_SPECULAR )
		dwSize += sizeof( DWORD );
	DWORD dwTexture = dwFormat & D3DFVF_TEXCOUNT_MASK;
	if ( dwTexture == D3DFVF_TEX0 )
		dwSize += 0;
	else if ( dwTexture == D3DFVF_TEX1 )
		dwSize += 1 * sizeof( float ) * 2;
	else if ( dwTexture == D3DFVF_TEX2 )
		dwSize += 2 * sizeof( float ) * 2;
	else if ( dwTexture == D3DFVF_TEX3 )
		dwSize += 3 * sizeof( float ) * 2;
	else if ( dwTexture == D3DFVF_TEX4 )
		dwSize += 4 * sizeof( float ) * 2;
	else if ( dwTexture == D3DFVF_TEX5 )
		dwSize += 5 * sizeof( float ) * 2;
	else if ( dwTexture == D3DFVF_TEX6 )
		dwSize += 6 * sizeof( float ) * 2;
	else if ( dwTexture == D3DFVF_TEX7 )
		dwSize += 7 * sizeof( float ) * 2;
	else if ( dwTexture == D3DFVF_TEX8 )
		dwSize += 8 * sizeof( float ) * 2;
	return dwSize;
}
int GetIndexSize( DWORD dwFormat )
{
	if ( dwFormat == D3DFMT_INDEX16 )
		return 2;
	else if ( dwFormat == D3DFMT_INDEX32 )
		return 4;
	NI_ASSERT_T( 0, NStr::Format("Unknown index format %d", dwFormat) );
	return 0;
}
