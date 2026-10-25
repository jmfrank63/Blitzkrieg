// bk-views-check: what `bk-views decompose` wrote for a building variant,
// judged by the engine's own readers. Every .san of <variant-dir> is read as
// SSpritesPack::Load reads it (chunk 127 the signature, chunk 1 the pack,
// through the engine's structure saver), every sprite is unpacked from the
// _h.dds by CSpritesPackBuilder::Unpack, and the result must equal the JSON
// and the PNGs in <decompose-dir>: the fields as text in the format
// bk_views.zig writes, the pixels one by one on the variant's canvas.
//
// A test program, not a modding tool: silent on success, one line per
// difference on stderr and exit 1 otherwise.
#include "StdAfx.h"

#include <algorithm>
#include <charconv>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "data_only_startup.h"
#include "tool_host.h"
#include "../../Sources/src/Formats/fmtSprite.h"
#include "../../Sources/src/Image/Image.h"
#include "../../Sources/src/RandomMapGen/IB_Types.h"

namespace fs = std::filesystem;

namespace
{

int nDifferences = 0;

void Differ( const std::string &szWhat )
{
	fprintf( stderr, "bk-views-check: %s\n", szWhat.c_str() );
	++nDifferences;
}

std::string ReadText( const fs::path &file )
{
	std::ifstream in( file, std::ios::binary );
	return std::string( ( std::istreambuf_iterator<char>( in ) ), std::istreambuf_iterator<char>() );
}

// Zig's {d} for an f32: the shortest digits that read back, never an exponent.
std::string Float( float fValue )
{
	char buffer[64];
	const auto result = std::to_chars( buffer, buffer + sizeof( buffer ), fValue, std::chars_format::fixed );
	return std::string( buffer, result.ptr );
}

std::string Floats( const float *pValues, int nCount )
{
	std::string sz = "[";
	for ( int i = 0; i < nCount; ++i )
		sz += ( i > 0 ? "," : "" ) + Float( pValues[i] );
	return sz + "]";
}

// writePackJson of bk_views.zig, from the engine's reading.
std::string PackJson( const SSpritesPack &pack )
{
	std::string sz = NStr::Format( "{\"signature\":%u,\"sprites\":[\n", unsigned( SSpritesPack::SIGNATURE ) );
	for ( int i = 0; i < int( pack.sprites.size() ); ++i )
	{
		const SSpritesPack::SSprite &sprite = pack.sprites[i];
		const auto &edges = sprite.edge.edges;
		if ( i > 0 )
			sz += ",\n";
		const float box[4] = { sprite.edge.rcBoundBox.minx, sprite.edge.rcBoundBox.miny, sprite.edge.rcBoundBox.maxx, sprite.edge.rcBoundBox.maxy };
		sz += NStr::Format( "{\"center\":[%d,%d],\"horizontal\":%s,\"bound_box\":", sprite.center.x, sprite.center.y, sprite.edge.bHorizontal ? "true" : "false" );
		sz += Floats( box, 4 );
		sz += NStr::Format( ",\"edge_size\":%d,\"line_count\":%d,\n\"line_sizes\":[", edges.GetSize(), edges.GetLineCount() );
		for ( int k = 0; k < edges.GetLineCount(); ++k )
			sz += NStr::Format( "%s%d", k > 0 ? "," : "", int( edges.GetLineSize( k ) ) );
		sz += "],\n\"edges\":[";
		const short *pEdges = const_cast<CVarArray2D<short, BYTE> &>( edges ).GetBuffer();
		for ( int k = 0; k < edges.GetSize(); ++k )
			sz += NStr::Format( "%s%d", k > 0 ? "," : "", int( pEdges[k] ) );
		sz += "],\n\"squares\":[";
		for ( int k = 0; k < int( sprite.squares.size() ); ++k )
		{
			const SSpritesPack::SSprite::SSquare &square = sprite.squares[k];
			const float values[9] = { square.vLeftTop.x, square.vLeftTop.y, square.fSize, square.rcMaps.minx, square.rcMaps.miny,
			                          square.rcMaps.maxx, square.rcMaps.maxy, square.fDepthLeft, square.fDepthRight };
			sz += k > 0 ? ",\n" : "\n";
			sz += Floats( values, 9 );
		}
		sz += "]}";
	}
	return sz + "]}\n";
}

bool LoadPack( const fs::path &file, SSpritesPack *pPack )
{
	CPtr<IDataStream> pStream = OpenFileStream( NToolHost::EnginePath( file ), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return false;
	CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
	CSaverAccessor saver = pSaver;
	DWORD dwSignature = -1;
	saver.Add( 127, &dwSignature );
	if ( dwSignature != SSpritesPack::SIGNATURE )
		return false;
	saver.Add( 1, pPack );
	return true;
}

// The _h.dds as a plain picture: its ARGB8888 pixels are SColor's bytes.
CPtr<IImage> LoadAtlas( const fs::path &file )
{
	CPtr<IDataStream> pStream = OpenFileStream( NToolHost::EnginePath( file ), STREAM_ACCESS_READ );
	if ( pStream == 0 )
		return 0;
	CPtr<IDDSImage> pDDS = GetImageProcessor()->LoadDDSImage( pStream );
	if ( pDDS == 0 || pDDS->GetBPP() != 32 )
		return 0;
	return GetImageProcessor()->CreateImage( pDDS->GetSizeX(), pDDS->GetSizeY(), pDDS->GetLFB() );
}

CPtr<IImage> LoadPicture( const fs::path &file )
{
	CPtr<IDataStream> pStream = OpenFileStream( NToolHost::EnginePath( file ), STREAM_ACCESS_READ );
	return pStream != 0 ? GetImageProcessor()->LoadImage( pStream ) : 0;
}

struct SPackFile
{
	std::string szName;
	SSpritesPack pack;
	CPtr<IImage> pAtlas;
};

void Check( const fs::path &variant, const fs::path &decomposed )
{
	std::vector<std::string> names;
	for ( const auto &entry : fs::directory_iterator( variant ) )
	{
		const std::string szFile = entry.path().filename().string();
		std::string szExtension = szFile.size() > 4 ? szFile.substr( szFile.size() - 4 ) : std::string();
		NStr::ToLower( szExtension );
		if ( szExtension == ".san" )
			names.push_back( szFile.substr( 0, szFile.size() - 4 ) );
	}
	std::sort( names.begin(), names.end() );
	std::vector<SPackFile> files( names.size() );
	CTRect<int> canvas( INT_MAX, INT_MAX, INT_MIN, INT_MIN );
	for ( int i = 0; i < int( names.size() ); ++i )
	{
		files[i].szName = names[i];
		if ( !LoadPack( variant / ( names[i] + ".san" ), &files[i].pack ) )
			return Differ( names[i] + ".san: the engine cannot read it as a sprite pack" );
		files[i].pAtlas = LoadAtlas( variant / ( names[i] + "_h.dds" ) );
		if ( files[i].pAtlas == 0 )
			return Differ( names[i] + "_h.dds: the engine cannot read it as ARGB8888" );
		for ( auto it = files[i].pack.sprites.cbegin(); it != files[i].pack.sprites.cend(); ++it )
		{
			CTRect<int> rect;
			CSpritesPackBuilder::GetActualRect( it, &rect );
			// decompose's canvas: from the 2003 picture's corner (the zero cross at center) to the squares' far edge.
			canvas = CTRect<int>( -it->center.x, -it->center.y, Max( canvas.maxx, rect.maxx ), Max( canvas.maxy, rect.maxy ) );
		}
	}
	canvas.maxx = Max( canvas.maxx, canvas.minx + 1 );
	canvas.maxy = Max( canvas.maxy, canvas.miny + 1 );
	for ( SPackFile &file : files )
	{
		const std::string szJson = ReadText( decomposed / ( file.szName + ".json" ) );
		const std::string szEngine = PackJson( file.pack );
		if ( szJson != szEngine )
		{
			size_t nAt = 0;
			while ( nAt < szJson.size() && nAt < szEngine.size() && szJson[nAt] == szEngine[nAt] )
				++nAt;
			Differ( NStr::Format( "%s.json differs from the engine's reading at byte %d: \"%s\" against \"%s\"", file.szName.c_str(), int( nAt ),
			                      szJson.substr( nAt, 40 ).c_str(), szEngine.substr( nAt, 40 ).c_str() ) );
		}
		for ( int i = 0; i < int( file.pack.sprites.size() ); ++i )
		{
			const std::string szPicture = NStr::Format( "%s_%d.png", file.szName.c_str(), i );
			CTRect<int> rect;
			CPtr<IImage> pUnpacked = CSpritesPackBuilder::Unpack( &file.pack, file.pAtlas, i, &rect );
			CPtr<IImage> pPicture = LoadPicture( decomposed / szPicture );
			if ( pUnpacked == 0 || pPicture == 0 || pPicture->GetSizeX() != canvas.Width() || pPicture->GetSizeY() != canvas.Height() )
			{
				Differ( szPicture + ": missing, unreadable or not the canvas size" );
				continue;
			}
			int nPixels = 0;
			for ( int y = 0; y < canvas.Height(); ++y )
				for ( int x = 0; x < canvas.Width(); ++x )
				{
					const int ux = x + canvas.minx - rect.minx, uy = y + canvas.miny - rect.miny;
					DWORD dwExpected = 0;
					if ( ux >= 0 && uy >= 0 && ux < pUnpacked->GetSizeX() && uy < pUnpacked->GetSizeY() )
						dwExpected = pUnpacked->GetLine( uy )[ux].color;
					nPixels += pPicture->GetLine( y )[x].color != dwExpected;
				}
			if ( nPixels > 0 )
				Differ( NStr::Format( "%s: %d pixels differ from the engine's Unpack", szPicture.c_str(), nPixels ) );
		}
	}
}

}

int main( int argc, char *argv[] )
{
	NToolHost::BinaryStdio();
	if ( argc != 4 )
	{
		fputs( "usage: bk-views-check <variant-dir> <decompose-dir> <module-root>\n", stderr );
		return 2;
	}
	const std::string szModuleRoot = NToolHost::ModuleRoot( argv[3] ).string();
	if ( !NDataOnly::StartStreamIO( szModuleRoot.c_str() ) || !NDataOnly::StartImage( szModuleRoot.c_str() ) )
	{
		fprintf( stderr, "bk-views-check: cannot start the engine libraries in %s\n", szModuleRoot.c_str() );
		return 1;
	}
	Check( NToolHost::InputPath( argv[1] ), NToolHost::InputPath( argv[2] ) );
	NDataOnly::Stop();
	return nDifferences == 0 ? 0 : 1;
}
