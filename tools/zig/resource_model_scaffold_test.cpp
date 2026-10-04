// Round-trip harness for Sources/src/ResourceModel/ over all 11 stats-only
// sub-editor fixtures (wpn, mcp, trc, scp, spt, unt, msh, obt, fnc, bld, bdg).
// For each extension this proves:
//   1. Load() + Save() reproduces the fixture bytes verbatim (rt_equal).
//   2. A second Save() of the same project reproduces the first Save() bytes
//      (rt_idempotent): Load->Save->Save is stable, which is what "idempotent
//      load/save" in the slice goal clause demands.
//   3. Planting a <FutureBlob attr="v"><child/></FutureBlob> element under the
//      root before serialising produces output that still contains the planted
//      bytes - a direct check that unknown subtrees survive a round-trip.
//
// Writes one line per extension to stderr:
//   SCAFFOLD <ext> bytes=<n> rt_equal=<0|1> rt_idempotent=<0|1>
//     unknown_preserved=<0|1> childs=<n> props=<n>
// (`childs`/`props` are what the task-plan "Observability Impact" asks for -
// a glance tells a future agent whether a factory registration dropped a type.)
// BK_DEBUG_LOG=1 adds depth/elements/root for each ext and prints a
// first-mismatch byte offset on failure. The harness exits 0 only when every
// extension passes rt_equal and rt_idempotent and unknown_preserved.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../../Sources/src/ResourceModel/future_blob.h"
#include "../../Sources/src/ResourceModel/items/stats_item.h"
#include "../../Sources/src/ResourceModel/project.h"
#include "../../Sources/src/ResourceModel/xml.h"

namespace
{

bool DebugLog()
{
	const char *env = std::getenv( "BK_DEBUG_LOG" );
	return env && env[0] == '1' && env[1] == 0;
}

std::string ReadAll( const std::string &path )
{
	std::ifstream in( path, std::ios::binary );
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

int TreeDepth( const NResourceXml::Node &n )
{
	int d = 0;
	for ( const auto &c : n.children )
		if ( c.kind == NResourceXml::Node::Element )
			d = std::max( d, 1 + TreeDepth( c ) );
	return d;
}

int ElementCount( const NResourceXml::Node &n )
{
	int count = ( n.kind == NResourceXml::Node::Element ) ? 1 : 0;
	for ( const auto &c : n.children )
		count += ElementCount( c );
	return count;
}

// Walk a project's root and count typed-prop values vs child items; feeds the
// "childs=<n> props=<n>" observability line in the per-ext stderr output.
void CountChildsAndProps( const NResourceModel::CTreeItem &root, int &childs, int &props )
{
	childs = (int)root.GetChildren().size();
	props = (int)root.GetValues().size();
}

bool RunExt( const std::string &ext, const std::string &path )
{
	const std::string in = ReadAll( path );
	if ( in.empty() )
	{
		std::fprintf( stderr, "SCAFFOLD %s FATAL fixture missing or empty: %s\n",
			ext.c_str(), path.c_str() );
		return false;
	}

	NResourceModel::Project project;
	std::string err;
	if ( !NResourceModel::Load( in, project, err ) )
	{
		std::fprintf( stderr, "SCAFFOLD %s FATAL parse: %s\n", ext.c_str(), err.c_str() );
		return false;
	}

	// (1) Byte-identity on the first save.
	const std::string out1 = NResourceModel::Save( project );
	const bool rt_equal = ( in == out1 );

	// (2) Idempotence - re-Load the first output and save again; the second
	// save must match the first byte-for-byte. This is the "idempotent" clause
	// of the slice goal.
	NResourceModel::Project project2;
	std::string err2;
	const bool parsed2 = NResourceModel::Load( out1, project2, err2 );
	const std::string out2 = parsed2 ? NResourceModel::Save( project2 ) : std::string();
	const bool rt_idempotent = parsed2 && ( out1 == out2 );

	// (3) Unknown-node preservation: plant a FutureBlob element under the root
	// of a freshly-loaded project, Save it, and confirm the planted bytes come
	// back out. This is the direct check the task plan calls for.
	NResourceModel::Project projectPlanted;
	std::string errPlanted;
	bool unknown_preserved = false;
	if ( NResourceModel::Load( in, projectPlanted, errPlanted ) && projectPlanted.root )
	{
		NResourceXml::Node planted;
		planted.kind = NResourceXml::Node::Element;
		planted.name = "FutureBlob";
		planted.attrs.push_back( { "attr", "v" } );
		NResourceXml::Node planted_child;
		planted_child.kind = NResourceXml::Node::Element;
		planted_child.name = "child";
		planted.children.push_back( std::move( planted_child ) );
		projectPlanted.root->AddChild(
			std::make_unique<NResourceModel::FutureBlob>( std::move( planted ) ) );
		const std::string plantedOut = NResourceModel::Save( projectPlanted );
		unknown_preserved = plantedOut.find( "<FutureBlob attr=\"v\">" ) != std::string::npos
			&& plantedOut.find( "<child/>" ) != std::string::npos;
	}

	int childs = 0, props = 0;
	if ( project.root ) CountChildsAndProps( *project.root, childs, props );

	std::fprintf( stderr,
		"SCAFFOLD %s bytes=%zu rt_equal=%d rt_idempotent=%d unknown_preserved=%d childs=%d props=%d\n",
		ext.c_str(), in.size(), rt_equal ? 1 : 0, rt_idempotent ? 1 : 0,
		unknown_preserved ? 1 : 0, childs, props );

	if ( DebugLog() )
	{
		std::fprintf( stderr, "SCAFFOLD %s depth=%d elements=%d root=%s typed=%d\n",
			ext.c_str(), TreeDepth( project.document.root ),
			ElementCount( project.document.root ),
			project.document.root.name.c_str(),
			project.root && !NResourceModel::FutureBlob::IsFutureBlob( *project.root ) ? 1 : 0 );
		if ( !rt_equal )
		{
			size_t mismatch = 0;
			size_t n = std::min( in.size(), out1.size() );
			while ( mismatch < n && in[mismatch] == out1[mismatch] ) ++mismatch;
			std::fprintf( stderr, "SCAFFOLD %s first mismatch at byte %zu (in=0x%02X out=0x%02X), sizes in=%zu out=%zu\n",
				ext.c_str(), mismatch,
				mismatch < in.size() ? (unsigned char)in[mismatch] : 0u,
				mismatch < out1.size() ? (unsigned char)out1[mismatch] : 0u,
				in.size(), out1.size() );
		}
	}

	return rt_equal && rt_idempotent && unknown_preserved;
}

}

int main( int argc, char **argv )
{
	// A single positional arg still runs one fixture (default: wpn), to keep
	// a tight loop while debugging a single sub-editor. Any further argv is
	// treated as "sweep every tracked extension" so CI never needs to shell
	// one invocation per ext.
	static const char *const kAllExts[] = {
		"wpn", "mcp", "trc", "scp", "spt", "unt", "msh", "obt", "fnc", "bld", "bdg"
	};

	if ( argc > 1 )
	{
		// Single-fixture mode: argv[1] is the project path; argv[2] (optional)
		// overrides the extension label.
		const std::string path = argv[1];
		std::string ext = "unknown";
		size_t slash = path.find_last_of( "/\\" );
		if ( slash != std::string::npos )
		{
			size_t prev = path.find_last_of( "/\\", slash - 1 );
			if ( prev != std::string::npos )
				ext = path.substr( prev + 1, slash - prev - 1 );
		}
		return RunExt( ext, path ) ? 0 : 1;
	}

	bool allOk = true;
	for ( const char *ext : kAllExts )
	{
		std::string path = std::string( "tools/zig/fixtures/resource_editor/" ) + ext
			+ "/project." + ext;
		if ( !RunExt( ext, path ) )
			allOk = false;
	}
	return allOk ? 0 : 1;
}
